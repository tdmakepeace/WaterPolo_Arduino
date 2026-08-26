#!/usr/bin/env python3
"""Generate HUB75 rainbow-ribbon colour charts matching the working Waveshare Mega wiring.

Pins match the working waterpolo_scoreboard sketch:
  R1..B2 = D24..D29, A..D = A0..A3, CLK = D11, LAT = D9, OE = D10.
  Ribbon: yellow #14 = LAT D9, green #15 = OE D10.
  Grey #8 is GND on this 64x32 panel (no E).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent

# Rainbow IDC colours, brown = pin 1
ROWS = [
    (1, "Brown", "#6D3B1E", "R1", "D24", False),
    (2, "Red", "#C62828", "G1", "D25", False),
    (3, "Orange", "#EF6C00", "B1", "D26", False),
    (4, "Yellow", "#F9A825", "GND", "GND", True),
    (5, "Green", "#2E7D32", "R2", "D27", False),
    (6, "Blue", "#1565C0", "G2", "D28", False),
    (7, "Violet", "#6A1B9A", "B2", "D29", False),
    (8, "Grey", "#9E9E9E", "GND", "GND", True),
    (9, "White", "#F5F5F5", "A", "A0", False),
    (10, "Black", "#212121", "B", "A1", False),
    (11, "Brown", "#6D3B1E", "C", "A2", False),
    (12, "Red", "#C62828", "D", "A3", False),
    (13, "Orange", "#EF6C00", "CLK", "D11", False),
    (14, "Yellow", "#F9A825", "LAT", "D9", False),
    (15, "Green", "#2E7D32", "OE", "D10", False),
    (16, "Blue", "#1565C0", "GND", "GND", True),
]

# Physical HUB75 IDC pairs (odd | even) → Mega
IDC_PAIRS = [
    ("R1", "G1", "D24", "D25"),
    ("B1", "GND", "D26", "GND"),
    ("R2", "G2", "D27", "D28"),
    ("B2", "GND", "D29", "GND"),
    ("A", "B", "A0", "A1"),
    ("C", "D", "A2", "A3"),
    ("CLK", "LAT", "D11", "D9"),
    ("OE", "GND", "D10", "GND"),
]


def _fonts():
    try:
        return (
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 36),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 18),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeuib.ttf", 20),
            ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 22),
            ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 16),
        )
    except OSError:
        d = ImageFont.load_default()
        return d, d, d, d, d


def _chip_text(hex_color: str) -> str:
    r = int(hex_color[1:3], 16)
    g = int(hex_color[3:5], 16)
    b = int(hex_color[5:7], 16)
    return "#111111" if (r * 299 + g * 587 + b * 114) / 1000 > 150 else "#FFFFFF"


def generate_ribbon_chart():
    font_title, font_sub, font_head, font_mono, font_note = _fonts()
    w, h = 1100, 720
    img = Image.new("RGB", (w, h), "#1B2430")
    draw = ImageDraw.Draw(img)

    draw.text((w / 2, 28), "HUB75 colour matrix", font=font_title, fill="#F4F7FB", anchor="mt")
    draw.text(
        (w / 2, 78),
        "Rainbow ribbon (brown = pin 1)  ->  Arduino Mega",
        font=font_sub,
        fill="#B8C4D4",
        anchor="mt",
    )
    draw.text(
        (w / 2, 104),
        "Waveshare P5/P3 64x32  ·  CLK D11  ·  LAT D9  ·  OE D10",
        font=font_note,
        fill="#8FA3BB",
        anchor="mt",
    )

    col_w = 500
    left_x = 40
    right_x = 560
    top = 140
    row_h = 58
    headers = ("Colour", "HUB75", "Mega")

    def draw_column(origin_x, items):
        hx = origin_x
        draw.rounded_rectangle(
            [hx, top, hx + col_w, top + 36 + 8 * row_h],
            radius=10,
            fill="#243044",
            outline="#3E5168",
            width=2,
        )
        draw.rectangle([hx, top, hx + col_w, top + 36], fill="#2E4258")
        xs = [hx + 70, hx + 250, hx + 410]
        for x, label in zip(xs, headers):
            draw.text((x, top + 18), label, font=font_head, fill="#D7E3F2", anchor="mm")

        for i, row in enumerate(items):
            n, name, color, hub, mega, is_gnd = row
            y0 = top + 36 + i * row_h
            y1 = y0 + row_h
            if i % 2:
                draw.rectangle([hx + 2, y0, hx + col_w - 2, y1], fill="#1F2C3D")
            cy = (y0 + y1) / 2
            chip = [hx + 16, y0 + 12, hx + 124, y1 - 12]
            draw.rounded_rectangle(chip, radius=8, fill=color, outline="#0B1118", width=1)
            draw.text(
                ((chip[0] + chip[2]) / 2, cy),
                f"#{n} {name}",
                font=font_note,
                fill=_chip_text(color),
                anchor="mm",
            )
            hub_fill = "#9FB3C8" if is_gnd else "#F4F7FB"
            mega_fill = "#9FB3C8" if is_gnd else "#7DFFB3"
            draw.text((hx + 250, cy), hub, font=font_mono, fill=hub_fill, anchor="mm")
            draw.text((hx + 410, cy), mega, font=font_mono, fill=mega_fill, anchor="mm")

    draw_column(left_x, ROWS[:8])
    draw_column(right_x, ROWS[8:])

    draw.text(
        (w / 2, h - 48),
        "Align brown edge with HUB75 pin-1 mark.   Grey ribbon: red stripe = pin 1.",
        font=font_note,
        fill="#B8C4D4",
        anchor="mm",
    )
    draw.text(
        (w / 2, h - 24),
        "Yellow #14 = LAT D9    Green #15 = OE D10    Grey #8 = GND",
        font=font_note,
        fill="#8FA3BB",
        anchor="mm",
    )

    path = OUT / "hub75_ribbon_colour_matrix.png"
    img.save(path, dpi=(150, 150))
    print(f"wrote {path}")


def generate_idc_pairs():
    font_title, font_sub, font_head, font_mono, font_note = _fonts()
    w, h = 920, 620
    img = Image.new("RGB", (w, h), "#1B2430")
    draw = ImageDraw.Draw(img)
    draw.text((w / 2, 24), "HUB75 IDC pairs  ->  Arduino Mega", font=font_title, fill="#F4F7FB", anchor="mt")
    draw.text(
        (w / 2, 72),
        "Waveshare 64x32  ·  LAT on D9  ·  OE on D10  ·  CLK on D11",
        font=font_sub,
        fill="#B8C4D4",
        anchor="mt",
    )

    table_x, table_y = 60, 120
    table_w, row_h = 800, 48
    col_w = table_w / 4
    headers = ("HUB75", "HUB75", "Mega", "Mega")

    draw.rounded_rectangle(
        [table_x, table_y, table_x + table_w, table_y + 40 + 8 * row_h],
        radius=10,
        fill="#243044",
        outline="#3E5168",
        width=2,
    )
    draw.rectangle([table_x, table_y, table_x + table_w, table_y + 40], fill="#2E4258")
    for i, label in enumerate(headers):
        draw.text(
            (table_x + col_w * i + col_w / 2, table_y + 20),
            label,
            font=font_head,
            fill="#D7E3F2",
            anchor="mm",
        )

    for r, (a, b, ma, mb) in enumerate(IDC_PAIRS):
        y0 = table_y + 40 + r * row_h
        y1 = y0 + row_h
        if r % 2:
            draw.rectangle([table_x + 2, y0, table_x + table_w - 2, y1], fill="#1F2C3D")
        cy = (y0 + y1) / 2
        vals = (a, b, ma, mb)
        for i, val in enumerate(vals):
            fill = "#9FB3C8" if val == "GND" else ("#7DFFB3" if i >= 2 else "#F4F7FB")
            draw.text(
                (table_x + col_w * i + col_w / 2, cy),
                val,
                font=font_mono,
                fill=fill,
                anchor="mm",
            )

    draw.text(
        (w / 2, h - 36),
        "Data D24–D29 are fixed (PORTA). Leave D22 and D23 free.",
        font=font_note,
        fill="#B8C4D4",
        anchor="mm",
    )

    path = OUT / "rgb_matrix_mega_wiring.png"
    img.save(path, dpi=(150, 150))
    print(f"wrote {path}")


if __name__ == "__main__":
    generate_ribbon_chart()
    generate_idc_pairs()
