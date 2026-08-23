# User guide — STOP mode (Start/Stop clock)

Use this guide when the timing menu **CLOCK** page is set to **STOP** (the default).

In STOP mode, **period**, **shot**, and **exclusion** clocks start and stop **together**.

Related guides:

- [Timing menu](user_guide_timing_menu.md)
- [RUN mode button guide](user_guide_run_mode.md)

Also summarised in the main [README — How to use](../README.md#how-to-use).

---

## Core idea

| Concept | Behaviour |
|---------|-----------|
| **Play** | Period + shot + exclusions are all running, or all stopped. |
| **START/STOP** | Toggles that whole group on/off. |
| **Timeout / interval** | Period and shot freeze; independent TO or IN/HT countdown runs. |

LCD `*` next to the shot value means play is running.

---

## START/STOP

| Press | Effect |
|-------|--------|
| **Short press** (play mode only) | Toggle period + shot + exclusions together. First start of a quarter marks the period as started. |
| **Hold ~5 s** (clocks **stopped**) | Reload period clock to the current match length (`PERIOD`). May also pull the shot clock down if it is longer than the new period. Does **not** start the clocks. |
| During **TIMEOUT** or **INTERVAL / HT** | No effect — use **RETURN** (or wait for the break to end). |

---

## Combos involving START/STOP

| Hold | Time | Effect |
|------|------|--------|
| **START/STOP** + **28s RESET** | ~**5 s** | **Full reset**: scores 0–0, P1, period 8:00, shot 28, clear TO/IN/HT/exclusions, clocks stopped, timing menu values → defaults, **CLOCK → STOP**. |
| **START/STOP** + **RETURN** | ~**2 s** | Reload period to match length; **keep** current shot and exclusion values; clocks stop. |

---

## Scoring

| Button | Effect |
|--------|--------|
| **+ HOME** / **+ AWAY** | Score +1 (max 99). **Stops** period + shot + exclusions. Shot → **28**. Clears both exclusion clocks. |
| **− HOME** / **− AWAY** | Score −1 (min 0). Clocks unchanged. |

After a goal, press **START/STOP** when play resumes.

---

## Shot clock

| Button | Effect |
|--------|--------|
| **28s RESET** | Shot → **28**. Period run state unchanged. **Clears** both exclusion clocks. |
| **18s RESET** | Shot → **18** only if current value is **&lt; 18**. Period run state unchanged. |
| **FORCE 18** | Shot → **18** always. **Stops** period + shot + exclusions. |
| Shot reaches **0** | Horn/relay (~0.5 s); LoRa `BUZZER`; **stops** all clocks; shot reloads to **28**. |

When period remaining is less than the shot value, the shot display follows the period seconds.

---

## Exclusions

| Button | Effect |
|--------|--------|
| **EXCL** (1st press) | Start exclusion clock 1 (**18 s**). **Stops** period + shot + exclusions from running. If shot &lt; 18, applies the 18s rule. |
| **EXCL** (2nd press) | Start exclusion clock 2 (same rules) while clock 1 is still active. |
| Both clocks busy | Further presses do nothing. |

Notes:

- A slot only arms from **0**; you cannot restart an active clock until it reaches 0.
- Exclusion timers tick only while play is running; they pause when play is stopped and during TO/IN.
- Remaining exclusion time is **kept** across **INTERVAL / HT** into the next period.
- A **goal** or **28s RESET** clears both clocks.

Press **START/STOP** to resume play (and continue any active exclusion countdown).

---

## Timeout and interval

| Button | Effect |
|--------|--------|
| **TIMEOUT** | Stops period + play. Runs independent timeout countdown (`TO`). Length from menu **TIMEOUT**. |
| **INTERVAL** | Stops period + play. Advances period (P1→P4). Loads match period length + shot **28**. Keeps remaining exclusions. Runs **IN** or **HT** (HT after leaving P2). |
| Period hits **0:00** | Horn (~1 s); LoRa `END`. On P1–P3 auto-starts **IN/HT** (same as **INTERVAL**). On P4 stays at 0:00. |

| **RETURN** | Effect |
|------------|--------|
| **Short press** during TO/IN/HT | End break early (silent). Back to period + shot display. |
| **Hold ~3 s** during TO/IN/HT | End break **and** sound buzzer (relay + LoRa). |
| **Hold ~3 s** in play (not in TO/IN) | Buzzer only. |
| Short press in play | No effect. |

After timeout: shot value is **kept**; press **START/STOP** to resume.  
After interval / HT: fresh period at match length / shot 28; exclusions kept; press **START/STOP** to start.

---

## Time adjust (play mode only for period)

| Button | Short press | Long press (~0.8 s) |
|--------|-------------|---------------------|
| **GAME +1s** | Period +1 s | Period +**10 s** |
| **GAME −1s** | Period −1 s | Period −**30 s** |
| **SHOT +1s** | Shot +1 s | Shot +**10 s** |
| **SHOT −1s** | Shot −1 s | Shot −**10 s** |

Limits: period **0:00–15:00** (live), shot **0–28**. Period adjust does nothing during TO/IN. If period is set to **0:00**, clocks stop.

If you change the period clock **before the first START** of the current period, that value becomes the **match length** for later periods.

---

## Timing menu access

| Hold | Time | Effect |
|------|------|--------|
| **18s RESET** + **28s RESET** | ~**2 s** | Open timing menu (see [Timing menu guide](user_guide_timing_menu.md)). |

---

## Quick reference — STOP mode

| Control | Short | Long / hold | Combo |
|---------|-------|-------------|-------|
| **START/STOP** | Toggle all clocks | ~5 s from stopped → reload period | +**28s** ~5 s full reset; +**RETURN** ~2 s reload period keep shot/excl |
| **+ HOME / + AWAY** | Goal; stop all; shot 28; clear excl | — | — |
| **− HOME / − AWAY** | Score −1 | — | — |
| **28s RESET** | Shot 28; clear excl | — | with **18s** → menu; with **S/S** → full reset |
| **18s RESET** | Shot → 18 if &lt; 18 | — | with **28s** → menu |
| **FORCE 18** | Shot 18; stop all | — | — |
| **EXCL** | Arm excl 1 then 2; stop all | — | — |
| **TIMEOUT** | Start TO | — | — |
| **INTERVAL** | Next period + IN/HT | — | — |
| **RETURN** | End TO/IN silent | ~3 s buzzer (+ end TO/IN) | with **S/S** → period reload |
| **GAME ±** | ±1 s period | +10 / −30 s | — |
| **SHOT ±** | ±1 s shot | ±10 s | — |
