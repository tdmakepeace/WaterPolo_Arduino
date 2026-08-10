#!/usr/bin/env python3
"""Generate dimensioned faceplate drawing + SVG for water-polo control panel."""

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

# 16 mm panel-mount momentary buttons (all 17 identical)
BTN_D = 16.0
BTN_HOLE = 16.2  # slight clearance

# Corner mounting screws (M4)
MOUNT_D = 4.5
MOUNT_INSET = 8.0

# Button centres (x, y) matching docs/button_box_panel.png layout
BUTTONS = [
    # label, x, y, diameter, hole, colour hint
    ("+ HOME", 35.0, 36.0, BTN_D, BTN_HOLE, "#c0392b"),
    ("- HOME", 35.0, 62.0, BTN_D, BTN_HOLE, "#c0392b"),
    ("+ AWAY", 215.0, 36.0, BTN_D, BTN_HOLE, "#2980b9"),
    ("- AWAY", 215.0, 62.0, BTN_D, BTN_HOLE, "#2980b9"),
    ("GAME +1s", 85.0, 95.0, BTN_D, BTN_HOLE, "#27ae60"),
    ("GAME -1s", 120.0, 95.0, BTN_D, BTN_HOLE, "#f1c40f"),
    ("SHOT +1s", 150.0, 95.0, BTN_D, BTN_HOLE, "#27ae60"),
    ("SHOT -1s", 185.0, 95.0, BTN_D, BTN_HOLE, "#f1c40f"),
    ("TIMEOUT 1:00", 55.0, 125.0, BTN_D, BTN_HOLE, "#e67e22"),
    ("INTERVAL 2:00", 150.0, 125.0, BTN_D, BTN_HOLE, "#1abc9c"),
    ("RETURN", 185.0, 125.0, BTN_D, BTN_HOLE, "#ecf0f1"),
    ("28s RESET", 40.0, 155.0, BTN_D, BTN_HOLE, "#e67e22"),
    ("18s RESET", 72.0, 155.0, BTN_D, BTN_HOLE, "#e67e22"),
    ("EXCL 1", 104.0, 155.0, BTN_D, BTN_HOLE, "#8e44ad"),
    ("EXCL 2", 136.0, 155.0, BTN_D, BTN_HOLE, "#8e44ad"),
    ("FORCE 18", 168.0, 155.0, BTN_D, BTN_HOLE, "#ecf0f1"),
    ("START/STOP", 215.0, 155.0, BTN_D, BTN_HOLE, "#2ecc71"),
]

