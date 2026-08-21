/*
 * Water Polo Scoreboard
 * Board: Arduino Mega 2560
 * Display: Waveshare RGB-Matrix-P3-64x32 (SKU 33840, HUB75)
 * Mirror: Freenove I2C IIC LCD 1602 (16x2) on Mega SDA/SCL
 * Remote shot clocks: LoRa TX via Serial1 (same protocol as lora_remote2)
 *
 * Libraries (Library Manager):
 *   - LiquidCrystal I2C (e.g. Frank de Brabander / Freenove zip)
 *   - If MATRIX_USE_BITBANG is 0: also Adafruit GFX + RGB matrix Panel
 *
 * Matrix: default MATRIX_USE_BITBANG 1 (software HUB75 — needed on the
 * current Waveshare when Adafruit test_matrix stays white/blue). Set to 0
 * after a healthy panel + clean Adafruit test_matrix.
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
 *   D35 = RETURN — end TO/IN early → period + shot (keeps shot value in timeout)
 *   D36 = period +1 s (long press +10 s)
 *   D37 = period -1 s (long press -30 s)
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
 * Shot clock runs only while the period clock is running (play mode).
 * When period remaining is less than the shot clock, the shot clock shows
 * (and follows) the period remaining — including when PERIOD_SECONDS /
 * match length is shorter than a full shot (e.g. 28 s).
 * At shot = 0: LoRa BUZZER, stop clocks, reset shot to 28 s.
 * Exclusions tick only with period clock (not shot clock; pause in TO/IN).
 * Remaining exclusion time is kept across INTERVAL / half-time into the next period.
 * A goal (home+ / away+) or D6 (shot → 28) clears both exclusion clocks.
 * Period counter P1–P4 advances on INTERVAL; half-time after P2 (display HT).
 * When the period clock hits 0: auto-start IN/HT for P1–P3; P4 stays at 0:00.
 * Period length defaults to PERIOD_SECONDS (8:00). Adjusting the period clock
 * before the first START of that period updates the match length carried to
 * later periods (INTERVAL / D2 long-press reload).
 *
 * D2 short press: start/stop immediately (play mode only).
 * Hold D2 ~5 s (from stopped): resets period clock to match length (cancels start).
 * Hold D2 + D6 together for 5 s: full reset (scores, clocks, exclusions → defaults).
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 1 = software HUB75 (works on this Waveshare when Adafruit stays blue/white)
// 0 = Adafruit RGBmatrixPanel (use after replacement panel if Adafruit test_matrix is clean)
#ifndef MATRIX_USE_BITBANG
#define MATRIX_USE_BITBANG 1
#endif

#if !MATRIX_USE_BITBANG
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
#else
#include "hub75_soft.h"
#endif

// ----- Button pins -----
const uint8_t PIN_CLOCK_TOGGLE = 2;
const uint8_t PIN_SHOT_FORCE18 = 3;   // force shot → 18
const uint8_t PIN_EXCL         = 4;   // exclusion: 1st press clock 1, 2nd press clock 2
const uint8_t PIN_SHOT_18      = 5;   // → 18 s if < 18
const uint8_t PIN_SHOT_RESET   = 6;   // → 28 s
const uint8_t PIN_TIMEOUT      = 7;   // 1:00 timeout
const uint8_t PIN_INTERVAL     = 8;   // between-period break (HT after P2)
const uint8_t PIN_RELAY        = 12;  // 5 V relay coil / module IN
const uint8_t PIN_RETURN       = 35;  // end TO/IN → period + shot
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

// ----- HUB75 pins -----
#define CLK 11
// Waveshare LAT/OE swapped vs Adafruit charts (confirmed with bitbang):
// green #15 → D9 = LAT, yellow #14 → D10 = OE.
#define OE  10
#define LAT  9
#define A   A0
#define B   A1
#define C   A2
#define D   A3
#define E   A4  // HUB75 pin 8 — hold LOW on this 64x32 panel

// Dead columns on the current panel (49–52). Keep UI left of this.
const int MATRIX_SAFE_WIDTH = 49;

#if MATRIX_USE_BITBANG
Hub75Soft matrix;
#else
const bool MATRIX_DOUBLE_BUFFER = false;
const uint8_t MATRIX_BRIGHT = 3;
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, MATRIX_DOUBLE_BUFFER, 64);
uint16_t COL_HOME, COL_AWAY, COL_CLOCK, COL_SHOT, COL_LABEL, COL_DIM, COL_EXCL;
#endif

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

const long LORA_CHANNEL_FREQS[] = {
  433050000L, 433100000L, 433150000L, 433200000L
};
const uint8_t LORA_CHANNEL_COUNT =
    sizeof(LORA_CHANNEL_FREQS) / sizeof(LORA_CHANNEL_FREQS[0]);
const uint8_t LORA_DEFAULT_CHANNEL = 2;

// ----- Scoreboard state -----
const int PERIOD_SECONDS     = 8 * 60;       // 8:00 default match period length
const int PERIOD_MAX         = 4;            // P1–P4
const int SHOT_FULL          = 28;
const int SHOT_PARTIAL       = 18;
const int TIMEOUT_SECONDS    = 60;
const int INTERVAL_SECONDS   = 2 * 60;       // between P1–P2 and P3–P4
const int HALF_TIME_SECONDS  = 2 * 60;       // after P2 (set to 5*60 for official half-time)
const int EXCLUSION_SECONDS  = 18;

int homeScore = 0;
int awayScore = 0;
int periodNum = 1;         // current period 1–PERIOD_MAX
int periodLength = PERIOD_SECONDS;  // carried match length (set before first START)
int secondsLeft = PERIOD_SECONDS;
int shotLeft = SHOT_FULL;
int timeoutLeft = 0;       // >0 = timeout mode
int intervalLeft = 0;      // >0 = interval / half-time mode
int excl1Left = 0;         // 0 = inactive; cannot restart while >0
int excl2Left = 0;
int lastShotSent = -1;
bool clockRunning = false;
bool intervalIsHalfTime = false;
bool periodStarted = false;  // true after first START of current period

bool inTimeout() { return timeoutLeft > 0; }
bool inInterval() { return intervalLeft > 0; }
bool inPlay() { return !inTimeout() && !inInterval(); }

void markDirty();
void setShotClock(int value);
void syncShotWithPeriod();
void ackCommand();
void drawLcd();
void serviceLcdAck();
void clearExclusions();
bool startNextExclusion();

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
const uint16_t LONG_PRESS_MS = 5000;   // D2 solo → period reset to periodLength
const uint16_t ADJUST_LONG_MS = 800;   // D36 +10 / D37 -30; shot ±10
const uint16_t COMBO_RESET_MS = 5000;  // D2 + D6 held → full reset

uint32_t clockBtnDownMs = 0;
bool clockLongHandled = false;
bool clockWasRunningAtPress = false;

uint8_t adjustPinDown = 0;
uint32_t adjustDownMs = 0;
bool adjustLongHandled = false;

uint32_t comboResetDownMs = 0;
bool comboResetHandled = false;

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
    secondsLeft = constrain(secondsLeft + deltaSec, 0, PERIOD_SECONDS);
    if (secondsLeft == 0) clockRunning = false;
    // Before first START of this period: carry adjusted length to later periods
    if (!periodStarted && secondsLeft > 0) {
      periodLength = secondsLeft;
    }
    syncShotWithPeriod();
    ackCommand();
  } else if (pin == PIN_SHOT_INC || pin == PIN_SHOT_DEC) {
    setShotClock(constrain(shotLeft + deltaSec, 0, SHOT_FULL));
    ackCommand();
  }
}

void serviceAdjustLongPress(uint32_t now) {
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

void fullResetToDefaults() {
  clockRunning = false;
  homeScore = 0;
  awayScore = 0;
  periodNum = 1;
  periodLength = PERIOD_SECONDS;
  periodStarted = false;
  secondsLeft = PERIOD_SECONDS;
  timeoutLeft = 0;
  intervalLeft = 0;
  intervalIsHalfTime = false;
  clearExclusions();
  setShotClock(SHOT_FULL);
  pulseRelay(RELAY_TO_MS);
  ackCommand();
}

void serviceComboReset(uint32_t now) {
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

void forwardToLoRa(const char *command) {
  waitLoRaAux();
  Serial1.println(command);
}

void setLoRaChannel(uint8_t ch) {
  if (ch >= LORA_CHANNEL_COUNT) return;
  char cmd[28];
  snprintf(cmd, sizeof(cmd), "AT+FREQ=%ld", LORA_CHANNEL_FREQS[ch]);
  forwardToLoRa(cmd);
}

void sendShotToLoRa(int value, bool force) {
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
  if (!inPlay()) return;
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
  sendShotToLoRa(shotLeft, true);
  markDirty();
}

void onShotExpired() {
  clockRunning = false;
  pulseRelay(RELAY_SHOT_MS);

  forwardToLoRa("0");
  forwardToLoRa("BUZZER");

  shotLeft = SHOT_FULL;
  sendShotToLoRa(shotLeft, true);
  markDirty();
}

void startTimeout() {
  clockRunning = false;
  intervalLeft = 0;
  timeoutLeft = TIMEOUT_SECONDS;
  lastTickMs = millis();
  markDirty();
}

void startInterval() {
  clockRunning = false;
  timeoutLeft = 0;
  // Half-time after period 2 (before advancing to P3)
  intervalIsHalfTime = (periodNum == 2);
  int breakSec = intervalIsHalfTime ? HALF_TIME_SECONDS : INTERVAL_SECONDS;
  if (periodNum < PERIOD_MAX) periodNum++;
  secondsLeft = periodLength;
  periodStarted = false;
  setShotClock(SHOT_FULL);
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
  // Matrix first — before LoRa / LCD.
  pinMode(E, OUTPUT);
  digitalWrite(E, LOW);
  matrix.begin();
#if !MATRIX_USE_BITBANG
  matrix.setTextWrap(false);
  COL_HOME  = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, MATRIX_BRIGHT);  // white
  COL_AWAY  = matrix.Color333(0, 0, MATRIX_BRIGHT);                          // blue
  COL_CLOCK = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, MATRIX_BRIGHT);
  COL_SHOT  = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT, 0);
  COL_LABEL = matrix.Color333(MATRIX_BRIGHT, MATRIX_BRIGHT / 2, 0);           // orange
  COL_DIM   = matrix.Color333(1, 1, 1);
  COL_EXCL  = matrix.Color333(MATRIX_BRIGHT, 0, MATRIX_BRIGHT);
  matrix.fillScreen(matrix.Color333(MATRIX_BRIGHT, 0, 0));
  delay(400);
  matrix.fillScreen(0);
#endif

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

  Serial1.begin(LORA_BAUD);
  Serial1.setTimeout(20);
  delay(50);
  setLoRaChannel(LORA_DEFAULT_CHANNEL);
  delay(50);

  initI2cLcd();

  drawAll(true);
  syncShotWithPeriod();
  sendShotToLoRa(shotLeft, true);
  lastTickMs = millis();
}

void loop() {
  pollButtons();
  updateClocks();
  serviceRelay();
  serviceLcdAck();
  pollButtons();

  if (inPlay() && clockRunning &&
      (millis() - lastLoRaResendMs >= LORA_RESEND_MS)) {
    sendShotToLoRa(shotLeft, true);
  }

  pollButtons();

  bool needsBlink = (secondsLeft == 0) || (timeoutLeft == 0 && intervalLeft == 0 &&
                     shotLeft == 0 && !clockRunning);
  if (displayDirty || (millis() - lastDrawMs >= (needsBlink ? 200UL : 250UL))) {
    drawMatrix();
    lastDrawMs = millis();
    displayDirty = false;
  }

#if MATRIX_USE_BITBANG
  // Keep scanning — soft driver has no timer ISR
  matrix.scan();
#endif
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
    // Skip if D6 also held — that path is the 5 s full combo reset
    if (!buttonIsDown(PIN_SHOT_RESET) && !clockWasRunningAtPress && inPlay()) {
      clockRunning = false;
      secondsLeft = periodLength;
      syncShotWithPeriod();
      clockLongHandled = true;
      ackCommand();
    }
  }

  serviceAdjustLongPress(now);
  serviceComboReset(now);
}

void onButtonPress(uint8_t pin, uint32_t now) {
  switch (pin) {
    case PIN_CLOCK_TOGGLE:
      // Start/stop only in play mode (not during TO/IN)
      if (!inPlay()) break;
      // If D6 already held, this is starting a combo-reset — don't toggle
      if (buttonIsDown(PIN_SHOT_RESET)) {
        clockBtnDownMs = now;
        clockLongHandled = false;
        clockWasRunningAtPress = clockRunning;
        break;
      }
      // D6/D5 must not change run state — drop ghost D2 edges after those buttons
      if ((int32_t)(now - ignoreClockToggleUntilMs) < 0) break;
      clockBtnDownMs = now;
      clockLongHandled = false;
      clockWasRunningAtPress = clockRunning;
      if (clockRunning) {
        clockRunning = false;
        ackCommand();
      } else if (secondsLeft > 0) {
        clockRunning = true;
        periodStarted = true;
        lastTickMs = now;
        ackCommand();
      }
      break;
    case PIN_HOME_INC:
      homeScore = min(99, homeScore + 1);
      clockRunning = false;
      clearExclusions();
      setShotClock(SHOT_FULL);
      ackCommand();
      break;
    case PIN_AWAY_INC:
      awayScore = min(99, awayScore + 1);
      clockRunning = false;
      clearExclusions();
      setShotClock(SHOT_FULL);
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
      // Shot only — leave period clock running or stopped as it was
      bool wasRunning = clockRunning;
      setShotClock(SHOT_FULL);
      clockRunning = wasRunning;
      clearExclusions();
      ignoreClockToggleUntilMs = now + SHOT_BTN_CLOCK_GUARD_MS;
      ackCommand();
      break;
    }
    case PIN_SHOT_18: {
      if (shotLeft < SHOT_PARTIAL) {
        bool wasRunning = clockRunning;
        setShotClock(SHOT_PARTIAL);
        clockRunning = wasRunning;
        ignoreClockToggleUntilMs = now + SHOT_BTN_CLOCK_GUARD_MS;
        ackCommand();
      }
      break;
    }
    case PIN_PERIOD_INC:
    case PIN_PERIOD_DEC:
    case PIN_SHOT_INC:
    case PIN_SHOT_DEC:
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
      clockRunning = false;
      setShotClock(SHOT_PARTIAL);
      ackCommand();
      break;
    case PIN_RETURN:
      if (inTimeout() || inInterval()) {
        timeoutLeft = 0;
        intervalLeft = 0;
        intervalIsHalfTime = false;
        ackCommand();
      }
      break;
    case PIN_EXCL:
      if (!startNextExclusion()) break;
      clockRunning = false;
      if (shotLeft < SHOT_PARTIAL) {
        setShotClock(SHOT_PARTIAL);
      }
      ackCommand();
      break;
  }
}

void onButtonRelease(uint8_t pin, uint32_t now) {
  (void)now;
  if (pin == PIN_CLOCK_TOGGLE) {
    clockBtnDownMs = 0;
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
  if (!clockRunning) return;

  if (secondsLeft > 0) {
    secondsLeft -= (int)elapsed;
    if (secondsLeft <= 0) {
      secondsLeft = 0;
      clockRunning = false;
      pulseRelay(RELAY_PERIOD_MS);
      forwardToLoRa("END");
      // P1–P3 → between-period break; P4 remains at 0:00
      if (periodNum < PERIOD_MAX) {
        startInterval();
        return;
      }
    }
  }

  if (clockRunning && shotLeft > 0) {
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
  if (shotLeft > secondsLeft) {
    shotLeft = secondsLeft;
    sendShotToLoRa(shotLeft, true);
  }

  // Exclusions follow period clock only (not shot clock)
  if (clockRunning) {
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

  char runCh = (inPlay() && clockRunning) ? '*' : ' ';
  char ackCh = (lcdAckUntilMs != 0 && (int32_t)(millis() - lcdAckUntilMs) < 0)
                   ? '*'
                   : ' ';
  snprintf(line, sizeof(line), "S%02d%c E %s, %s  %c",
           constrain(shotLeft, 0, 99), runCh, e1, e2, ackCh);

  lcd.setCursor(0, 1);
  n = 0;
  for (; line[n] != '\0' && n < 16; n++) lcd.write(line[n]);
  for (; n < 16; n++) lcd.write(' ');
}

// Main clock colours: white >1:00, orange 0:29–1:00 (incl. 0:38–1:00), red ≤0:28.
#if MATRIX_USE_BITBANG
uint8_t mainClockColor(int secLeft) {
  if (secLeft == 0) return ((millis() / 400) & 1) ? H75_OFF : H75_R;
  if (secLeft > 60) return H75_W;
  if (secLeft > 28) return H75_RG;  // orange through 1:00
  return H75_R;
}
#else
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
#endif

#if MATRIX_USE_BITBANG
void drawMatrix() {
  matrix.clear();
  (void)MATRIX_SAFE_WIDTH;

  char buf[8];
  // Top: home score | P#/TO/IN/HT | away score (no H/A labels)
  snprintf(buf, sizeof(buf), "%02d", homeScore);
  matrix.printAt(1, 1, buf, H75_W, 2);
  snprintf(buf, sizeof(buf), "%02d", awayScore);
  matrix.printAt(47, 1, buf, H75_B, 2);

  if (inTimeout()) {
    matrix.printAt(28, 3, "TO", H75_RG, 1);
  } else if (inInterval()) {
    matrix.printAt(28, 3, intervalIsHalfTime ? "HT" : "IN", H75_RG, 1);
  } else {
    snprintf(buf, sizeof(buf), "P%d", periodNum);
    matrix.printAt(28, 3, buf, H75_RG, 1);
  }

  // Middle: large main clock (period / TO / IN / HT)
  int mainSec = secondsLeft;
  if (inTimeout()) mainSec = timeoutLeft;
  else if (inInterval()) mainSec = intervalLeft;
  formatTime(mainSec, buf);
  // 4-char m:ss at scale 2 → 32 px wide; centre on 64
  matrix.printAt(16, 12, buf, mainClockColor(mainSec), 2);

  // Bottom left: shot clock (always red)
  char sbuf[3];
  sprintf(sbuf, "%02d", shotLeft);
  uint8_t shotCol = H75_R;
  if (shotLeft == 0) shotCol = ((millis() / 400) & 1) ? H75_OFF : H75_R;
  matrix.printAt(1, 24, sbuf, shotCol, 1);

  if (inTimeout()) {
    // Frozen period clock copied to bottom right next to shot
    formatTime(secondsLeft, buf);
    matrix.printAt(40, 24, buf, H75_W, 1);
  } else if (!inTimeout()) {
    if (excl1Left > 0) {
      char e1[3];
      sprintf(e1, "%02d", excl1Left);
      matrix.printAt(20, 24, e1, H75_RB, 1);
    }
    if (excl2Left > 0) {
      char e2[3];
      sprintf(e2, "%02d", excl2Left);
      matrix.printAt(28, 24, e2, H75_RB, 1);
    }
  }
}
#else
void drawDigitPair(int x, int y, int value, uint16_t color) {
  char buf[3];
  sprintf(buf, "%02d", value);
  matrix.setTextSize(2);
  matrix.setTextColor(color);
  matrix.setCursor(x, y);
  matrix.print(buf);
}

void drawMatrix() {
  matrix.fillScreen(0);

  // Top: home score | P#/TO/IN/HT | away score
  drawDigitPair(1, 1, homeScore, COL_HOME);
  drawDigitPair(45, 1, awayScore, COL_AWAY);

  matrix.setTextSize(1);
  matrix.setTextColor(COL_LABEL);
  if (inTimeout()) {
    matrix.setCursor(26, 3);
    matrix.print("TO");
  } else if (inInterval()) {
    matrix.setCursor(26, 3);
    matrix.print(intervalIsHalfTime ? "HT" : "IN");
  } else {
    matrix.setCursor(26, 3);
    matrix.print('P');
    matrix.print(periodNum);
  }

  // Middle: large main clock
  int mainSec = secondsLeft;
  if (inTimeout()) mainSec = timeoutLeft;
  else if (inInterval()) mainSec = intervalLeft;
  char tbuf[8];
  formatTime(mainSec, tbuf);
  matrix.setTextSize(2);
  matrix.setTextColor(mainClockColor(mainSec));
  matrix.setCursor(10, 12);
  matrix.print(tbuf);

  // Bottom left: shot (red)
  char sbuf[3];
  sprintf(sbuf, "%02d", shotLeft);
  uint16_t shotColor = matrix.Color333(MATRIX_BRIGHT, 0, 0);
  if (shotLeft == 0 && ((millis() / 400) & 1)) shotColor = COL_DIM;
  matrix.setTextSize(1);
  matrix.setTextColor(shotColor);
  matrix.setCursor(1, 24);
  matrix.print(sbuf);

  if (inTimeout()) {
    formatTime(secondsLeft, tbuf);
    matrix.setTextColor(COL_CLOCK);
    matrix.setCursor(38, 24);
    matrix.print(tbuf);
  } else if (!inTimeout()) {
    if (excl1Left > 0) {
      char e1[3];
      sprintf(e1, "%02d", excl1Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(20, 24);
      matrix.print(e1);
    }
    if (excl2Left > 0) {
      char e2[3];
      sprintf(e2, "%02d", excl2Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(28, 24);
      matrix.print(e2);
    }
  }

  (void)MATRIX_SAFE_WIDTH;
  if (MATRIX_DOUBLE_BUFFER) {
    matrix.swapBuffers(false);
  }
}
#endif
