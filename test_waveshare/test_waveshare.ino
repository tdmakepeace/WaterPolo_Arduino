// Waveshare RGB-Matrix demo (from R7FA4-PLUS-A example), adapted for
// Arduino Mega 2560 + Waveshare RGB-Matrix-P3-64x32 (HUB75).
//
// Libraries are vendored in this folder (Adafruit_GFX + RGBmatrixPanel).
//
// Wiring (confirmed Mega map):
//   R1 D24  G1 D25  B1 D26  R2 D27  G2 D28  B2 D29  (PORTA, fixed by lib)
//   A A0  B A1  C A2  D A3  E A4 (driven LOW)
//   CLK D11  LAT D9  OE D10
//   green #15 → D9 (LAT), yellow #14 → D10 (OE)
//   VH4 → external 5 V ≥2.5 A; PSU GND ↔ Mega GND
//
// Serial @ 115200, Newline — vendor solid-colour tests:
//   r / g / b   hold full red / green / blue
//   m           mark cols 49–52 green, rest red (isolate stuck strip)
//   c           auto-cycle r→g→b→mark (default)
//   d           original Waveshare Demo_0 shapes
//   h           help
//
// Expected on a healthy panel: solid full-screen colours.
// On the failing unit: cols 49–52 stay red under green/blue; blue incomplete.

#include "RGBmatrixPanel.h"

#include "bit_bmp.h"
#include "Fonts/fonts.h"
#include <stdlib.h>
#include <string.h>

// HUB75 pin mapping — Arduino Mega 2560 (same names as Waveshare R7FA4 example)
//
// R1..B2 MUST be these Mega pins. On AVR Mega the library writes the whole
// PORTA register for speed — it does NOT read a pinlist like the R7FA4 demo.
// Changing R1..B2 numbers below will NOT remount the library; wire to D24–D29.
#define CONFIG_HUB75_PIN_R1 24
#define CONFIG_HUB75_PIN_G1 25
#define CONFIG_HUB75_PIN_B1 26
#define CONFIG_HUB75_PIN_R2 27
#define CONFIG_HUB75_PIN_G2 28
#define CONFIG_HUB75_PIN_B2 29
#define CONFIG_HUB75_PIN_A A0
#define CONFIG_HUB75_PIN_B A1
#define CONFIG_HUB75_PIN_C A2
#define CONFIG_HUB75_PIN_D A3
#define CONFIG_HUB75_PIN_E A4
#define CONFIG_HUB75_PIN_LAT 9
#define CONFIG_HUB75_PIN_OE 10
#define CONFIG_HUB75_PIN_CLK 11

#define CLK CONFIG_HUB75_PIN_CLK
#define OE CONFIG_HUB75_PIN_OE
#define LAT CONFIG_HUB75_PIN_LAT
#define A CONFIG_HUB75_PIN_A
#define B CONFIG_HUB75_PIN_B
#define C CONFIG_HUB75_PIN_C
#define D CONFIG_HUB75_PIN_D
#define E CONFIG_HUB75_PIN_E

// Waveshare RGB-Matrix-P3-64x32
#define Matrix_Width 64
#define Matrix_Height 32

#define HUB75_PANEL_STANDARD 1
#define CONFIG_HUB75_PANEL_MODE HUB75_PANEL_STANDARD
#define CONFIG_HUB75_PANEL_OPTIONS RGBMATRIX_PANEL_OPTION_NONE

// Known bad column band observed with bitbang on the failing panel
const uint8_t BAD_COL_LO = 49;
const uint8_t BAD_COL_HI = 52;

// Constructor only takes address + CLK/LAT/OE on Mega.
// R1..B2 are implied = D24..D29 (library DATAPORT = PORTA).
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, Matrix_Width,
                      CONFIG_HUB75_PANEL_OPTIONS);

enum Mode : uint8_t {
  MODE_CYCLE,
  MODE_RED,
  MODE_GREEN,
  MODE_BLUE,
  MODE_MARK,
  MODE_DEMO0
};
Mode mode = MODE_CYCLE;
uint8_t cycleStep = 0;
uint32_t stepMs = 0;
const uint32_t HOLD_MS = 3000;

char lineBuf[8];
uint8_t lineLen = 0;

static void panel_delay(uint32_t ms) {
  uint32_t start_ms = millis();
  while ((millis() - start_ms) < ms) {
    matrix.updateDisplay();
  }
}

void screen_clear() {
  matrix.fillRect(0, 0, matrix.width(), matrix.height(),
                  matrix.Color333(0, 0, 0));
}

void fillSolid(uint8_t r, uint8_t g, uint8_t b) {
  matrix.fillScreen(matrix.Color333(r, g, b));
}

