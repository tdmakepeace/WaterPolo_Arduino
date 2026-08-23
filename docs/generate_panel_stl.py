#!/usr/bin/env python3
"""Build a printable STL faceplate from button_box_panel measurements (mm)."""

from __future__ import annotations

from pathlib import Path

import mapbox_earcut as earcut
import numpy as np
from shapely.geometry import Point, box
from shapely.ops import unary_union
from stl import mesh

OUT = Path(__file__).resolve().parent

# ----- Design (mm) — matches generate_panel_dimensions.py -----
PANEL_W, PANEL_H = 250.0, 180.0
CORNER_R = 8.0
FACE_THICK = 3.0

LCD_BEZEL_W, LCD_BEZEL_H = 72.0, 25.0
LCD_MOUNT_DX, LCD_MOUNT_DY = 75.0, 31.0
LCD_MOUNT_D = 3.2
LCD_CX, LCD_CY = 125.0, 48.0

BTN_HOLE = 16.2
MOUNT_D = 4.5
MOUNT_INSET = 8.0
CIRCLE_SEGMENTS = 48

BUTTON_CENTRES = [
    # HOME / AWAY
    (35.0, 36.0),
    (35.0, 62.0),
    (215.0, 36.0),
    (215.0, 62.0),
    # 3×4 grid (matches generate_panel_dimensions.py)
    (55.0, 95.0),
    (105.0, 95.0),
    (155.0, 95.0),
    (205.0, 95.0),
    (55.0, 125.0),
    (105.0, 125.0),
    (155.0, 125.0),
    (205.0, 125.0),
    (55.0, 155.0),
    (105.0, 155.0),
    (155.0, 155.0),
    (205.0, 155.0),
]


def rounded_rect(width: float, height: float, radius: float):
    r = min(radius, width / 2, height / 2)
    if r <= 0:
        return box(0, 0, width, height)
    return unary_union(
        [
            box(r, 0, width - r, height),
            box(0, r, width, height - r),
            Point(r, r).buffer(r),
            Point(width - r, r).buffer(r),
            Point(r, height - r).buffer(r),
            Point(width - r, height - r).buffer(r),
        ]
    )


