/*
 * HUB75 bit-bang — Arduino Mega 2560 + Waveshare 64x32 (1/16 scan)
 *
 * Wiring (same as scoreboard):
 *   R1 D24  G1 D25  B1 D26
 *   R2 D27  G2 D28  B2 D29
 *   A  A0   B  A1   C  A2   D  A3
 *   E  A4   (hold LOW) — grey #8
 *   CLK D11  LAT D9  OE D10
 *   Ribbon: green #15 → D9 (LAT), yellow #14 → D10 (OE)
 *   GND: yellow #4 + blue #16 → Mega GND
 *   VH4: 5 V PSU; PSU GND ↔ Mega GND
 *   Ribbon in HUB75 **INPUT** only
 *
 * Serial @ 9600, Newline:
 *   r/g/b  full colour     t top-red   n bottom-red   0 off
 *   s      row walk (row N and N+16 together is correct 1/16 scan)
 *   c      column walk — bright red vertical line 0→63
 *   m      ruler: cols 0,16,32,48,63 green
 *   q      section walk — each 16-col band (0-15,16-31,32-47,48-63)
 *   f      flip shift direction (if column walk moves the wrong way)
 *   i      OE polarity    x LAT/OE swap    h help
 */

#include <Arduino.h>

const uint8_t PIN_R1 = 24, PIN_G1 = 25, PIN_B1 = 26;
const uint8_t PIN_R2 = 27, PIN_G2 = 28, PIN_B2 = 29;
const uint8_t PIN_A = A0, PIN_B = A1, PIN_C = A2, PIN_D = A3, PIN_E = A4;
const uint8_t PIN_CLK = 11;
// Confirmed on both panels: green #15 → D9 LAT, yellow #14 → D10 OE
const uint8_t PIN_LAT_HW = 9;
const uint8_t PIN_OE_HW = 10;

uint8_t pinLat = PIN_LAT_HW;
uint8_t pinOe = PIN_OE_HW;
bool oeActiveHighBlank = true;
bool flipShift = false;

enum Pattern : uint8_t {
  PAT_OFF, PAT_RED, PAT_GREEN, PAT_BLUE, PAT_TOP, PAT_BOT,
  PAT_ROWWALK, PAT_COLWALK, PAT_RULER, PAT_SECTION
};
Pattern pat = PAT_RED;
uint8_t walkRow = 0;
uint8_t walkCol = 0;
uint8_t walkSection = 0;
uint32_t walkMs = 0;

void setAddr(uint8_t row) {
  digitalWrite(PIN_A, row & 1);
  digitalWrite(PIN_B, (row >> 1) & 1);
  digitalWrite(PIN_C, (row >> 2) & 1);
  digitalWrite(PIN_D, (row >> 3) & 1);
  digitalWrite(PIN_E, LOW);
}

void pulseLat() {
  digitalWrite(pinLat, HIGH);
  digitalWrite(pinLat, LOW);
}

void oeBlank() {
  digitalWrite(pinOe, oeActiveHighBlank ? HIGH : LOW);
}

void oeShow() {
  digitalWrite(pinOe, oeActiveHighBlank ? LOW : HIGH);
}

// Mega: R1..B2 = PORTA bits 2..7 (D24–D29), CLK = PORTB bit 5 (D11).
void writePixel(bool topOn, bool botOn, bool r, bool g, bool b) {
  uint8_t bits = 0;
  if (topOn && r) bits |= _BV(2);
  if (topOn && g) bits |= _BV(3);
  if (topOn && b) bits |= _BV(4);
  if (botOn && r) bits |= _BV(5);
  if (botOn && g) bits |= _BV(6);
  if (botOn && b) bits |= _BV(7);
  PORTA = (PORTA & 0x03) | bits;
  PORTB |= _BV(5);
  PORTB &= ~_BV(5);
}

uint8_t mapX(uint8_t i) {
  return flipShift ? (uint8_t)(63 - i) : i;
}

void shiftSolid(bool topOn, bool botOn, bool r, bool g, bool b) {
  for (uint8_t i = 0; i < 64; i++) {
    writePixel(topOn, botOn, r, g, b);
  }
}

void shiftOneColumn(uint8_t col, bool topOn, bool botOn, bool r, bool g, bool b) {
  uint8_t col2 = (uint8_t)((col + 1) & 63);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = mapX(i);
    bool on = (x == col) || (x == col2);
    writePixel(topOn && on, botOn && on, r, g, b);
  }
}

void shiftRuler(bool topOn, bool botOn) {
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = mapX(i);
    bool tick = (x == 0 || x == 16 || x == 32 || x == 48 || x == 63);
    if (tick) {
      writePixel(topOn, botOn, false, true, false);
    } else {
      writePixel(false, false, false, false, false);
    }
  }
}

void shiftSection(uint8_t section, bool topOn, bool botOn) {
  uint8_t lo = (uint8_t)(section * 16);
  uint8_t hi = (uint8_t)(lo + 15);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = mapX(i);
    bool on = (x >= lo && x <= hi);
    writePixel(topOn && on, botOn && on, false, true, false);
  }
}

void latchAndScanRows(uint16_t onUs) {
  for (uint8_t row = 0; row < 16; row++) {
    oeBlank();
    setAddr(row);
    pulseLat();
    oeShow();
    delayMicroseconds(onUs);
  }
  oeBlank();
}

