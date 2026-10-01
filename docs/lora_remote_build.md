# LoRa remote shot clock — wiring

One remote shows the shot clock sent by the Mega scoreboard. The sketch is `lora_remote/lora_remote.ino` (pins in `lora_remote/scoreboard_commands.h`). This guide is the wiring only: no case, no PCB.

The master and this remote must use the same LoRa settings. Both sketches default to **channel 2 = 433.150 MHz**, spreading factor **7**, power **22 dBm**.

---

## Shopping list (one remote)

Prices change. Links are the Amazon UK pages for these parts.

| Qty | Part | Use | Link |
| --- | --- | --- | --- |
| 1 | Arduino Nano Every **with headers** (ABX00033) | Controller | [B07WWK29XF](https://www.amazon.co.uk/dp/B07WWK29XF) |
| 2 | PATIKIL 4 inch common-anode digit, red, 7.2 V, 10 pin (FJS40101DH) | Tens digit and ones digit. The listing is **one** digit | [B0C5MSYB28](https://www.amazon.co.uk/dp/B0C5MSYB28) |
| 1 | DX-LR32 UART LoRa module + antenna, from the 2-pack | Radio. The second module in the pack can go on the Mega master | [B0GV3KN1HC](https://www.amazon.co.uk/dp/B0GV3KN1HC) |
| 1 | 12 V mini siren, 120 dB | Horn. The sketch pulses it; it is not a 5 V buzzer | [B07CV7SHYL](https://www.amazon.co.uk/dp/B07CV7SHYL) |
| 1 | Round SPST rocker, from the 10-pack | Power switch in the battery positive lead | [B07ZTJNRXT](https://www.amazon.co.uk/dp/B07ZTJNRXT) |
| 2 | LM2596 buck modules, from the 10-pack | One set to **5.0 V** (Nano + LoRa). One set to **9.0 V** (digit anodes) | [B0B5GQTS64](https://www.amazon.co.uk/dp/B0B5GQTS64) |
| 1 | 12 V 6800 mAh rechargeable pack, DC 5.5×2.1 mm, with its charger | Power. Blue pack in the parts photo | [B00BWW9WRM](https://www.amazon.co.uk/dp/B00BWW9WRM) |
| 16 | 150 Ω resistors, ¼ W | One in each segment and decimal-point lead | — |
| 1 | Small NPN (2N2222 or similar) and a 1 kΩ resistor | Switches the 12 V siren from Nano **D10** | — |
| 1 | 1N4007 diode | Across the siren | — |

Also: USB lead for the Nano Every (USB micro), hook-up wire, and a multimeter. The buck modules leave the factory near full input voltage. **Measure and set them before they touch the Nano or the digits.**

The linked LoRa size is **DX-LR32-900T22D**. The listing text says the DX-LR32 series can be set from 433 MHz through 930 MHz. This sketch sends `AT+FREQ=433150000`. After upload, Serial Monitor must show that command with no error from the module. If the module rejects 433 MHz, it will not hear a master that is on channel 2.

---

## Power

```text
Battery +  -- rocker --+-- LM2596 #1 IN+
Battery −  ------------+-- LM2596 #1 IN−
                        |
                        +-- LM2596 #2 IN+
                        +-- LM2596 #2 IN−   (same battery −)

LM2596 #1 OUT+  = 5.0 V  -->  Nano 5V,  LoRa VCC
LM2596 #1 OUT−  = GND    -->  Nano GND, LoRa GND

LM2596 #2 OUT+  = 9.0 V  -->  digit common anodes only (pins 1 and 8)
LM2596 #2 OUT−  = GND    -->  Nano GND
```

1. Turn the **battery pack switch** on. The rocker is a second switch in the positive lead, so the remote can be switched off without opening the pack.
2. With the Nano **unplugged**, set buck 1 to **5.0 V** and buck 2 to **9.0 V**.
3. Program the Nano from USB with the **rocker off**, so USB 5 V and the buck are not both feeding the board. Run the remote from the battery with the USB lead **unplugged**.
4. Do not connect the 9 V rail, or the raw 12 V battery, to any Nano pin.

The digits drop about **7.2 V** at full brightness. The 9 V rail plus **150 Ω** in each cathode lead is about **12 mA** per segment: `(9.0 − 7.2) / 150`. Do not use a lower resistor. A review of this digit reports segments failing when held near 18 mA. The Nano sinks that current directly (the sketch has no segment-driver chip), so two digits showing `8` is a lot of pin current. Leave the resistors at 150 Ω.

---

## Digits

Both modules are **common anode**. Pins **1 and 8** are the commons. Tie pin 1 and pin 8 on each digit to the **9 V** buck. Do not tie the two digits' segment pins together.

The sketch lights a segment by driving that Nano pin **LOW**. A **HIGH** pin is off.

| Segment | Ones digit (right) | Tens digit (left) |
| --- | --- | --- |
| A | D8 | A6 |
| B | D7 | A5 |
| C | D6 | A4 |
| D | D5 | A3 |
| E | D4 | A2 |
| F | D3 | A1 |
| G | D2 | A0 |
| DP | D9 | A7 |

Put the **150 Ω** resistor in the wire from the digit pin to the Nano pin. Either end of that wire is fine.

The listing names the commons and not the segment letters. Before soldering all sixteen wires, check one cathode at a time: 9 V on pins 1 and 8, 150 Ω from a cathode pin to GND. Note which bar lights, then connect that letter to the pin in the table. Decimal points stay wired; the sketch holds them off except for a `TEST` command.

Numbers **1–9** leave the tens digit blank. **10–28** use both digits. A shot value of **0** clears the display (the horn is a separate `BUZZER` message from the master).

---

## Horn

D10 goes **HIGH** for about half a second on `BUZZER`, and for about one second on `END`. The siren is a **12 V** load. Do not connect it to D10 and GND.

```text
Switched 12 V ----+---- siren red (+)
                  |
               1N4007 cathode
                  |
Siren black (−) --+---- NPN collector
                       NPN emitter ---- GND
Nano D10 -- 1 kΩ -- NPN base
```

Diode band (cathode) toward the 12 V side, across the siren. Any small NPN such as a 2N2222 is enough for the short pulses this sketch sends.

---

## LoRa module

Nano Every **Serial** is the USB port (debug, 9600). **Serial1** is the radio, also 9600:

| DX-LR32 | Nano Every |
| --- | --- |
| VCC | 5 V (buck 1) |
| GND | GND |
| TXD | D0 (RX) |
| RXD | D1 (TX) |

TX and RX cross. Leave AUX unconnected. Fit the antenna before you power the module.

`LORA_DEFAULT_CHANNEL` in `lora_remote.ino` must match the master. Default on both is **2**.

| Channel | Frequency |
| --- | --- |
| 0 | 433.050 MHz |
| 1 | 433.100 MHz |
| 2 | 433.150 MHz |
| 3 | 433.200 MHz |

---

## Upload

1. Arduino IDE, board **Arduino Nano Every**, sketch `lora_remote/lora_remote.ino`.
2. Rocker **off**. Upload over USB.
3. Open Serial Monitor at **9600**. You should see `LoRa remote scoreboard ready`, then `AT+FREQ=433150000`, `AT+SF7`, and `AT+POWE22`.
4. Unplug USB, turn the rocker on, and start the master shot clock. The digits should follow the shot time. At **0** the digits clear and the siren pulses.

The sketch does not wait for the USB port, so it will start on the battery with no computer attached.