def panel_polygon():
    # Convert UI coords (Y down from top) to CAD (Y up from bottom)
    def ui_to_cad(x: float, y: float) -> tuple[float, float]:
        return x, PANEL_H - y

    solid = rounded_rect(PANEL_W, PANEL_H, CORNER_R)

    holes = []
    for cx, cy in BUTTON_CENTRES:
        x, y = ui_to_cad(cx, cy)
        holes.append(Point(x, y).buffer(BTN_HOLE / 2, resolution=CIRCLE_SEGMENTS // 4))

    bx0 = LCD_CX - LCD_BEZEL_W / 2
    by0 = LCD_CY - LCD_BEZEL_H / 2
    # LCD rectangle in UI → CAD
    x0, y_top = ui_to_cad(bx0, by0)
    x1, y_bot = ui_to_cad(bx0 + LCD_BEZEL_W, by0 + LCD_BEZEL_H)
    holes.append(box(min(x0, x1), min(y_bot, y_top), max(x0, x1), max(y_bot, y_top)))

    for sx, sy in (
        (LCD_CX - LCD_MOUNT_DX / 2, LCD_CY - LCD_MOUNT_DY / 2),
        (LCD_CX + LCD_MOUNT_DX / 2, LCD_CY - LCD_MOUNT_DY / 2),
        (LCD_CX - LCD_MOUNT_DX / 2, LCD_CY + LCD_MOUNT_DY / 2),
        (LCD_CX + LCD_MOUNT_DX / 2, LCD_CY + LCD_MOUNT_DY / 2),
    ):
        x, y = ui_to_cad(sx, sy)
        holes.append(Point(x, y).buffer(LCD_MOUNT_D / 2, resolution=12))

    for mx, my in (
        (MOUNT_INSET, MOUNT_INSET),
        (PANEL_W - MOUNT_INSET, MOUNT_INSET),
        (MOUNT_INSET, PANEL_H - MOUNT_INSET),
        (PANEL_W - MOUNT_INSET, PANEL_H - MOUNT_INSET),
    ):
        x, y = ui_to_cad(mx, my)
        holes.append(Point(x, y).buffer(MOUNT_D / 2, resolution=12))

    cut = unary_union(holes)
    return solid.difference(cut)


def _ring_coords(ring) -> np.ndarray:
    coords = np.asarray(ring.coords[:-1], dtype=np.float64)
    return coords


def triangulate_polygon(poly) -> tuple[np.ndarray, np.ndarray]:
    """Return (vertices Nx2, faces Mx3) for a polygon with holes."""
    if poly.geom_type == "MultiPolygon":
        # take largest piece if boolean fragments
        poly = max(poly.geoms, key=lambda g: g.area)

    exterior = _ring_coords(poly.exterior)
    interiors = [_ring_coords(r) for r in poly.interiors]
    rings = [exterior, *interiors]
    verts = np.vstack(rings)
    ring_ends = np.cumsum([len(r) for r in rings], dtype=np.uint32)
    indices = earcut.triangulate_float64(verts, ring_ends)
    faces = np.asarray(indices, dtype=np.int64).reshape(-1, 3)
    return verts, faces


def extrude_to_triangles(poly, thickness: float) -> np.ndarray:
    """Return Nx3x3 triangle array for a solid extrusion."""
    verts2d, faces = triangulate_polygon(poly)
    n = len(verts2d)
    bottom = np.column_stack([verts2d, np.zeros(n)])
    top = np.column_stack([verts2d, np.full(n, thickness)])

    tris = []

    # Bottom face (CW when viewed from +Z so outward normal is -Z)
    for a, b, c in faces:
        tris.append([bottom[a], bottom[c], bottom[b]])
    # Top face (CCW from +Z)
    for a, b, c in faces:
        tris.append([top[a], top[b], top[c]])

    def wall_ring(ring_coords: np.ndarray, outward: bool):
        m = len(ring_coords)
        for i in range(m):
            j = (i + 1) % m
            p0 = np.array([ring_coords[i][0], ring_coords[i][1], 0.0])
            p1 = np.array([ring_coords[j][0], ring_coords[j][1], 0.0])
            p2 = np.array([ring_coords[j][0], ring_coords[j][1], thickness])
            p3 = np.array([ring_coords[i][0], ring_coords[i][1], thickness])
            if outward:
                tris.append([p0, p1, p2])
                tris.append([p0, p2, p3])
            else:
                # hole wall: normals point into hole (outward from solid material)
                tris.append([p0, p2, p1])
                tris.append([p0, p3, p2])

    if poly.geom_type == "MultiPolygon":
        poly = max(poly.geoms, key=lambda g: g.area)

    wall_ring(_ring_coords(poly.exterior), outward=True)
    for hole in poly.interiors:
        wall_ring(_ring_coords(hole), outward=False)

    return np.asarray(tris, dtype=np.float64)


def write_stl(path: Path, triangles: np.ndarray) -> None:
    data = np.zeros(len(triangles), dtype=mesh.Mesh.dtype)
    stl_mesh = mesh.Mesh(data)
    stl_mesh.vectors = triangles
    stl_mesh.update_normals()
    stl_mesh.save(path)


def write_obj(path: Path, triangles: np.ndarray) -> None:
    # Deduplicate vertices for a cleaner OBJ
    flat = triangles.reshape(-1, 3)
    # Round to avoid float dup noise
    keys = np.round(flat, 5)
    uniq, inv = np.unique(keys, axis=0, return_inverse=True)
    faces = inv.reshape(-1, 3) + 1
    with path.open("w", encoding="utf-8") as f:
        f.write("# Water Polo Control faceplate (mm)\n")
        f.write(f"# {PANEL_W} x {PANEL_H} x {FACE_THICK}\n")
        for v in uniq:
            f.write(f"v {v[0]:.5f} {v[1]:.5f} {v[2]:.5f}\n")
        for a, b, c in faces:
            f.write(f"f {a} {b} {c}\n")


def main():
    poly = panel_polygon()
    if poly.is_empty:
        raise SystemExit("Panel boolean failed — empty geometry")
    tris = extrude_to_triangles(poly, FACE_THICK)
    stl_path = OUT / "button_box_panel_faceplate.stl"
    obj_path = OUT / "button_box_panel_faceplate.obj"
    write_stl(stl_path, tris)
    write_obj(obj_path, tris)

    # Sanity checks
    volume_mm3 = poly.area * FACE_THICK
    print(f"Wrote {stl_path}")
    print(f"Wrote {obj_path}")
    print(f"Triangles: {len(tris)}")
    print(f"Approx solid volume: {volume_mm3:.0f} mm³ ({volume_mm3 / 1000:.1f} cm³)")
    print(f"Panel: {PANEL_W}×{PANEL_H}×{FACE_THICK} mm")
    print(f"Button holes: {len(BUTTON_CENTRES)} × Ø{BTN_HOLE}")
    print(f"LCD cutout: {LCD_BEZEL_W}×{LCD_BEZEL_H} mm")


if __name__ == "__main__":
    main()
