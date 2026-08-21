/*
 * Software HUB75 driver for Mega — used when Adafruit ISR can't drive the panel.
 * Pins match Waveshare wiring confirmed by test_matrix_bitbang (LAT=D9, OE=D10).
 */
#pragma once

#include <Arduino.h>
#include <string.h>

#ifndef HUB75_SOFT_W
#define HUB75_SOFT_W 64
#endif
#ifndef HUB75_SOFT_H
#define HUB75_SOFT_H 32
#endif

enum Hub75Color : uint8_t {
  H75_OFF = 0,
  H75_R   = 1,
  H75_G   = 2,
  H75_RG  = 3,
  H75_B   = 4,
  H75_RB  = 5,
  H75_GB  = 6,
  H75_W   = 7
};

class Hub75Soft {
public:
  uint8_t pinR1 = 24, pinG1 = 25, pinB1 = 26;
  uint8_t pinR2 = 27, pinG2 = 28, pinB2 = 29;
  uint8_t pinA = A0, pinB = A1, pinC = A2, pinD = A3, pinE = A4;
  uint8_t pinClk = 11, pinLat = 9, pinOe = 10;
  bool oeActiveHighBlank = true;

  uint8_t fb[HUB75_SOFT_H][HUB75_SOFT_W];

  void begin() {
    uint8_t pins[] = {
      pinR1, pinG1, pinB1, pinR2, pinG2, pinB2,
      pinA, pinB, pinC, pinD, pinE, pinClk, pinLat, pinOe
    };
    for (uint8_t i = 0; i < sizeof(pins); i++) {
      pinMode(pins[i], OUTPUT);
      digitalWrite(pins[i], LOW);
    }
    oeBlank();
    clear();
  }

  void clear() { memset(fb, 0, sizeof(fb)); }

  void setPixel(int16_t x, int16_t y, uint8_t color) {
    if ((uint16_t)x >= HUB75_SOFT_W || (uint16_t)y >= HUB75_SOFT_H) return;
    fb[y][x] = color & 7;
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t yy = y; yy < y + h; yy++) {
      for (int16_t xx = x; xx < x + w; xx++) setPixel(xx, yy, color);
    }
  }

  const uint8_t *glyphRows(char ch) {
    static const uint8_t DIGITS[][5] = {
      {0b111, 0b101, 0b101, 0b101, 0b111},
      {0b010, 0b110, 0b010, 0b010, 0b111},
      {0b111, 0b001, 0b111, 0b100, 0b111},
      {0b111, 0b001, 0b111, 0b001, 0b111},
      {0b101, 0b101, 0b111, 0b001, 0b001},
      {0b111, 0b100, 0b111, 0b001, 0b111},
      {0b111, 0b100, 0b111, 0b101, 0b111},
      {0b111, 0b001, 0b001, 0b001, 0b001},
      {0b111, 0b101, 0b111, 0b101, 0b111},
      {0b111, 0b101, 0b111, 0b001, 0b111},
    };
    static const uint8_t L_H[5] = {0b101, 0b101, 0b111, 0b101, 0b101};
    static const uint8_t L_A[5] = {0b010, 0b101, 0b111, 0b101, 0b101};
    static const uint8_t L_P[5] = {0b111, 0b101, 0b111, 0b100, 0b100};
    static const uint8_t L_S[5] = {0b111, 0b100, 0b111, 0b001, 0b111};
    static const uint8_t L_T[5] = {0b111, 0b010, 0b010, 0b010, 0b010};
    static const uint8_t L_O[5] = {0b111, 0b101, 0b101, 0b101, 0b111};
    static const uint8_t L_I[5] = {0b111, 0b010, 0b010, 0b010, 0b111};
    static const uint8_t L_N[5] = {0b101, 0b111, 0b111, 0b101, 0b101};
    static const uint8_t L_E[5] = {0b111, 0b100, 0b111, 0b100, 0b111};
    static const uint8_t COLON[5] = {0b000, 0b010, 0b000, 0b010, 0b000};
    if (ch >= '0' && ch <= '9') return DIGITS[ch - '0'];
    if (ch == 'H' || ch == 'h') return L_H;
    if (ch == 'A' || ch == 'a') return L_A;
    if (ch == 'P' || ch == 'p') return L_P;
    if (ch == 'S' || ch == 's') return L_S;
    if (ch == 'T' || ch == 't') return L_T;
    if (ch == 'O' || ch == 'o') return L_O;
    if (ch == 'I' || ch == 'i') return L_I;
    if (ch == 'N' || ch == 'n') return L_N;
    if (ch == 'E' || ch == 'e') return L_E;
    if (ch == ':') return COLON;
    return nullptr;
  }

  void drawGlyph(int16_t x, int16_t y, char ch, uint8_t color, uint8_t scale) {
    const uint8_t *g = glyphRows(ch);
    if (!g) return;
    for (uint8_t row = 0; row < 5; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        if (!(g[row] & (0b100 >> col))) continue;
        if (scale <= 1) setPixel(x + col, y + row, color);
        else fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }

  void printAt(int16_t x, int16_t y, const char *s, uint8_t color, uint8_t scale = 1) {
    uint8_t step = (uint8_t)(3 * scale + scale); // glyph + gap
    while (*s) {
      drawGlyph(x, y, *s++, color, scale);
      x += step;
    }
  }

  void oeBlank() { digitalWrite(pinOe, oeActiveHighBlank ? HIGH : LOW); }
  void oeShow() { digitalWrite(pinOe, oeActiveHighBlank ? LOW : HIGH); }

  void setAddr(uint8_t row) {
    digitalWrite(pinA, row & 1);
    digitalWrite(pinB, (row >> 1) & 1);
    digitalWrite(pinC, (row >> 2) & 1);
    digitalWrite(pinD, (row >> 3) & 1);
    digitalWrite(pinE, LOW);
  }

  void pulseClk() {
    digitalWrite(pinClk, HIGH);
    digitalWrite(pinClk, LOW);
  }

  void pulseLat() {
    digitalWrite(pinLat, HIGH);
    digitalWrite(pinLat, LOW);
  }

  void shiftPair(uint8_t yTop, uint8_t yBot) {
    for (uint8_t x = 0; x < HUB75_SOFT_W; x++) {
      uint8_t t = fb[yTop][x];
      uint8_t b = fb[yBot][x];
      digitalWrite(pinR1, (t & H75_R) ? HIGH : LOW);
      digitalWrite(pinG1, (t & H75_G) ? HIGH : LOW);
      digitalWrite(pinB1, (t & H75_B) ? HIGH : LOW);
      digitalWrite(pinR2, (b & H75_R) ? HIGH : LOW);
      digitalWrite(pinG2, (b & H75_G) ? HIGH : LOW);
      digitalWrite(pinB2, (b & H75_B) ? HIGH : LOW);
      pulseClk();
    }
  }

  void scan() {
    oeBlank();
    for (uint8_t row = 0; row < 16; row++) {
      shiftPair(row, (uint8_t)(row + 16));
      setAddr(row);
      pulseLat();
      oeShow();
      delayMicroseconds(50);
      oeBlank();
    }
  }
};