void paintOnce() {
  if (pat == PAT_ROWWALK) {
    if (millis() - walkMs >= 500) {
      walkMs = millis();
      walkRow = (walkRow + 1) & 0x0F;
      Serial.print(F("Row "));
      Serial.print(walkRow);
      Serial.print(F(" + "));
      Serial.println((int)walkRow + 16);
    }
    oeBlank();
    shiftSolid(true, true, true, false, false);
    setAddr(walkRow);
    pulseLat();
    oeShow();
    delayMicroseconds(800);
    return;
  }

  if (pat == PAT_COLWALK) {
    if (millis() - walkMs >= 280) {
      walkMs = millis();
      walkCol = (walkCol + 1) & 63;
      Serial.print(F("Col "));
      Serial.print(walkCol);
      Serial.print(F("-"));
      Serial.print((walkCol + 1) & 63);
      Serial.print(F(" flip="));
      Serial.println(flipShift ? F("Y") : F("N"));
    }
    oeBlank();
    shiftOneColumn(walkCol, true, true, true, false, false);
    latchAndScanRows(500);
    return;
  }

  if (pat == PAT_RULER) {
    oeBlank();
    shiftRuler(true, true);
    latchAndScanRows(400);
    return;
  }

  if (pat == PAT_SECTION) {
    if (millis() - walkMs >= 1500) {
      walkMs = millis();
      walkSection = (walkSection + 1) & 3;
      Serial.print(F("Section "));
      Serial.print(walkSection);
      Serial.print(F(" cols "));
      Serial.print((int)walkSection * 16);
      Serial.print(F("-"));
      Serial.println((int)walkSection * 16 + 15);
    }
    oeBlank();
    shiftSection(walkSection, true, true);
    latchAndScanRows(400);
    return;
  }

  bool topOn = true, botOn = true, r = false, g = false, b = false;
  switch (pat) {
    case PAT_OFF:   topOn = botOn = false; break;
    case PAT_RED:   r = true; break;
    case PAT_GREEN: g = true; break;
    case PAT_BLUE:  b = true; break;
    case PAT_TOP:   r = true; botOn = false; break;
    case PAT_BOT:   r = true; topOn = false; break;
    default: break;
  }
  oeBlank();
  shiftSolid(topOn, botOn, r, g, b);
  latchAndScanRows(300);
}

void printHelp() {
  Serial.println(F("Bit-bang matrix test (new panel)"));
  Serial.println(F("  r/g/b  t/n  0 off"));
  Serial.println(F("  s row-walk (row N and N+16 together = OK)"));
  Serial.println(F("  c col-walk (bright 2px red line 0->63)"));
  Serial.println(F("  m ruler cols 0,16,32,48,63 green"));
  Serial.println(F("  q section walk (16-col bands)"));
  Serial.println(F("  f flip shift dir   i OE pol   x LAT/OE   h help"));
  Serial.print(F("flip="));
  Serial.print(flipShift ? F("Y") : F("N"));
  Serial.print(F("  LAT=D"));
  Serial.print(pinLat);
  Serial.print(F(" OE=D"));
  Serial.println(pinOe);
}

void setup() {
  Serial.begin(9600);
  uint8_t outs[] = {
    PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2,
    PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_CLK, PIN_LAT_HW, PIN_OE_HW
  };
  for (uint8_t i = 0; i < sizeof(outs); i++) {
    pinMode(outs[i], OUTPUT);
    digitalWrite(outs[i], LOW);
  }
  oeBlank();
  delay(200);
  printHelp();
  Serial.println(F("Pattern=RED  LAT=D9 OE=D10"));
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') continue;
    if (c == 'h' || c == 'H' || c == '?') { printHelp(); continue; }
    if (c == '0') { pat = PAT_OFF; Serial.println(F("OFF")); }
    else if (c == 'r' || c == 'R') { pat = PAT_RED; Serial.println(F("RED")); }
    else if (c == 'g' || c == 'G') { pat = PAT_GREEN; Serial.println(F("GREEN")); }
    else if (c == 'b' || c == 'B') { pat = PAT_BLUE; Serial.println(F("BLUE")); }
    else if (c == 't' || c == 'T') { pat = PAT_TOP; Serial.println(F("TOP RED")); }
    else if (c == 'n' || c == 'N') { pat = PAT_BOT; Serial.println(F("BOTTOM RED")); }
    else if (c == 's' || c == 'S') {
      pat = PAT_ROWWALK;
      walkRow = 0;
      walkMs = millis();
      Serial.println(F("ROW WALK — rows N and N+16 together is correct"));
    } else if (c == 'c' || c == 'C') {
      pat = PAT_COLWALK;
      walkCol = 0;
      walkMs = millis();
      Serial.println(F("COL WALK — 2px red line should sweep left->right"));
    } else if (c == 'm' || c == 'M') {
      pat = PAT_RULER;
      Serial.println(F("RULER — green ticks at cols 0 16 32 48 63"));
    } else if (c == 'q' || c == 'Q') {
      pat = PAT_SECTION;
      walkSection = 0;
      walkMs = millis();
      Serial.println(F("SECTION WALK — each 16-col band green in turn"));
    } else if (c == 'f' || c == 'F') {
      flipShift = !flipShift;
      Serial.print(F("flipShift="));
      Serial.println(flipShift ? F("Y") : F("N"));
    } else if (c == 'i' || c == 'I') {
      oeActiveHighBlank = !oeActiveHighBlank;
      Serial.print(F("OE blank ACTIVE_"));
      Serial.println(oeActiveHighBlank ? F("HIGH") : F("LOW"));
    } else if (c == 'x' || c == 'X') {
      uint8_t tmp = pinLat;
      pinLat = pinOe;
      pinOe = tmp;
      Serial.print(F("LAT=D"));
      Serial.print(pinLat);
      Serial.print(F(" OE=D"));
      Serial.println(pinOe);
    }
  }
  paintOnce();
}
