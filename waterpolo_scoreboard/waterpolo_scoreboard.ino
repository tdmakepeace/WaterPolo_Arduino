/*
 * Water Polo Scoreboard
 * Board: Arduino Mega 2560
 * Display: Waveshare RGB-Matrix-P3-64x32 (SKU 33840, HUB75)
 * Mirror: Freenove I2C IIC LCD 1602 (16x2) on Mega SDA/SCL
 * Remote shot clocks: LoRa TX via Serial1 (same protocol as lora_remote2)
 *
 * Matrix driver: Waveshare RGBmatrixPanel + Adafruit_GFX vendored in this
 * folder (same files as test_waveshare/). Call Reginit() before matrix.begin().
 *
 * Libraries (Library Manager):
 *   - LiquidCrystal I2C (e.g. Frank de Brabander / Freenove zip)
 *
 * Buttons (active LOW, internal pull-ups — wire other side to GND):
 *   D2  = period clock start / stop
 *   D3  = force shot clock to 18 s (always; pauses period clock)
 *   D4  = exclusion (18 s, no reset) — 1st press clock 1, 2nd press clock 2
 *   D5  = shot clock set to 18 s if < 18 (preserves running/stopped state)
 *   D6  = shot → 28 (preserves period clock running/stopped state; clears exclusions)
 *   D7  = timeout 1:00 (pauses period+shot; independent countdown)
 *   D8  = interval (reloads match period length + shot 28; advances period 1→4;
 *         keeps remaining exclusions; after period 2 uses HALF_TIME_SECONDS,
 *         else INTERVAL_SECONDS)
 *   D35 = RETURN — short: end TO/IN early (no horn); long ~3 s: end TO/IN with
 *         buzzer (relay + LoRa). Not in TO/IN: long press is buzzer only.
 *         Also exits the timing menu.
 *   D36 = period +1 s (long press +10 s); in timing menu +30 s (shot pages ±1 s)
 *   D37 = period -1 s (long press -30 s); in timing menu -30 s (shot pages ±1 s)
 *   D38 = shot +1 s (long press +10 s)
 *   D39 = shot -1 s (long press -10 s)
 *   D40 = home score +  (also stops period clock, shot → 28, clears exclusions)
 *   D41 = home score -
 *   D42 = away score +  (also stops period clock, shot → 28, clears exclusions)
 *   D43 = away score -
 *
 * Relay output: D12 drives a 5V relay (HIGH = energised by default)
 * LoRa: Serial1 @ LORA_BAUD — Mega TX1=18 / RX1=19
 *
 * Shot clock runs while play is active (D2 / START in play mode).
 * In CLOCK=RUN menu mode the period may keep running while shot/exclusions are stopped.
 * When period remaining is less than the shot clock, the shot clock shows
 * (and follows) the period remaining — including when PERIOD_SECONDS /
 * match length is shorter than a full shot (e.g. 28 s).
 * At shot = 0: LoRa BUZZER, stop play (period keeps in RUN mode), reset shot to 28 s.
 * Exclusions: STOP and RUN follow shot/play; RUN-NS follows the period clock.
 * Remaining exclusion time is kept across INTERVAL / half-time into the next period.
 * A goal (home+ / away+) or D6 (shot → 28) clears both exclusion clocks.
 * Period counter P1–P4 advances on INTERVAL; half-time after P2 (display HT).
 * When the period clock hits 0: auto-start IN/HT for P1–P3; P4 stays at 0:00.
 * Period length defaults to PERIOD_SECONDS (8:00). Adjusting the period clock
 * before the first START of that period updates the match length carried to
 * later periods (INTERVAL / D2 long-press reload).
 *
 * D2 short press: start/stop immediately (play mode only).
 * Hold D2 ~3 s (STOP mode, from stopped): resets period clock to match length.
 * Hold D2 ~3 s (RUN mode): stops the period clock (and play).
 * Hold D2 + D35 ~3 s: reload period to match length; keeps shot + exclusions.
 * Hold D2 + D6 together for 3 s: full reset (scores, clocks, exclusions → defaults).
 * Hold D5 + D6 ~3 s: timing menu (PERIOD → INTERVAL → HALFTIME → TIMEOUT →
 *   SHOT 28 → SHOT 18 → CLOCK).
 *   D36 / D37 = ±30 s, shot pages ±1 s, CLOCK page: cycle STOP/RUN/RUN-NS.
 *   D2 = next / confirm · D35 exit.
 * D35 short: silent return from TO/IN. Hold D35 ~3 s: buzzer; also returns if in TO/IN.
 *
 * CLOCK menu: STOP (default) = D2 toggles period + shot + exclusions together.
 * RUN = first D2 of quarter (or after timeout) starts period + play; later D2 toggles
 * shot + exclusions only while period keeps running (timeout still stops period).
 * RUN-NS = same period behaviour as RUN but shot clock disabled (hidden, no tick/expiry).
 * Long D2 in RUN/RUN-NS stops the period; D2+D35 reloads period length without clearing shot/excl.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RGBmatrixPanel.h"
#include "Adafruit_GFX.h"

// ----- Button pins -----
const uint8_t PIN_CLOCK_TOGGLE = 2;
const uint8_t PIN_SHOT_FORCE18 = 3;   // force shot → 18
const uint8_t PIN_EXCL         = 4;   // exclusion: 1st press clock 1, 2nd press clock 2
const uint8_t PIN_SHOT_18      = 5;   // → 18 s if < 18
const uint8_t PIN_SHOT_RESET   = 6;   // → 28 s
const uint8_t PIN_TIMEOUT      = 7;   // 1:00 timeout
const uint8_t PIN_INTERVAL     = 8;   // between-period break (HT after P2)
const uint8_t PIN_RELAY        = 12;  // 5 V relay coil / module IN
const uint8_t PIN_RETURN       = 35;  // short: end TO/IN; long 3 s: buzzer (+ return)
const uint8_t PIN_PERIOD_INC   = 36;  // period +1 s (long +10 s)
const uint8_t PIN_PERIOD_DEC   = 37;  // period -1 s (long -30 s)
const uint8_t PIN_SHOT_INC     = 38;  // shot +1 s
const uint8_t PIN_SHOT_DEC     = 39;  // shot -1 s
const uint8_t PIN_HOME_INC     = 40;
const uint8_t PIN_HOME_DEC     = 41;
const uint8_t PIN_AWAY_INC     = 42;
const uint8_t PIN_AWAY_DEC     = 43;

// true  = HIGH energises relay (bare transistor driver / many modules)
// false = LOW energises relay (common optocoupler relay boards marked LOW)
const bool RELAY_ACTIVE_HIGH = true;

// ----- HUB75 pins (working Waveshare 64x32 — do not swap LAT/OE) -----
// R1 D24  G1 D25  B1 D26  R2 D27  G2 D28  B2 D29  (PORTA, fixed by lib)
// Ribbon: yellow #14 → LAT D9,  green #15 → OE D10,  orange #13 → CLK D11
#define CLK 11
#define OE  10
#define LAT  9
#define A   A0
#define B   A1
#define C   A2
#define D   A3

const bool MATRIX_DOUBLE_BUFFER = true;
const uint8_t MATRIX_BRIGHT = 3;
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, MATRIX_DOUBLE_BUFFER, 64);
uint16_t COL_HOME, COL_AWAY, COL_CLOCK, COL_SHOT, COL_LABEL, COL_DIM, COL_EXCL;

// Dead columns on some panels (49–52). Keep UI left of this.
const int MATRIX_SAFE_WIDTH = 49;

// Waveshare panel register init — MUST run before matrix.begin()
void Reginit()
{
  pinMode(24, OUTPUT); // R1
  pinMode(25, OUTPUT); // G1
  pinMode(26, OUTPUT); // B1

  pinMode(27, OUTPUT); // R2
  pinMode(28, OUTPUT); // G2
  pinMode(29, OUTPUT); // B2

  pinMode(CLK, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(LAT, OUTPUT);

  digitalWrite(OE, HIGH);
  digitalWrite(LAT, LOW);
  digitalWrite(CLK, LOW);

  int MaxLed = 64;

  int C12[16] =
  {
    0, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1
  };

  int C13[16] =
  {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 0, 0
  };

  for (int l = 0; l < MaxLed; l++)
  {
    int y = l % 16;

    digitalWrite(24, LOW);
    digitalWrite(25, LOW);
    digitalWrite(26, LOW);
    digitalWrite(27, LOW);
    digitalWrite(28, LOW);
    digitalWrite(29, LOW);

    if (C12[y] == 1)
    {
      digitalWrite(24, HIGH);
      digitalWrite(25, HIGH);
      digitalWrite(26, HIGH);
      digitalWrite(27, HIGH);
      digitalWrite(28, HIGH);
      digitalWrite(29, HIGH);
    }

    if (l > MaxLed - 12)
      digitalWrite(LAT, HIGH);
    else
      digitalWrite(LAT, LOW);

    digitalWrite(CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(CLK, LOW);
  }

  digitalWrite(LAT, LOW);
  digitalWrite(CLK, LOW);

  for (int l = 0; l < MaxLed; l++)
  {
    int y = l % 16;

    digitalWrite(24, LOW);
    digitalWrite(25, LOW);
    digitalWrite(26, LOW);
    digitalWrite(27, LOW);
    digitalWrite(28, LOW);
    digitalWrite(29, LOW);

    if (C13[y] == 1)
    {
      digitalWrite(24, HIGH);
      digitalWrite(25, HIGH);
      digitalWrite(26, HIGH);
      digitalWrite(27, HIGH);
      digitalWrite(28, HIGH);
      digitalWrite(29, HIGH);
    }

    if (l > MaxLed - 13)
      digitalWrite(LAT, HIGH);
    else
      digitalWrite(LAT, LOW);

    digitalWrite(CLK, HIGH);
    delayMicroseconds(2);
    digitalWrite(CLK, LOW);
  }

  digitalWrite(LAT, LOW);
  digitalWrite(CLK, LOW);
}

// ----- Freenove I2C LCD 1602 (SDA=20, SCL=21 on Mega) -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool i2CAddrTest(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void initI2cLcd() {
  Wire.begin();
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

// ----- LoRa (Serial1) -----
const unsigned long LORA_BAUD = 9600;
const int LORA_AUX_PIN = -1;

// ---------------------------------------------------------------------------
// LoRa CHANNEL CONFIGURATION (same table as lora_remote)
// ---------------------------------------------------------------------------
const long LORA_CHANNEL_FREQS[] = {
  433050000L,  // CH0  433.050 MHz
  433100000L,  // CH1  433.100 MHz
  433150000L,  // CH2  433.150 MHz
  433200000L   // CH3  433.200 MHz
};
const uint8_t LORA_CHANNEL_COUNT =
    sizeof(LORA_CHANNEL_FREQS) / sizeof(LORA_CHANNEL_FREQS[0]);
const uint8_t LORA_DEFAULT_CHANNEL = 2;  // 433.150 MHz

// Pool link: ~30 m, over water, bodies in the path.
// SF7 keeps shot-clock packets well under the 0.5 s resend window.
// 22 dBm is module max — fade margin through water and people, not range.
const uint8_t LORA_SPREAD_FACTOR = 7;   // SF5–12; 7 = fast, plenty for 30 m
const uint8_t LORA_TX_POWER_DBM  = 22;  // 0–22 dBm; 22 = max punch through bodies

// USB Serial (9600) logs AT commands, TX packets, and any module UART replies.
const bool LORA_TX_LOG = false;

// ----- Scoreboard state -----
const int PERIOD_SECONDS     = 8 * 60;       // 8:00 default match period length
const int INTERVAL_SECONDS   = 2 * 60;       // default between P1–P2 and P3–P4
const int HALF_TIME_SECONDS  = 2 * 60;       // default after P2 (official half-time is 5:00)
const int TIMEOUT_SECONDS    = 60;           // default timeout
const int PERIOD_MAX         = 4;            // P1–P4
const int SHOT_FULL          = 28;           // default 28s RESET length
const int SHOT_PARTIAL       = 18;           // default 18s RESET / FORCE 18
const int EXCLUSION_SECONDS  = 18;

const int MENU_STEP_SECONDS    = 30;
const int PERIOD_MENU_MIN      = 30;
const int PERIOD_MENU_MAX      = 15 * 60;
const int INTERVAL_MENU_MIN    = 0;
const int INTERVAL_MENU_MAX    = 3 * 60;
const int HALF_TIME_MENU_MIN   = 0;
const int HALF_TIME_MENU_MAX   = 7 * 60;
const int TIMEOUT_MENU_MIN     = 30;
const int TIMEOUT_MENU_MAX     = 5 * 60;
const int SHOT_MENU_MIN        = 1;
const int SHOT_MENU_MAX        = 60;
const uint8_t MENU_ITEM_PERIOD    = 0;
const uint8_t MENU_ITEM_INTERVAL  = 1;
const uint8_t MENU_ITEM_HALFTIME = 2;
const uint8_t MENU_ITEM_TIMEOUT   = 3;
const uint8_t MENU_ITEM_SHOT28    = 4;
const uint8_t MENU_ITEM_SHOT18    = 5;
const uint8_t MENU_ITEM_CLOCK     = 6;
const uint8_t MENU_ITEM_COUNT     = 7;

int homeScore = 0;
int awayScore = 0;
int periodNum = 1;         // current period 1–PERIOD_MAX
int periodLength = PERIOD_SECONDS;  // carried match length (set before first START)
int intervalLength = INTERVAL_SECONDS;
int halfTimeLength = HALF_TIME_SECONDS;
int timeoutLength = TIMEOUT_SECONDS;
int shotFull = SHOT_FULL;          // 28s RESET / goal / expiry reload
int shotPartial = SHOT_PARTIAL;    // 18s RESET (if shorter) / FORCE 18
int secondsLeft = PERIOD_SECONDS;
int shotLeft = SHOT_FULL;
int timeoutLeft = 0;       // >0 = timeout mode
int intervalLeft = 0;      // >0 = interval / half-time mode
int excl1Left = 0;         // 0 = inactive; cannot restart while >0
int excl2Left = 0;
int lastShotSent = -1;
bool clockRunning = false;     // play: shot (exclusions follow period in RUN / RUN-NS)
bool periodRunning = false;    // main period countdown (may run without play in RUN mode)
const uint8_t CLOCK_MODE_STOP   = 0;
const uint8_t CLOCK_MODE_RUN    = 1;
const uint8_t CLOCK_MODE_RUN_NS = 2;
uint8_t clockMode = CLOCK_MODE_STOP;  // menu CLOCK: STOP / RUN / RUN-NS
bool intervalIsHalfTime = false;
bool periodStarted = false;  // true after first START of current period

bool inSettingsMenu = false;
uint8_t menuItem = 0;          // 0 PERIOD · 1 INTERVAL · 2 HALFTIME · 3 TIMEOUT · 4 CLOCK

bool inTimeout() { return timeoutLeft > 0; }
bool inInterval() { return intervalLeft > 0; }
bool inPlay() { return !inTimeout() && !inInterval() && !inSettingsMenu; }
bool inMenu() { return inSettingsMenu; }

void markDirty();
void setShotClock(int value);
void syncShotWithPeriod();
void ackCommand();
void drawLcd();
void serviceLcdAck();
void clearExclusions();
bool startNextExclusion();
void enterSettingsMenu();
void exitSettingsMenu();
void adjustMenuValue(int delta);
void serviceMenuCombo(uint32_t now);
void forwardToLoRa(const char *command);
void soundBuzzer();
void endTimeoutOrInterval();
void serviceReturnLongPress(uint32_t now);
void stopAllClocks();
void stopPlayClocks();
void startPlayClocks(uint32_t now);
void toggleClockToggle(uint32_t now);
bool menuIsClockItem();
bool menuIsShotItem();
void toggleClockMode();
const char *menuClockLabel();
bool isRunningClock();
bool shotClockEnabled();
bool exclTicking();

uint32_t lastTickMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastLoRaResendMs = 0;
const uint32_t LORA_RESEND_MS = 500;
bool displayDirty = true;

uint32_t lcdAckUntilMs = 0;
const uint16_t LCD_ACK_MS = 300;  // command-ack '*' flash on LCD bottom-right

uint32_t relayOffAtMs = 0;
const uint16_t RELAY_SHOT_MS = 500;
const uint16_t RELAY_PERIOD_MS = 1000;
const uint16_t RELAY_TO_MS = 500;

struct Btn {
  uint8_t pin;
  bool stable;
  bool lastRaw;
  uint32_t lastChangeMs;
};

Btn buttons[] = {
  {PIN_CLOCK_TOGGLE, HIGH, HIGH, 0},
  {PIN_SHOT_FORCE18, HIGH, HIGH, 0},
  {PIN_EXCL,         HIGH, HIGH, 0},
  {PIN_SHOT_18,      HIGH, HIGH, 0},
  {PIN_SHOT_RESET,   HIGH, HIGH, 0},
  {PIN_TIMEOUT,      HIGH, HIGH, 0},
  {PIN_INTERVAL,     HIGH, HIGH, 0},
  {PIN_RETURN,       HIGH, HIGH, 0},
  {PIN_PERIOD_INC,   HIGH, HIGH, 0},
  {PIN_PERIOD_DEC,   HIGH, HIGH, 0},
  {PIN_SHOT_INC,     HIGH, HIGH, 0},
  {PIN_SHOT_DEC,     HIGH, HIGH, 0},
  {PIN_HOME_INC,     HIGH, HIGH, 0},
  {PIN_HOME_DEC,     HIGH, HIGH, 0},
  {PIN_AWAY_INC,     HIGH, HIGH, 0},
  {PIN_AWAY_DEC,     HIGH, HIGH, 0},
};

const uint8_t BTN_COUNT = sizeof(buttons) / sizeof(buttons[0]);
const uint16_t DEBOUNCE_MS = 15;
const uint16_t LONG_PRESS_MS = 3000;   // D2 solo: STOP reload / RUN stop period
const uint16_t ADJUST_LONG_MS = 800;   // D36 +10 / D37 -30; shot ±10
const uint16_t RETURN_LONG_MS = 3000;  // D35 long → buzzer (+ return if TO/IN)
const uint16_t COMBO_RESET_MS = 3000;  // D2 + D6 held → full reset
const uint16_t COMBO_PERIOD_RELOAD_MS = 3000;  // D2 + D35 → periodLength, keep shot/excl
const uint16_t MENU_HOLD_MS   = 3000;  // D5 + D6 held → timing menu

uint32_t clockBtnDownMs = 0;
bool clockLongHandled = false;
bool clockWasRunningAtPress = false;
bool clockWasPeriodRunningAtPress = false;
bool clockPendingStartPlay = false;  // RUN: defer start-play until release if period already live

uint8_t adjustPinDown = 0;
uint32_t adjustDownMs = 0;
bool adjustLongHandled = false;

uint32_t returnBtnDownMs = 0;
bool returnLongHandled = false;

uint32_t comboResetDownMs = 0;
bool comboResetHandled = false;

uint32_t periodReloadComboDownMs = 0;
bool periodReloadComboHandled = false;

uint32_t menuComboDownMs = 0;
bool menuComboHandled = false;
bool pendingShot18 = false;
bool pendingShot28 = false;

// Ignore D2 start/stop edges shortly after D6/D5 (avoids ghost toggles)
uint32_t ignoreClockToggleUntilMs = 0;
const uint16_t SHOT_BTN_CLOCK_GUARD_MS = 80;

bool buttonIsDown(uint8_t pin) {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    if (buttons[i].pin == pin) return buttons[i].stable == LOW;
  }
  return false;
}

bool isAdjustPin(uint8_t pin) {
  return pin == PIN_PERIOD_INC || pin == PIN_PERIOD_DEC ||
         pin == PIN_SHOT_INC || pin == PIN_SHOT_DEC;
}

void applyAdjust(uint8_t pin, int deltaSec) {
  if (pin == PIN_PERIOD_INC || pin == PIN_PERIOD_DEC) {
    if (!inPlay()) return;
    secondsLeft = constrain(secondsLeft + deltaSec, 0, PERIOD_MENU_MAX);
    if (secondsLeft == 0) stopAllClocks();
    // Before first START of this period: carry adjusted length to later periods
    if (!periodStarted && secondsLeft > 0) {
      periodLength = secondsLeft;
    }
    syncShotWithPeriod();
    ackCommand();
  } else if (pin == PIN_SHOT_INC || pin == PIN_SHOT_DEC) {
    if (!shotClockEnabled()) return;
    setShotClock(constrain(shotLeft + deltaSec, 0, shotFull));
    ackCommand();
  }
}

void serviceAdjustLongPress(uint32_t now) {
  if (inSettingsMenu) return;
  if (adjustPinDown == 0 || adjustLongHandled) return;
  if ((now - adjustDownMs) < ADJUST_LONG_MS) return;
  int step;
  if (adjustPinDown == PIN_PERIOD_INC)      step = 10;
  else if (adjustPinDown == PIN_PERIOD_DEC) step = -30;
  else if (adjustPinDown == PIN_SHOT_INC)   step = 10;
  else                                      step = -10;
  applyAdjust(adjustPinDown, step);
  adjustLongHandled = true;
}

void serviceReturnLongPress(uint32_t now) {
  if (inSettingsMenu) return;
  if (returnBtnDownMs == 0 || returnLongHandled) return;
  // D2 held → period-reload combo owns RETURN; do not buzz
  if (buttonIsDown(PIN_CLOCK_TOGGLE)) return;
  if ((now - returnBtnDownMs) < RETURN_LONG_MS) return;
  // Horn + remote buzzers. Also leave TO/IN if that is the current mode.
  bool leavingBreak = inTimeout() || inInterval();
  soundBuzzer();
  if (leavingBreak) endTimeoutOrInterval();
  returnLongHandled = true;
  ackCommand();
}

void stopAllClocks() {
  clockRunning = false;
  periodRunning = false;
}

bool isRunningClock() {
  return clockMode != CLOCK_MODE_STOP;
}

bool shotClockEnabled() {
  return clockMode != CLOCK_MODE_RUN_NS;
}

bool exclTicking() {
  // STOP + RUN: exclusions start/stop with shot/play; RUN-NS: with period
  return clockMode == CLOCK_MODE_RUN_NS ? periodRunning : clockRunning;
}

void stopPlayClocks() {
  clockRunning = false;
  if (!isRunningClock()) {
    periodRunning = false;
  }
}

void startPlayClocks(uint32_t now) {
  clockRunning = true;
  periodRunning = true;
  periodStarted = true;
  lastTickMs = now;
}

void toggleClockToggle(uint32_t now) {
  if (!isRunningClock()) {
    if (clockRunning) {
      stopAllClocks();
    } else if (secondsLeft > 0) {
      startPlayClocks(now);
    }
  } else {
    if (!periodRunning) {
      if (secondsLeft > 0) {
        startPlayClocks(now);
      }
    } else if (clockRunning) {
      stopPlayClocks();
    } else if (secondsLeft > 0) {
      clockRunning = true;
      lastTickMs = now;
    }
  }
}

bool menuIsClockItem() {
  return menuItem == MENU_ITEM_CLOCK;
}

bool menuIsShotItem() {
  return menuItem == MENU_ITEM_SHOT28 || menuItem == MENU_ITEM_SHOT18;
}

void toggleClockMode() {
  clockMode = (clockMode + 1) % 3;
  if (shotClockEnabled()) {
    sendShotToLoRa(shotLeft, true);
  } else {
    forwardToLoRa("0");
    lastShotSent = 0;
  }
  ackCommand();
}

const char *menuClockLabel() {
  switch (clockMode) {
    case CLOCK_MODE_RUN:    return "RUN";
    case CLOCK_MODE_RUN_NS: return "RUN-NS";
    default:                return "STOP";
  }
}

void fullResetToDefaults() {
  stopAllClocks();
  clockMode = CLOCK_MODE_STOP;
  homeScore = 0;
  awayScore = 0;
  periodNum = 1;
  periodLength = PERIOD_SECONDS;
  intervalLength = INTERVAL_SECONDS;
  halfTimeLength = HALF_TIME_SECONDS;
  timeoutLength = TIMEOUT_SECONDS;
  shotFull = SHOT_FULL;
  shotPartial = SHOT_PARTIAL;
  periodStarted = false;
  secondsLeft = PERIOD_SECONDS;
  timeoutLeft = 0;
  intervalLeft = 0;
  intervalIsHalfTime = false;
  inSettingsMenu = false;
  clearExclusions();
  setShotClock(shotFull);
  pulseRelay(RELAY_TO_MS);
  ackCommand();
}

void serviceComboReset(uint32_t now) {
  if (inSettingsMenu) return;
  bool both = buttonIsDown(PIN_CLOCK_TOGGLE) && buttonIsDown(PIN_SHOT_RESET);
  if (!both) {
    comboResetDownMs = 0;
    comboResetHandled = false;
    return;
  }
  if (comboResetDownMs == 0) comboResetDownMs = now;
  if (!comboResetHandled && (now - comboResetDownMs) >= COMBO_RESET_MS) {
    fullResetToDefaults();
    comboResetHandled = true;
    // Suppress leftover D2 long-press reset
    clockLongHandled = true;
    clockPendingStartPlay = false;
  }
}

void reloadPeriodKeepShotExcl() {
  stopAllClocks();
  secondsLeft = periodLength;
  // Keep shotLeft and exclusion clocks as-is (no syncShotWithPeriod / clearExclusions)
  markDirty();
}

void servicePeriodReloadCombo(uint32_t now) {
  if (inSettingsMenu) return;
  bool both = buttonIsDown(PIN_CLOCK_TOGGLE) && buttonIsDown(PIN_RETURN);
  if (!both) {
    periodReloadComboDownMs = 0;
    periodReloadComboHandled = false;
    return;
  }
  if (periodReloadComboDownMs == 0) periodReloadComboDownMs = now;
  if (!periodReloadComboHandled &&
      (now - periodReloadComboDownMs) >= COMBO_PERIOD_RELOAD_MS) {
    reloadPeriodKeepShotExcl();
    periodReloadComboHandled = true;
    clockLongHandled = true;
    clockPendingStartPlay = false;
    returnLongHandled = true;
    ackCommand();
  }
}

// ---------------------------------------------------------------------------
// Relay
// ---------------------------------------------------------------------------

void relayWrite(bool on) {
  bool level = RELAY_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
}

void pulseRelay(uint16_t durationMs) {
  relayWrite(true);
  relayOffAtMs = millis() + durationMs;
}

void soundBuzzer() {
  pulseRelay(RELAY_TO_MS);
  forwardToLoRa("BUZZER");
}

void endTimeoutOrInterval() {
  timeoutLeft = 0;
  intervalLeft = 0;
  intervalIsHalfTime = false;
}

void serviceRelay() {
  if (relayOffAtMs == 0) return;
  if ((int32_t)(millis() - relayOffAtMs) >= 0) {
    relayWrite(false);
    relayOffAtMs = 0;
  }
}

// ---------------------------------------------------------------------------
// LoRa
// ---------------------------------------------------------------------------

void waitLoRaAux() {
  if (LORA_AUX_PIN < 0) return;
  uint32_t waitStart = millis();
  while (digitalRead(LORA_AUX_PIN) == LOW) {
    if (millis() - waitStart > 500) break;
  }
}

void logLoRa(const char *tag, const char *command) {
  if (!LORA_TX_LOG) return;
  Serial.print(tag);
  Serial.println(command);
}

void forwardToLoRa(const char *command) {
  waitLoRaAux();
  Serial1.println(command);
  logLoRa("LoRa TX: ", command);
}

void forwardLoRaAT(const char *command) {
  waitLoRaAux();
  Serial1.println(command);
  logLoRa("LoRa AT: ", command);
}

void setLoRaChannel(uint8_t ch) {
  if (ch >= LORA_CHANNEL_COUNT) return;
  char cmd[28];
  snprintf(cmd, sizeof(cmd), "AT+FREQ=%ld", LORA_CHANNEL_FREQS[ch]);
  forwardLoRaAT(cmd);
}

void configureLoRa() {
  setLoRaChannel(LORA_DEFAULT_CHANNEL);
  delay(50);

  char cmd[14];
  snprintf(cmd, sizeof(cmd), "AT+SF%u", LORA_SPREAD_FACTOR);
  forwardLoRaAT(cmd);
  delay(50);

  snprintf(cmd, sizeof(cmd), "AT+POWE%u", LORA_TX_POWER_DBM);
  forwardLoRaAT(cmd);
  delay(50);
}

void pollLoRaModuleEcho() {
  if (!LORA_TX_LOG) return;
  char line[48];
  size_t n = 0;
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (n > 0) {
        line[n] = '\0';
        logLoRa("LoRa MOD: ", line);
        n = 0;
      }
      continue;
    }
    if (n + 1 < sizeof(line)) line[n++] = c;
  }
}

void sendShotToLoRa(int value, bool force) {
  if (!shotClockEnabled()) return;
  value = constrain(value, 0, 99);
  if (!force && value == lastShotSent) return;

  char cmd[4];
  snprintf(cmd, sizeof(cmd), "%d", value);
  forwardToLoRa(cmd);

  lastShotSent = value;
  lastLoRaResendMs = millis();
}

// In play mode, shot never exceeds period remaining (display + LoRa remotes).
void syncShotWithPeriod() {
  if (!shotClockEnabled()) return;
  if (inTimeout() || inInterval()) return;
  if (shotLeft <= secondsLeft) return;
  shotLeft = secondsLeft;
  sendShotToLoRa(shotLeft, true);
  markDirty();
}

void setShotClock(int value) {
  shotLeft = constrain(value, 0, 99);
  if (inPlay() && shotLeft > secondsLeft) {
    shotLeft = secondsLeft;
  }
  if (shotClockEnabled()) {
    sendShotToLoRa(shotLeft, true);
  }
  markDirty();
}

void onShotExpired() {
  stopPlayClocks();
  pulseRelay(RELAY_SHOT_MS);

  forwardToLoRa("0");
  forwardToLoRa("BUZZER");

  shotLeft = shotFull;
  sendShotToLoRa(shotLeft, true);
  markDirty();
}

void startTimeout() {
  stopAllClocks();
  intervalLeft = 0;
  timeoutLeft = timeoutLength;
  lastTickMs = millis();
  markDirty();
}

void startInterval() {
  stopAllClocks();
  timeoutLeft = 0;
  // Half-time after period 2 (before advancing to P3)
  intervalIsHalfTime = (periodNum == 2);
  int breakSec = intervalIsHalfTime ? halfTimeLength : intervalLength;
  if (periodNum < PERIOD_MAX) periodNum++;
  secondsLeft = periodLength;
  periodStarted = false;
  if (shotClockEnabled()) {
    setShotClock(shotFull);
  }
  // Keep remaining exclusion time — it resumes when the next period starts
  intervalLeft = breakSec;
  lastTickMs = millis();
  markDirty();
}

void clearExclusions() {
  excl1Left = 0;
  excl2Left = 0;
}

void startExclusion(int *slot) {
  // Cannot reset while active — only arm when idle (0)
  if (*slot > 0) return;
  *slot = EXCLUSION_SECONDS;
  markDirty();
}

// Single D4 button: first free slot is clock 1, then clock 2. Both busy → no-op.
bool startNextExclusion() {
  if (excl1Left == 0) {
    startExclusion(&excl1Left);
    return true;
  }
  if (excl2Left == 0) {
    startExclusion(&excl2Left);
    return true;
  }
  return false;
}

int *menuValueSlot() {
  switch (menuItem) {
    case MENU_ITEM_PERIOD:    return &periodLength;
    case MENU_ITEM_INTERVAL:  return &intervalLength;
    case MENU_ITEM_HALFTIME: return &halfTimeLength;
    case MENU_ITEM_TIMEOUT:   return &timeoutLength;
    case MENU_ITEM_SHOT28:    return &shotFull;
    case MENU_ITEM_SHOT18:    return &shotPartial;
    default:                  return &timeoutLength;
  }
}

void menuValueBounds(int *lo, int *hi) {
  switch (menuItem) {
    case MENU_ITEM_PERIOD:
      *lo = PERIOD_MENU_MIN;
      *hi = PERIOD_MENU_MAX;
      break;
    case MENU_ITEM_INTERVAL:
      *lo = INTERVAL_MENU_MIN;
      *hi = INTERVAL_MENU_MAX;
      break;
    case MENU_ITEM_HALFTIME:
      *lo = HALF_TIME_MENU_MIN;
      *hi = HALF_TIME_MENU_MAX;
      break;
    case MENU_ITEM_TIMEOUT:
      *lo = TIMEOUT_MENU_MIN;
      *hi = TIMEOUT_MENU_MAX;
      break;
    default:
      *lo = SHOT_MENU_MIN;
      *hi = SHOT_MENU_MAX;
      break;
  }
}

const char *menuItemName() {
  switch (menuItem) {
    case MENU_ITEM_PERIOD:    return "PERIOD";
    case MENU_ITEM_INTERVAL:  return "INTERVAL";
    case MENU_ITEM_HALFTIME: return "HALFTIME";
    case MENU_ITEM_TIMEOUT:   return "TIMEOUT";
    case MENU_ITEM_SHOT28:    return "SHOT 28";
    case MENU_ITEM_SHOT18:    return "SHOT 18";
    default:                  return "CLOCK";
  }
}

int menuItemValue() {
  return *menuValueSlot();
}

void enterSettingsMenu() {
  stopAllClocks();
  inSettingsMenu = true;
  menuItem = 0;
  pendingShot18 = false;
  pendingShot28 = false;
  clockLongHandled = true;
  lastTickMs = millis();
  ackCommand();
}

void exitSettingsMenu() {
  inSettingsMenu = false;
  if (!periodStarted && secondsLeft != periodLength && periodLength > 0) {
    secondsLeft = periodLength;
    syncShotWithPeriod();
  }
  lastTickMs = millis();
  ackCommand();
}

void adjustMenuValue(int delta) {
  int lo, hi;
  menuValueBounds(&lo, &hi);
  int *slot = menuValueSlot();
  int prev = *slot;
  *slot = constrain(*slot + delta, lo, hi);
  if (menuItem == MENU_ITEM_PERIOD && !periodStarted) {
    secondsLeft = periodLength;
    syncShotWithPeriod();
  }
  if (menuItem == MENU_ITEM_SHOT28) {
    if (shotLeft >= prev || shotLeft > shotFull) {
      setShotClock(shotFull);
    }
  }
  ackCommand();
}

void serviceMenuCombo(uint32_t now) {
  bool both = buttonIsDown(PIN_SHOT_18) && buttonIsDown(PIN_SHOT_RESET);
  if (!both) {
    menuComboDownMs = 0;
    menuComboHandled = false;
    return;
  }
  if (inSettingsMenu) return;
  if (menuComboDownMs == 0) menuComboDownMs = now;
  if (!menuComboHandled && (now - menuComboDownMs) >= MENU_HOLD_MS) {
    enterSettingsMenu();
    menuComboHandled = true;
  }
}

void markDirty() {
  displayDirty = true;
  drawLcd();
}

void ackCommand() {
  lcdAckUntilMs = millis() + LCD_ACK_MS;
  markDirty();
}

void serviceLcdAck() {
  if (lcdAckUntilMs == 0) return;
  if ((int32_t)(millis() - lcdAckUntilMs) >= 0) {
    lcdAckUntilMs = 0;
    drawLcd();
  }
}

void setup() {
  // Matrix first — Waveshare register init MUST run before begin().
  Reginit();
  delay(100);
  matrix.begin();
  delay(500);
  matrix.setTextWrap(false);
  COL_HOME  = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, MATRIX_BRIGHT);  // white
  COL_AWAY  = matrix.Color333(0, 0, MATRIX_BRIGHT);                          // blue
  COL_CLOCK = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, MATRIX_BRIGHT);
  COL_SHOT  = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, 0);
  COL_LABEL = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT / 2, 0);           // orange
  COL_DIM   = matrix.Color333(1, 1, 1);
  COL_EXCL  = matrix.Color333(MATRIX_BRIGHT, 0, MATRIX_BRIGHT);
  matrix.fillScreen(matrix.Color333(MATRIX_BRIGHT, 0, 0));
  if (MATRIX_DOUBLE_BUFFER) matrix.swapBuffers(false);
  delay(400);
  matrix.fillScreen(0);
  if (MATRIX_DOUBLE_BUFFER) matrix.swapBuffers(false);

  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].stable = digitalRead(buttons[i].pin);
    buttons[i].lastRaw = buttons[i].stable;
  }

  pinMode(PIN_RELAY, OUTPUT);
  relayWrite(false);

  if (LORA_AUX_PIN >= 0) {
    pinMode(LORA_AUX_PIN, INPUT_PULLUP);
  }

  Serial.begin(9600);
  if (LORA_TX_LOG) {
    Serial.println(F("Scoreboard LoRa TX log on (USB Serial @ 9600)"));
  }

  Serial1.begin(LORA_BAUD);
  Serial1.setTimeout(20);
  delay(50);
  configureLoRa();

  initI2cLcd();

  drawAll(true);
  syncShotWithPeriod();
  sendShotToLoRa(shotLeft, true);
  lastTickMs = millis();
}

void loop() {
  pollLoRaModuleEcho();
  pollButtons();
  updateClocks();
  serviceRelay();
  serviceLcdAck();
  pollButtons();

  if (inPlay() && clockRunning && shotClockEnabled() &&
      (millis() - lastLoRaResendMs >= LORA_RESEND_MS)) {
    sendShotToLoRa(shotLeft, true);
  }

  pollButtons();

  bool needsBlink = false;
  if (!inSettingsMenu) {
    int mainSec = secondsLeft;
    if (inTimeout()) mainSec = timeoutLeft;
    else if (inInterval()) mainSec = intervalLeft;
    needsBlink = (mainSec == 0) || (shotClockEnabled() && shotLeft == 0);
  }
  if (displayDirty || (needsBlink && (millis() - lastDrawMs >= 400UL))) {
    drawMatrix();
    lastDrawMs = millis();
    displayDirty = false;
  }
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

void pollButtons() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    bool raw = digitalRead(buttons[i].pin);
    if (raw != buttons[i].lastRaw) {
      buttons[i].lastRaw = raw;
      buttons[i].lastChangeMs = now;
    }
    if ((now - buttons[i].lastChangeMs) < DEBOUNCE_MS) continue;
    if (raw == buttons[i].stable) continue;

    bool was = buttons[i].stable;
    buttons[i].stable = raw;

    if (was == HIGH && raw == LOW) {
      onButtonPress(buttons[i].pin, now);
    }
    if (was == LOW && raw == HIGH) {
      onButtonRelease(buttons[i].pin, now);
    }
  }

  if (buttons[0].stable == LOW && !clockLongHandled &&
      clockBtnDownMs != 0 && (now - clockBtnDownMs) >= LONG_PRESS_MS) {
    // Skip if D6 or D35 also held — those are combo paths
    if (!inSettingsMenu && !buttonIsDown(PIN_SHOT_RESET) &&
        !buttonIsDown(PIN_RETURN) && inPlay()) {
      if (isRunningClock()) {
        // RUN: long press stops the period clock
        if (clockWasPeriodRunningAtPress || periodRunning) {
          stopAllClocks();
          clockPendingStartPlay = false;
          clockLongHandled = true;
          ackCommand();
        }
      } else if (!clockWasPeriodRunningAtPress) {
        // STOP: from stopped → reload period to match length
        stopAllClocks();
        secondsLeft = periodLength;
        syncShotWithPeriod();
        clockLongHandled = true;
        ackCommand();
      }
    }
  }

  serviceAdjustLongPress(now);
  serviceReturnLongPress(now);
  serviceComboReset(now);
  servicePeriodReloadCombo(now);
  serviceMenuCombo(now);
}

void onButtonPress(uint8_t pin, uint32_t now) {
  if (inSettingsMenu) {
    switch (pin) {
      case PIN_CLOCK_TOGGLE:
        menuItem++;
        if (menuItem >= MENU_ITEM_COUNT) {
          exitSettingsMenu();
        } else {
          ackCommand();
        }
        break;
      case PIN_PERIOD_INC:
      case PIN_PERIOD_DEC:
        if (menuIsClockItem()) {
          toggleClockMode();
        } else if (menuIsShotItem()) {
          adjustMenuValue((pin == PIN_PERIOD_INC) ? 1 : -1);
        } else if (pin == PIN_PERIOD_INC) {
          adjustMenuValue(MENU_STEP_SECONDS);
        } else {
          adjustMenuValue(-MENU_STEP_SECONDS);
        }
        break;
      case PIN_RETURN:
        exitSettingsMenu();
        break;
      default:
        break;
    }
    return;
  }

  switch (pin) {
    case PIN_CLOCK_TOGGLE:
      // Start/stop only in play mode (not during TO/IN)
      if (!inPlay()) break;
      // If D6 already held, this is starting a combo-reset — don't toggle
      if (buttonIsDown(PIN_SHOT_RESET)) {
        clockBtnDownMs = now;
        clockLongHandled = false;
        clockPendingStartPlay = false;
        clockWasRunningAtPress = clockRunning;
        clockWasPeriodRunningAtPress = periodRunning;
        break;
      }
      // If D35 already held, this is period-reload combo — don't toggle
      if (buttonIsDown(PIN_RETURN)) {
        clockBtnDownMs = now;
        clockLongHandled = false;
        clockPendingStartPlay = false;
        clockWasRunningAtPress = clockRunning;
        clockWasPeriodRunningAtPress = periodRunning;
        break;
      }
      // D6/D5 must not change run state — drop ghost D2 edges after those buttons
      if ((int32_t)(now - ignoreClockToggleUntilMs) < 0) break;
      clockBtnDownMs = now;
      clockLongHandled = false;
      clockPendingStartPlay = false;
      clockWasRunningAtPress = clockRunning;
      clockWasPeriodRunningAtPress = periodRunning;
      if (isRunningClock() && periodRunning) {
        // Period already live: stop play immediately if running; if play was
        // already stopped, defer start until release (so long press can stop period)
        if (clockRunning) {
          stopPlayClocks();
          ackCommand();
        } else {
          clockPendingStartPlay = true;
        }
      } else {
        toggleClockToggle(now);
        ackCommand();
      }
      break;
    case PIN_HOME_INC:
      homeScore = min(99, homeScore + 1);
      stopPlayClocks();
      clearExclusions();
      if (shotClockEnabled()) setShotClock(shotFull);
      ackCommand();
      break;
    case PIN_AWAY_INC:
      awayScore = min(99, awayScore + 1);
      stopPlayClocks();
      clearExclusions();
      if (shotClockEnabled()) setShotClock(shotFull);
      ackCommand();
      break;
    case PIN_HOME_DEC:
      if (homeScore > 0) {
        homeScore--;
        ackCommand();
      }
      break;
    case PIN_AWAY_DEC:
      if (awayScore > 0) {
        awayScore--;
        ackCommand();
      }
      break;
    case PIN_SHOT_RESET: {
      // If D2 already held, this is starting a combo-reset — don't change shot
      if (buttonIsDown(PIN_CLOCK_TOGGLE)) break;
      // D5 already down → timing-menu combo; wait for hold, don't fire 28
      if (buttonIsDown(PIN_SHOT_18)) {
        pendingShot18 = false;
        pendingShot28 = false;
        break;
      }
      pendingShot28 = true;
      break;
    }
    case PIN_SHOT_18: {
      // D6 already down → timing-menu combo
      if (buttonIsDown(PIN_SHOT_RESET)) {
        pendingShot18 = false;
        pendingShot28 = false;
        break;
      }
      pendingShot18 = true;
      break;
    }
    case PIN_PERIOD_INC:
    case PIN_PERIOD_DEC:
    case PIN_SHOT_INC:
    case PIN_SHOT_DEC:
      if ((pin == PIN_SHOT_INC || pin == PIN_SHOT_DEC) && !shotClockEnabled()) break;
      // Short = ±1 on release; long = ±10/±30 while held
      adjustPinDown = pin;
      adjustDownMs = now;
      adjustLongHandled = false;
      break;
    case PIN_TIMEOUT:
      startTimeout();
      ackCommand();
      break;
    case PIN_INTERVAL:
      startInterval();
      ackCommand();
      break;
    case PIN_SHOT_FORCE18:
      if (!shotClockEnabled()) break;
      stopPlayClocks();
      setShotClock(shotPartial);
      ackCommand();
      break;
    case PIN_RETURN:
      // If D2 already held, this is period-reload combo — don't start RETURN actions
      if (buttonIsDown(PIN_CLOCK_TOGGLE)) {
        returnBtnDownMs = now;
        returnLongHandled = false;
        break;
      }
      // Short vs long decided on hold/release — do not return yet
      returnBtnDownMs = now;
      returnLongHandled = false;
      break;
    case PIN_EXCL:
      if (!startNextExclusion()) break;
      stopPlayClocks();
      if (shotClockEnabled() && shotLeft < shotPartial) {
        setShotClock(shotPartial);
      }
      ackCommand();
      break;
  }
}

void onButtonRelease(uint8_t pin, uint32_t now) {
  if (pin == PIN_CLOCK_TOGGLE) {
    // RUN: deferred start-play after short press while period already running
    if (clockPendingStartPlay && !clockLongHandled && !periodReloadComboHandled &&
        inPlay() && periodRunning && !clockRunning && secondsLeft > 0) {
      clockRunning = true;
      lastTickMs = now;
      ackCommand();
    }
    clockPendingStartPlay = false;
    clockBtnDownMs = 0;
  }

  if (inSettingsMenu) {
    if (isAdjustPin(pin) && pin == adjustPinDown) {
      adjustPinDown = 0;
      adjustLongHandled = false;
    }
    if (pin == PIN_SHOT_18) pendingShot18 = false;
    if (pin == PIN_SHOT_RESET) pendingShot28 = false;
    if (pin == PIN_RETURN) {
      returnBtnDownMs = 0;
      returnLongHandled = false;
    }
    return;
  }

  if (pin == PIN_RETURN) {
    // Quick press: silent return from TO/IN. Long press already fired at 3 s.
    // Skip if D2+D35 period-reload combo just ran (or D2 still held for combo).
    if (!returnLongHandled && !periodReloadComboHandled &&
        !buttonIsDown(PIN_CLOCK_TOGGLE) &&
        (inTimeout() || inInterval())) {
      endTimeoutOrInterval();
      ackCommand();
    }
    returnBtnDownMs = 0;
    returnLongHandled = false;
    return;
  }

  if (pin == PIN_SHOT_RESET) {
    if (pendingShot28 && shotClockEnabled() &&
        !buttonIsDown(PIN_CLOCK_TOGGLE) && !comboResetHandled &&
        !menuComboHandled) {
      bool wasRunning = clockRunning;
      setShotClock(shotFull);
      clockRunning = wasRunning;
      clearExclusions();
      ignoreClockToggleUntilMs = now + SHOT_BTN_CLOCK_GUARD_MS;
      ackCommand();
    }
    pendingShot28 = false;
    return;
  }
  if (pin == PIN_SHOT_18) {
    if (pendingShot18 && shotClockEnabled() && !menuComboHandled) {
      if (shotLeft < shotPartial) {
        bool wasRunning = clockRunning;
        setShotClock(shotPartial);
        clockRunning = wasRunning;
        ignoreClockToggleUntilMs = now + SHOT_BTN_CLOCK_GUARD_MS;
        ackCommand();
      }
    }
    pendingShot18 = false;
    return;
  }

  if (isAdjustPin(pin) && pin == adjustPinDown) {
    if (!adjustLongHandled) {
      int step = (pin == PIN_PERIOD_INC || pin == PIN_SHOT_INC) ? 1 : -1;
      applyAdjust(pin, step);
    }
    adjustPinDown = 0;
    adjustLongHandled = false;
  }
}

void updateClocks() {
  uint32_t now = millis();
  if (inSettingsMenu) {
    lastTickMs = now;
    return;
  }
  if (now - lastTickMs < 1000UL) return;

  uint32_t elapsed = (now - lastTickMs) / 1000UL;
  lastTickMs += elapsed * 1000UL;

  // ----- Timeout: independent of period/shot -----
  if (inTimeout()) {
    timeoutLeft -= (int)elapsed;
    if (timeoutLeft <= 0) {
      timeoutLeft = 0;
      pulseRelay(RELAY_TO_MS);
      // Back to frozen period + shot; resume with D2
    }
    markDirty();
    return;
  }

  // ----- Interval / half-time: independent; period/shot already reset on start -----
  if (inInterval()) {
    intervalLeft -= (int)elapsed;
    if (intervalLeft <= 0) {
      intervalLeft = 0;
      intervalIsHalfTime = false;
      pulseRelay(RELAY_TO_MS);
      // Back to new period clock / shot 28; start with D2
    }
    markDirty();
    return;
  }

  // ----- Play mode -----
  if (!periodRunning && !clockRunning) return;

  if (periodRunning && secondsLeft > 0) {
    secondsLeft -= (int)elapsed;
    if (secondsLeft <= 0) {
      secondsLeft = 0;
      stopAllClocks();
      pulseRelay(RELAY_PERIOD_MS);
      forwardToLoRa("END");
      // P1–P3 → between-period break; P4 remains at 0:00
      if (periodNum < PERIOD_MAX) {
        startInterval();
        return;
      }
    }
  }

  if (clockRunning && shotClockEnabled() && shotLeft > 0) {
    int prev = shotLeft;
    shotLeft -= (int)elapsed;
    if (shotLeft <= 0) {
      shotLeft = 0;
      // Period end already handled this second — do not also fire shot expiry
      if (secondsLeft > 0) {
        onShotExpired();
        return;
      }
    } else if (shotLeft != prev) {
      sendShotToLoRa(shotLeft, true);
    }
  }

  // Period remaining shorter than shot → shot display follows period
  if (shotClockEnabled() && periodRunning && shotLeft > secondsLeft) {
    shotLeft = secondsLeft;
    sendShotToLoRa(shotLeft, true);
  }

  // Exclusions: STOP/RUN = shot/play clock; RUN-NS = period clock
  if (exclTicking()) {
    if (excl1Left > 0) {
      excl1Left -= (int)elapsed;
      if (excl1Left < 0) excl1Left = 0;
    }
    if (excl2Left > 0) {
      excl2Left -= (int)elapsed;
      if (excl2Left < 0) excl2Left = 0;
    }
  }

  markDirty();
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void formatTime(int totalSec, char *out) {
  int m = totalSec / 60;
  int s = totalSec % 60;
  sprintf(out, "%d:%02d", m, s);
}

void drawAll(bool force) {
  drawLcd();
  drawMatrix();
  lastDrawMs = millis();
  displayDirty = false;
  (void)force;
}

void drawLcd() {
  char line[17];
  char tbuf[8];
  char e1[3];
  char e2[3];

  if (inSettingsMenu) {
    snprintf(line, sizeof(line), "SET %-8s %u/%u", menuItemName(),
             (unsigned)(menuItem + 1), (unsigned)MENU_ITEM_COUNT);
    lcd.setCursor(0, 0);
    uint8_t n = 0;
    for (; line[n] != '\0' && n < 16; n++) lcd.write(line[n]);
    for (; n < 16; n++) lcd.write(' ');

    if (menuIsClockItem()) {
      snprintf(tbuf, sizeof(tbuf), "%s", menuClockLabel());
    } else if (menuIsShotItem()) {
      snprintf(tbuf, sizeof(tbuf), "%ds", menuItemValue());
    } else {
      formatTime(menuItemValue(), tbuf);
    }
    char ackCh = (lcdAckUntilMs != 0 && (int32_t)(millis() - lcdAckUntilMs) < 0)
                     ? '*'
                     : ' ';
    if (menuIsClockItem()) {
      snprintf(line, sizeof(line), "%-6s S/S OK %c", tbuf, ackCh);
    } else {
      snprintf(line, sizeof(line), "%-5s S/S next %c", tbuf, ackCh);
    }
    lcd.setCursor(0, 1);
    n = 0;
    for (; line[n] != '\0' && n < 16; n++) lcd.write(line[n]);
    for (; n < 16; n++) lcd.write(' ');
    return;
  }

  // Top: PeriodTime + Period + scores in play; TO/IN/HT drop period (more space)
  // Play:  "8:00 P1 H03-02A"   (15)
  // Break: "TO 0:45 H03-02A"   (15)
  if (inTimeout()) {
    formatTime(timeoutLeft, tbuf);
    snprintf(line, sizeof(line), "TO %s H%02d-%02dA",
             tbuf, homeScore, awayScore);
  } else if (inInterval()) {
    formatTime(intervalLeft, tbuf);
    snprintf(line, sizeof(line), "%s %s H%02d-%02dA",
             intervalIsHalfTime ? "HT" : "IN", tbuf, homeScore, awayScore);
  } else {
    formatTime(secondsLeft, tbuf);
    snprintf(line, sizeof(line), "%s P%d H%02d-%02dA",
             tbuf, periodNum, homeScore, awayScore);
  }
  lcd.setCursor(0, 0);
  uint8_t n = 0;
  for (; line[n] != '\0' && n < 16; n++) lcd.write(line[n]);
  for (; n < 16; n++) lcd.write(' ');

  // Bottom: S shot, run *, " E tt, tt", ack * at far right
  //         "S28* E 18, 12  *"
  if (excl1Left > 0) snprintf(e1, sizeof(e1), "%02d", min(excl1Left, 99));
  else               snprintf(e1, sizeof(e1), "--");
  if (excl2Left > 0) snprintf(e2, sizeof(e2), "%02d", min(excl2Left, 99));
  else               snprintf(e2, sizeof(e2), "--");

  char runCh = (inPlay() && exclTicking()) ? '*' : ' ';
  char ackCh = (lcdAckUntilMs != 0 && (int32_t)(millis() - lcdAckUntilMs) < 0)
                   ? '*'
                   : ' ';
  if (shotClockEnabled()) {
    snprintf(line, sizeof(line), "S%02d%c E %s, %s  %c",
             constrain(shotLeft, 0, 99), runCh, e1, e2, ackCh);
  } else {
    snprintf(line, sizeof(line), "   %c E %s, %s  %c", runCh, e1, e2, ackCh);
  }

  lcd.setCursor(0, 1);
  n = 0;
  for (; line[n] != '\0' && n < 16; n++) lcd.write(line[n]);
  for (; n < 16; n++) lcd.write(' ');
}

// Main clock colours: white >1:00, orange 0:29–1:00 (incl. 0:38–1:00), red ≤0:28.
uint16_t mainClockColor(int secLeft) {
  if (secLeft == 0) {
    return ((millis() / 400) & 1)
               ? COL_DIM
               : matrix.Color333(MATRIX_BRIGHT, 0, 0);
  }
  if (secLeft > 60) return COL_CLOCK;
  if (secLeft > 28) {
    return matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT / 2, 0);  // orange
  }
  return matrix.Color333(MATRIX_BRIGHT, 0, 0);
}

void drawDigitPair(int x, int y, int value, uint16_t color) {
  char buf[3];
  sprintf(buf, "%02d", value);
  matrix.setTextSize(1);
  matrix.setTextColor(color);
  matrix.setCursor(x, y);
  matrix.print(buf);
}

void drawMatrix() {
  matrix.fillScreen(0);

  if (inSettingsMenu) {
    matrix.setTextSize(1);
    matrix.setTextColor(COL_LABEL);
    matrix.setCursor(1, 1);
    matrix.print("SET");
    matrix.setTextColor(COL_CLOCK);
    matrix.setCursor(46, 1);
    matrix.print(menuItem + 1);
    matrix.print("/");
    matrix.print(MENU_ITEM_COUNT);
    matrix.setCursor(1, 9);
    matrix.print(menuItemName());
    matrix.setTextColor(COL_CLOCK);
    matrix.setCursor(10, 17);
    if (menuIsClockItem()) {
      if (clockMode == CLOCK_MODE_RUN_NS) {
        matrix.setTextSize(1);
        matrix.print(menuClockLabel());
      } else {
        matrix.setTextSize(2);
        matrix.print(menuClockLabel());
      }
    } else {
      matrix.setTextSize(2);
      if (menuIsShotItem()) {
        matrix.print(menuItemValue());
        matrix.print('s');
      } else {
        char tbuf[8];
        formatTime(menuItemValue(), tbuf);
        matrix.print(tbuf);
      }
    }
    if (MATRIX_DOUBLE_BUFFER) {
      matrix.swapBuffers(false);
    }
    return;
  }

  // Top: home score | P#/TO/IN/HT | away score (same size-1 font)
  drawDigitPair(4, 1, homeScore, COL_HOME);
  drawDigitPair(49, 1, awayScore, COL_AWAY);

  matrix.setTextSize(1);
  matrix.setTextColor(COL_LABEL);
  if (inTimeout()) {
    matrix.setCursor(26, 1);
    matrix.print("TO");
  } else if (inInterval()) {
    matrix.setCursor(26, 1);
    matrix.print(intervalIsHalfTime ? "HT" : "IN");
  } else {
    matrix.setCursor(26, 1);
    matrix.print('P');
    matrix.print(periodNum);
  }

  // Middle: large main clock (rows 10–23)
  int mainSec = secondsLeft;
  if (inTimeout()) mainSec = timeoutLeft;
  else if (inInterval()) mainSec = intervalLeft;
  char tbuf[8];
  formatTime(mainSec, tbuf);
  matrix.setTextSize(2);
  matrix.setTextColor(mainClockColor(mainSec));
  matrix.setCursor(10, 10);
  matrix.print(tbuf);

  // Bottom left: shot (red) — rows 25–31, below the clock
  if (shotClockEnabled()) {
    char sbuf[3];
    sprintf(sbuf, "%02d", shotLeft);
    uint16_t shotColor = matrix.Color333(MATRIX_BRIGHT, 0, 0);
    if (shotLeft == 0 && ((millis() / 400) & 1)) shotColor = COL_DIM;
    matrix.setTextSize(1);
    matrix.setTextColor(shotColor);
    matrix.setCursor(1, 25);
    matrix.print(sbuf);
  }

  if (inTimeout()) {
    formatTime(secondsLeft, tbuf);
    matrix.setTextColor(COL_CLOCK);
    matrix.setCursor(38, 25);
    matrix.print(tbuf);
  } else if (!inTimeout()) {
    if (excl1Left > 0) {
      char e1[3];
      sprintf(e1, "%02d", excl1Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(20, 25);
      matrix.print(e1);
    }
    if (excl2Left > 0) {
      char e2[3];
      sprintf(e2, "%02d", excl2Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(35, 25);
      matrix.print(e2);
    }
  }

  (void)MATRIX_SAFE_WIDTH;
  if (MATRIX_DOUBLE_BUFFER) {
    matrix.swapBuffers(false);
  }
}
