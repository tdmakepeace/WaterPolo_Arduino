# Water Polo Scoreboard (Arduino Mega)

Operator scoreboard for water polo: **period clock**, **home/away scores**, and a **28 s shot clock**. The shot clock is shown on the local RGB matrix and 16×2 LCD, and is broadcast to external LoRa remote shot clocks (`lora_remote2`).

---

## Parts list

| Qty | Part | Notes / SKU |
|----:|------|-------------|
| 1 | Arduino Mega 2560 (or compatible) | Required — Uno does not have enough pins |
| 1 | Waveshare RGB LED matrix 64×32 P3 | **SKU 33840** — RGB-Matrix-P3-64x32 (HUB75) |
| 1 | 5 V power supply for matrix | **≥ 2.5 A** (Waveshare rating); VH4 connector |
| 1 | HUB75 16-pin IDC ribbon | Usually included with the panel |
| 1 | VH4 2-pin power cable | Usually included with the panel |
| 1 | **Freenove I2C IIC LCD 1602** | Serial 16×2 “New Type” (I2C backpack onboard) |
| 17 | Momentary push-buttons | Score / clock / shot / TO / IN / Return / excl |
| 1 | **5 V relay module** (or 5 V coil + driver) | Driven by **D12**; switches horn / siren / lamp |
| 1 | LoRa UART module (TX side) | e.g. DX-LR02 / DX-LR32 433 MHz class |
| 1+ | LoRa remote shot-clock units | Running `lora_remote2.ino` |
| — | Dupont / jumper wires, breadboard or proto | As needed |
| — | USB cable (A–B) | Mega programming / optional 5 V for MCU only |

**Optional:** level shifter if LoRa module is strictly 3.3 V on UART. For a bare relay coil (no PCB module), use an NPN/MOSFET driver and a flyback diode across the coil — do **not** drive a coil from a Mega pin directly.

---

## What it does

| Control | Pin | Action |
|--------|-----|--------|
| Period start / stop | **D2** | Short press toggles countdown (shot clock follows) |
| Home + | **D3** | Home score +1; **stops** period clock; shot → **28** |
| Away + | **D4** | Away score +1; **stops** period clock; shot → **28** |
| Home − | **D5** | Decrease home |
| Away − | **D6** | Decrease away |
| Shot → 28 | **D7** | Shot→28; period clock **keeps running** |
| Shot → 18 | **D8** | Set shot to 18 if &lt;18; period clock **keeps running** |
| Relay (horn / alarm) | **D12** | Pulses when shot expires (~0.5 s) or period hits 0 (~1 s) |
| Period +1 s | **D36** | Short +1 s · long press **+10 s** (max 6:30) |
| Period −1 s | **D37** | Short −1 s · long press **−10 s** (min 0:00) |
| Shot +1 s | **D38** | Short +1 s · long press **+10 s** (max 28) |
| Shot −1 s | **D39** | Short −1 s · long press **−10 s** (min 0) |
| Timeout 1:00 | **D40** | Pause period + shot; run independent 1:00 timeout |
| Interval 2:00 | **D41** | Reset period→6:30 and shot→28; run independent 2:00 interval |
| Force shot 18 | **D42** | Set shot clock to 18 s always (ignores &lt;18 rule) |
| Return | **D43** | End **TO/IN** early → period+shot (**keeps shot value** after timeout) |
| Exclusion 1 | **D47** | Start 18 s exclusion; **pauses** period clock; applies **D8** if shot &lt;18 |
| Exclusion 2 | **D48** | Same as D47 for second exclusion |

- **D2** short press → start/stop immediately (play mode only)  
- Hold **D2** ~5 s (from a stopped start) → reset period to **6:30**  
- Hold **D2 + D7** together for **5 s** → **full reset** (scores 0–0, period 6:30, shot 28, clear TO/IN/exclusions, clocks stopped)  
- Shot clock counts **only** while the period clock is running in **play** mode  
- Value adjust buttons do **not** start/stop the clocks  
- Shot value is sent to LoRa remotes on every change, and **re-broadcast every 0.5 s while the clock is running**  
- **Timeout:** period and shot freeze; when TO ends (or **D43 Return**), display returns to period+shot with **same shot value**; resume with **D2**  
- **Interval:** loads a fresh period (6:30 / 28); when IN ends (or **D43 Return**), display returns to period+shot; start play with **D2**  
- **Exclusions:** start only from idle (0); cannot be restarted until they reach 0; pause during timeout/interval and when period clock is stopped  

