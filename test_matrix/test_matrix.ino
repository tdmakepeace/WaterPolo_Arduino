/*
 * Matrix hardware test — Waveshare RGB-Matrix-P3-64x32 on Arduino Mega 2560
 *
 * Default: software HUB75 (same driver as scoreboard) — works when Adafruit
 * stays white/blue on this panel.
 *
 * Set MATRIX_USE_BITBANG to 0 to test Adafruit RGBmatrixPanel instead
 * (use after a healthy replacement panel).
 *
 * Wiring: HUB75 INPUT; green #15 → D9 (LAT); yellow #14 → D10 (OE);
 *         grey #8 → A4 (E, held LOW); VH4 5 V; PSU GND ↔ Mega GND.
 *
 * Serial @ 9600, Newline:
 *   0 full red   1 R1 top   2 R2 bottom
 *   3 full green 4 full blue 5 cycle
 *   h help
 */

#ifndef MATRIX_USE_BITBANG
#define MATRIX_USE_BITBANG 1
#endif

#include <Arduino.h>

#if MATRIX_USE_BITBANG
#include "hub75_soft.h"
#else
#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>
#endif

#define CLK 11
#define OE  10
#define LAT  9
#define A   A0
#define B   A1
#define C   A2
#define D   A3
#define E   A4

#ifndef DIAG
#define DIAG 0
#endif

#if MATRIX_USE_BITBANG
Hub75Soft matrix;
#else
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);
uint16_t COL_R, COL_G, COL_B, COL_W, COL_Y, COL_C;
#endif

uint8_t diagMode = DIAG;
uint8_t step = 0;
uint32_t lastMs = 0;
const uint32_t STEP_MS = 2500;
int8_t bounceX = 0;
int8_t bounceDir = 1;
char lineBuf[8];
uint8_t lineLen = 0;

void printHelp() {
  Serial.println();
#if MATRIX_USE_BITBANG
  Serial.println(F("Matrix DIAG — SOFTWARE HUB75 (bitbang)"));
#else
  Serial.println(F("Matrix DIAG — Adafruit RGBmatrixPanel"));
#endif
  Serial.println(F("  0  full red (both halves)"));
  Serial.println(F("  1  R1 top half only"));
  Serial.println(F("  2  R2 bottom half only"));
  Serial.println(F("  3  full green"));
  Serial.println(F("  4  full blue"));
  Serial.println(F("  5  cycle patterns"));
  Serial.println(F("  h  help"));
  Serial.println(F("Wiring: green#15->D9 LAT, yellow#14->D10 OE, grey#8->A4"));
  Serial.print(F("Current DIAG="));
  Serial.println(diagMode);
}

#if MATRIX_USE_BITBANG
void showStatic() {
  matrix.clear();
  switch (diagMode) {
    case 1:
      matrix.fillRect(0, 0, 64, 16, H75_R);
      break;
    case 2:
      matrix.fillRect(0, 16, 64, 16, H75_R);
      break;
    case 3:
      matrix.fillRect(0, 0, 64, 32, H75_G);
      break;
    case 4:
      matrix.fillRect(0, 0, 64, 32, H75_B);
      break;
    default:
      matrix.fillRect(0, 0, 64, 32, H75_R);
      break;
  }
}

void drawStep() {
  matrix.clear();
  switch (step) {
    case 0: matrix.fillRect(0, 0, 64, 32, H75_R); break;
    case 1: matrix.fillRect(0, 0, 64, 32, H75_G); break;
    case 2: matrix.fillRect(0, 0, 64, 32, H75_B); break;
    case 3:
      matrix.fillRect(0, 0, 64, 16, H75_R);
      matrix.printAt(2, 12, "R1", H75_W, 1);
      break;
    case 4:
      matrix.fillRect(0, 16, 64, 16, H75_R);
      matrix.printAt(2, 20, "R2", H75_W, 1);
      break;
    case 5:
      matrix.fillRect(0, 0, 64, 16, H75_G);
      matrix.printAt(2, 12, "G1", H75_W, 1);
      break;
    case 6:
      matrix.fillRect(0, 16, 64, 16, H75_G);
      matrix.printAt(2, 20, "G2", H75_W, 1);
      break;
    case 7:
      matrix.fillRect(0, 0, 64, 16, H75_B);
      matrix.printAt(2, 12, "B1", H75_W, 1);
      break;
    case 8:
      matrix.fillRect(0, 16, 64, 16, H75_B);
      matrix.printAt(2, 20, "B2", H75_W, 1);
      break;
    case 9:
      for (int x = 0; x < 64; x++) {
        uint8_t c = (x < 21) ? H75_R : (x < 42) ? H75_G : H75_B;
        matrix.fillRect(x, 0, 1, 32, c);
      }
      break;
    case 10:
      matrix.printAt(2, 4, "OK", H75_W, 2);
      matrix.printAt(14, 20, "64", H75_GB, 1);
      break;
    case 11:
      matrix.fillRect(0, 0, 64, 1, H75_W);
      matrix.fillRect(0, 31, 64, 1, H75_W);
      matrix.fillRect(0, 0, 1, 32, H75_W);
      matrix.fillRect(63, 0, 1, 32, H75_W);
      matrix.fillRect(bounceX, 12, 4, 8, H75_RG);
      break;
  }
}
#else
void showStatic() {
  matrix.fillScreen(0);
  switch (diagMode) {
    case 1: matrix.fillRect(0, 0, 64, 16, COL_R); break;
    case 2: matrix.fillRect(0, 16, 64, 16, COL_R); break;
    case 3: matrix.fillScreen(COL_G); break;
    case 4: matrix.fillScreen(COL_B); break;
    default: matrix.fillScreen(COL_R); break;
  }
}

