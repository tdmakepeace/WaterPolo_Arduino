#!/usr/bin/env python3
"""Generate faceplate drawings + SVG for water-polo control panel.

Layout (mm, origin = outer top-left):
  HOME / LCD / AWAY stay at the top.
  Below: 3 rows × 4 columns —

    Game +1s | Game −1s | Shot +1s | Shot −1s
    TIMEOUT  | INTERVAL | FORCE 18 | RETURN
    28s RESET| 18s RESET| EXCLUSION| START/STOP
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent

# ----- Design (mm) — origin = outer top-left of panel -----
PANEL_W, PANEL_H = 250.0, 180.0
CORNER_R = 8.0
FACE_THICK = 3.0  # suggested print thickness

# Freenove I2C LCD 1602 (classic module)
LCD_PCB_W, LCD_PCB_H = 80.0, 36.0
LCD_BEZEL_W, LCD_BEZEL_H = 72.0, 25.0  # front cutout for metal bezel
LCD_VIEW_W, LCD_VIEW_H = 64.5, 16.0
LCD_MOUNT_DX, LCD_MOUNT_DY = 75.0, 31.0  # hole centres
LCD_MOUNT_D = 3.2  # M3 clearance
LCD_CX, LCD_CY = 125.0, 48.0

# 16 mm panel-mount momentary buttons
BTN_D = 16.0
BTN_HOLE = 16.2  # slight clearance

# Corner mounting screws (M4)
MOUNT_D = 4.5
MOUNT_INSET = 8.0

# 3×4 grid below the LCD (pitch 50 mm)
GRID_XS = (55.0, 105.0, 155.0, 205.0)
GRID_YS = (95.0, 125.0, 155.0)
GRID_PITCH_X = GRID_XS[1] - GRID_XS[0]
GRID_PITCH_Y = GRID_YS[1] - GRID_YS[0]

# Button centres (x, y) — 4 score + 12 grid = 16
BUTTONS = [
    # label, x, y, diameter, hole, colour
    ("+ HOME", 35.0, 36.0, BTN_D, BTN_HOLE, "#c0392b"),
    ("- HOME", 35.0, 62.0, BTN_D, BTN_HOLE, "#c0392b"),
    ("+ AWAY", 215.0, 36.0, BTN_D, BTN_HOLE, "#2980b9"),
    ("- AWAY", 215.0, 62.0, BTN_D, BTN_HOLE, "#2980b9"),
    # Row 1
    ("GAME +1s", GRID_XS[0], GRID_YS[0], BTN_D, BTN_HOLE, "#27ae60"),
    ("GAME -1s", GRID_XS[1], GRID_YS[0], BTN_D, BTN_HOLE, "#f1c40f"),
    ("SHOT +1s", GRID_XS[2], GRID_YS[0], BTN_D, BTN_HOLE, "#27ae60"),
    ("SHOT -1s", GRID_XS[3], GRID_YS[0], BTN_D, BTN_HOLE, "#f1c40f"),
    # Row 2
    ("TIMEOUT", GRID_XS[0], GRID_YS[1], BTN_D, BTN_HOLE, "#e67e22"),
    ("INTERVAL", GRID_XS[1], GRID_YS[1], BTN_D, BTN_HOLE, "#1abc9c"),
    ("FORCE 18", GRID_XS[2], GRID_YS[1], BTN_D, BTN_HOLE, "#ecf0f1"),
    ("RETURN", GRID_XS[3], GRID_YS[1], BTN_D, BTN_HOLE, "#bdc3c7"),
    # Row 3
    ("28s RESET", GRID_XS[0], GRID_YS[2], BTN_D, BTN_HOLE, "#e67e22"),
    ("18s RESET", GRID_XS[1], GRID_YS[2], BTN_D, BTN_HOLE, "#e67e22"),
    ("EXCLUSION", GRID_XS[2], GRID_YS[2], BTN_D, BTN_HOLE, "#8e44ad"),
    ("START/STOP", GRID_XS[3], GRID_YS[2], BTN_D, BTN_HOLE, "#2ecc71"),
]

FRAMES = [
    # (x0,y0,x1,y1, title)
    (18, 22, 52, 76, "HOME"),
    (198, 22, 232, 76, "AWAY"),
]


def lcd_rects():
    bezel = (
        LCD_CX - LCD_BEZEL_W / 2,
        LCD_CY - LCD_BEZEL_H / 2,
        LCD_CX + LCD_BEZEL_W / 2,
        LCD_CY + LCD_BEZEL_H / 2,
    )
    pcb = (
        LCD_CX - LCD_PCB_W / 2,
        LCD_CY - LCD_PCB_H / 2,
        LCD_CX + LCD_PCB_W / 2,
        LCD_CY + LCD_PCB_H / 2,
    )
    view = (
        LCD_CX - LCD_VIEW_W / 2,
        LCD_CY - LCD_VIEW_H / 2,
        LCD_CX + LCD_VIEW_W / 2,
        LCD_CY + LCD_VIEW_H / 2,
    )
    holes = [
        (LCD_CX - LCD_MOUNT_DX / 2, LCD_CY - LCD_MOUNT_DY / 2),
        (LCD_CX + LCD_MOUNT_DX / 2, LCD_CY - LCD_MOUNT_DY / 2),
        (LCD_CX - LCD_MOUNT_DX / 2, LCD_CY + LCD_MOUNT_DY / 2),
        (LCD_CX + LCD_MOUNT_DX / 2, LCD_CY + LCD_MOUNT_DY / 2),
    ]
    return bezel, pcb, view, holes


def mount_holes():
    i = MOUNT_INSET
    return [(i, i), (PANEL_W - i, i), (i, PANEL_H - i), (PANEL_W - i, PANEL_H - i)]


def _fonts():
    try:
        return (
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 14),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 11),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 18),
            ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 12),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 22),
            ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 16),
        )
    except OSError:
        d = ImageFont.load_default()
        return d, d, d, d, d, d


def write_svg():
    bezel, pcb, view, lcd_holes = lcd_rects()
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{PANEL_W}mm" height="{PANEL_H}mm"',
        f'  viewBox="0 0 {PANEL_W} {PANEL_H}">',
        "  <!-- Water Polo Control faceplate — units: millimetres -->",
        f'  <rect x="0" y="0" width="{PANEL_W}" height="{PANEL_H}" rx="{CORNER_R}" ry="{CORNER_R}"',
        '    fill="#1a1a1a" stroke="#ffffff" stroke-width="0.5"/>',
        '  <text x="125" y="14" text-anchor="middle" fill="#ffffff"',
        '    font-family="Arial,sans-serif" font-size="5" font-weight="bold">WATER POLO CONTROL</text>',
    ]
    for x0, y0, x1, y1, title in FRAMES:
        lines.append(
            f'  <rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}"'
            ' fill="none" stroke="#ffffff" stroke-width="0.4"/>'
        )
        lines.append(
            f'  <text x="{(x0 + x1) / 2}" y="{y0 - 1.5}" text-anchor="middle" fill="#ffffff"'
            f' font-family="Arial,sans-serif" font-size="3">{title}</text>'
        )

    bx0, by0, bx1, by1 = bezel
    lines.append(f'  <!-- LCD bezel cutout {LCD_BEZEL_W}×{LCD_BEZEL_H} -->')
    lines.append(
        f'  <rect x="{bx0}" y="{by0}" width="{LCD_BEZEL_W}" height="{LCD_BEZEL_H}"'
        ' fill="#0d3d2e" stroke="#7dcea0" stroke-width="0.4"/>'
    )
    px0, py0, _, _ = pcb
    lines.append(
        f'  <rect x="{px0}" y="{py0}" width="{LCD_PCB_W}" height="{LCD_PCB_H}"'
        ' fill="none" stroke="#7dcea0" stroke-width="0.25" stroke-dasharray="2 1"/>'
    )
    for hx, hy in lcd_holes:
        r = LCD_MOUNT_D / 2
        lines.append(
            f'  <circle cx="{hx}" cy="{hy}" r="{r}" fill="none" stroke="#7dcea0" stroke-width="0.35"/>'
        )

    for label, x, y, dia, hole, color in BUTTONS:
        r = hole / 2
        lines.append(
            f'  <circle cx="{x}" cy="{y}" r="{r}" fill="{color}" stroke="#ffffff" stroke-width="0.35"'
            f' opacity="0.9"/>'
        )
        lines.append(
            f'  <text x="{x}" y="{y + dia / 2 + 3.5}" text-anchor="middle" fill="#cccccc"'
            f' font-family="Arial,sans-serif" font-size="2.2">{label}</text>'
        )

    for mx, my in mount_holes():
        lines.append(
            f'  <circle cx="{mx}" cy="{my}" r="{MOUNT_D / 2}" fill="none" stroke="#aaaaaa" stroke-width="0.4"/>'
        )

    lines.append("</svg>")
    (OUT / "button_box_panel_dimensions.svg").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_dimensions_png():
    scale = 5.0  # px per mm
    margin = 70
    font, font_sm, font_title, font_dim, _, _ = _fonts()
    img_w = int(PANEL_W * scale + 2 * margin)
    img_h = int(PANEL_H * scale + 2 * margin + 50)
    img = Image.new("RGB", (img_w, img_h), "#f4f4f0")
    draw = ImageDraw.Draw(img)

    ox, oy = margin, margin + 20

    def px(mm_x, mm_y=None):
        if mm_y is None:
            return ox + mm_x * scale
        return ox + mm_x * scale, oy + mm_y * scale

    def rect_mm(x0, y0, x1, y1, **kw):
        draw.rectangle([*px(x0, y0), *px(x1, y1)], **kw)

    def circle_mm(cx, cy, d, **kw):
        r = d / 2
        draw.ellipse([*px(cx - r, cy - r), *px(cx + r, cy + r)], **kw)

    def text_mm(x, y, s, f=font, fill="#222", anchor="mm"):
        draw.text(px(x, y), s, font=f, fill=fill, anchor=anchor)

    def dim_h(x0, x1, y, label, offset=8):
        y_line = y + offset
        p0 = px(x0, y_line)
        p1 = px(x1, y_line)
        draw.line([p0, p1], fill="#c0392b", width=2)
        for x in (x0, x1):
            draw.line([px(x, y_line - 2), px(x, y_line + 2)], fill="#c0392b", width=2)
        mid = ((p0[0] + p1[0]) / 2, p0[1] - 8)
        draw.text(mid, label, font=font_dim, fill="#c0392b", anchor="mb")

    def dim_v(y0, y1, x, label, offset=-10):
        x_line = x + offset
        p0 = px(x_line, y0)
        p1 = px(x_line, y1)
        draw.line([p0, p1], fill="#c0392b", width=2)
        for y in (y0, y1):
            draw.line([px(x_line - 2, y), px(x_line + 2, y)], fill="#c0392b", width=2)
        mid = (p0[0] - 6, (p0[1] + p1[1]) / 2)
        draw.text(mid, label, font=font_dim, fill="#c0392b", anchor="rm")

    draw.text(
        (img_w / 2, 12),
        "Water Polo Control — 3D-print faceplate measurements (mm)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    rect_mm(0, 0, PANEL_W, PANEL_H, fill="#1c1c1c", outline="#333", width=2)

    for x0, y0, x1, y1, title in FRAMES:
        rect_mm(x0, y0, x1, y1, outline="#ffffff", width=2)
        text_mm((x0 + x1) / 2, y0 - 2.5, title, font_sm, fill="#ffffff", anchor="mb")

    text_mm(PANEL_W / 2, 12, "WATER POLO CONTROL", font_title, fill="#ffffff", anchor="mm")

    bezel, pcb, view, lcd_holes = lcd_rects()
    rect_mm(*pcb, outline="#7dcea0", width=1)
    rect_mm(*bezel, fill="#0d3d2e", outline="#7dcea0", width=2)
    rect_mm(*view, fill="#145a32", outline="#27ae60", width=1)
    text_mm(LCD_CX, LCD_CY - 4, "Freenove I2C LCD 1602", font_sm, fill="#d5f5e3", anchor="mm")
    text_mm(
        LCD_CX,
        LCD_CY + 5,
        f"cutout {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f}",
        font_sm,
        fill="#a9dfbf",
        anchor="mm",
    )
    for hx, hy in lcd_holes:
        circle_mm(hx, hy, LCD_MOUNT_D, outline="#7dcea0", width=1)

    for label, x, y, dia, hole, color in BUTTONS:
        circle_mm(x, y, hole, fill=color, outline="#ffffff", width=2)
        draw.line([px(x - 2, y), px(x + 2, y)], fill="#111", width=1)
        draw.line([px(x, y - 2), px(x, y + 2)], fill="#111", width=1)
        text_mm(x, y + dia / 2 + 4, label, font_sm, fill="#dddddd", anchor="mt")

    for mx, my in mount_holes():
        circle_mm(mx, my, MOUNT_D, outline="#888", width=2)

    dim_h(0, PANEL_W, PANEL_H, f"{PANEL_W:.0f} mm", offset=12)
    dim_v(0, PANEL_H, 0, f"{PANEL_H:.0f} mm", offset=-10)
    dim_h(bezel[0], bezel[2], bezel[3], f"LCD {LCD_BEZEL_W:.0f}", offset=6)
    dim_v(bezel[1], bezel[3], bezel[2], f"{LCD_BEZEL_H:.0f}", offset=8)
    dim_h(GRID_XS[0], GRID_XS[1], GRID_YS[2], f"{GRID_PITCH_X:.0f}", offset=10)
    dim_v(GRID_YS[0], GRID_YS[1], GRID_XS[0], f"{GRID_PITCH_Y:.0f}", offset=-14)
    dim_v(36, 62, 35, "26", offset=-14)

    lx, ly = ox, oy + PANEL_H * scale + 28
    legend = [
        f"Panel outer: {PANEL_W:.0f} × {PANEL_H:.0f} × {FACE_THICK:.0f} mm (suggested thickness)",
        f"Buttons: {len(BUTTONS)} × Ø{BTN_D:.0f} mm panel-mount → drill Ø{BTN_HOLE} mm  |  corner mounts: 4 × Ø{MOUNT_D}",
        f"Grid: 3 rows × 4 cols  ·  pitch {GRID_PITCH_X:.0f} mm (X) × {GRID_PITCH_Y:.0f} mm (Y)",
        f"LCD (Freenove 1602): PCB {LCD_PCB_W:.0f}×{LCD_PCB_H:.0f}  ·  bezel cutout {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f}",
        "Origin: top-left outer corner  ·  all coordinates are hole centres (mm)",
    ]
    for i, line in enumerate(legend):
        draw.text((lx, ly + i * 15), line, font=font_sm, fill="#333")

    img.save(OUT / "button_box_panel_dimensions.png", dpi=(150, 150))


def write_panel_preview_png():
    """Operator-facing coloured mockup (button_box_panel.png)."""
    scale = 4.0
    pad = 40
    font, font_sm, font_title, _, font_big, font_lcd = _fonts()
    img_w = int(PANEL_W * scale + 2 * pad)
    img_h = int(PANEL_H * scale + 2 * pad)
    img = Image.new("RGB", (img_w, img_h), "#2c3e50")
    draw = ImageDraw.Draw(img)

    ox, oy = pad, pad

    def px(x, y=None):
        if y is None:
            return ox + x * scale
        return ox + x * scale, oy + y * scale

    def rounded_panel():
        # approximate rounded rect
        r = CORNER_R * scale
        x0, y0 = px(0, 0)
        x1, y1 = px(PANEL_W, PANEL_H)
        draw.rounded_rectangle([x0, y0, x1, y1], radius=r, fill="#111111", outline="#ecf0f1", width=3)

    def circle(cx, cy, d, fill, outline="#ffffff"):
        r = d / 2
        draw.ellipse([*px(cx - r, cy - r), *px(cx + r, cy + r)], fill=fill, outline=outline, width=2)

    def label(x, y, text, fill="#ecf0f1"):
        draw.text(px(x, y), text, font=font_sm, fill=fill, anchor="mt")

    rounded_panel()
    draw.text(px(PANEL_W / 2, 12), "WATER POLO CONTROL", font=font_big, fill="#ffffff", anchor="mm")

    for x0, y0, x1, y1, title in FRAMES:
        draw.rectangle([*px(x0, y0), *px(x1, y1)], outline="#ffffff", width=2)
        draw.text(px((x0 + x1) / 2, y0 - 2), title, font=font_sm, fill="#ffffff", anchor="mb")

    bezel, _, view, _ = lcd_rects()
    draw.rectangle([*px(bezel[0], bezel[1]), *px(bezel[2], bezel[3])], fill="#0d3d2e", outline="#7dcea0", width=2)
    draw.rectangle([*px(view[0], view[1]), *px(view[2], view[3])], fill="#145a32")
    draw.text(px(LCD_CX, LCD_CY - 5), "6:00 P1 H00-00A", font=font_lcd, fill="#d5f5e3", anchor="mm")
    draw.text(px(LCD_CX, LCD_CY + 6), "S28  E --, --", font=font_lcd, fill="#a9dfbf", anchor="mm")

    # Row hint labels above grid
    draw.text(px((GRID_XS[0] + GRID_XS[1]) / 2, GRID_YS[0] - 14), "GAME CLOCK", font=font_sm, fill="#95a5a6", anchor="mm")
    draw.text(px((GRID_XS[2] + GRID_XS[3]) / 2, GRID_YS[0] - 14), "SHOT CLOCK", font=font_sm, fill="#95a5a6", anchor="mm")

    for name, x, y, dia, hole, color in BUTTONS:
        circle(x, y, hole, color)
        # Dark text on light caps for readability
        text_fill = "#1a1a1a" if color.lower() in ("#ecf0f1", "#bdc3c7", "#f1c40f") else "#ecf0f1"
        label(x, y + dia / 2 + 3.5, name, fill=text_fill)

    img.save(OUT / "button_box_panel.png", dpi=(150, 150))


def write_print_guide_png():
    """Annotated print guide (button_box_panel_3d_print_guide.png)."""
    scale = 4.5
    margin = 90
    font, font_sm, font_title, font_dim, _, _ = _fonts()
    img_w = int(PANEL_W * scale + 2 * margin)
    img_h = int(PANEL_H * scale + 2 * margin + 30)
    img = Image.new("RGB", (img_w, img_h), "#ffffff")
    draw = ImageDraw.Draw(img)
    ox, oy = margin, margin

    def px(x, y=None):
        if y is None:
            return ox + x * scale
        return ox + x * scale, oy + y * scale

    def circle_outline(cx, cy, d, color="#333"):
        r = d / 2
        draw.ellipse([*px(cx - r, cy - r), *px(cx + r, cy + r)], outline=color, width=2)

    draw.text(
        (img_w / 2, 18),
        "Water Polo Control — faceplate print guide (mm, ±0.2)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    # Panel outline
    draw.rounded_rectangle(
        [*px(0, 0), *px(PANEL_W, PANEL_H)],
        radius=CORNER_R * scale,
        outline="#111",
        width=3,
        fill="#fafafa",
    )

    bezel, _, _, lcd_holes = lcd_rects()
    draw.rectangle([*px(bezel[0], bezel[1]), *px(bezel[2], bezel[3])], outline="#27ae60", width=2)
    draw.text(px(LCD_CX, LCD_CY), "LCD 72×25", font=font_sm, fill="#1e8449", anchor="mm")
    for hx, hy in lcd_holes:
        circle_outline(hx, hy, LCD_MOUNT_D, "#27ae60")

    colors = {
        "+ HOME": "#c0392b",
        "- HOME": "#c0392b",
        "+ AWAY": "#2980b9",
        "- AWAY": "#2980b9",
        "GAME +1s": "#27ae60",
        "GAME -1s": "#f39c12",
        "SHOT +1s": "#27ae60",
        "SHOT -1s": "#f39c12",
        "TIMEOUT": "#e67e22",
        "INTERVAL": "#16a085",
        "FORCE 18": "#7f8c8d",
        "RETURN": "#7f8c8d",
        "28s RESET": "#e67e22",
        "18s RESET": "#e67e22",
        "EXCLUSION": "#8e44ad",
        "START/STOP": "#27ae60",
    }
    for name, x, y, dia, hole, _ in BUTTONS:
        circle_outline(x, y, hole, colors.get(name, "#333"))
        draw.text(px(x, y + 11), name, font=font_sm, fill="#333", anchor="mt")

    for mx, my in mount_holes():
        circle_outline(mx, my, MOUNT_D, "#555")

    # Overall dims
    y_dim = PANEL_H + 10
    draw.line([px(0, y_dim), px(PANEL_W, y_dim)], fill="#c0392b", width=2)
    draw.text(px(PANEL_W / 2, y_dim + 4), f"{PANEL_W:.0f}", font=font_dim, fill="#c0392b", anchor="mt")
    x_dim = -10
    draw.line([px(x_dim, 0), px(x_dim, PANEL_H)], fill="#c0392b", width=2)
    draw.text(px(x_dim - 4, PANEL_H / 2), f"{PANEL_H:.0f}", font=font_dim, fill="#c0392b", anchor="rm")

    # Grid pitches
    draw.line([px(GRID_XS[0], GRID_YS[2] + 12), px(GRID_XS[1], GRID_YS[2] + 12)], fill="#c0392b", width=2)
    draw.text(
        px((GRID_XS[0] + GRID_XS[1]) / 2, GRID_YS[2] + 14),
        f"{GRID_PITCH_X:.0f}",
        font=font_dim,
        fill="#c0392b",
        anchor="mt",
    )
    draw.line([px(GRID_XS[0] - 12, GRID_YS[0]), px(GRID_XS[0] - 12, GRID_YS[1])], fill="#c0392b", width=2)
    draw.text(
        px(GRID_XS[0] - 14, (GRID_YS[0] + GRID_YS[1]) / 2),
        f"{GRID_PITCH_Y:.0f}",
        font=font_dim,
        fill="#c0392b",
        anchor="rm",
    )

    # Callouts
    draw.text(
        (18, img_h - 70),
        f"16 × Ø{BTN_HOLE} button holes  ·  3×4 grid pitch {GRID_PITCH_X:.0f}×{GRID_PITCH_Y:.0f} mm",
        font=font_sm,
        fill="#333",
    )
    draw.text(
        (18, img_h - 52),
        f"4 × Ø{MOUNT_D} corner mounts  ·  LCD bezel {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f}  ·  thickness {FACE_THICK:.0f} mm",
        font=font_sm,
        fill="#333",
    )
    draw.text(
        (18, img_h - 34),
        "Row1: Game± / Shot±   Row2: Timeout · Interval · Force18 · Return   Row3: 28s · 18s · Exclusion · Start/Stop",
        font=font_sm,
        fill="#555",
    )

    img.save(OUT / "button_box_panel_3d_print_guide.png", dpi=(150, 150))


def write_markdown():
    bezel, pcb, view, lcd_holes = lcd_rects()
    rows = [f"| {label} | {x:.1f} | {y:.1f} | Ø{hole} |" for label, x, y, dia, hole, _ in BUTTONS]
    lcd_hole_rows = "\n".join(
        f"| LCD mount {i + 1} | {hx:.1f} | {hy:.1f} | Ø{LCD_MOUNT_D} |"
        for i, (hx, hy) in enumerate(lcd_holes)
    )
    mount_rows = "\n".join(
        f"| Panel screw {i + 1} | {mx:.1f} | {my:.1f} | Ø{MOUNT_D} |"
        for i, (mx, my) in enumerate(mount_holes())
    )

    md = f"""# Button-box faceplate — 3D print measurements