---

## How to use

This is the operator guide for the control panel (button box + LCD). You do not need to know the wiring or pin map to run a game — use the labels on the panel and the steps below.

Panel layout reference:

![Operator control panel button layout](docs/button_box_panel.png)

### What you see

| Display | What it shows |
|---------|----------------|
| LCD top line | Scores (`H:` / `A:`) and shot clock (`SC:`) |
| LCD bottom line | Period time in play (`TIME m:ss`, with `*` when running), or `TO m:ss` / `IN m:ss` during timeout / interval; exclusions show as `X` values when active |
| RGB matrix | Large HOME / AWAY scores, period clock, shot clock, and exclusion timers |

Defaults at power-up / full reset: scores **0–0**, period **6:30**, shot **28**, clocks **stopped**.

### Start / stop the period (START/STOP)

| Action | Result |
|--------|--------|
| Short press **START/STOP** | Toggle period clock on/off (play mode only) |
| While period runs | Shot clock also counts down |
| While period is stopped | Shot clock is frozen |
| Hold **START/STOP** ~5 s (from stopped) | Reset period to **6:30** (does not start the clock) |

**START/STOP** does nothing during a timeout or interval — use **RETURN** first (or wait for the timer to finish).

### Scoring (HOME / AWAY)

| Button | Result |
|--------|--------|
| **+ HOME** / **+ AWAY** | Score +1 (max 99); **stops** the period clock; shot → **28** |
| **− HOME** / **− AWAY** | Score −1 (min 0); clocks and shot are unchanged |

After a goal, press **START/STOP** when play resumes.

### Shot clock

| Button | Result |
|--------|--------|
| **28s RESET** | Shot → **28**; period clock **keeps running** if it was running |
| **18s RESET** | Shot → **18** only if current value is **&lt; 18**; period keeps running |
| **FORCE 18** | Shot → **18** always (ignores the &lt; 18 rule) |
| Shot reaches **0** | Horn/relay (~0.5 s); remote buzzers; **both clocks stop**; shot reloads to **28** |

Shot value is also shown on LoRa remote shot clocks and updated while the clock is running.

### Timeouts and intervals

| Button | Result |
|--------|--------|
| **TIMEOUT 1:00** | Pause period + shot; run a separate **1:00** countdown (`TO` on displays) |
| **INTERVAL 2:00** | Load a fresh period (**6:30**) and shot (**28**); run a separate **2:00** countdown (`IN` on displays) |
| **RETURN** | End TO/IN early and go back to the period + shot display |
| TO or IN reaches **0** | Short horn/relay; display returns to period + shot automatically |

After timeout: shot value is **kept**; press **START/STOP** to resume.  
After interval: you have a new **6:30 / 28** ready; press **START/STOP** to start play.

### Exclusions (EXCL 1 / EXCL 2)

| Button | Result |
|--------|--------|
| **EXCL 1** or **EXCL 2** | Start an **18 s** exclusion timer; **pauses** the period clock; applies **18s RESET** rule if shot &lt; 18 |

Notes:

- Each exclusion starts only when that slot is **0** (idle). You cannot restart it until it counts down to 0.
- Exclusion timers tick only while the period clock is running; they pause during timeout/interval and when the period is stopped.
- Press **START/STOP** to resume play (and continue the exclusion countdown).

### Fine adjust (GAME CLOCK / SHOT CLOCK ±1s)

Use these to correct time without starting or stopping play yourself (they do not toggle the clocks):

| Button | Short press | Long press (~0.8 s) |
|--------|-------------|---------------------|
| Game **+1s** / **−1s** | Period ±1 s | Period ±10 s |
| Shot **+1s** / **−1s** | Shot ±1 s | Shot ±10 s |

