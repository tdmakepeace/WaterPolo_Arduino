/*
 * HUB75 ribbon / Mega pin cable test — NO matrix PSU required
 *
 * Board: Arduino Mega 2560
 * Do NOT connect VH4 power for this test.
 * Best: unplug the ribbon from the panel and probe the IDC free end,
 *   OR leave it on the panel but leave VH4 disconnected (panel may glow
 *   faintly from parasitic power — ignore that; use a multimeter).
 *
 * How to use:
 *   1. Upload this sketch (Mega 2560).
 *   2. Open Serial Monitor @ 9600 baud — set line ending to "Newline" or "Both".
 *   3. Type a cable number 1–16 and press Enter to drive that net.
 *   4. Type 0 + Enter to set all signal pins LOW.
 *   5. Type h + Enter for the menu / help.
 *   6. Multimeter (DC V, black on Mega GND): probe the named ribbon colour.
 *      Expect ~4.5–5 V on signal nets; for GND nets use continuity to Mega GND.
 *
 * Libraries: none
 */

#include <Arduino.h>

struct Net {
  const char *name;
  const char *colour;
  uint8_t pin;  // 255 = GND (no Mega drive; verify continuity to GND)
  bool isGnd;
};

// Order matches README rainbow table (brown = pin 1)
const Net NETS[] = {
  {"R1",  "Brown",  24, false},
  {"G1",  "Red",    25, false},
  {"B1",  "Orange", 26, false},
  {"GND", "Yellow", 255, true},
  {"R2",  "Green",  27, false},
  {"G2",  "Blue",   28, false},
  {"B2",  "Violet", 29, false},
  {"GND", "Grey",   255, true},
  {"A",   "White",  A0, false},
  {"B",   "Black",  A1, false},
  {"C",   "Brown",  A2, false},  // 2nd brown (#11)
  {"D",   "Red",    A3, false},  // 2nd red (#12)
  {"CLK", "Orange", 11, false},  // 2nd orange (#13)
  {"LAT", "Yellow", 10, false},  // 2nd yellow (#14)
  {"OE",  "Green",  9,  false},  // 2nd green (#15)
  {"GND", "Blue",   255, true},  // 2nd blue (#16)
};

const uint8_t NET_COUNT = sizeof(NETS) / sizeof(NETS[0]);

int8_t activeNet = -1;  // 0..15 selected; -1 = all low
char lineBuf[16];
uint8_t lineLen = 0;

void allSignalPinsLow() {
  for (uint8_t i = 0; i < NET_COUNT; i++) {
    if (NETS[i].isGnd) continue;
    pinMode(NETS[i].pin, OUTPUT);
    digitalWrite(NETS[i].pin, LOW);
  }
}

void printPin(uint8_t pin) {
  if (pin >= A0 && pin <= A15) {
    Serial.print(F("A"));
    Serial.print(pin - A0);
  } else {
    Serial.print(F("D"));
    Serial.print(pin);
  }
}

void printMenu() {
  Serial.println();
  Serial.println(F("HUB75 cable test (no PSU)"));
  Serial.println(F("Enter 1-16 = select cable,  0 = all LOW,  h = help"));
  Serial.println(F("Meter black lead = Mega GND"));
  Serial.println(F("--------------------------------"));
  for (uint8_t i = 0; i < NET_COUNT; i++) {
    Serial.print(F("  "));
    if (i + 1 < 10) Serial.print(' ');
    Serial.print(i + 1);
    Serial.print(F("  "));
    Serial.print(NETS[i].name);
    Serial.print(F("  "));
    Serial.print(NETS[i].colour);
    if (NETS[i].isGnd) {
      Serial.println(F("  (continuity -> GND)"));
    } else {
      Serial.print(F("  -> "));
      printPin(NETS[i].pin);
      Serial.println();
    }
  }
  Serial.println(F("--------------------------------"));
  Serial.println(F("Ready. Type 1-16 + Enter:"));
}

void activateNet(int8_t index) {
  allSignalPinsLow();
  activeNet = index;

  if (index < 0) {
    Serial.println(F("All signal pins LOW."));
    return;
  }

  const Net &n = NETS[index];
  Serial.print(F("Active #"));
  Serial.print(index + 1);
  Serial.print(F("  "));
  Serial.print(n.name);
  Serial.print(F("  colour="));
  Serial.print(n.colour);

  if (n.isGnd) {
    Serial.println(F("  -> check CONTINUITY to Mega GND (ohms)"));
  } else {
    pinMode(n.pin, OUTPUT);
    digitalWrite(n.pin, HIGH);
    Serial.print(F("  Mega "));
    printPin(n.pin);
    Serial.println(F(" HIGH — probe that colour for ~5 V"));
  }
}

void handleCommand(const char *cmd) {
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  if (*cmd == '\0') return;

  char c = cmd[0];
  if (c == 'h' || c == 'H' || c == '?') {
    printMenu();
    return;
  }

  // Parse integer 0–16
  int value = 0;
  bool anyDigit = false;
  while (*cmd >= '0' && *cmd <= '9') {
    anyDigit = true;
    value = value * 10 + (*cmd - '0');
    cmd++;
    if (value > 16) break;
  }

  if (!anyDigit) {
    Serial.println(F("Unknown command. Enter 1-16, 0, or h"));
    return;
  }

  if (value == 0) {
    activateNet(-1);
    return;
  }

  if (value < 1 || value > 16) {
    Serial.println(F("Out of range. Enter 1-16"));
    return;
  }

  activateNet((int8_t)(value - 1));
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
    if (lineLen + 1 < sizeof(lineBuf)) {
      lineBuf[lineLen++] = c;
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(200);
  allSignalPinsLow();
  printMenu();
}

void loop() {
  pollSerial();
}
