# User guide — RUN mode (running clock)

Use this guide when the timing menu **CLOCK** page is set to **RUN**.

In RUN mode the **period** can keep counting while **shot** and **exclusion** clocks are stopped (for goals, exclusions, shot expiry, etc.).

Related guides:

- [Timing menu](user_guide_timing_menu.md)
- [STOP mode button guide](user_guide_stop_mode.md)

Also summarised in the main [README — How to use](../README.md#how-to-use).

---

## Core idea

| Concept | Behaviour |
|---------|-----------|
| **Period running** | Main game clock is counting down. |
| **Play running** | Shot + exclusions are counting (LCD `*` on). |
| After first start of a quarter | Short **START/STOP** only toggles **play**. Period keeps going. |
| To stop the period | **Long-press START/STOP** (~5 s), or call **TIMEOUT** / **INTERVAL**, or wait for 0:00. |

Timeout still **stops the period**. After **RETURN** from timeout, press **START/STOP** to start period + play again.

---

## START/STOP

| Situation | Short press | Hold ~5 s |
|-----------|-------------|-----------|
| Period **not** running (new quarter, or after timeout) | Starts **period + play** together | — (no period to stop) |
| Period running, **play running** | Stops **play** only (period keeps running) | Stops **period + play** |
| Period running, **play stopped** | Release starts **play** (press alone waits so a long hold can stop the period) | Stops **period** (and play stays stopped) |
| During **TIMEOUT** or **INTERVAL / HT** | No effect — use **RETURN** first | — |

### Typical flow each quarter

1. Short **START/STOP** → period and shot start.
2. Goal / exclusion / shot expiry → play stops; **period keeps running**.
3. Short **START/STOP** → shot (and exclusions) resume; period still running.
4. To freeze the whole quarter early → hold **START/STOP** ~5 s.

---

## Combos involving START/STOP

| Hold | Time | Effect |
|------|------|--------|
| **START/STOP** + **28s RESET** | ~**5 s** | **Full reset** (same as STOP mode): scores, clocks, exclusions, timing defaults, **CLOCK → STOP**. |
| **START/STOP** + **RETURN** | ~**2 s** | Reload period to match length; **keep** current shot and exclusion values; **stops** period + play. |

---

## Scoring

| Button | Period (RUN) | Play / shot / exclusions |
|--------|--------------|---------------------------|
| **+ HOME** / **+ AWAY** | **Keeps running** | Score +1. Shot → **28**. Clear exclusions. **Play stops** until **START/STOP**. |
| **− HOME** / **− AWAY** | Unchanged | Score −1 only. |

---

## Shot clock

| Button / event | Period (RUN) | Play / shot |
|----------------|--------------|-------------|
| **28s RESET** | Unchanged | Shot → **28**; clear exclusions; **run state unchanged** (play stays running or stopped). |
| **18s RESET** | Unchanged | Shot → **18** only if &lt; 18; run state unchanged. |
| **FORCE 18** | **Keeps running** | Shot → **18**; **play stops**. |
| Shot reaches **0** | **Keeps running** | Horn + LoRa `BUZZER`; shot → **28**; **play stops**. |

When period remaining is less than the shot value **and the period is running**, the shot display follows the period seconds.

---

## Exclusions

| Button | Period (RUN) | Play |
|--------|--------------|------|
| **EXCL** (1st / 2nd press) | **Keeps running** | Arm clock 1 then 2 (**18 s**). If shot &lt; 18, apply 18s rule. **Play stops**. |

Same slot rules as STOP mode: cannot restart an active clock; remaining time kept across IN/HT; goal or **28s RESET** clears both.

Press **START/STOP** to resume play (period is usually still running).

---

## Timeout and interval

Same break behaviour as STOP mode — these **do** stop the period:

| Button | Effect |
|--------|--------|
| **TIMEOUT** | Stops **period + play**. Independent `TO` countdown. |
| **INTERVAL** | Stops **period + play**. Advances period; loads match length + shot 28; keeps exclusions; runs **IN** or **HT**. |
| Period hits **0:00** | Horn; LoRa `END`; auto **IN/HT** on P1–P3. |

| **RETURN** | Effect |
|------------|--------|
| **Short press** during TO/IN/HT | End break early (silent). |
| **Hold ~3 s** during TO/IN/HT | End break + buzzer. |
| **Hold ~3 s** in play | Buzzer only. |
| Short press in play | No effect. |

After timeout or break, the period is **stopped** until you short-press **START/STOP** (starts period + play again).

---

## Time adjust

Same controls as STOP mode:

| Button | Short | Long (~0.8 s) |
|--------|-------|---------------|
| **GAME +1s / −1s** | Period ±1 s | +10 / −30 s |
| **SHOT +1s / −1s** | Shot ±1 s | ±10 s |

Period adjust only in play display (not during TO/IN). Adjusting period to **0:00** stops period + play.

Pre-first-START period edits still update match length for later quarters.

---

## Timing menu access

| Hold | Time | Effect |
|------|------|--------|
| **18s RESET** + **28s RESET** | ~**2 s** | Open timing menu. Stops period + play. See [Timing menu guide](user_guide_timing_menu.md). |

---

## STOP vs RUN — quick comparison

| Event | STOP mode | RUN mode |
|-------|-----------|----------|
| Short **START/STOP** | Toggle period + play | First start: both. Later: play only |
| Long **START/STOP** (~5 s) | From stopped: reload period | Stop period (+ play) |
| Goal / EXCL / FORCE 18 / shot = 0 | Stop period + play | Period keeps; play stops |
| **TIMEOUT** | Stop period + play | Stop period + play |
| **28s / 18s RESET** | Do not toggle run state | Same |

---

## Quick reference — RUN mode

| Control | Short | Long / hold | Combo |
|---------|-------|-------------|-------|
| **START/STOP** | Start period+play, or toggle play only | ~5 s → stop period | +**28s** full reset; +**RETURN** period reload keep shot/excl |
| **+ HOME / + AWAY** | Goal; play stops; period keeps; shot 28; clear excl | — | — |
| **− HOME / − AWAY** | Score −1 | — | — |
| **28s RESET** | Shot 28; clear excl; run state kept | — | with **18s** → menu; with **S/S** → full reset |
| **18s RESET** | Shot → 18 if &lt; 18 | — | with **28s** → menu |
| **FORCE 18** | Shot 18; play stops; period keeps | — | — |
| **EXCL** | Arm excl; play stops; period keeps | — | — |
| **TIMEOUT** | Start TO (stops period) | — | — |
| **INTERVAL** | Next period + IN/HT | — | — |
| **RETURN** | End TO/IN silent | ~3 s buzzer (+ end TO/IN) | with **S/S** → period reload |
| **GAME ±** | ±1 s period | +10 / −30 s | — |
| **SHOT ±** | ±1 s shot | ±10 s | — |