Limits: period **0:00–6:30**, shot **0–28**. Period adjust only works in play mode (not during TO/IN). If you adjust period to **0:00**, the clock stops.

### Full game reset

Hold **START/STOP** and **28s RESET** together for **~5 s**:

- Scores → **0–0**
- Period → **6:30**, shot → **28**
- Clears timeout / interval / exclusions
- Clocks stopped

### Typical period workflow

1. Confirm scores and clocks (or do a full reset).
2. Press **START/STOP** to begin the period.
3. On a shot-clock reset foul / new possession → **28s RESET** (or **18s RESET** / **FORCE 18** as required).
4. On a goal → **+ HOME** or **+ AWAY** (clock stops, shot → 28); resume with **START/STOP**.
5. On exclusion → **EXCL 1** or **EXCL 2**, then **START/STOP** when play resumes.
6. Team timeout → **TIMEOUT 1:00**; when done (or **RETURN**), **START/STOP** to resume.
7. Between periods → **INTERVAL 2:00**; when done, **START/STOP** for the next period.
8. If the period hits **0:00**, the horn sounds (~1 s) and remotes get `END` — use **INTERVAL** or reset the period for the next one.

### Button quick reference

| Panel label | Function |
|-------------|----------|
| **START/STOP** | Start/stop period (and shot) in play |
| **+ HOME** / **− HOME** | Home score ±1 (+ also stops clock, shot → 28) |
| **+ AWAY** / **− AWAY** | Away score ±1 (+ also stops clock, shot → 28) |
| **28s RESET** | Shot → 28 |
| **18s RESET** | Shot → 18 if &lt; 18 |
| **FORCE 18** | Shot → 18 always |
| **TIMEOUT 1:00** | 1-minute timeout |
| **INTERVAL 2:00** | 2-minute interval (fresh 6:30 / 28) |
| **RETURN** | Leave timeout / interval early |
| **EXCL 1** / **EXCL 2** | 18 s exclusion timers |
| Game / Shot **±1s** | Nudge clocks (hold for ±10 s) |

---

## Libraries

Arduino IDE → **Tools → Manage Libraries**:

1. **Adafruit GFX Library**
2. **RGB matrix Panel** (Adafruit)
3. **LiquidCrystal I2C** (e.g. by Frank de Brabander), or Freenove’s `LiquidCrystal_I2C.zip` from their kit docs

**Board:** Tools → Board → **Arduino Mega or Mega 2560**

I2C address: sketch tries **0x27** (PCF8574T), then **0x3F** (PCF8574AT) automatically.

---

## Full Arduino Mega pin map

Status legend: **USED** = wired by this project · **RESERVED** = claimed by the matrix library (leave free) · **SPARE** = available for expansion · **AVOID** = prefer not to use

### Digital pins D0–D53