void label(const char *msg, uint16_t color) {
  matrix.setTextSize(1);
  matrix.setTextColor(color);
  matrix.setCursor(2, 12);
  matrix.print(msg);
}

void fillBand(int y0, int h, uint16_t color) {
  matrix.fillRect(0, y0, 64, h, color);
}

void drawStep() {
  matrix.fillScreen(0);
  switch (step) {
    case 0: matrix.fillScreen(COL_R); break;
    case 1: matrix.fillScreen(COL_G); break;
    case 2: matrix.fillScreen(COL_B); break;
    case 3: fillBand(0, 16, COL_R); label("R1 TOP", COL_W); break;
    case 4: fillBand(16, 16, COL_R); label("R2 BOT", COL_W); break;
    case 5: fillBand(0, 16, COL_G); label("G1 TOP", COL_W); break;
    case 6: fillBand(16, 16, COL_G); label("G2 BOT", COL_W); break;
    case 7: fillBand(0, 16, COL_B); label("B1 TOP", COL_W); break;
    case 8: fillBand(16, 16, COL_B); label("B2 BOT", COL_W); break;
    case 9:
      for (int x = 0; x < 64; x++) {
        uint16_t c = (x < 21) ? COL_R : (x < 42) ? COL_G : COL_B;
        matrix.drawFastVLine(x, 0, 32, c);
      }
      break;
    case 10:
      matrix.setTextSize(1);
      matrix.setTextColor(COL_W);
      matrix.setCursor(2, 4);
      matrix.print("MATRIX OK");
      matrix.setTextColor(COL_C);
      matrix.setCursor(14, 16);
      matrix.print("64x32");
      break;
    case 11:
      matrix.drawRect(0, 0, 64, 32, COL_W);
      matrix.fillRect(bounceX, 12, 4, 8, COL_Y);
      break;
  }
}
#endif

void setDiag(uint8_t mode) {
  if (mode > 5) {
    Serial.println(F("Out of range. Use 0-5"));
    return;
  }
  diagMode = mode;
  step = 0;
  bounceX = 0;
  bounceDir = 1;
  lastMs = millis();
  Serial.print(F("DIAG="));
  Serial.println(diagMode);
  if (diagMode != 5) showStatic();
  else drawStep();
}

void handleCommand(const char *cmd) {
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == '\0') return;
  char c = cmd[0];
  if (c == 'h' || c == 'H' || c == '?') {
    printHelp();
    return;
  }
  if (c >= '0' && c <= '9' && cmd[1] == '\0') {
    setDiag((uint8_t)(c - '0'));
    return;
  }
  Serial.println(F("Unknown. Type 0-5 or h"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) handleCommand(lineBuf);
      lineLen = 0;
      continue;
    }
    if (lineLen + 1 < sizeof(lineBuf)) lineBuf[lineLen++] = c;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(E, OUTPUT);
  digitalWrite(E, LOW);
  matrix.begin();
#if !MATRIX_USE_BITBANG
  matrix.setTextWrap(false);
  COL_R = matrix.Color333(3, 0, 0);
  COL_G = matrix.Color333(0, 3, 0);
  COL_B = matrix.Color333(0, 0, 3);
  COL_W = matrix.Color333(3, 3, 3);
  COL_Y = matrix.Color333(3, 3, 0);
  COL_C = matrix.Color333(0, 3, 3);
#endif
  delay(200);
  printHelp();
  setDiag(diagMode);
}

void loop() {
  pollSerial();

#if MATRIX_USE_BITBANG
  matrix.scan();
#endif

  if (diagMode != 5) {
#if !MATRIX_USE_BITBANG
    static uint32_t refreshMs = 0;
    if (millis() - refreshMs > 500) {
      refreshMs = millis();
      showStatic();
    }
#endif
    return;
  }

  uint32_t now = millis();
  if (now - lastMs >= STEP_MS) {
    lastMs = now;
    step = (step + 1) % 12;
    bounceX = 0;
    bounceDir = 1;
    drawStep();
  }

  if (step == 11) {
    static uint32_t animMs = 0;
    if (now - animMs >= 40) {
      animMs = now;
      bounceX += bounceDir;
      if (bounceX <= 0 || bounceX >= 60) bounceDir = -bounceDir;
#if MATRIX_USE_BITBANG
      drawStep();
#else
      matrix.fillScreen(0);
      matrix.drawRect(0, 0, 64, 32, COL_W);
      matrix.fillRect(bounceX, 12, 4, 8, COL_Y);
#endif
    }
  }
}
