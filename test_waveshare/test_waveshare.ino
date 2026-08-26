/*
  Waveshare RGB Matrix P5 64x32
  Arduino Mega 2560
  Official RGBmatrixPanel-based diagnostic

  IMPORTANT:
  Use the RGB wiring exactly as below:

    R1  D24
    G1  D25
    B1  D26

    R2  D27
    G2  D28
    B2  D29

    CLK D11
    OE  D10
    LAT D9

    A   A0
    B   A1
    C   A2
    D   A3

  Panel powered from external 5V supply.
  Mega GND connected to panel PSU GND.

  Serial commands at 115200:

    r = red
    g = green
    b = blue
    w = white
    0 = black/off

    c = column walk
    y = row walk

    q = 16-column sections
    t = test screen

    h = help
*/

#include <Arduino.h>
#include "RGBmatrixPanel.h"
#include "Adafruit_GFX.h"

// --------------------------------------------------
// PANEL PINS - MATCH WAVESHARE EXAMPLE
// --------------------------------------------------

#define CLK 11
#define OE  10
#define LAT 9

#define A A0
#define B A1
#define C A2
#define D A3

RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

// --------------------------------------------------
// TEST STATE
// --------------------------------------------------

enum Pattern
{
  PAT_OFF,
  PAT_RED,
  PAT_GREEN,
  PAT_BLUE,
  PAT_WHITE,
  PAT_COLUMN,
  PAT_ROW,
  PAT_SECTION,
  PAT_TEST
};

Pattern pattern = PAT_RED;

uint8_t column = 0;
uint8_t row = 0;
uint8_t section = 0;

unsigned long lastUpdate = 0;


// --------------------------------------------------
// WAVESHARE PANEL INITIALISATION
// IMPORTANT - DO NOT REMOVE
// --------------------------------------------------

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

  // Control register 12
  int C12[16] =
  {
    0, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1
  };

  // Control register 13
  int C13[16] =
  {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 0, 0
  };

  // ---------------------------------------------
  // Send register 12
  // ---------------------------------------------

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

  // ---------------------------------------------
  // Send register 13
  // ---------------------------------------------

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


// --------------------------------------------------
// CLEAR SCREEN
// --------------------------------------------------

void clearScreen()
{
  matrix.fillScreen(matrix.Color333(0, 0, 0));
}


// --------------------------------------------------
// SOLID COLOURS
// --------------------------------------------------

void showSolid()
{
  switch (pattern)
  {
    case PAT_OFF:
      matrix.fillScreen(matrix.Color333(0, 0, 0));
      break;

    case PAT_RED:
      matrix.fillScreen(matrix.Color333(7, 0, 0));
      break;

    case PAT_GREEN:
      matrix.fillScreen(matrix.Color333(0, 7, 0));
      break;

    case PAT_BLUE:
      matrix.fillScreen(matrix.Color333(0, 0, 7));
      break;

    case PAT_WHITE:
      matrix.fillScreen(matrix.Color333(7, 7, 7));
      break;

    default:
      break;
  }
}


// --------------------------------------------------
// COLUMN WALK
// ONE RED VERTICAL LINE
// --------------------------------------------------

void showColumn()
{
  matrix.fillScreen(matrix.Color333(0, 0, 0));

  matrix.drawFastVLine(
    column,
    0,
    32,
    matrix.Color333(7, 0, 0)
  );
}


// --------------------------------------------------
// ROW WALK
// ONE RED HORIZONTAL LINE
// --------------------------------------------------

void showRow()
{
  matrix.fillScreen(matrix.Color333(0, 0, 0));

  matrix.drawFastHLine(
    0,
    row,
    64,
    matrix.Color333(7, 0, 0)
  );
}


// --------------------------------------------------
// 16-COLUMN SECTION TEST
// --------------------------------------------------

void showSection()
{
  matrix.fillScreen(matrix.Color333(0, 0, 0));

  int x = section * 16;

  matrix.fillRect(
    x,
    0,
    16,
    32,
    matrix.Color333(0, 7, 0)
  );
}


// --------------------------------------------------
// SIMPLE SCREEN TEST
// --------------------------------------------------