| Pin | Status | Connection / notes |
|----:|--------|--------------------|
| D0 | AVOID | Serial RX0 (USB serial) — leave free for programming/debug |
| D1 | AVOID | Serial TX0 (USB serial) — leave free for programming/debug |
| D2 | USED | Button — period clock start / stop |
| D3 | USED | Button — home score + |
| D4 | USED | Button — away score + |
| D5 | USED | Button — home score − |
| D6 | USED | Button — away score − |
| D7 | USED | Button — shot clock → 28 s |
| D8 | USED | Button — shot clock → 18 s (if &lt; 18) |
| D9 | USED | Matrix HUB75 **OE** |
| D10 | USED | Matrix HUB75 **LAT / STB** |
| D11 | USED | Matrix HUB75 **CLK** (must be on PORTB) |
| D12 | USED | **5 V relay** IN (default HIGH = ON; see `RELAY_ACTIVE_HIGH`) |
| D13 | SPARE | Also onboard LED — usable if LED flash is acceptable |
| D14 | SPARE | Serial3 TX — free unless you need Serial3 |
| D15 | SPARE | Serial3 RX — free unless you need Serial3 |
| D16 | SPARE | Serial2 TX — free unless you need Serial2 |
| D17 | SPARE | Serial2 RX — free unless you need Serial2 |
| D18 | USED | LoRa module **RX** ← Mega **TX1** (Serial1) |
| D19 | USED | LoRa module **TX** → Mega **RX1** (Serial1) |
| D20 | USED | Freenove LCD **SDA** (I2C) |
| D21 | USED | Freenove LCD **SCL** (I2C) |
| D22 | RESERVED | Matrix library (PORTA) — **do not use** |
| D23 | RESERVED | Matrix library (PORTA) — **do not use** |
| D24 | USED | Matrix HUB75 **R1** |
| D25 | USED | Matrix HUB75 **G1** |
| D26 | USED | Matrix HUB75 **B1** |
| D27 | USED | Matrix HUB75 **R2** |
| D28 | USED | Matrix HUB75 **G2** |
| D29 | USED | Matrix HUB75 **B2** |
| D30 | SPARE | Was parallel LCD RS — free with I2C LCD |
| D31 | SPARE | |
| D32 | SPARE | |
| D33 | SPARE | |
| D34 | SPARE | |
| D35 | SPARE | |
| D36 | USED | Button — period clock **+1 s** |
| D37 | USED | Button — period clock **−1 s** |
| D38 | USED | Button — shot clock **+1 s** |
| D39 | USED | Button — shot clock **−1 s** |
| D40 | USED | Button — **timeout 1:00** |
| D41 | USED | Button — **interval 2:00** |
| D42 | USED | Button — **force shot → 18** |
| D43 | USED | Button — **RETURN** (end TO/IN) |
| D44 | SPARE | PWM-capable |
| D45 | SPARE | PWM-capable |
| D46 | SPARE | PWM-capable |
| D47 | USED | Button — **exclusion 1** (18 s, no reset) |
| D48 | USED | Button — **exclusion 2** (18 s, no reset) |
| D49 | SPARE | |
| D50 | SPARE | SPI MISO — free unless using SPI |
| D51 | SPARE | SPI MOSI — free unless using SPI |
| D52 | SPARE | SPI SCK — free unless using SPI |
| D53 | SPARE | SPI SS — free unless using SPI |

### Analog pins A0–A15 (also usable as digital)

| Pin | Status | Connection / notes |
|----:|--------|--------------------|
| A0 | USED | Matrix HUB75 address **A** |
| A1 | USED | Matrix HUB75 address **B** |
| A2 | USED | Matrix HUB75 address **C** |
| A3 | USED | Matrix HUB75 address **D** |
| A4 | SPARE | Matrix **E** not used on this 64×32 panel |
| A5 | SPARE | |
| A6 | SPARE | |
| A7 | SPARE | |
| A8 | SPARE | |
| A9 | SPARE | |
| A10 | SPARE | |
| A11 | SPARE | |
| A12 | SPARE | |
| A13 | SPARE | |
| A14 | SPARE | |
| A15 | SPARE | |

### Spare pins summary (quick list)

**Digital:** D13, D14, D15, D16, D17, D30, D31, D32, D33, D34, D35, D44, D45, D46, D49, D50, D51, D52, D53  

**Analog / digital:** A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15  

**Do not use:** D0, D1 (USB serial), D22, D23 (matrix library)

---

## Wiring diagram

### Operator control panel

Suggested faceplate layout for the operator box (buttons + I2C LCD):

![Operator control panel button layout](docs/button_box_panel.png)

| Position | Control | Pin |
|----------|---------|-----|
| Top left | Home + / − | **D3** / **D5** |
| Top right | Away + / − | **D4** / **D6** |
| Mid left of LCD | Timeout 1:00 | **D40** |
| Center | I2C LCD 16×2 | **SDA D20** / **SCL D21** |
| Mid right of LCD | Interval 2:00 · **Return** | **D41** · **D43** |
| Under LCD left | Period +1 s / −1 s | **D36** / **D37** |
| Under LCD right | Shot +1 s / −1 s | **D38** / **D39** |
| Bottom row L→R | Shot → 28 · Shot → 18 · Excl 1 · Excl 2 · Force 18 · Start/Stop | **D7** · **D8** · **D47** · **D48** · **D42** · **D2** |

### RGB matrix → Mega (HUB75)

![RGB Matrix HUB75 wiring to Arduino Mega](docs/rgb_matrix_mega_wiring.png)

