/*
 * Waveshare RGB-Matrix P5 64x32 - Arduino Mega 2560
 * Single-column diagnostic test
 *
 * PURPOSE
 * -------
 * This sketch tests column addressing on the Waveshare 64x32 P5 HUB75 panel.
 *
 * Based on the observed behaviour, a 10-column circular offset is applied:
 *     physical_column = requested_column - 10 (mod 64)
 *
 * The sketch displays a single RED vertical line and walks it from column 0
 * through column 63, once per second.
 *
 * WIRING (same as your current test setup)
 * -----------------------------------------
 * R1  -> D24
 * G1  -> D25
 * B1  -> D26
 * R2  -> D27
 * G2  -> D28
 * B2  -> D29
 * A   -> A0
 * B   -> A1
 * C   -> A2
 * D   -> A3
 * E   -> A4  (held LOW)
 * CLK -> D11
 * LAT -> D9
 * OE  -> D10
 *
 * POWER
 * -----
 * Panel: external regulated 5 V supply
 * Mega GND and panel PSU GND MUST be connected together.
 *
 * SERIAL MONITOR
 * --------------
 * 9600 baud
 *
 * Commands:
 *   c = start column walk
 *   s = stop
 *   r = reset to column 0
 *   m = show current offset
 *   + = increase offset by 1
 *   - = decrease offset by 1
 *   h = help
 *
 * IMPORTANT
 * ---------
 * The 10-column offset is currently a test value based on your measured
 * positions. If the line lands correctly at columns 0,1,2... then we have
 * confirmed the offset and can move on to your actual scoreboard renderer.
 */

#include <Arduino.h>

// -----------------------------------------------------------------------------
// HUB75 data pins
// -----------------------------------------------------------------------------
const uint8_t PIN_R1 = 24;
const uint8_t PIN_G1 = 25;
const uint8_t PIN_B1 = 26;

const uint8_t PIN_R2 = 27;
const uint8_t PIN_G2 = 28;
const uint8_t PIN_B2 = 29;

// HUB75 row address pins
const uint8_t PIN_A = A0;
const uint8_t PIN_B = A1;
const uint8_t PIN_C = A2;
const uint8_t PIN_D = A3;
const uint8_t PIN_E = A4;

// HUB75 control pins
const uint8_t PIN_CLK = 11;
const uint8_t PIN_LAT = 9;
const uint8_t PIN_OE  = 10;

// Most HUB75 panels use active-HIGH blanking on OE.
const bool OE_ACTIVE_HIGH_BLANK = true;

// -----------------------------------------------------------------------------
// Column compensation
// -----------------------------------------------------------------------------
// Measured from your panel:
//   Requested 5  -> physical ~59
//   Requested 21 -> physical ~11
//   Requested 37 -> physical ~27
//
// Therefore physical ~= requested - 10 (mod 64).
//
// A positive SOFTWARE offset here means we compensate by shifting the requested
// column forward by this amount in the serial data stream.
//
// Start with +10 because that is the value indicated by your measurements.
int8_t columnOffset = 10;

// -----------------------------------------------------------------------------
// Walk state
// -----------------------------------------------------------------------------
bool running = false;
uint8_t requestedColumn = 0;
uint32_t lastMoveMs = 0;
const uint16_t STEP_TIME_MS = 1000;

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------
uint8_t wrapColumn(int16_t value)
{
  while (value < 0)   value += 64;
  while (value >= 64) value -= 64;
  return (uint8_t)value;
}

// Convert requested/display column to the column whose bit must be shifted.
uint8_t compensatedColumn(uint8_t requested)
{
  // The measured behaviour was approximately:
  //     requested 21 -> physical 11
  // So to make physical 21 light up, we need to shift the data for column 31.
  return wrapColumn((int16_t)requested + columnOffset);
}

// -----------------------------------------------------------------------------
// Set 1/16-scan row address
// -----------------------------------------------------------------------------
void setAddress(uint8_t row)
{
  digitalWrite(PIN_A, (row >> 0) & 0x01);
  digitalWrite(PIN_B, (row >> 1) & 0x01);
  digitalWrite(PIN_C, (row >> 2) & 0x01);
  digitalWrite(PIN_D, (row >> 3) & 0x01);

  // Your P5 panel is 1/16 scan; E is not used for row selection here.
  digitalWrite(PIN_E, LOW);
}

// -----------------------------------------------------------------------------
// OE control
// -----------------------------------------------------------------------------
void blankDisplay()
{
  digitalWrite(PIN_OE, OE_ACTIVE_HIGH_BLANK ? HIGH : LOW);
}

void showDisplay()
{
  digitalWrite(PIN_OE, OE_ACTIVE_HIGH_BLANK ? LOW : HIGH);
}