// Rest of panel red; cols 49–52 green. Stuck-red columns will not turn green.
void markBadBand() {
  uint16_t red = matrix.Color333(7, 0, 0);
  uint16_t green = matrix.Color333(0, 7, 0);
  for (int16_t x = 0; x < Matrix_Width; x++) {
    uint16_t c = (x >= BAD_COL_LO && x <= BAD_COL_HI) ? green : red;
    matrix.drawFastVLine(x, 0, Matrix_Height, c);
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("test_waveshare vendor diag (Waveshare lib on Mega)"));
  Serial.println(F("  r  full RED     g  full GREEN     b  full BLUE"));
  Serial.println(F("  m  mark cols 49-52 green (rest red)"));
  Serial.println(F("  c  auto-cycle r/g/b/mark"));
  Serial.println(F("  d  Waveshare Demo_0 shapes"));
  Serial.println(F("  h  help"));
  Serial.println(F("Photo solid R/G/B + mark for vendor if cols 49-52 stay red."));
}

void applyHoldPattern() {
  switch (mode) {
    case MODE_RED:
      fillSolid(7, 0, 0);
      Serial.println(F("HOLD: full RED"));
      break;
    case MODE_GREEN:
      fillSolid(0, 7, 0);
      Serial.println(F("HOLD: full GREEN (cols 49-52 should be green)"));
      break;
    case MODE_BLUE:
      fillSolid(0, 0, 7);
      Serial.println(F("HOLD: full BLUE (cols 49-52 should be blue)"));
      break;
    case MODE_MARK:
      markBadBand();
      Serial.println(F("HOLD: mark 49-52 green / rest red"));
      break;
    default:
      break;
  }
}

void handleCommand(const char *cmd) {
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == '\0') return;
  char c = cmd[0];
  if (c == 'h' || c == 'H' || c == '?') {
    printHelp();
    return;
  }
  if (c == 'c' || c == 'C') {
    mode = MODE_CYCLE;
    cycleStep = 0;
    stepMs = millis();
    Serial.println(F("MODE: cycle"));
    return;
  }
  if (c == 'd' || c == 'D') {
    mode = MODE_DEMO0;
    Serial.println(F("MODE: Demo_0"));
    return;
  }
  if (c == 'r' || c == 'R') {
    mode = MODE_RED;
    applyHoldPattern();
    return;
  }
  if (c == 'g' || c == 'G') {
    mode = MODE_GREEN;
    applyHoldPattern();
    return;
  }
  if (c == 'b' || c == 'B') {
    mode = MODE_BLUE;
    applyHoldPattern();
    return;
  }
  if (c == 'm' || c == 'M') {
    mode = MODE_MARK;
    applyHoldPattern();
    return;
  }
  Serial.println(F("Unknown. Type h"));
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