| HUB75 | Mega | HUB75 | Mega |
|-------|------|-------|------|
| R1 | **D24** | G1 | **D25** |
| B1 | **D26** | R2 | **D27** |
| G2 | **D28** | B2 | **D29** |
| A | **A0** | B | **A1** |
| C | **A2** | D | **A3** |
| OE | **D9** | LAT / STB | **D10** |
| CLK | **D11** | GND | **GND** |
| E | *not connected* | VH4 VCC/GND | **external 5 V ≥2.5 A PSU** (not Mega 5 V) |

Data lines **D24–D29** are fixed by the Adafruit matrix library (`PORTA`). Leave **D22** and **D23** free.

### Full system overview

```text
                         +---------------------------+
                         |     5 V / ≥2.5 A PSU      |
                         |  (+) -------------- (-)   |
                         +-------|------|------+-----+
                                 |      |
                                 |      +---- common GND ----+
                                 |                           |
                                 v                           v
                    +------------------------+      +------------------+
                    | Waveshare 64x32 Matrix |      |  Arduino Mega    |
                    | VH4: VCC / GND         |      |  2560            |
                    | HUB75 INPUT (IDC)      |      |                  |
                    +-----------+------------+      |  USB (PC)        |
                                |                   |                  |
           HUB75 ribbon                                         |                   |  Buttons         |
                                |                   |  Relay D12       |
                                |                   |  I2C LCD 20/21   |
                                |                   |  Serial1 18/19   |
                                v                   +--------+---------+
                    R1->24  G1->25  B1->26                   |
                    R2->27  G2->28  B2->29                   |
                    A->A0   B->A1   C->A2   D->A3            |
                    OE->9   LAT->10 CLK->11  GND->GND         |
                    E = leave open                            |
                                                             |
              +------------------+     +---------------------+--+
              | Freenove I2C     |     | LoRa TX module       |
              | LCD 1602         |     | RX  <- Mega 18 (TX1) |
              | GND -> GND       |     | TX  -> Mega 19 (RX1) |
              | VCC -> 5V        |     | GND <-> Mega GND     |
              | SDA -> 20        |     | VCC = 3V3/5V (datasheet) |
              | SCL -> 21        |     +----------+-----------+
              +------------------+                |
                                                  |  RF @ 433 MHz
                                                  v
                                               LoRa remotes
                                               (lora_remote2)

  Buttons (each: Mega pin ----- button ----- GND)
    D2  period start/stop
    D3  home+     D5  home-
    D4  away+     D6  away-
    D7  shot → 28     D8  shot → 18 (if < 18)
    D36 period +1s    D37 period -1s
    D38 shot +1s      D39 shot -1s
    D40 timeout 1:00  D41 interval 2:00  D43 RETURN
    D42 force shot 18
    D47 exclusion 1   D48 exclusion 2

  Relay (5 V module — preferred):
    Mega 5V  ----- VCC (JD-VCC jumper on if module supports it)
    Mega GND ----- GND
    Mega D12 ----- IN   (signal)
    COM/NO/NC ---- wire to your horn / siren / lamp supply (isolated from Mega)

  Bare 5 V relay coil (not recommended without a driver):
    Use a transistor or MOSFET + flyback diode across the coil.
    Mega pin cannot source coil current safely.

  If the module is "active LOW" (LED on when IN is grounded), set in the sketch:
    const bool RELAY_ACTIVE_HIGH = false;
```

### Power rules

1. Power the **matrix from the external 5 V supply** on the **VH4** connector — never from the Mega 5 V pin.
2. Tie **PSU GND**, **Mega GND**, **matrix GND**, **LCD GND**, **LoRa GND**, and **button commons** together.
3. Mega may be powered by USB during development; for stand-alone use a separate Mega supply (USB power bank / VIN) is fine.

---

## Build & upload instructions