Dimensioned layout for a printable / CNC / laser faceplate matching [`button_box_panel.png`](button_box_panel.png).

![Dimensioned faceplate](button_box_panel_dimensions.png)

Print guide overview:

![3D print measurement guide](button_box_panel_3d_print_guide.png)

Vector (1:1 mm): [`button_box_panel_dimensions.svg`](button_box_panel_dimensions.svg)

## Layout (below LCD)

Three rows of four buttons (left → right):

| Row | Buttons |
|-----|---------|
| 1 | Game Clock **+1s** · Game Clock **−1s** · Shot Clock **+1s** · Shot Clock **−1s** |
| 2 | **TIMEOUT** · **INTERVAL** · **FORCE 18** · **RETURN** |
| 3 | **28s RESET** · **18s RESET** · **EXCLUSION** · **START/STOP** |

HOME / AWAY score buttons and the Freenove LCD stay in the top band.

## 3D print files

| File | Format | Notes |
|------|--------|-------|
| [`button_box_panel_faceplate.stl`](button_box_panel_faceplate.stl) | STL | Ready for slicer (Cura / PrusaSlicer / Bambu) |
| [`button_box_panel_faceplate.obj`](button_box_panel_faceplate.obj) | OBJ | Same geometry |

Regenerate mesh:

