/*
 * Minimal test: D2 button + Freenove I2C LCD 1602
 * Board: Arduino Mega 2560
 *
 * Wiring:
 *   Button D2 ----- push-button ----- GND  (internal pull-up)
 *   LCD GND -> GND
 *   LCD VCC -> 5V
 *   LCD SDA -> D20
 *   LCD SCL -> D21
 *
 * Library: LiquidCrystal I2C (Frank de Brabander or Freenove zip)
 *
 * Expected:
 *   LCD shows "BTN: released" then "BTN: PRESSED" while D2 is held.
 *   Press count increments on each press.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t PIN_BTN = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool i2CAddrTest(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void setupLcd() {
  Wire.begin();  // Mega: SDA=20, SCL=21
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("D2 + I2C LCD OK");
  lcd.setCursor(0, 1);
  lcd.print("BTN: released   ");
}

void setup() {
  pinMode(PIN_BTN, INPUT_PULLUP);
  setupLcd();
}

void loop() {
  static bool lastPressed = false;
  static uint16_t presses = 0;
  static uint32_t lastChangeMs = 0;

  bool pressed = (digitalRead(PIN_BTN) == LOW);  // active LOW
  uint32_t now = millis();

  // Simple debounce
  if (pressed != lastPressed && (now - lastChangeMs) > 40) {
    lastChangeMs = now;
    lastPressed = pressed;

    if (pressed) {
      presses++;
    }

    lcd.setCursor(0, 1);
    if (pressed) {
      lcd.print("BTN: PRESSED ");
    } else {
      lcd.print("BTN: released");
    }

    // Show press count on the right of line 0
    lcd.setCursor(13, 0);
    if (presses < 10) lcd.print(' ');
    if (presses < 100) lcd.print(' ');
    lcd.print(presses);
  }
}