1. **Assemble power first** — connect the matrix VH4 cable to the 5 V / ≥2.5 A supply and common GND **before** plugging in the HUB75 ribbon for long sessions.
2. **Wire HUB75** exactly as in the pin map (data lines **must** be 24–29).
3. **Wire buttons** D2–D8, D36–D43, D47–D48 to GND (internal pull-ups; pressed = LOW).
4. **Wire Freenove I2C LCD** — **GND→GND**, **VCC→5V**, **SDA→D20**, **SCL→D21**. Contrast/backlight are onboard (no pot needed).
5. **Wire the 5 V relay** to **D12** (see relay wiring above). Set `RELAY_ACTIVE_HIGH` to match your module.
6. **Wire LoRa** module to Serial1 (18/19) and set `LORA_DEFAULT_CHANNEL` in the sketch to match remotes (default **2** = 433.150 MHz).
7. Install the two Adafruit libraries listed above.
8. Open `waterpolo_scoreboard/waterpolo_scoreboard.ino`.
9. Tools → Board → **Arduino Mega or Mega 2560**, select the correct COM port.
10. Upload, then power the matrix supply and reboot/reset the Mega.

### First-run checks

| Check | Expected |
|-------|----------|
| LCD | `H:00 A:00 SC:28` / `TIME 6:30` |
| Matrix | HOME/AWAY 00, `6:30` and `28` |
| D2 short press | Clocks run (`*` on LCD); shot counts down with period |
| D7 | Shot → 28 on LCD, matrix, and remotes |
| D8 with shot &lt; 18 | Shot → 18; if ≥ 18, unchanged |
| D36 / D37 | Period time ±1 s |
| D38 / D39 | Shot time ±1 s (updates remotes) |
| Clock running | Remotes update ~every **0.5 s** |
| Shot → 0 | LoRa `0` + `BUZZER`; local relay ~0.5 s; **clocks stop**; shot resets to **28** |
| Period → 0 | Relay ~1 s; LoRa `END` |

If RGB colours look wrong (G/B swapped on some panels), swap the physical **G1↔B1** and **G2↔B2** wires.

---

## Displays

### LCD (Freenove I2C 1602)

Play:
```text
H:03 A:02 SC:28
5:42* X18/12
```

Timeout / interval:
```text
H:03 A:02 SC:28
TO 0:45
```
```text
H:03 A:02 SC:28
IN 1:30
```

### RGB matrix (64×32)

Play:
```text
HOME           AWAY
 03             02
5:42  e1 e2    28
```

Timeout / interval replace the bottom time with `TO m:ss` or `IN m:ss`.

- Home green, Away orange  
- Period white when running, dim when paused, red flash at 0:00  
- Shot yellow when running, red at ≤5 s  
- Exclusions magenta (only while &gt; 0)  

---

## LoRa protocol (remote shot clocks)

Compatible with `WaterPoloScoreBoard/BLE_small_sample/lora_remote2` and `scoreboard_commands.h`.

| Item | Value |
|------|--------|
| Link | Mega **Serial1** @ **9600** baud (`LORA_BAUD`) |
| Framing | Newline-terminated text |
| Shot value | Decimal string `28`, `18`, `0`, … |
| Shot expired | `0` then `BUZZER`, then master stops clocks and reloads shot to `28` |
| Period expired | `END` |
| While running | Re-send current shot value every **0.5 s** |
| Channel table | CH0–3 → 433.050 / .100 / .150 / .200 MHz |

Remotes must use the **same** `LORA_DEFAULT_CHANNEL` as this master.

---

## Freenove I2C LCD notes

| LCD pin | Mega |
|---------|------|
| GND | GND |
| VCC | 5 V |
| SDA | **D20** |
| SCL | **D21** |

- Library: **LiquidCrystal_I2C**
- Address auto-select: `0x27` then `0x3F`
- Pins **D30–D35** are free (no longer used for parallel LCD)

---

## Quick hardware test (D2 + LCD only)

Sketch folder: `test_d2_lcd/`

1. Wire Freenove LCD (**GND/VCC/SDA/SCL** → **GND/5V/D20/D21**)
2. Wire a button between **D2** and **GND**
3. Open `test_d2_lcd/test_d2_lcd.ino`, select Mega 2560, upload
4. LCD should show `D2 + I2C LCD OK` and toggle `BTN: PRESSED` / `BTN: released` when you press D2