```bash
python docs/generate_panel_stl.py
```

Print flat on the bed, **{FACE_THICK:.0f} mm** thick, no supports. Ream button holes to Ø{BTN_HOLE} if the slicer shrinks circles.

## Design basis

| Item | Spec |
|------|------|
| Momentary buttons | **Ø16 mm** panel-mount (**{len(BUTTONS)}** pcs) |
| Button panel holes | **Ø16.2 mm** (0.2 mm clearance) |
| Display | **Freenove I2C IIC LCD 1602** (FNK0079) |
| Faceplate outer | **{PANEL_W:.0f} × {PANEL_H:.0f} mm**, corner R **{CORNER_R:.0f}** |
| Suggested thickness | **{FACE_THICK:.0f} mm** (PLA/PETG); leave ≥18 mm depth behind for LCD + I2C backpack |
| Grid pitch | **{GRID_PITCH_X:.0f} mm** (X) × **{GRID_PITCH_Y:.0f} mm** (Y) |
| Coordinate origin | Outer **top-left** corner of panel |

## Freenove LCD 1602 cutout

Classic I2C 1602 module (Freenove FNK0079 class):

| Feature | Size (mm) |
|---------|-----------|
| PCB outline (behind panel) | {LCD_PCB_W:.0f} × {LCD_PCB_H:.0f} |
| **Front bezel cutout** (through faceplate) | **{LCD_BEZEL_W:.0f} × {LCD_BEZEL_H:.0f}** |
| Viewing area (reference) | {LCD_VIEW_W} × {LCD_VIEW_H} |
| Mounting-hole centres | **{LCD_MOUNT_DX:.0f} × {LCD_MOUNT_DY:.0f}** |
| Mounting-hole drill | Ø{LCD_MOUNT_D} (M3 clear) |
| Module centre on panel | ({LCD_CX:.0f}, {LCD_CY:.0f}) |
| Bezel cutout top-left | ({bezel[0]:.1f}, {bezel[1]:.1f}) |

