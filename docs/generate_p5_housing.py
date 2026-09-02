#!/usr/bin/env python3
"""Generate housing drawings for Waveshare RGB-Matrix-P5-64x32 (mm)."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent

# ----- Waveshare RGB-Matrix-P5-64x32 (SKU 25848) -----
PANEL_W, PANEL_H = 320.0, 160.0
PANEL_THICK = 16.0  # typical LED mask + PCB (measure yours)
CONNECTOR_PROTRUDE = 12.0  # HUB75 sockets / ICs behind PCB

# ----- Housing (6 mm plywood / acrylic sides, 3 mm rear) -----
WALL = 6.0
REAR_THICK = 3.0
CABLE_CAVITY = 28.0  # service loop for ribbon + VH4 pigtail
INNER_D = PANEL_THICK + CONNECTOR_PROTRUDE + CABLE_CAVITY  # 56
SIDE_D = INNER_D  # internal depth to inside face of rear panel
OVERALL_W = PANEL_W + 2 * WALL  # 332
OVERALL_H = PANEL_H + 2 * WALL  # 172
OVERALL_D = SIDE_D + REAR_THICK  # 59

# Rear-panel openings (origin = outer top-left, Y down)
RIBBON_W, RIBBON_H = 28.0, 10.0
RIBBON_X, RIBBON_Y = 122.0, 128.0  # slot top-left
POWER_D = 12.0
POWER_CX, POWER_CY = 186.0, 133.0
MOUNT_D = 4.5
MOUNT_INSET = 8.0
VENT_D = 8.0
KEYHOLE_CX = (80.0, 252.0)
KEYHOLE_CY = 22.0
KEYHOLE_HEAD = 8.0
KEYHOLE_NECK = 4.5
KEYHOLE_DROP = 10.0


def _fonts():
    try:
        return (
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 14),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 12),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 18),
            ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 13),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 22),
        )
    except OSError:
        d = ImageFont.load_default()
        return d, d, d, d, d


def mount_holes():
    i = MOUNT_INSET
    return [
        (i, i),
        (OVERALL_W - i, i),
        (i, OVERALL_H - i),
        (OVERALL_W - i, OVERALL_H - i),
    ]


def write_plan_png():
    """Front / plan view of the box around the 320×160 panel."""
    scale = 3.2
    margin = 80
    font, font_sm, font_title, font_dim, _ = _fonts()
    img_w = int(OVERALL_W * scale + 2 * margin)
    img_h = int(OVERALL_H * scale + 2 * margin + 40)
    img = Image.new("RGB", (img_w, img_h), "#f4f4f0")
    draw = ImageDraw.Draw(img)
    ox, oy = margin, margin + 16

    def px(x, y=None):
        if y is None:
            return ox + x * scale
        return ox + x * scale, oy + y * scale

    def dim_h(x0, x1, y, label, offset=10):
        y_line = y + offset
        p0 = px(x0, y_line)
        p1 = px(x1, y_line)
        draw.line([p0, p1], fill="#c0392b", width=2)
        for x in (x0, x1):
            draw.line([px(x, y_line - 2), px(x, y_line + 2)], fill="#c0392b", width=2)
        mid = ((p0[0] + p1[0]) / 2, p0[1] - 6)
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
        (img_w / 2, 10),
        "P5 64×32 housing — front / plan (mm)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    # Outer box
    draw.rectangle(
        [*px(0, 0), *px(OVERALL_W, OVERALL_H)],
        fill="#c8b896",
        outline="#5d4c32",
        width=2,
    )
    # Inner pocket = panel
    draw.rectangle(
        [*px(WALL, WALL), *px(WALL + PANEL_W, WALL + PANEL_H)],
        fill="#111111",
        outline="#222",
        width=2,
    )
    # Pixel hint grid (every 8 LEDs)
    for i in range(1, 64, 8):
        x = WALL + i * 5.0
        draw.line([px(x, WALL), px(x, WALL + PANEL_H)], fill="#2a2a2a", width=1)
    for j in range(1, 32, 8):
        y = WALL + j * 5.0
        draw.line([px(WALL, y), px(WALL + PANEL_W, y)], fill="#2a2a2a", width=1)

    cx, cy = WALL + PANEL_W / 2, WALL + PANEL_H / 2
    draw.text(px(cx, cy - 10), "Waveshare RGB-Matrix-P5-64x32", font=font, fill="#ecf0f1", anchor="mm")
    draw.text(px(cx, cy + 8), "320 × 160 mm  ·  64×32  ·  5 mm pitch", font=font_sm, fill="#bdc3c7", anchor="mm")
    draw.text(px(cx, cy + 24), "LED face flush with front of walls", font=font_sm, fill="#95a5a6", anchor="mm")

    draw.text(px(WALL / 2, OVERALL_H / 2), "6", font=font_sm, fill="#3e2723", anchor="mm")
    draw.text(px(OVERALL_W - WALL / 2, OVERALL_H / 2), "6", font=font_sm, fill="#3e2723", anchor="mm")

    dim_h(0, OVERALL_W, OVERALL_H, f"{OVERALL_W:.0f} outer", offset=12)
    dim_h(WALL, WALL + PANEL_W, 0, f"{PANEL_W:.0f} panel", offset=-14)
    dim_v(0, OVERALL_H, 0, f"{OVERALL_H:.0f}", offset=-12)
    dim_v(WALL, WALL + PANEL_H, OVERALL_W, f"{PANEL_H:.0f}", offset=12)

    legend = [
        f"Outer: {OVERALL_W:.0f} × {OVERALL_H:.0f} × {OVERALL_D:.0f} mm  ·  walls {WALL:.0f} mm  ·  rear {REAR_THICK:.0f} mm",
        "Front is open — do not add a bezel over the LEDs (that would hide edge pixels).",
        "Panel drops in from the rear; walls sit beside the 320×160 outline.",
    ]
    ly = oy + OVERALL_H * scale + 28
    for i, line in enumerate(legend):
        draw.text((ox, ly + i * 16), line, font=font_sm, fill="#333")

    img.save(OUT / "rgb_matrix_p5_housing_plan.png", dpi=(150, 150))


def write_section_png():
    """Side section: panel, connectors, cable cavity, rear exits."""
    scale = 4.4
    margin_l, margin_r = 120, 220
    margin_t, margin_b = 50, 70
    font, font_sm, font_title, font_dim, _ = _fonts()

    # Draw a slice through the 172 mm height, showing 60 mm depth
    box_h = OVERALL_H
    box_d = OVERALL_D
    img_w = int(box_d * scale + margin_l + margin_r)
    img_h = int(box_h * scale + margin_t + margin_b)
    img = Image.new("RGB", (img_w, img_h), "#f4f4f0")
    draw = ImageDraw.Draw(img)
    ox, oy = margin_l, margin_t

    def px(x, y=None):
        """x = depth from front, y = height from top."""
        if y is None:
            return ox + x * scale
        return ox + x * scale, oy + y * scale

    draw.text(
        (img_w / 2, 12),
        "P5 64×32 housing — side section (mm)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    # Walls (top and bottom rails in section)
    draw.rectangle([*px(0, 0), *px(SIDE_D, WALL)], fill="#c8b896", outline="#5d4c32", width=2)
    draw.rectangle(
        [*px(0, OVERALL_H - WALL), *px(SIDE_D, OVERALL_H)],
        fill="#c8b896",
        outline="#5d4c32",
        width=2,
    )
    # Rear panel
    draw.rectangle(
        [*px(SIDE_D, 0), *px(OVERALL_D, OVERALL_H)],
        fill="#d7ccc8",
        outline="#5d4c32",
        width=2,
    )

    # Panel body (LED + PCB)
    y0, y1 = WALL, WALL + PANEL_H
    draw.rectangle([*px(0, y0), *px(PANEL_THICK, y1)], fill="#111", outline="#000", width=2)
    # Connector / IC zone
    draw.rectangle(
        [*px(PANEL_THICK, y0 + 20), *px(PANEL_THICK + CONNECTOR_PROTRUDE, y1 - 20)],
        fill="#1a5276",
        outline="#154360",
        width=1,
    )
    # Cable cavity hatch
    cav_x0 = PANEL_THICK + CONNECTOR_PROTRUDE
    draw.rectangle(
        [*px(cav_x0, y0), *px(SIDE_D, y1)],
        fill="#fdebd0",
        outline="#d35400",
        width=1,
    )
    # Ribbon fold sketch
    mid_y = OVERALL_H / 2
    draw.arc(
        [px(cav_x0 + 4, mid_y - 18), px(SIDE_D - 4, mid_y + 18)],
        start=270,
        end=90,
        fill="#8e44ad",
        width=3,
    )
    # Power pigtail
    draw.line(
        [px(PANEL_THICK + 4, mid_y + 28), px(SIDE_D, mid_y + 36)],
        fill="#c0392b",
        width=3,
    )
    draw.line(
        [px(PANEL_THICK + 4, mid_y + 34), px(SIDE_D, mid_y + 42)],
        fill="#111",
        width=3,
    )

    # Exit arrows through rear
    draw.polygon(
        [
            px(OVERALL_D + 2, mid_y - 8),
            px(OVERALL_D + 18, mid_y),
            px(OVERALL_D + 2, mid_y + 8),
        ],
        fill="#8e44ad",
    )
    draw.polygon(
        [
            px(OVERALL_D + 2, mid_y + 30),
            px(OVERALL_D + 18, mid_y + 38),
            px(OVERALL_D + 2, mid_y + 46),
        ],
        fill="#c0392b",
    )

    draw.text(px(PANEL_THICK / 2, OVERALL_H / 2), "P5", font=font_sm, fill="#ecf0f1", anchor="mm")
    draw.text(
        px(PANEL_THICK + CONNECTOR_PROTRUDE / 2, OVERALL_H / 2),
        "HUB75",
        font=font_sm,
        fill="#d4e6f1",
        anchor="mm",
    )
    draw.text(px(cav_x0 + CABLE_CAVITY / 2, WALL + 16), "cable cavity", font=font_sm, fill="#6e2c00", anchor="mm")
    draw.text(px(cav_x0 + CABLE_CAVITY / 2, WALL + 32), f"{CABLE_CAVITY:.0f} mm", font=font_dim, fill="#d35400", anchor="mm")

    # Depth dimension along bottom
    def dim_depth(x0, x1, y, label, color="#c0392b"):
        y_line = y
        p0 = px(x0, y_line)
        p1 = px(x1, y_line)
        draw.line([p0, p1], fill=color, width=2)
        for x in (x0, x1):
            draw.line([px(x, y_line - 2), px(x, y_line + 2)], fill=color, width=2)
        mid = ((p0[0] + p1[0]) / 2, p0[1] + 4)
        draw.text(mid, label, font=font_dim, fill=color, anchor="mt")

    dim_depth(0, PANEL_THICK, OVERALL_H + 8, f"{PANEL_THICK:.0f}")
    dim_depth(PANEL_THICK, PANEL_THICK + CONNECTOR_PROTRUDE, OVERALL_H + 8, f"{CONNECTOR_PROTRUDE:.0f}")
    dim_depth(PANEL_THICK + CONNECTOR_PROTRUDE, SIDE_D, OVERALL_H + 8, f"{CABLE_CAVITY:.0f}")
    dim_depth(SIDE_D, OVERALL_D, OVERALL_H + 8, f"{REAR_THICK:.0f}")

    # Overall depth on top
    p0 = px(0, -8)
    p1 = px(OVERALL_D, -8)
    draw.line([p0, p1], fill="#1a5276", width=2)
    draw.text(
        ((p0[0] + p1[0]) / 2, p0[1] - 4),
        f"overall depth {OVERALL_D:.0f} mm",
        font=font_dim,
        fill="#1a5276",
        anchor="mb",
    )

    # Height
    p0 = px(-10, 0)
    p1 = px(-10, OVERALL_H)
    draw.line([p0, p1], fill="#c0392b", width=2)
    draw.text((p0[0] - 6, (p0[1] + p1[1]) / 2), f"{OVERALL_H:.0f}", font=font_dim, fill="#c0392b", anchor="rm")

    # Callouts
    draw.text(px(OVERALL_D + 22, mid_y), "HUB75 ribbon", font=font_sm, fill="#8e44ad", anchor="lm")
    draw.text(px(OVERALL_D + 22, mid_y + 16), "(rear slot)", font=font_sm, fill="#8e44ad", anchor="lm")
    draw.text(px(OVERALL_D + 22, mid_y + 38), "VH4 power", font=font_sm, fill="#c0392b", anchor="lm")
    draw.text(px(OVERALL_D + 22, mid_y + 54), "(rear grommet)", font=font_sm, fill="#c0392b", anchor="lm")

    draw.text(px(2, WALL / 2), "front", font=font_sm, fill="#3e2723", anchor="lm")
    draw.text(px(SIDE_D - 2, WALL / 2), "rear", font=font_sm, fill="#3e2723", anchor="rm")

    img.save(OUT / "rgb_matrix_p5_housing_section.png", dpi=(150, 150))


def write_rear_png():
    """Dimensioned rear panel with ribbon slot + power grommet."""
    scale = 3.4
    margin = 80
    font, font_sm, font_title, font_dim, _ = _fonts()
    img_w = int(OVERALL_W * scale + 2 * margin)
    img_h = int(OVERALL_H * scale + 2 * margin + 50)
    img = Image.new("RGB", (img_w, img_h), "#f4f4f0")
    draw = ImageDraw.Draw(img)
    ox, oy = margin, margin + 16

    def px(x, y=None):
        if y is None:
            return ox + x * scale
        return ox + x * scale, oy + y * scale

    def dim_h(x0, x1, y, label, offset=10):
        y_line = y + offset
        p0 = px(x0, y_line)
        p1 = px(x1, y_line)
        draw.line([p0, p1], fill="#c0392b", width=2)
        for x in (x0, x1):
            draw.line([px(x, y_line - 2), px(x, y_line + 2)], fill="#c0392b", width=2)
        mid = ((p0[0] + p1[0]) / 2, p0[1] - 6)
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
        (img_w / 2, 10),
        "P5 64×32 housing — rear panel (outside view, mm)",
        font=font_title,
        fill="#111",
        anchor="mt",
    )

    draw.rectangle(
        [*px(0, 0), *px(OVERALL_W, OVERALL_H)],
        fill="#d7ccc8",
        outline="#5d4c32",
        width=2,
    )
    # Inner wall ghost
    draw.rectangle(
        [*px(WALL, WALL), *px(OVERALL_W - WALL, OVERALL_H - WALL)],
        outline="#a1887f",
        width=1,
    )

    # Ribbon slot
    draw.rounded_rectangle(
        [*px(RIBBON_X, RIBBON_Y), *px(RIBBON_X + RIBBON_W, RIBBON_Y + RIBBON_H)],
        radius=3 * scale / 3,
        fill="#4a235a",
        outline="#8e44ad",
        width=2,
    )
    draw.text(
        px(RIBBON_X + RIBBON_W / 2, RIBBON_Y - 6),
        "HUB75 ribbon",
        font=font_sm,
        fill="#6c3483",
        anchor="mb",
    )
    draw.text(
        px(RIBBON_X + RIBBON_W / 2, RIBBON_Y + RIBBON_H / 2),
        f"{RIBBON_W:.0f}×{RIBBON_H:.0f}",
        font=font_dim,
        fill="#f5eef8",
        anchor="mm",
    )

    # Power grommet
    r = POWER_D / 2
    draw.ellipse(
        [*px(POWER_CX - r, POWER_CY - r), *px(POWER_CX + r, POWER_CY + r)],
        fill="#7b241c",
        outline="#c0392b",
        width=2,
    )
    draw.text(px(POWER_CX, POWER_CY - r - 6), "VH4 power", font=font_sm, fill="#922b21", anchor="mb")
    draw.text(px(POWER_CX, POWER_CY), f"Ø{POWER_D:.0f}", font=font_dim, fill="#fadbd8", anchor="mm")

    # Vents
    for vx in (40.0, OVERALL_W - 40.0):
        vr = VENT_D / 2
        vy = OVERALL_H / 2
        draw.ellipse(
            [*px(vx - vr, vy - vr), *px(vx + vr, vy + vr)],
            outline="#7f8c8d",
            width=2,
        )
        draw.text(px(vx, vy + vr + 8), f"vent Ø{VENT_D:.0f}", font=font_sm, fill="#7f8c8d", anchor="mt")

    # Keyholes
    for kx in KEYHOLE_CX:
        kr = KEYHOLE_HEAD / 2
        draw.ellipse(
            [*px(kx - kr, KEYHOLE_CY - kr), *px(kx + kr, KEYHOLE_CY + kr)],
            outline="#1a5276",
            width=2,
        )
        neck_y = KEYHOLE_CY + KEYHOLE_DROP
        nr = KEYHOLE_NECK / 2
        draw.line([px(kx, KEYHOLE_CY), px(kx, neck_y)], fill="#1a5276", width=int(KEYHOLE_NECK * scale * 0.35))
        draw.ellipse(
            [*px(kx - nr, neck_y - nr), *px(kx + nr, neck_y + nr)],
            fill="#d7ccc8",
            outline="#1a5276",
            width=2,
        )
    draw.text(px(OVERALL_W / 2, 8), "wall keyholes (optional)", font=font_sm, fill="#1a5276", anchor="mm")

    for mx, my in mount_holes():
        mr = MOUNT_D / 2
        draw.ellipse(
            [*px(mx - mr, my - mr), *px(mx + mr, my + mr)],
            outline="#5d4c32",
            width=2,
        )

    dim_h(0, OVERALL_W, OVERALL_H, f"{OVERALL_W:.0f}", offset=12)
    dim_v(0, OVERALL_H, 0, f"{OVERALL_H:.0f}", offset=-12)
    dim_h(RIBBON_X, RIBBON_X + RIBBON_W, RIBBON_Y + RIBBON_H, f"{RIBBON_W:.0f}", offset=8)
    dim_h(0, RIBBON_X, RIBBON_Y, f"{RIBBON_X:.0f}", offset=-18)
    dim_h(0, POWER_CX, POWER_CY + POWER_D / 2, f"{POWER_CX:.0f}", offset=22)

    legend = [
        "Origin: outer top-left of rear panel  ·  view from behind the scoreboard",
        f"Ribbon slot {RIBBON_W:.0f}×{RIBBON_H:.0f} at ({RIBBON_X:.0f}, {RIBBON_Y:.0f})  ·  power Ø{POWER_D:.0f} at ({POWER_CX:.0f}, {POWER_CY:.0f})",
        f"4× Ø{MOUNT_D} cover screws, {MOUNT_INSET:.0f} mm inset  ·  cables exit downward when wall-hung",
        "1:1 vector: rgb_matrix_p5_housing_rear.svg",
    ]
    ly = oy + OVERALL_H * scale + 30
    for i, line in enumerate(legend):
        draw.text((ox, ly + i * 15), line, font=font_sm, fill="#333")

    img.save(OUT / "rgb_matrix_p5_housing_rear.png", dpi=(150, 150))


def write_rear_svg():
    """1:1 mm SVG of the rear panel for laser / CNC."""
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{OVERALL_W}mm" height="{OVERALL_H}mm"',
        f'  viewBox="0 0 {OVERALL_W} {OVERALL_H}">',
        "  <!-- P5 64x32 housing rear panel — units: millimetres -->",
        f'  <rect x="0" y="0" width="{OVERALL_W}" height="{OVERALL_H}"',
        '    fill="#d7ccc8" stroke="#000000" stroke-width="0.4"/>',
        f'  <rect x="{WALL}" y="{WALL}" width="{PANEL_W}" height="{PANEL_H}"',
        '    fill="none" stroke="#888888" stroke-width="0.2" stroke-dasharray="2 1"/>',
        f'  <!-- HUB75 ribbon slot {RIBBON_W}x{RIBBON_H} -->',
        f'  <rect x="{RIBBON_X}" y="{RIBBON_Y}" width="{RIBBON_W}" height="{RIBBON_H}" rx="2" ry="2"',
        '    fill="none" stroke="#000000" stroke-width="0.35"/>',
        f'  <!-- VH4 power grommet Ø{POWER_D} -->',
        f'  <circle cx="{POWER_CX}" cy="{POWER_CY}" r="{POWER_D / 2}"',
        '    fill="none" stroke="#000000" stroke-width="0.35"/>',
    ]
    for vx in (40.0, OVERALL_W - 40.0):
        lines.append(
            f'  <circle cx="{vx}" cy="{OVERALL_H / 2}" r="{VENT_D / 2}"'
            ' fill="none" stroke="#000000" stroke-width="0.3"/>'
        )
    hr = KEYHOLE_HEAD / 2
    nr = KEYHOLE_NECK / 2
    for kx in KEYHOLE_CX:
        ky0 = KEYHOLE_CY
        ky1 = KEYHOLE_CY + KEYHOLE_DROP
        lines.append(
            f'  <circle cx="{kx}" cy="{ky0}" r="{hr}"'
            ' fill="none" stroke="#000000" stroke-width="0.3"/>'
        )
        lines.append(
            f'  <circle cx="{kx}" cy="{ky1}" r="{nr}"'
            ' fill="none" stroke="#000000" stroke-width="0.3"/>'
        )
        lines.append(
            f'  <rect x="{kx - nr}" y="{ky0}" width="{KEYHOLE_NECK}" height="{KEYHOLE_DROP}"'
            ' fill="none" stroke="#000000" stroke-width="0.3"/>'
        )
    for mx, my in mount_holes():
        lines.append(
            f'  <circle cx="{mx}" cy="{my}" r="{MOUNT_D / 2}"'
            ' fill="none" stroke="#000000" stroke-width="0.35"/>'
        )
    lines.append("</svg>")
    (OUT / "rgb_matrix_p5_housing_rear.svg").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    write_plan_png()
    write_section_png()
    write_rear_png()
    write_rear_svg()
    print("Wrote:")
    for name in (
        "rgb_matrix_p5_housing_plan.png",
        "rgb_matrix_p5_housing_section.png",
        "rgb_matrix_p5_housing_rear.png",
        "rgb_matrix_p5_housing_rear.svg",
    ):
        print(" ", OUT / name)