// -----------------------------------------------------------------------------
// Clock one HUB75 pixel
// -----------------------------------------------------------------------------
inline void shiftPixel(bool redOn)
{
  // Top half
  digitalWrite(PIN_R1, redOn ? HIGH : LOW);
  digitalWrite(PIN_G1, LOW);
  digitalWrite(PIN_B1, LOW);

  // Bottom half
  digitalWrite(PIN_R2, redOn ? HIGH : LOW);
  digitalWrite(PIN_G2, LOW);
  digitalWrite(PIN_B2, LOW);

  digitalWrite(PIN_CLK, HIGH);
  digitalWrite(PIN_CLK, LOW);
}

// -----------------------------------------------------------------------------
// Shift one complete 64-column row.
// Only the compensated column is red.
// -----------------------------------------------------------------------------
void shiftSingleColumn(uint8_t requestedColumn)
{
  const uint8_t shiftColumn = compensatedColumn(requestedColumn);

  for (uint8_t x = 0; x < 64; ++x)
  {
    shiftPixel(x == shiftColumn);
  }
}

// -----------------------------------------------------------------------------
// Latch the 64 shifted pixels
// -----------------------------------------------------------------------------
void pulseLatch()
{
  digitalWrite(PIN_LAT, HIGH);
  digitalWrite(PIN_LAT, LOW);
}

// -----------------------------------------------------------------------------
// Refresh one complete 32-row display.
//
// For each of the 16 scan addresses:
//   1. blank
//   2. select row address
//   3. shift exactly 64 pixel clocks
//   4. latch
//   5. show
//
// This gives a full-height vertical line.
// -----------------------------------------------------------------------------
void refreshDisplay(uint8_t requestedColumn)
{
  for (uint8_t row = 0; row < 16; ++row)
  {
    blankDisplay();

    // Select the scan address before latching the new row data.
    setAddress(row);

    // Shift exactly 64 columns.
    shiftSingleColumn(requestedColumn);

    // Transfer shift register contents to the panel output latches.
    pulseLatch();

    // Display this scan row briefly.
    showDisplay();
    delayMicroseconds(1000);
  }

  blankDisplay();
}

// -----------------------------------------------------------------------------
// Serial help
// -----------------------------------------------------------------------------
void printHelp()
{
  Serial.println();
  Serial.println(F("Waveshare P5 64x32 - Mega column diagnostic"));
  Serial.println(F("---------------------------------------------"));
  Serial.println(F("c = start column walk"));
  Serial.println(F("s = stop"));
  Serial.println(F("r = reset requested column to 0"));
  Serial.println(F("m = show current offset"));
  Serial.println(F("+ = offset +1"));
  Serial.println(F("- = offset -1"));
  Serial.println(F("h = help"));
  Serial.print(F("Current offset = "));
  Serial.println(columnOffset);
  Serial.println();
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(9600);

  const uint8_t outputs[] = {
    PIN_R1, PIN_G1, PIN_B1,
    PIN_R2, PIN_G2, PIN_B2,
    PIN_A, PIN_B, PIN_C, PIN_D, PIN_E,
    PIN_CLK, PIN_LAT, PIN_OE
  };

  for (uint8_t i = 0; i < sizeof(outputs); ++i)
  {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);
  }

  // Start blank.
  blankDisplay();

  // Ensure control lines have known idle states.
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_LAT, LOW);

  // Initial row address.
  setAddress(0);

  delay(250);
  printHelp();

  Serial.println(F("Test begins at requested column 0."));
  Serial.println(F("Send 'c' to start."));
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
void loop()
{
  // Serial commands
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();

    if (c == '\r' || c == '\n')
      continue;

    switch (c)
    {
      case 'c':
      case 'C':
        running = true;
        requestedColumn = 0;
        lastMoveMs = millis();
        Serial.println(F("COLUMN WALK START"));
        break;

      case 's':
      case 'S':
        running = false;
        blankDisplay();
        Serial.println(F("STOP"));
        break;

      case 'r':
      case 'R':
        requestedColumn = 0;
        Serial.println(F("Requested column reset to 0"));
        break;

      case 'm':
      case 'M':
        Serial.print(F("Offset = "));
        Serial.println(columnOffset);
        Serial.print(F("Requested column = "));
        Serial.println(requestedColumn);
        Serial.print(F("Shifted column = "));
        Serial.println(compensatedColumn(requestedColumn));
        break;

      case '+':
        if (columnOffset < 63)
          ++columnOffset;
        Serial.print(F("Offset = "));
        Serial.println(columnOffset);
        break;

      case '-':
        if (columnOffset > -63)
          --columnOffset;
        Serial.print(F("Offset = "));
        Serial.println(columnOffset);
        break;

      case 'h':
      case 'H':
        printHelp();
        break;

      default:
        break;
    }
  }

  // Advance to the next requested column once per second.
  if (running && (millis() - lastMoveMs >= STEP_TIME_MS))
  {
    lastMoveMs = millis();

    Serial.print(F("Requested column "));
    Serial.print(requestedColumn);
    Serial.print(F(" -> shifted column "));
    Serial.println(compensatedColumn(requestedColumn));

    requestedColumn = (requestedColumn + 1) & 63;
  }

  // Continuously refresh current column.
  refreshDisplay(requestedColumn);
}
