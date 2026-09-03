# test_waveshare — panel / HUB75 cable diagnostic

Use this sketch **before** the scoreboard firmware to confirm the Waveshare 64×32 panel, the 16-way ribbon, and the Mega pins.

It uses the same vendored Waveshare `RGBmatrixPanel` driver and `Reginit()` as `waterpolo_scoreboard/`. Wiring is in the main [README — RGB matrix → Mega (HUB75)](../README.md#rgb-matrix--mega-hub75).

## Power first (required)

The panel must be powered from the **external 5 V ≥2.5 A** supply on the **VH4** connector, with **PSU GND** tied to **Mega GND**.

Do **not** judge the display with only USB / Mega power. With no VH4 supply, the ribbon can still weakly light some LEDs (often a **red** strip on the left) by leaking current through the data pins. That is **parasitic power**, can stress the Mega, and is **not** a valid test result.

Plug the ribbon into the panel **HUB75 INPUT** (often labelled **IN** / arrow in) — **not** the OUTPUT / cascade port.

## Upload and run

1. Connect **VH4** to the external **5 V ≥2.5 A** PSU (**VCC** and **GND**); tie **PSU GND** to **Mega GND**.
2. Wire the HUB75 ribbon exactly as in the table below (**brown = pin 1 / R1 → D24**, …).
3. Open `test_waveshare/test_waveshare.ino`, select **Arduino Mega or Mega 2560**, upload.
4. Open Serial Monitor @ **115200**. The panel starts **red**.
5. Type a single key (no Enter required) and watch the panel.

| Key | What a healthy panel shows |
| --- | -------------------------- |
| `r` | Full **red** (both halves) |
| `g` | Full **green** (both halves) |
| `b` | Full **blue** (both halves) |
| `w` | Full **white** (both halves) |
| `0` | Off / black |
| `c` | **Column walk** — one red vertical line sweeping 0 → 63 |
| `y` | **Row walk** — one red horizontal line sweeping 0 → 31 |
| `q` | 16-column green sections walking left → right |
| `t` | Centre cross (red vertical, green horizontal) + blue corner pixels |
| `h` | Reprint help in Serial |

A healthy run looks like: `r` / `g` / `b` / `w` fill the whole 64×32, column walk is a clean vertical bar across **both** halves, row walk is a clean horizontal bar on **every** row including the top and bottom 16.

## How the panel is driven

A 64×32 HUB75 panel is two 64×16 halves on one ribbon. Colour on each half is a **separate** data wire. Address / clock / latch / OE are shared.

| Half | Rows | Red | Green | Blue |
| ---- | ---- | --- | ----- | ---- |
| **Top** | 0–15 | R1 | G1 | **B1** |
| **Bottom** | 16–31 | R2 | G2 | **B2** |

That is why a single missing colour on **only one half** points at one data cable, not at CLK / LAT / OE / A–D.

Row walk and column walk in this sketch are **red**, so they prove scan and shift (and R1/R2) — they do **not** prove green or blue. Always finish with `r`, `g`, `b`, and `w`.

White is the fastest colour check: missing blue looks **yellow**, missing green looks **magenta**, missing red looks **cyan**, on the half that owns that wire.

## Rainbow ribbon map

**Brown is conductor 1** (= HUB75 **R1**). Align it with the pin-1 mark on the HUB75 header (triangle / arrow). There are **two oranges** — do not mix them up:

- **Orange #3** = **B1** (top-half blue) → **D26**
- **Orange #13** = **CLK** → **D11**

If CLK were open, column walk would fail and the picture would scramble. A clean column walk with blue missing on the top half is therefore **#3**, not **#13**.

| # | Colour | HUB75 | Mega | What it drives |
| - | ------ | ----- | ---- | -------------- |
| 1 | Brown | R1 | **D24** | Top red |
| 2 | Red | G1 | **D25** | Top green |
| 3 | Orange | B1 | **D26** | Top blue |
| 4 | Yellow | GND | **GND** | Ground |
| 5 | Green | R2 | **D27** | Bottom red |
| 6 | Blue | G2 | **D28** | Bottom green |
| 7 | Violet | B2 | **D29** | Bottom blue |
| 8 | Grey | GND | **GND** | Ground (not address E on this 64×32) |
| 9 | White | A | **A0** | Row address |
| 10 | Black | B | **A1** | Row address |
| 11 | Brown | C | **A2** | Row address |
| 12 | Red | D | **A3** | Row address |
| 13 | Orange | CLK | **D11** | Shift clock |
| 14 | Yellow | LAT | **D9** | Latch |
| 15 | Green | OE | **D10** | Output enable |
| 16 | Blue | GND | **GND** | Ground |

## Worked example — blue missing on the top half

Observed:

- Row walk (`y`) works
- Column walk (`c`) works
- Full red (`r`) works on both halves
- Full green (`g`) works on both halves
- Full blue (`b`) is **missing on the top half** (bottom half is blue)

What that already rules out:

- Address **A–D**, **CLK**, **LAT**, **OE** — row and column walks would fail or scramble
- **R1, G1, R2, G2** — red and green fill both halves
- **B2** (violet #7) — bottom-half blue is present

**Fault: orange #3 — HUB75 B1 → Mega D26** (open, unseated, or on the wrong pin).

On `b` the bottom half is blue and the top half is dark. On `w` the top half looks **yellow** (red + green, no blue).

Reseat / re-wire orange **#3** (the first orange, next to red #2) onto **D26**. Do not confuse it with orange **#13 CLK**.

## Output → cable at fault

Use this after `c`, `y`, then `r` / `g` / `b` / `w`. “Open” means the named ribbon conductor is not making it to the Mega pin (loose Dupont, wrong pin, broken IDC, or brown edge not aligned to pin 1).

| What you see | Cable at fault |
| ------------ | -------------- |
| `c` and `y` work; `r` and `g` full; `b` missing **top** half only; `w` top looks yellow | **#3 Orange B1 → D26** |
| `c` and `y` work; `r` and `g` full; `b` missing **bottom** half only; `w` bottom looks yellow | **#7 Violet B2 → D29** |
| `g` missing **top** half; `r`/`b` OK; `w` top looks magenta | **#2 Red G1 → D25** |
| `g` missing **bottom** half; `r`/`b` OK; `w` bottom looks magenta | **#6 Blue G2 → D28** |
| `r` missing **top** half; `g`/`b` OK; `w` top looks cyan; column walk missing on top | **#1 Brown R1 → D24** |
| `r` missing **bottom** half; `g`/`b` OK; `w` bottom looks cyan; column walk missing on bottom | **#5 Green R2 → D27** |
| Green and blue **swapped** on the top half (and usually the bottom too) | **G1↔B1** (#2 ↔ #3) and **G2↔B2** (#6 ↔ #7) physically swapped |
| Row walk skips / duplicates rows, or only 16 of 32 rows ever light, but colours are otherwise OK | Address **A–D**: **#9 White A→A0**, **#10 Black B→A1**, **#11 Brown C→A2**, **#12 Red D→A3** |
| Column walk jumps, smears, or does not walk 0→63; picture scrambled | **#13 Orange CLK → D11** |
| Image present but frozen / ghosted / very dim, or inverted brightness | **#14 Yellow LAT → D9** or **#15 Green OE → D10** (this project: yellow #14 = LAT, green #15 = OE) |
| Blank, scrambled, or full white with VH4 powered | Common GND first (**#4 / #8 / #16** and PSU GND ↔ Mega GND), then CLK / LAT / OE |
| Top half solid, bottom half flashes / unstable (cables already checked) | Ribbon in **OUTPUT** not **INPUT**; or PSU sagging below ~4.5 V at VH4; or grey **#8** wired to A4 instead of **GND** |
| `b` looks solid but red/green are scattered or “short” | Blue wires are probably fine — re-check **#1 R1**, **#2 G1**, **#5 R2**, **#6 G2** |
| Red strip with **VH4 unplugged** | Parasitic glow — not a cable fault. Connect the 5 V PSU before diagnosing. |

If a colour is missing on **both** halves, two data wires of the same colour are open (e.g. B1 **and** B2), or the panel is not actually receiving that colour at the IDC — still start with the matching `#` rows above.

## Confirm a suspected data wire

1. Hold the colour that failed (`b` in the worked example).
2. Wiggle the named Dupont at the Mega pin — if the missing half flashes in, the crimp / pin is the fault.
3. Optional: `test_hub75_cables/` (USB only, VH4 **off**) — type that cable number **1–16** and probe the ribbon colour for ~5 V vs Mega GND.

Grey **#8** is **GND** on this 64×32 Waveshare — wire it to **Mega GND**, not A4.