void runCycleStep() {
  switch (cycleStep % 4) {
    case 0:
      fillSolid(7, 0, 0);
      Serial.println(F("CYCLE: full RED"));
      break;
    case 1:
      fillSolid(0, 7, 0);
      Serial.println(F("CYCLE: full GREEN"));
      break;
    case 2:
      fillSolid(0, 0, 7);
      Serial.println(F("CYCLE: full BLUE"));
      break;
    default:
      markBadBand();
      Serial.println(F("CYCLE: mark cols 49-52"));
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // This Waveshare panel exposes E on IDC pin 8 — hold LOW (not used for 1/16 scan).
  pinMode(E, OUTPUT);
  digitalWrite(E, LOW);

  matrix.begin();
  panel_delay(500);
  Serial.println(F("test_waveshare: Mega + Waveshare 64x32"));
  Serial.println(F("Expected wiring (R1-B2 fixed by lib to PORTA):"));
  Serial.print(F("  R1=")); Serial.print(CONFIG_HUB75_PIN_R1);
  Serial.print(F(" G1=")); Serial.print(CONFIG_HUB75_PIN_G1);
  Serial.print(F(" B1=")); Serial.print(CONFIG_HUB75_PIN_B1);
  Serial.print(F(" R2=")); Serial.print(CONFIG_HUB75_PIN_R2);
  Serial.print(F(" G2=")); Serial.print(CONFIG_HUB75_PIN_G2);
  Serial.print(F(" B2=")); Serial.println(CONFIG_HUB75_PIN_B2);
  Serial.print(F("  A=")); Serial.print(A);
  Serial.print(F(" B=")); Serial.print(B);
  Serial.print(F(" C=")); Serial.print(C);
  Serial.print(F(" D=")); Serial.print(D);
  Serial.print(F(" E=")); Serial.print(E);
  Serial.print(F(" CLK=")); Serial.print(CLK);
  Serial.print(F(" LAT=")); Serial.print(LAT);
  Serial.print(F(" OE=")); Serial.println(OE);
  printHelp();
  stepMs = millis();
  runCycleStep();
}

void loop() {
  pollSerial();

  if (mode == MODE_DEMO0) {
    Demo_0();
    return;
  }

  if (mode == MODE_CYCLE) {
    if (millis() - stepMs >= HOLD_MS) {
      stepMs = millis();
      cycleStep++;
      runCycleStep();
    }
  }
  // Hold modes: framebuffer already set; ISR keeps refreshing.
}

static int16_t centered_text_x(const char *text, int16_t y_baseline,
                               const GFXfont *font) {
  int16_t x1, y1;
  uint16_t w, h;
  matrix.setFont(font);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.getTextBounds((char *)text, 0, y_baseline, &x1, &y1, &w, &h);
  int16_t x = (matrix.width() - (int16_t)w) / 2;
  if (x < 0) {
    x = 0;
  }
  return x;
}

static void print_centered_rainbow_text(int16_t y_baseline, const char *text,
                                        uint8_t color_offset) {
  matrix.setFont(NULL);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  int16_t x = centered_text_x(text, y_baseline, NULL);
  matrix.setCursor(x, y_baseline);

  for (uint8_t i = 0; text[i] != 0; i++) {
    matrix.setTextColor(Wheel((i + color_offset) % 24));
    matrix.print(text[i]);
  }
}

void Demo_0() {
  int16_t panel_width = matrix.width();
  int16_t panel_height = matrix.height();
  int16_t min_size = (panel_width < panel_height) ? panel_width : panel_height;
  int16_t circle_radius = min_size / 6;
  if (circle_radius < 2) {
    circle_radius = 2;
  }

  screen_clear();
  matrix.setFont(NULL);
  matrix.drawPixel(0, 0, matrix.Color333(7, 7, 7));
  panel_delay(500);

  matrix.fillRect(0, 0, matrix.width(), matrix.height(),
                  matrix.Color333(0, 7, 0));
  panel_delay(500);

  matrix.drawRect(0, 0, matrix.width(), matrix.height(),
                  matrix.Color333(7, 7, 0));
  panel_delay(500);

  matrix.drawLine(0, 0, matrix.width() - 1, matrix.height() - 1,
                  matrix.Color333(7, 0, 0));
  matrix.drawLine(matrix.width() - 1, 0, 0, matrix.height() - 1,
                  matrix.Color333(7, 0, 0));
  panel_delay(500);

  matrix.drawCircle(circle_radius + 1, circle_radius + 1, circle_radius,
                    matrix.Color333(0, 0, 7));
  panel_delay(500);

  matrix.fillCircle((panel_width * 3) / 4, panel_height / 3, circle_radius,
                    matrix.Color333(7, 0, 7));
  panel_delay(500);

  screen_clear();

  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  uint8_t draw_y = Matrix_Height / 4;
  print_centered_rainbow_text(draw_y * 0, "Waveshare", 0);
  print_centered_rainbow_text(draw_y * 1, "Electronics", 4);
  print_centered_rainbow_text(draw_y * 2, "RGB MATRIX", 8);
  char resolution_text[24];
  snprintf(resolution_text, sizeof(resolution_text), "%dx%d  RGB",
          panel_width, panel_height);
  print_centered_rainbow_text(draw_y * 3, resolution_text, 12);

  panel_delay(2000);
}

uint16_t Wheel(byte WheelPos) {
  if (WheelPos < 8) {
    return matrix.Color333(7 - WheelPos, WheelPos, 0);
  } else if (WheelPos < 16) {
    WheelPos -= 8;
    return matrix.Color333(0, 7 - WheelPos, WheelPos);
  } else {
    WheelPos -= 16;
    return matrix.Color333(WheelPos, 0, 7 - WheelPos);
  }
}

void display_Image(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w,
                   int16_t h) {
  matrix.display_image(x, y, bitmap, w, h);
}

#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSerif9pt7b.h"
#include "Fonts/FreeSerifBoldItalic9pt7b.h"
#include "Fonts/RobotoMono_Thin7pt7b.h"

void display_text(int x, int y, char *str, const GFXfont *f, int color,
                  int pixels_size) {
  matrix.setTextSize(pixels_size);
  matrix.setTextWrap(false);
  matrix.setFont(f);
  matrix.setCursor(x, y);
  matrix.setTextColor(color);
  matrix.println(str);
}

void Demo_1() {
  screen_clear();
  display_text(-1, 14, "R", &FreeSerif9pt7b, matrix.Color333(7, 0, 0), 1);
  display_text(8, 14, "G", &FreeSerif9pt7b, matrix.Color333(0, 7, 0), 1);
  display_text(19, 14, "B", &FreeSerif9pt7b, matrix.Color333(0, 0, 7), 1);
  display_text(31, 5, "m", NULL, 0x7800, 1);
  display_text(37, 5, "a", NULL, 0xFFE0, 1);
  display_text(42, 5, "t", NULL, 0x07E0, 1);
  display_text(48, 5, "r", NULL, 0X001F, 1);
  display_text(53, 5, "i", NULL, 0x07FF, 1);
  display_text(58, 5, "x", NULL, 0x780F, 1);
  display_text(-1, 30, "P2", &RobotoMono_Thin7pt7b, 0xFFE0, 1);
  display_text(12, 25, ".", NULL, 0xFFE0, 1);
  display_text(21, 30, "6", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(29, 30, "4", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(38, 30, "x", &FreeSans9pt7b, 0x780F, 1);
  display_text(46, 30, "6", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(54, 30, "4", &FreeSerifBoldItalic9pt7b, 0x780F, 1);

  display_text(-1, 14 + 32, "R", &FreeSerif9pt7b, matrix.Color333(7, 0, 0), 1);
  display_text(8, 14 + 32, "G", &FreeSerif9pt7b, matrix.Color333(0, 7, 0), 1);
  display_text(19, 14 + 32, "B", &FreeSerif9pt7b, matrix.Color333(0, 0, 7), 1);
  display_text(31, 5 + 32, "M", NULL, 0x7800, 1);
  display_text(37, 5 + 32, "a", NULL, 0xFFE0, 1);
  display_text(42, 5 + 32, "t", NULL, 0x07E0, 1);
  display_text(48, 5 + 32, "r", NULL, 0X001F, 1);
  display_text(53, 5 + 32, "i", NULL, 0x07FF, 1);
  display_text(58, 5 + 32, "x", NULL, 0x780F, 1);
  display_text(-1, 30 + 32, "P2", &RobotoMono_Thin7pt7b, 0xFFE0, 1);
  display_text(12, 25 + 32, ".", NULL, 0xFFE0, 1);
  display_text(21, 30 + 32, "6", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(29, 30 + 32, "4", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(38, 30 + 32, "x", &FreeSans9pt7b, 0x780F, 1);
  display_text(46, 30 + 32, "6", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  display_text(54, 30 + 32, "4", &FreeSerifBoldItalic9pt7b, 0x780F, 1);
  panel_delay(2000);

  display_Image(0, 0, gImage_image, 64, 64);
  panel_delay(2000);
}

void Demo_2() {
  screen_clear();
  matrix.DrawString_CN(1, 12, "微", &Font16CN, matrix.Color333(7, 0, 0));
  matrix.DrawString_CN(1 + 16, 12, "雪", &Font16CN, matrix.Color333(6, 1, 0));
  matrix.DrawString_CN(1 + 32, 12, "电", &Font16CN, matrix.Color333(5, 2, 7));
  matrix.DrawString_CN(1 + 48, 12, "子", &Font16CN, matrix.Color333(0, 7, 0));
  matrix.DrawString_CN(1, 35, "欢", &Font16CN, matrix.Color333(0, 6, 1));
  matrix.DrawString_CN(1 + 16, 35, "迎", &Font16CN, matrix.Color333(0, 5, 2));
  matrix.DrawString_CN(1 + 32, 35, "您", &Font16CN, matrix.Color333(0, 0, 7));
  matrix.DrawString_CN(1 + 48, 35, "！", &Font16CN, matrix.Color333(1, 0, 6));
  panel_delay(2000);
  screen_clear();
  matrix.DrawString_CN(1, 0, "微", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1 + 32, 0, "雪", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1, 32, "电", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1 + 32, 32, "子", &Font32CN, matrix.Color333(0, 7, 7));
  panel_delay(2000);
  screen_clear();
  matrix.DrawString_CN(1, 0, "欢", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1 + 32, 0, "迎", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1, 32, "您", &Font32CN, matrix.Color333(0, 7, 7));
  matrix.DrawString_CN(1 + 37, 32, "！", &Font32CN, matrix.Color333(0, 7, 7));
  panel_delay(2000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "微", &Font64CN, matrix.Color333(7, 0, 0));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "雪", &Font64CN, matrix.Color333(0, 7, 0));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "电", &Font64CN, matrix.Color333(0, 0, 7));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "子", &Font64CN, matrix.Color333(7, 7, 7));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "欢", &Font64CN, matrix.Color333(7, 5, 0));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "迎", &Font64CN, matrix.Color333(0, 7, 5));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(0, 0, "您", &Font64CN, matrix.Color333(5, 0, 7));
  panel_delay(1000);
  screen_clear();
  matrix.DrawString_CN(13, 0, "！", &Font64CN, matrix.Color333(7, 7, 7));
  panel_delay(2000);
  screen_clear();
}
