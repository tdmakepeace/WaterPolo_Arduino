/*
 * Vendor-style colour test on Arduino Mega 2560
 * (same sequence as Waveshare / ESP32-HUB75 DMA SimpleTestShapes loop)
 *
 * Board: Arduino Mega 2560  — NOT ESP32
 * Uses software HUB75 (hub75_soft.h) — same driver as scoreboard bitbang path.
 *
 * Mega HUB75 pinout (this project / Waveshare P3-64x32):
 *   R1 D24   G1 D25   B1 D26
 *   R2 D27   G2 D28   B2 D29
 *   A  A0    B  A1    C  A2    D  A3
 *   E  A4    (held LOW)
 *   CLK D11  LAT D9   OE D10
 *   green #15 → D9 (LAT), yellow #14 → D10 (OE), grey #8 → A4
 *
 * Power: VH4 → 5 V ≥2.5 A; PSU GND ↔ Mega GND
 * Ribbon: HUB75 INPUT
 *
 * Serial @ 9600 optional (status only).
 */

#include "hub75_soft.h"

Hub75Soft matrix;

uint8_t wheelval = 0;

// Approximate colour-wheel as 1-bit RGB for soft driver
uint8_t colorWheelSoft(uint8_t pos) {
  if (pos < 85) {
    // R→G
    if (pos < 42) return H75_R;
    return H75_RG;
  } else if (pos < 170) {
    pos -= 85;
    if (pos < 42) return H75_G;
    return H75_GB;
  } else {
    pos -= 170;
    if (pos < 42) return H75_B;
    return H75_RB;
  }
}

void drawText(int colorWheelOffset) {
  matrix.clear();
  const char *str = "MEGA HUB";
  int16_t x = 2;
  for (uint8_t w = 0; w < strlen(str); w++) {
    matrix.drawGlyph(x, 0, str[w], colorWheelSoft((w * 32) + colorWheelOffset), 1);
    x += 4;
  }
  matrix.printAt(2, 8, "LED MATRIX", H75_W, 1);
  matrix.printAt(2, 16, "64x32", H75_GB, 1);
  matrix.printAt(2, 24, "*RGB*", H75_RG, 1);
}

void fillSolid(uint8_t color) {
  matrix.fillRect(0, 0, 64, 32, color);
}

// Keep scanning during long delays so the panel does not go blank
void delayWithScan(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    matrix.scan();
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(A4, OUTPUT);
  digitalWrite(A4, LOW);

  // Explicit Mega pin allocation (matches project README / scoreboard)
  matrix.pinR1 = 24;
  matrix.pinG1 = 25;
  matrix.pinB1 = 26;
  matrix.pinR2 = 27;
  matrix.pinG2 = 28;
  matrix.pinB2 = 29;
  matrix.pinA = A0;
  matrix.pinB = A1;
  matrix.pinC = A2;
  matrix.pinD = A3;
  matrix.pinE = A4;
  matrix.pinClk = 11;
  matrix.pinLat = 9;   // green #15
  matrix.pinOe = 10;   // yellow #14

  matrix.begin();
  Serial.println(F("Vendor-style test on Mega (soft HUB75)"));
  Serial.println(F("LAT=D9 OE=D10 CLK=D11 R1-B2=D24-29 A-D=A0-A3 E=A4"));

  // Short shape intro (vendor SimpleTestShapes style)
  fillSolid(H75_W);
  delayWithScan(300);
  fillSolid(H75_G);
  delayWithScan(400);
  matrix.clear();
  matrix.fillRect(0, 0, 64, 1, H75_RG);
  matrix.fillRect(0, 31, 64, 1, H75_RG);
  matrix.fillRect(0, 0, 1, 32, H75_RG);
  matrix.fillRect(63, 0, 1, 32, H75_RG);
  delayWithScan(400);
  matrix.clear();
}

void loop() {
  // Same sequence as Waveshare / ESP32 DMA vendor loop
  drawText(wheelval);
  wheelval += 1;
  delayWithScan(2000);

  matrix.clear();
  fillSolid(H75_OFF);  // myBLACK
  delayWithScan(2000);

  fillSolid(H75_R);    // myRED
  delayWithScan(2000);

  fillSolid(H75_G);    // myGREEN
  delayWithScan(2000);

  fillSolid(H75_B);    // myBLUE
  delayWithScan(2000);

  fillSolid(H75_W);    // myWHITE
  delayWithScan(2000);

  matrix.clear();
}