> Measure your module before final print: some Freenove boards are ~80×36, others closer to ~86×36. Adjust the bezel cutout if your metal frame differs; keep the **75×31** mount pattern unless your PCB says otherwise.

## Hole centre table (mm)

| Control | X | Y | Hole |
|---------|--:|--:|------|
{chr(10).join(rows)}
{lcd_hole_rows}
{mount_rows}

## Useful pitches (centre-to-centre)

| Pair | Pitch |
|------|------:|
| HOME + to − (vertical) | 26 mm |
| AWAY + to − (vertical) | 26 mm |
| Grid columns (adjacent) | {GRID_PITCH_X:.0f} mm |
| Grid rows (adjacent) | {GRID_PITCH_Y:.0f} mm |

## 3D-print notes

1. Print faceplate face-up; use **0.2 mm** layer height for clean holes.
2. Drill/ream **all** button holes to **Ø16.2** after printing if your slicer undersizes circles (including START/STOP).
3. LCD sits from the **rear**; bezel shows through the {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f} window; secure with 4× M3.
4. Labels can be engraved, vinyl, or a second printed legend layer.

## Regenerate drawings

```bash
python docs/generate_panel_dimensions.py
```
"""
    (OUT / "button_box_panel_measurements.md").write_text(md, encoding="utf-8")


if __name__ == "__main__":
    write_svg()
    write_dimensions_png()
    write_panel_preview_png()
    write_print_guide_png()
    write_markdown()
    print("Wrote:")
    for name in (
        "button_box_panel.png",
        "button_box_panel_dimensions.png",
        "button_box_panel_dimensions.svg",
        "button_box_panel_3d_print_guide.png",
        "button_box_panel_measurements.md",
    ):
        print(" ", OUT / name)
