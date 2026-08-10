/*
 * Minimal test: Waveshare RGB-Matrix-P3-64x32 (HUB75) on Arduino Mega 2560
 *
 * Libraries: Adafruit GFX + RGB matrix Panel (Adafruit)
 *
 * Wiring: README "RGB matrix → Mega (HUB75)" — use panel HUB75 **INPUT** (not OUTPUT).
 * Power: external 5 V ≥2.5 A on VH4; PSU GND ↔ Mega GND (short, thick).
 *
 * Set DIAG below, upload, and watch one pattern (no cycling).
 */

#include <Adafruit_GFX.h>
#include <RGBmatrixPanel.h>

#define CLK 11
#define OE   9
#define LAT 10
#define A   A0
#define B   A1
#define C   A2
#define D   A3

// 0 = full red (both halves)
// 1 = R1 top only
// 2 = R2 bottom only
// 3 = full green
// 4 = full blue
// 5 = cycle all patterns (original demo)
#ifndef DIAG
#define DIAG 0
#endif

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

uint16_t COL_R, COL_G, COL_B, COL_W, COL_Y, COL_C;
uint8_t step = 0;
uint32_t lastMs = 0;
const uint32_t STEP_MS = 2500;
int8_t bounceX = 0;
int8_t bounceDir = 1;

void showStatic() {
  matrix.fillScreen(0);
  switch (DIAG) {
    case 1:
      matrix.fillRect(0, 0, 64, 16, COL_R);
      break;
    case 2:
      matrix.fillRect(0, 16, 64, 16, COL_R);
      break;
    case 3:
      matrix.fillScreen(COL_G);
      break;
    case 4:
      matrix.fillScreen(COL_B);
      break;
    default:
      matrix.fillScreen(COL_R);
      break;
  }
}

void setup() {
  matrix.begin();
  matrix.setTextWrap(false);
  // Moderate brightness — easier on the PSU while debugging flicker
  COL_R = matrix.Color333(3, 0, 0);
  COL_G = matrix.Color333(0, 3, 0);
  COL_B = matrix.Color333(0, 0, 3);
  COL_W = matrix.Color333(3, 3, 3);
  COL_Y = matrix.Color333(3, 3, 0);
  COL_C = matrix.Color333(0, 3, 3);

  if (DIAG != 5) {
    showStatic();
  } else {
    lastMs = millis();
    drawStep();
  }
}

void fillBand(int y0, int h, uint16_t color) {
  matrix.fillRect(0, y0, 64, h, color);
}

void label(const char *msg, uint16_t color) {
  matrix.setTextSize(1);
  matrix.setTextColor(color);
  matrix.setCursor(2, 12);
  matrix.print(msg);
}

void loop() {
  if (DIAG != 5) {
    // Hold static pattern; refresh occasionally in case of glitches
    static uint32_t refreshMs = 0;
    if (millis() - refreshMs > 500) {
      refreshMs = millis();
      showStatic();
    }
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
      matrix.fillScreen(0);
      matrix.drawRect(0, 0, 64, 32, COL_W);
      matrix.fillRect(bounceX, 12, 4, 8, COL_Y);
    }
  }
}

void drawStep() {
  matrix.fillScreen(0);

  switch (step) {
    case 0: matrix.fillScreen(COL_R); break;
    case 1: matrix.fillScreen(COL_G); break;
    case 2: matrix.fillScreen(COL_B); break;
    case 3:
      fillBand(0, 16, COL_R);
      label("R1 TOP", COL_W);
      break;
    case 4:
      fillBand(16, 16, COL_R);
      label("R2 BOT", COL_W);
      break;
    case 5:
      fillBand(0, 16, COL_G);
      label("G1 TOP", COL_W);
      break;
    case 6:
      fillBand(16, 16, COL_G);
      label("G2 BOT", COL_W);
      break;
    case 7:
      fillBand(0, 16, COL_B);
      label("B1 TOP", COL_W);
      break;
    case 8:
      fillBand(16, 16, COL_B);
      label("B2 BOT", COL_W);
      break;
    case 9: {
      for (int x = 0; x < 64; x++) {
        uint16_t c = (x < 21) ? COL_R : (x < 42) ? COL_G : COL_B;
        matrix.drawFastVLine(x, 0, 32, c);
      }
      break;
    }
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
      matrix.fillRect(0, 12, 4, 8, COL_Y);
      break;
  }
}
