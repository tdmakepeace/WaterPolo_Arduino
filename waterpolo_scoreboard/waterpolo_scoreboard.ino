/*
 * Water Polo Scoreboard
 * Board: Arduino Mega 2560
 * Display: Waveshare RGB-Matrix-P3-64x32 (SKU 33840, HUB75)
 * Mirror: Freenove I2C IIC LCD 1602 (16x2) on Mega SDA/SCL
 * Remote shot clocks: LoRa TX via Serial1 (same protocol as lora_remote2)
 *
 * Libraries (Library Manager):
 *   - Adafruit GFX Library
 *   - RGB matrix Panel (Adafruit)
 *   - LiquidCrystal I2C (e.g. Frank de Brabander / Freenove zip)
 *
 * Buttons (active LOW, internal pull-ups — wire other side to GND):
 *   D2  = period clock start / stop
 *   D3  = home score +  (also stops period clock, shot → 28)
 *   D4  = away score +  (also stops period clock, shot → 28)
 *   D5  = home score -
 *   D6  = away score -
 *   D7  = shot → 28 (preserves period clock running/stopped state)
 *   D8  = shot clock set to 18 s if < 18 (preserves running/stopped state)
 *   D36 = period +1 s (long press +10 s)
 *   D37 = period -1 s (long press -30 s)
 *   D38 = shot +1 s (long press +10 s)
 *   D39 = shot -1 s (long press -10 s)
 *   D40 = timeout 1:00 (pauses period+shot; independent countdown)
 *   D41 = interval (reloads match period length + shot 28; advances period 1→4;
 *         after period 2 uses HALF_TIME_SECONDS, else INTERVAL_SECONDS)
 *   D42 = force shot clock to 18 s (always; pauses period clock)
 *   D43 = RETURN — end TO/IN early → period + shot (keeps shot value in timeout)
 *   D46 = exclusion 1 → 18 s (pauses period clock; applies D8 if <18; no excl reset)
 *   D47 = exclusion 2 → 18 s (same rules as D46)
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
 * Period counter P1–P4 advances on INTERVAL; half-time after P2 (display HT).
 * When the period clock hits 0: auto-start IN/HT for P1–P3; P4 stays at 0:00.
 * Period length defaults to PERIOD_SECONDS (8:00). Adjusting the period clock
 * before the first START of that period updates the match length carried to
 * later periods (INTERVAL / D2 long-press reload).
 *
 * D2 short press: start/stop immediately (play mode only).
 * Hold D2 ~5 s (from stopped): resets period clock to match length (cancels start).
 * Hold D2 + D7 together for 5 s: full reset (scores, clocks, exclusions → defaults).
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
#include <LiquidCrystal_I2C.h>

// ----- Button pins -----
const uint8_t PIN_CLOCK_TOGGLE = 2;
const uint8_t PIN_HOME_INC     = 3;
const uint8_t PIN_AWAY_INC     = 4;
const uint8_t PIN_HOME_DEC     = 5;
const uint8_t PIN_AWAY_DEC     = 6;
const uint8_t PIN_SHOT_RESET   = 7;   // → 28 s
const uint8_t PIN_SHOT_18      = 8;   // → 18 s if < 18
const uint8_t PIN_RELAY        = 12;  // 5 V relay coil / module IN
const uint8_t PIN_PERIOD_INC   = 36;  // period +1 s (long +10 s)
const uint8_t PIN_PERIOD_DEC   = 37;  // period -1 s (long -30 s)
const uint8_t PIN_SHOT_INC     = 38;  // shot +1 s
const uint8_t PIN_SHOT_DEC     = 39;  // shot -1 s
const uint8_t PIN_TIMEOUT      = 40;  // 1:00 timeout
const uint8_t PIN_INTERVAL     = 41;  // between-period break (HT after P2)
const uint8_t PIN_SHOT_FORCE18 = 42;  // force shot → 18
const uint8_t PIN_RETURN       = 43;  // end TO/IN → period + shot
const uint8_t PIN_EXCL1        = 46;  // exclusion 1 → 18 (no reset)
const uint8_t PIN_EXCL2        = 47;  // exclusion 2 → 18 (no reset)

// true  = HIGH energises relay (bare transistor driver / many modules)
// false = LOW energises relay (common optocoupler relay boards marked LOW)
const bool RELAY_ACTIVE_HIGH = true;

// ----- HUB75 pins (Adafruit Mega requirements) -----
#define CLK 11
#define OE   9
#define LAT 10
#define A   A0
#define B   A1
#define C   A2
#define D   A3

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, true, 64);

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
  {PIN_HOME_INC,     HIGH, HIGH, 0},
  {PIN_AWAY_INC,     HIGH, HIGH, 0},
  {PIN_HOME_DEC,     HIGH, HIGH, 0},
  {PIN_AWAY_DEC,     HIGH, HIGH, 0},
  {PIN_SHOT_RESET,   HIGH, HIGH, 0},
  {PIN_SHOT_18,      HIGH, HIGH, 0},
  {PIN_PERIOD_INC,   HIGH, HIGH, 0},
  {PIN_PERIOD_DEC,   HIGH, HIGH, 0},
  {PIN_SHOT_INC,     HIGH, HIGH, 0},
  {PIN_SHOT_DEC,     HIGH, HIGH, 0},
  {PIN_TIMEOUT,      HIGH, HIGH, 0},
  {PIN_INTERVAL,     HIGH, HIGH, 0},
  {PIN_SHOT_FORCE18, HIGH, HIGH, 0},
  {PIN_RETURN,       HIGH, HIGH, 0},
  {PIN_EXCL1,        HIGH, HIGH, 0},
  {PIN_EXCL2,        HIGH, HIGH, 0},
};

const uint8_t BTN_COUNT = sizeof(buttons) / sizeof(buttons[0]);
const uint16_t DEBOUNCE_MS = 15;
const uint16_t LONG_PRESS_MS = 5000;   // D2 solo → period reset to periodLength
const uint16_t ADJUST_LONG_MS = 800;   // D36 +10 / D37 -30; shot ±10
const uint16_t COMBO_RESET_MS = 5000;  // D2 + D7 held → full reset

uint32_t clockBtnDownMs = 0;
bool clockLongHandled = false;
bool clockWasRunningAtPress = false;

uint8_t adjustPinDown = 0;
uint32_t adjustDownMs = 0;
bool adjustLongHandled = false;

uint32_t comboResetDownMs = 0;
bool comboResetHandled = false;

// Ignore D2 start/stop edges shortly after D7/D8 (avoids ghost toggles)
uint32_t ignoreClockToggleUntilMs = 0;
const uint16_t SHOT_BTN_CLOCK_GUARD_MS = 80;

uint16_t COL_HOME;
uint16_t COL_AWAY;
uint16_t COL_CLOCK;
uint16_t COL_SHOT;
uint16_t COL_LABEL;
uint16_t COL_DIM;
uint16_t COL_EXCL;

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
  excl1Left = 0;
  excl2Left = 0;
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
  excl1Left = 0;
  excl2Left = 0;
  intervalLeft = breakSec;
  lastTickMs = millis();
  markDirty();
}

void startExclusion(int *slot) {
  // Cannot reset while active — only arm when idle (0)
  if (*slot > 0) return;
  *slot = EXCLUSION_SECONDS;
  markDirty();
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
  delay(200);
  setLoRaChannel(LORA_DEFAULT_CHANNEL);
  delay(100);

  matrix.begin();
  COL_HOME  = matrix.Color333(0, 7, 0);
  COL_AWAY  = matrix.Color333(7, 2, 0);
  COL_CLOCK = matrix.Color333(7, 7, 7);
  COL_SHOT  = matrix.Color333(7, 7, 0);
  COL_LABEL = matrix.Color333(0, 4, 7);
  COL_DIM   = matrix.Color333(2, 2, 2);
  COL_EXCL  = matrix.Color333(7, 0, 7);

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
  if (displayDirty || (needsBlink && (millis() - lastDrawMs >= 200))) {
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
    // Skip if D7 also held — that path is the 5 s full combo reset
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
      // If D7 already held, this is starting a combo-reset — don't toggle
      if (buttonIsDown(PIN_SHOT_RESET)) {
        clockBtnDownMs = now;
        clockLongHandled = false;
        clockWasRunningAtPress = clockRunning;
        break;
      }
      // D7/D8 must not change run state — drop ghost D2 edges after those buttons
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
      setShotClock(SHOT_FULL);
      ackCommand();
      break;
    case PIN_AWAY_INC:
      awayScore = min(99, awayScore + 1);
      clockRunning = false;
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
    case PIN_EXCL1:
      startExclusion(&excl1Left);
      clockRunning = false;
      if (shotLeft < SHOT_PARTIAL) {
        setShotClock(SHOT_PARTIAL);
      }
      ackCommand();
      break;
    case PIN_EXCL2:
      startExclusion(&excl2Left);
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

  matrix.setTextSize(1);
  matrix.setTextColor(COL_LABEL);
  matrix.setCursor(2, 1);
  matrix.print("HOME");
  matrix.setCursor(40, 1);
  matrix.print("AWAY");

  // Period number centred between HOME / AWAY
  matrix.setTextColor(COL_CLOCK);
  matrix.setCursor(28, 1);
  matrix.print('P');
  matrix.print(periodNum);

  drawDigitPair(4, 10, homeScore, COL_HOME);
  drawDigitPair(40, 10, awayScore, COL_AWAY);

  char tbuf[8];
  matrix.setTextSize(1);

  if (inTimeout()) {
    formatTime(timeoutLeft, tbuf);
    matrix.setTextColor(matrix.Color333(0, 7, 7));
    matrix.setCursor(2, 24);
    matrix.print("TO ");
    matrix.print(tbuf);
  } else if (inInterval()) {
    formatTime(intervalLeft, tbuf);
    matrix.setTextColor(matrix.Color333(0, 4, 7));
    matrix.setCursor(2, 24);
    matrix.print(intervalIsHalfTime ? "HT " : "IN ");
    matrix.print(tbuf);
  } else {
    formatTime(secondsLeft, tbuf);
    uint16_t clockColor = COL_CLOCK;
    if (secondsLeft == 0) {
      if ((millis() / 400) & 1) clockColor = COL_DIM;
      else clockColor = matrix.Color333(7, 0, 0);
    } else if (!clockRunning) {
      clockColor = COL_DIM;
    }
    matrix.setTextColor(clockColor);
    matrix.setCursor(2, 24);
    matrix.print(tbuf);

    // Exclusion chips in centre
    if (excl1Left > 0) {
      char e1[3];
      sprintf(e1, "%02d", excl1Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(26, 24);
      matrix.print(e1);
    }
    if (excl2Left > 0) {
      char e2[3];
      sprintf(e2, "%02d", excl2Left);
      matrix.setTextColor(COL_EXCL);
      matrix.setCursor(38, 24);
      matrix.print(e2);
    }

    uint16_t shotColor = COL_SHOT;
    if (shotLeft == 0) {
      if ((millis() / 400) & 1) shotColor = COL_DIM;
      else shotColor = matrix.Color333(7, 0, 0);
    } else if (!clockRunning) {
      shotColor = COL_DIM;
    } else if (shotLeft <= 5) {
      shotColor = matrix.Color333(7, 0, 0);
    }
    char sbuf[3];
    sprintf(sbuf, "%02d", shotLeft);
    matrix.setTextColor(shotColor);
    matrix.setCursor(52, 24);
    matrix.print(sbuf);

    if (clockRunning) {
      matrix.fillRect(20, 25, 2, 2, COL_CLOCK);
    }
  }

  matrix.swapBuffers(false);
}