void showTest()
{
  matrix.fillScreen(matrix.Color333(0, 0, 0));

  // Vertical centre
  matrix.drawFastVLine(
    32,
    0,
    32,
    matrix.Color333(7, 0, 0)
  );

  // Horizontal centre
  matrix.drawFastHLine(
    0,
    16,
    64,
    matrix.Color333(0, 7, 0)
  );

  // Four corners
  matrix.drawPixel(
    0,
    0,
    matrix.Color333(0, 0, 7)
  );

  matrix.drawPixel(
    63,
    0,
    matrix.Color333(0, 0, 7)
  );

  matrix.drawPixel(
    0,
    31,
    matrix.Color333(0, 0, 7)
  );

  matrix.drawPixel(
    63,
    31,
    matrix.Color333(0, 0, 7)
  );
}


// --------------------------------------------------
// HELP
// --------------------------------------------------

void help()
{
  Serial.println();
  Serial.println(F("Waveshare P5 64x32 Mega test"));
  Serial.println();
  Serial.println(F("r = RED"));
  Serial.println(F("g = GREEN"));
  Serial.println(F("b = BLUE"));
  Serial.println(F("w = WHITE"));
  Serial.println(F("0 = OFF"));
  Serial.println(F("c = COLUMN WALK"));
  Serial.println(F("y = ROW WALK"));
  Serial.println(F("q = 16 COLUMN SECTIONS"));
  Serial.println(F("t = SCREEN TEST"));
  Serial.println(F("h = HELP"));
  Serial.println();
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup()
{
  Serial.begin(115200);

  // IMPORTANT:
  // Waveshare driver initialisation BEFORE matrix.begin()
  Reginit();

  delay(100);

  matrix.begin();

  delay(500);

  clearScreen();

  help();

  Serial.println(F("Ready."));
}


// --------------------------------------------------
// LOOP
// --------------------------------------------------

void loop()
{
  // -----------------------------------------------
  // SERIAL COMMANDS
  // -----------------------------------------------

  while (Serial.available())
  {
    char ch = Serial.read();

    if (ch == '\r' || ch == '\n')
      continue;

    switch (ch)
    {
      case 'r':
      case 'R':
        pattern = PAT_RED;
        Serial.println(F("RED"));
        showSolid();
        break;

      case 'g':
      case 'G':
        pattern = PAT_GREEN;
        Serial.println(F("GREEN"));
        showSolid();
        break;

      case 'b':
      case 'B':
        pattern = PAT_BLUE;
        Serial.println(F("BLUE"));
        showSolid();
        break;

      case 'w':
      case 'W':
        pattern = PAT_WHITE;
        Serial.println(F("WHITE"));
        showSolid();
        break;

      case '0':
        pattern = PAT_OFF;
        Serial.println(F("OFF"));
        showSolid();
        break;

      case 'c':
      case 'C':
        pattern = PAT_COLUMN;
        column = 0;
        lastUpdate = millis();

        Serial.println(F("COLUMN WALK"));
        showColumn();
        break;

      case 'y':
      case 'Y':
        pattern = PAT_ROW;
        row = 0;
        lastUpdate = millis();

        Serial.println(F("ROW WALK"));
        showRow();
        break;

      case 'q':
      case 'Q':
        pattern = PAT_SECTION;
        section = 0;
        lastUpdate = millis();

        Serial.println(F("16-COLUMN SECTION WALK"));
        showSection();
        break;

      case 't':
      case 'T':
        pattern = PAT_TEST;
        Serial.println(F("SCREEN TEST"));
        showTest();
        break;

      case 'h':
      case 'H':
        help();
        break;
    }
  }


  // -----------------------------------------------
  // ANIMATION
  // -----------------------------------------------

  if (millis() - lastUpdate >= 500)
  {
    lastUpdate = millis();

    if (pattern == PAT_COLUMN)
    {
      showColumn();

      Serial.print(F("Column "));
      Serial.println(column);

      column++;

      if (column >= 64)
        column = 0;
    }

    else if (pattern == PAT_ROW)
    {
      showRow();

      Serial.print(F("Row "));
      Serial.println(row);

      row++;

      if (row >= 32)
        row = 0;
    }

    else if (pattern == PAT_SECTION)
    {
      showSection();

      Serial.print(F("Section "));
      Serial.println(section);

      section++;

      if (section >= 4)
        section = 0;
    }
  }
}