FRAMES = [
    # (x0,y0,x1,y1, title)
    (18, 22, 52, 76, "HOME"),
    (198, 22, 232, 76, "AWAY"),
    (68, 80, 137, 110, "GAME CLOCK"),
    (133, 80, 202, 110, "SHOT CLOCK"),
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
    lines.append(
        f'  <!-- LCD bezel cutout {LCD_BEZEL_W}×{LCD_BEZEL_H} -->'
    )
    lines.append(
        f'  <rect x="{bx0}" y="{by0}" width="{LCD_BEZEL_W}" height="{LCD_BEZEL_H}"'
        ' fill="#0d3d2e" stroke="#7dcea0" stroke-width="0.4"/>'
    )
    px0, py0, _, _ = pcb
    lines.append(
        f'  <!-- LCD PCB outline {LCD_PCB_W}×{LCD_PCB_H} (behind panel) -->'
    )
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


def write_png():
    scale = 5.0  # px per mm
    margin = 70  # px for dimension callouts
    img_w = int(PANEL_W * scale + 2 * margin)
    img_h = int(PANEL_H * scale + 2 * margin + 40)
    img = Image.new("RGB", (img_w, img_h), "#f4f4f0")
    draw = ImageDraw.Draw(img)

    try:
        font = ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 14)
        font_sm = ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 11)
        font_title = ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 18)
        font_dim = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 12)
    except OSError:
        font = font_sm = font_title = font_dim = ImageFont.load_default()

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
        """Horizontal dimension below/above a feature."""
        y_line = y + offset
        p0 = px(x0, y_line)
        p1 = px(x1, y_line)
        draw.line([p0, p1], fill="#c0392b", width=2)
        # ticks
        for x in (x0, x1):
            a = px(x, y_line - 2)
            b = px(x, y_line + 2)
            draw.line([a, b], fill="#c0392b", width=2)
        mid = ((p0[0] + p1[0]) / 2, p0[1] - 8)
        draw.text(mid, label, font=font_dim, fill="#c0392b", anchor="mb")

    def dim_v(y0, y1, x, label, offset=-10):
        x_line = x + offset
        p0 = px(x_line, y0)
        p1 = px(x_line, y1)
        draw.line([p0, p1], fill="#c0392b", width=2)
        for y in (y0, y1):
            a = px(x_line - 2, y)
            b = px(x_line + 2, y)
            draw.line([a, b], fill="#c0392b", width=2)
        mid = (p0[0] - 6, (p0[1] + p1[1]) / 2)
        draw.text(mid, label, font=font_dim, fill="#c0392b", anchor="rm")

    # Title
    draw.text(
        (img_w / 2, 12),
        "Water Polo Control — 3D-print faceplate measurements (mm)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    # Panel body
    # rounded rect approximation
    rect_mm(0, 0, PANEL_W, PANEL_H, fill="#1c1c1c", outline="#333", width=2)

    # Frames
    for x0, y0, x1, y1, title in FRAMES:
        rect_mm(x0, y0, x1, y1, outline="#ffffff", width=2)
        text_mm((x0 + x1) / 2, y0 - 2.5, title, font_sm, fill="#ffffff", anchor="mb")

    text_mm(PANEL_W / 2, 12, "WATER POLO CONTROL", font_title, fill="#ffffff", anchor="mm")

    bezel, pcb, view, lcd_holes = lcd_rects()
    rect_mm(*pcb, outline="#7dcea0", width=1)
    rect_mm(*bezel, fill="#0d3d2e", outline="#7dcea0", width=2)
    rect_mm(*view, fill="#145a32", outline="#27ae60", width=1)
    text_mm(LCD_CX, LCD_CY - 4, "Freenove I2C LCD 1602", font_sm, fill="#d5f5e3", anchor="mm")
    text_mm(LCD_CX, LCD_CY + 5, f"cutout {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f}", font_sm, fill="#a9dfbf", anchor="mm")
    for hx, hy in lcd_holes:
        circle_mm(hx, hy, LCD_MOUNT_D, outline="#7dcea0", width=1)

    for label, x, y, dia, hole, color in BUTTONS:
        circle_mm(x, y, hole, fill=color, outline="#ffffff", width=2)
        # crosshair
        draw.line([px(x - 2, y), px(x + 2, y)], fill="#111", width=1)
        draw.line([px(x, y - 2), px(x, y + 2)], fill="#111", width=1)
        text_mm(x, y + dia / 2 + 4, label, font_sm, fill="#dddddd", anchor="mt")

    for mx, my in mount_holes():
        circle_mm(mx, my, MOUNT_D, outline="#888", width=2)

    # Overall dimensions
    dim_h(0, PANEL_W, PANEL_H, f"{PANEL_W:.0f} mm", offset=12)
    dim_v(0, PANEL_H, 0, f"{PANEL_H:.0f} mm", offset=-10)

    # Key feature dims
    dim_h(bezel[0], bezel[2], bezel[3], f"LCD {LCD_BEZEL_W:.0f}", offset=6)
    dim_v(bezel[1], bezel[3], bezel[2], f"{LCD_BEZEL_H:.0f}", offset=8)

    # Button pitch examples
    dim_h(40, 72, 155, "32", offset=10)
    dim_v(36, 62, 35, "26", offset=-14)
    dim_h(85, 120, 95, "35", offset=-10)

    # Legend box
    lx, ly = ox, oy + PANEL_H * scale + 28
    legend = [
        f"Panel outer: {PANEL_W:.0f} × {PANEL_H:.0f} × {FACE_THICK:.0f} mm (suggested thickness)",
        f"Buttons: 17 × Ø{BTN_D:.0f} mm panel-mount → drill Ø{BTN_HOLE} mm  |  corner mounts: 4 × Ø{MOUNT_D}",
        f"LCD (Freenove 1602): PCB {LCD_PCB_W:.0f}×{LCD_PCB_H:.0f}  ·  bezel cutout {LCD_BEZEL_W:.0f}×{LCD_BEZEL_H:.0f}  ·  mounts {LCD_MOUNT_DX:.0f}×{LCD_MOUNT_DY:.0f} Ø{LCD_MOUNT_D}",
        "Origin: top-left outer corner  ·  all coordinates are hole centres (mm)  ·  see button_box_panel_measurements.md",
    ]
    for i, line in enumerate(legend):
        draw.text((lx, ly + i * 16), line, font=font_sm, fill="#333")

    img.save(OUT / "button_box_panel_dimensions.png", dpi=(150, 150))


def write_markdown():
    bezel, pcb, view, lcd_holes = lcd_rects()
    rows = []
    for label, x, y, dia, hole, _ in BUTTONS:
        rows.append(f"| {label} | {x:.1f} | {y:.1f} | Ø{hole} |")

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
| Momentary buttons | **Ø16 mm** panel-mount (17 pcs) |
| Button panel holes | **Ø16.2 mm** (0.2 mm clearance) |
| Display | **Freenove I2C IIC LCD 1602** (FNK0079) |
| Faceplate outer | **{PANEL_W:.0f} × {PANEL_H:.0f} mm**, corner R **{CORNER_R:.0f}** |
| Suggested thickness | **{FACE_THICK:.0f} mm** (PLA/PETG); leave ≥18 mm depth behind for LCD + I2C backpack |
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
| GAME +1s to −1s | 35 mm |
| SHOT +1s to −1s | 35 mm |
| Bottom-row buttons | 32 mm |
| INTERVAL to RETURN | 35 mm |

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
    write_png()
    write_markdown()
    print("Wrote:")
    print(" ", OUT / "button_box_panel_dimensions.png")
    print(" ", OUT / "button_box_panel_dimensions.svg")
    print(" ", OUT / "button_box_panel_measurements.md")
