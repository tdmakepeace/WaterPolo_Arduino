# User guide — Timing menu

How to open the timing menu and what each page does. Values are kept in memory until a **full reset** (they are not stored when power is removed).

Related guides:

- [Buttons in STOP (Start/Stop) mode](user_guide_stop_mode.md)
- [Buttons in RUN mode](user_guide_run_mode.md)

Also summarised in the main [README — How to use](../README.md#how-to-use).

---

## Open and leave the menu

| Action | Result |
|--------|--------|
| Hold **18s RESET** + **28s RESET** together ~**3 s** | Opens the timing menu. All clocks stop. |
| **START/STOP** (shown as **S/S** on the LCD) | Next page. On the last page (**CLOCK**), confirms and **exits**. |
| **RETURN** | Exit immediately (keeps changes already made). |
| Any other button | Ignored while the menu is open. |

LCD example:

```text
SET PERIOD  1/7
8:00  S/S next *
```

Shot-clock pages show seconds (e.g. `28s`) instead of `m:ss`. Last page shows `S/S OK` instead of `S/S next`. The `*` is a brief command-ack flash.

---

## Menu pages (1 → 7)

| # | Page | Default | Range | Adjust with **GAME +1s / −1s** |
|---|------|---------|-------|--------------------------------|
| 1/7 | **PERIOD** | 8:00 | 0:30–15:00 | ±**30 s** per press |
| 2/7 | **INTERVAL** | 2:00 | 0:00–3:00 | ±**30 s** |
| 3/7 | **HALFTIME** | 2:00 | 0:00–7:00 | ±**30 s** |
| 4/7 | **TIMEOUT** | 1:00 | 0:30–5:00 | ±**30 s** |
| 5/7 | **SHOT 28** | 28 s | 1–60 s | ±**1 s** |
| 6/7 | **SHOT 18** | 18 s | 1–60 s | ±**1 s** |
| 7/7 | **CLOCK** | **STOP** | STOP, RUN, or RUN-NS | Cycle **STOP → RUN → RUN-NS** (no time step) |

Long-press on **GAME +1s / −1s** is not used in the menu — each press steps once (±30 s, ±1 s on shot pages, or cycles CLOCK).

---

## What each option means

### PERIOD

Match length for each quarter (P1–P4). Used when:

- You press **INTERVAL** (loads the new period)
- You reload the period (mode-specific — see STOP / RUN guides)
- Full reset restores the factory default (**8:00**)

If the current period has **not** been started yet, changing **PERIOD** also updates the live period clock on the displays.

### INTERVAL

Break length between periods when the break is labelled **IN** (after P1 and after P3). After P2 the board uses **HALFTIME** instead.

### HALFTIME

Break length after **P2** (display label **HT**). Official half-time is often 5:00; the default in firmware is currently **2:00** — set it here for your competition.

### TIMEOUT

Length of a team timeout when you press **TIMEOUT** (display label **TO**).

### SHOT 28

Length loaded by **28s RESET**, a goal (**+ HOME** / **+ AWAY**), **INTERVAL**, and shot-clock expiry. Live shot **+1s / −1s** also cannot go above this value.

If the live shot is already at the old **SHOT 28** length (or longer than the new value), changing this page updates the live shot clock to match.

### SHOT 18

Length loaded by **18s RESET** (only if the current shot is **shorter** than this value) and **FORCE 18**. **EXCL** uses the same rule when the shot is shorter than this value.

### CLOCK

Chooses how **START/STOP** behaves during play:

| Setting | Meaning |
|---------|---------|
| **STOP** (default) | Start/Stop mode: period, shot, and exclusions start and stop together. |
| **RUN** | Running-clock mode: after the period is live, short **START/STOP** only starts/stops shot + exclusions; the period keeps counting until you stop it with a long press (or timeout / interval / period end). |
| **RUN-NS** | Same period behaviour as **RUN**, but the shot clock is **disabled**: hidden on the LCD/matrix, blanked on LoRa remotes, and shot buttons are ignored. Exclusions count down with the **period** clock. |

Full button guides:

- [STOP mode](user_guide_stop_mode.md)
- [RUN mode](user_guide_run_mode.md) (covers **RUN** and **RUN-NS**)

---

## Typical setup before a game

1. Hold **18s RESET** + **28s RESET** ~3 s.
2. Set **PERIOD** (e.g. 8:00 or 6:30).
3. Press **S/S** → set **INTERVAL**.
4. Press **S/S** → set **HALFTIME**.
5. Press **S/S** → set **TIMEOUT**.
6. Press **S/S** → set **SHOT 28** (e.g. 28 or 30).
7. Press **S/S** → set **SHOT 18** (e.g. 18 or 20).
8. Press **S/S** → set **CLOCK** to **STOP**, **RUN**, or **RUN-NS**.
9. Press **S/S** (**OK**) to leave, or press **RETURN** at any time.

---

## Notes

- Opening the menu **stops** the period and play clocks.
- Leaving the menu does **not** auto-start play — press **START/STOP** when ready.
- A **full reset** (hold **START/STOP** + **28s RESET** ~3 s) restores all timing values and sets **CLOCK** back to **STOP**.
