/*
 * Ultra-slow HUB75 bit-bang — no Adafruit library.
 * Board: Arduino Mega 2560 + Waveshare 64x32 (1/16 scan)
 *
 * Wiring (same as scoreboard):
 *   R1 D24  G1 D25  B1 D26
 *   R2 D27  G2 D28  B2 D29
 *   A  A0   B  A1   C  A2   D  A3
 *   E  A4   (hold LOW) — grey #8
 *   CLK D11  LAT D9  OE D10
 *   Wire: green #15 → D9, yellow #14 → D10 (this panel needs LAT/OE
 *   swapped vs Adafruit charts — press x only if you change wiring)
 *   GND: yellow #4 + blue #16 → Mega GND
 *   VH4: 5 V PSU; PSU GND ↔ Mega GND
 *   Ribbon in HUB75 **INPUT** only
 *
 * Serial @ 9600, Newline:
 *   r/g/b  full colour     t top-red   n bottom-red   0 off
 *   s      row walk
 *   c      column walk (single red vertical line 0→63)
 *   m      mark cols 49–52 green, rest red (isolate bad strip)
 *   q      section walk — light each 16-col band (0-15,16-31,32-47,48-63)
 *   f      flip shift direction (if column walk moves the wrong way)
 *   i      OE polarity    x LAT/OE swap    h help
 */

#include <Arduino.h>

const uint8_t PIN_R1 = 24, PIN_G1 = 25, PIN_B1 = 26;
const uint8_t PIN_R2 = 27, PIN_G2 = 28, PIN_B2 = 29;
const uint8_t PIN_A = A0, PIN_B = A1, PIN_C = A2, PIN_D = A3, PIN_E = A4;
const uint8_t PIN_CLK = 11;
// Working map on this Waveshare (matches bitbang after "x"): LAT=D9, OE=D10
const uint8_t PIN_LAT_HW = 9;
const uint8_t PIN_OE_HW = 10;

// Known bad column band on the current (failing) panel
const uint8_t BAD_COL_LO = 49;
const uint8_t BAD_COL_HI = 52;

uint8_t pinLat = PIN_LAT_HW;
uint8_t pinOe = PIN_OE_HW;
bool oeActiveHighBlank = true;
bool flipShift = false;

enum Pattern : uint8_t {
  PAT_OFF, PAT_RED, PAT_GREEN, PAT_BLUE, PAT_TOP, PAT_BOT,
  PAT_ROWWALK, PAT_COLWALK, PAT_COLMARK, PAT_SECTION
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

void pulseClk() {
  digitalWrite(PIN_CLK, HIGH);
  digitalWrite(PIN_CLK, LOW);
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

void writePixel(bool topOn, bool botOn, bool r, bool g, bool b) {
  digitalWrite(PIN_R1, topOn && r);
  digitalWrite(PIN_G1, topOn && g);
  digitalWrite(PIN_B1, topOn && b);
  digitalWrite(PIN_R2, botOn && r);
  digitalWrite(PIN_G2, botOn && g);
  digitalWrite(PIN_B2, botOn && b);
  pulseClk();
}

void shiftSolid(bool topOn, bool botOn, bool r, bool g, bool b) {
  for (uint8_t i = 0; i < 64; i++) {
    writePixel(topOn, botOn, r, g, b);
  }
}

void shiftOneColumn(uint8_t col, bool topOn, bool botOn, bool r, bool g, bool b) {
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = flipShift ? (uint8_t)(63 - i) : i;
    bool on = (x == col);
    writePixel(topOn && on, botOn && on, r, g, b);
  }
}

void shiftMarkBadBand(bool topOn, bool botOn) {
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = flipShift ? (uint8_t)(63 - i) : i;
    bool band = (x >= BAD_COL_LO && x <= BAD_COL_HI);
    // Rest red; bad band green — if band stays wrong colour, those columns are stuck
    if (band) {
      writePixel(topOn, botOn, false, true, false);
    } else {
      writePixel(topOn, botOn, true, false, false);
    }
  }
}

// Light one 16-column driver section green; others off.
void shiftSection(uint8_t section, bool topOn, bool botOn) {
  uint8_t lo = (uint8_t)(section * 16);
  uint8_t hi = (uint8_t)(lo + 15);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t x = flipShift ? (uint8_t)(63 - i) : i;
    bool on = (x >= lo && x <= hi);
    writePixel(topOn && on, botOn && on, false, true, false);
  }
}

bool gTop = true, gBot = true, gR = true, gG = false, gB = false;
uint8_t gCol = 0;
uint8_t gSection = 0;

void shSolid() { shiftSolid(gTop, gBot, gR, gG, gB); }
void shCol() { shiftOneColumn(gCol, true, true, true, false, false); }
void shMark() { shiftMarkBadBand(true, true); }
void shSection() { shiftSection(gSection, true, true); }

void scanAllRows(void (*shifter)()) {
  oeBlank();
  for (uint8_t row = 0; row < 16; row++) {
    shifter();
    setAddr(row);
    pulseLat();
    oeShow();
    delayMicroseconds(80);
    oeBlank();
  }
}

void paintOnce() {
  if (pat == PAT_ROWWALK) {
    if (millis() - walkMs >= 500) {
      walkMs = millis();
      walkRow = (walkRow + 1) & 0x0F;
      Serial.print(F("Row "));
      Serial.println(walkRow);
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
    if (millis() - walkMs >= 120) {
      walkMs = millis();
      walkCol = (walkCol + 1) & 63;
      if (walkCol == 0 || walkCol == BAD_COL_LO || walkCol == BAD_COL_HI) {
        Serial.print(F("Col "));
        Serial.print(walkCol);
        Serial.print(F(" flip="));
        Serial.println(flipShift ? F("Y") : F("N"));
      }
    }
    gCol = walkCol;
    scanAllRows(shCol);
    return;
  }

  if (pat == PAT_COLMARK) {
    scanAllRows(shMark);
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
    gSection = walkSection;
    scanAllRows(shSection);
    return;
  }

  gTop = true;
  gBot = true;
  gR = gG = gB = false;
  switch (pat) {
    case PAT_OFF:   gTop = gBot = false; break;
    case PAT_RED:   gR = true; break;
    case PAT_GREEN: gG = true; break;
    case PAT_BLUE:  gB = true; break;
    case PAT_TOP:   gR = true; gBot = false; break;
    case PAT_BOT:   gR = true; gTop = false; break;
    default: break;
  }
  scanAllRows(shSolid);
}

void printHelp() {
  Serial.println(F("Bit-bang matrix test"));
  Serial.println(F("  r/g/b  t/n  0 off  s row-walk  c col-walk"));
  Serial.print(F("  m mark cols "));
  Serial.print(BAD_COL_LO);
  Serial.print(F("-"));
  Serial.print(BAD_COL_HI);
  Serial.println(F(" green, rest red"));
  Serial.println(F("  q section walk (16-col bands 0/1/2/3)"));
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
  Serial.println(F("Pattern=RED"));
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
      Serial.println(F("ROW WALK"));
    } else if (c == 'c' || c == 'C') {
      pat = PAT_COLWALK;
      walkCol = 0;
      walkMs = millis();
      Serial.println(F("COL WALK — red line should sweep left→right"));
    } else if (c == 'm' || c == 'M') {
      pat = PAT_COLMARK;
      Serial.print(F("MARK "));
      Serial.print(BAD_COL_LO);
      Serial.print(F("-"));
      Serial.print(BAD_COL_HI);
      Serial.println(F(" green, rest red"));
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
