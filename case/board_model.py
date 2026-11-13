#!/usr/bin/env python3
"""3D reference model of the Seengreat RGB Matrix HUB75 S3 board, for designing the case around.

    python3 board_model.py      -> stl/seengreat_board.stl, board_spec.json, board_drawing.png, board_3d.png

Coordinates (mm): X right / Y up as seen from the TOP (component side, speaker side), origin at the
bottom-left corner of the PCB. Z up: PCB bottom surface at z=0, top surface at z=PCB_T.

Where the numbers came from:
  "photo"   measured from Seengreat's dimensioned photo (65 x 57.5 mm), ~ +/-0.5 mm
  "std"     standard part dimensions (USB-C, WROOM-1 module, 2.54 mm headers, ...)
  "EST"     estimated height - MEASURE WITH CALIPERS when the board arrives, then edit below
"""
import json
import os

import numpy as np
import trimesh
from manifold3d import Manifold, OpType

PCB_W, PCB_H, PCB_T = 65.0, 57.5, 1.6        # photo / photo / std (1.6 mm FR4)
HOLE_D = 2.7                                  # M2.5 clearance (photo: ~2.6)
HOLES = [(2.5, 2.5), (62.5, 2.5), (2.5, 55.0), (62.5, 55.0)]   # photo: (2.2,2.5) (62.5,2.6) (2.2,55.1) (62.3,55.3)

# name, side, x0, y0, x1, y1, height above that PCB face, color, source, note
# side "top" = speaker side (faces the back cover in our case), "bot" = HUB75 socket side (faces the panel)
PARTS = [
    ("speaker",            "top", 14.7, 4.9, 52.1, 30.8, 6.0, "#222222", "photo+EST", "40x28-class oval speaker on foam; height EST"),
    ("esp32_module",       "top", 11.2, 32.0, 29.2, 57.5, 3.2, "#b8b8b8", "photo+std", "ESP32-S3-WROOM-1 (18 x 25.5 x 3.1)"),
    ("microsd_socket",     "top", 33.0, 43.1, 48.1, 57.5, 2.0, "#c0c0c0", "photo+std", "card enters from the TOP edge (y=57.5)"),
    ("hub75_box_header",   "top", 56.5, 23.2, 65.3, 52.4, 9.0, "#111111", "photo+EST", "right-angle 2x8 shrouded header, ribbon enters from the RIGHT (+X)"),
    ("hub75_header_leads", "top", 52.0, 23.2, 56.5, 52.4, 3.0, "#d4af37", "photo+EST", "bent pins"),
    ("vh4p_panel_power",   "top", 54.7, 4.9, 65.7, 22.4, 9.5, "#f2f2f2", "photo+EST", "VH 3.96 4-pin, panel power lead plugs from the RIGHT (+X)"),
    ("usb_c_board",        "top", -1.7, 17.5, 5.6, 26.8, 3.2, "#9a9a9a", "photo+std", "USB-C: power + flashing (USE THIS ONE). Overhangs left edge ~1.7"),
    ("usb_c_power",        "top", -1.7, 5.6, 5.6, 14.8, 3.2, "#9a9a9a", "photo+std", "USB-C: extra panel power only (not needed)"),
    ("thumbwheel_body",    "top", 0.3, 38.9, 6.3, 51.2, 3.0, "#b0b0b0", "photo+EST", "3-way thumbwheel housing"),
    ("btn_en",             "top", 9.9, 23.6, 13.5, 27.8, 2.0, "#e8d8c8", "photo+std", "EN (reset)"),
    ("btn_boot",           "top", 9.9, 16.7, 13.5, 20.9, 2.0, "#e8d8c8", "photo+std", "BOOT"),
    ("jst_i2c_4p",         "top", 0.6, 29.3, 4.7, 36.3, 2.9, "#f2f2f2", "photo+std", "SH1.0 4-pin I2C expansion"),
    ("jst_rtc_bat",        "top", 50.5, 53.7, 54.5, 57.2, 2.9, "#f2f2f2", "photo+std", "SH1.0 2-pin RTC battery"),
    ("jst_spk",            "top", 50.9, 0.7, 54.9, 4.5, 4.5, "#f2f2f2", "photo+EST", "speaker plug"),
    ("mic2",               "top", 12.9, 1.5, 14.3, 3.4, 1.0, "#d4af37", "photo", "microphone 2"),
    ("mic1",               "top", 58.3, 1.7, 59.7, 3.5, 1.0, "#d4af37", "photo", "microphone 1"),
    ("inductor_top",       "top", 35.0, 33.9, 39.0, 37.9, 3.0, "#333333", "photo+EST", "2R2 inductor"),
    ("hub75_socket_bottom","bot", 44.4, 31.3, 50.5, 54.3, 8.5, "#151515", "photo+std", "2x8 female socket: plugs straight onto the panel's HUB75 IN header"),
    ("bottom_parts_zone",  "bot", 6.0, 6.0, 44.0, 50.0, 3.0, "#3a3a3a", "EST", "level shifters, codec, regulators: keep-out"),
]

# Thumbwheel: wheel lies flat (axis vertical), its rim sticks out past the LEFT edge
WHEEL = {"cx": 0.8, "cy": 45.4, "d": 13.7, "thick": 2.5, "z_center": PCB_T + 1.8, "source": "photo+EST"}

COLORS = {}


def box(x0, y0, x1, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def build():
    pcb = box(0, 0, PCB_W, PCB_H, 0, PCB_T)
    for hx, hy in HOLES:
        pcb = pcb - Manifold.cylinder(PCB_T + 2, HOLE_D / 2, HOLE_D / 2, 32).translate((hx, hy, -1))
    meshes = [("pcb", pcb, "#14321f")]
    for name, side, x0, y0, x1, y1, h, color, *_ in PARTS:
        z0, z1 = (PCB_T, PCB_T + h) if side == "top" else (-h, 0)
        if name.startswith("usb_c"):          # mid-mount USB-C sits partly through the board (EST)
            z0, z1 = PCB_T - 0.8, PCB_T - 0.8 + h
        meshes.append((name, box(x0, y0, x1, y1, z0, z1), color))
    w = WHEEL
    wheel = Manifold.cylinder(w["thick"], w["d"] / 2, w["d"] / 2, 64).translate(
        (w["cx"], w["cy"], w["z_center"] - w["thick"] / 2))
    meshes.append(("thumbwheel", wheel, "#2a2a2a"))
    return meshes


def to_trimesh(m, color):
    mesh = m.to_mesh()
    t = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)
    rgb = [int(color[i:i + 2], 16) for i in (1, 3, 5)] + [255]
    t.visual.face_colors = np.tile(rgb, (len(t.faces), 1))
    return t


def envelope():
    """Overall size including overhangs, and the tallest thing on each side."""
    xs, ys = [0, PCB_W], [0, PCB_H]
    top_h, bot_h = 0, 0
    for name, side, x0, y0, x1, y1, h, *_ in PARTS:
        xs += [x0, x1]
        ys += [y0, y1]
        if side == "top":
            top_h = max(top_h, h)
        else:
            bot_h = max(bot_h, h)
    xs.append(WHEEL["cx"] - WHEEL["d"] / 2)
    return {"x_min": round(min(xs), 2), "x_max": round(max(xs), 2), "y_min": round(min(ys), 2),
            "y_max": round(max(ys), 2), "tallest_top": top_h, "tallest_bottom": bot_h,
            "total_thickness": round(top_h + PCB_T + bot_h, 2)}


def drawing(path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Circle, Rectangle

    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(17, 8.5), gridspec_kw={"width_ratios": [1.25, 1]})
    ax.add_patch(Rectangle((0, 0), PCB_W, PCB_H, fc="#1d4d2e", ec="k", lw=1.5))
    for hx, hy in HOLES:
        ax.add_patch(Circle((hx, hy), HOLE_D / 2, fc="white", ec="k"))
        ax.annotate("(%.1f, %.1f)" % (hx, hy), (hx, hy), xytext=(hx + (3 if hx < 30 else -14), hy + (3 if hy < 30 else -5)),
                    fontsize=7, color="white")
    for name, side, x0, y0, x1, y1, h, color, src, note in PARTS:
        if side == "bot":
            ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc="none", ec="#ff5050", lw=1.2, ls="--"))
            ax.text((x0 + x1) / 2, (y0 + y1) / 2, name.replace("_", "\n") + "\n(underside)\n%.1f tall" % h,
                    ha="center", va="center", fontsize=6.5, color="#ff8080")
            continue
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=color, ec="#444", alpha=0.85))
        est = " EST" if "EST" in src else ""
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, "%s\nh=%.1f%s" % (name.replace("_", " "), h, est),
                ha="center", va="center", fontsize=6.3, color="white" if color < "#888888" else "black")
    w = WHEEL
    ax.add_patch(Circle((w["cx"], w["cy"]), w["d"] / 2, fc="none", ec="#ffaa00", lw=1.5))
    ax.text(w["cx"] - 5.5, w["cy"] - 9.5, "thumbwheel\nrim to x=%.1f" % (w["cx"] - w["d"] / 2), fontsize=6.5, color="#cc7700")
    ax.set_xlim(-9, 68)
    ax.set_ylim(-3, 61)
    ax.set_aspect("equal")
    ax.set_xticks(range(0, 66, 5))
    ax.set_yticks(range(0, 58, 5))
    ax.grid(alpha=0.25)
    ax.set_title("Seengreat RGB Matrix HUB75 S3 - TOP view (speaker side), mm", fontsize=11)

    # side view (looking from the bottom edge, y=0): x vs z
    ax2.add_patch(Rectangle((0, 0), PCB_W, PCB_T, fc="#1d4d2e", ec="k"))
    for name, side, x0, y0, x1, y1, h, color, src, note in PARTS:
        z0 = PCB_T if side == "top" else -h
        if name.startswith("usb_c"):
            z0 = PCB_T - 0.8
        ax2.add_patch(Rectangle((x0, z0), x1 - x0, h, fc=color, ec="#444", alpha=0.6))
    ax2.add_patch(Rectangle((w["cx"] - w["d"] / 2, w["z_center"] - w["thick"] / 2), w["d"], w["thick"],
                            fc="#2a2a2a", ec="#ffaa00"))
    env = envelope()
    ax2.axhline(PCB_T + env["tallest_top"], color="r", ls=":", lw=1)
    ax2.axhline(-env["tallest_bottom"], color="r", ls=":", lw=1)
    ax2.text(66, PCB_T + env["tallest_top"], " top %.1f" % (PCB_T + env["tallest_top"]), color="r", fontsize=8, va="center")
    ax2.text(66, -env["tallest_bottom"], " bottom -%.1f" % env["tallest_bottom"], color="r", fontsize=8, va="center")
    ax2.set_xlim(-9, 75)
    ax2.set_ylim(-11, 13)
    ax2.set_aspect("equal")
    ax2.grid(alpha=0.25)
    ax2.set_title("Side view (heights; EST = measure with calipers)", fontsize=11)
    ax2.set_xlabel("x (mm)")
    ax2.set_ylabel("z (mm)  PCB 0-1.6")
    plt.tight_layout()
    plt.savefig(path, dpi=130, facecolor="white")


def render3d(meshes, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(12, 9), dpi=110)
    ax = fig.add_subplot(111, projection="3d")
    light = np.array([0.3, -0.5, 0.8]) / np.linalg.norm([0.3, -0.5, 0.8])
    tris, cols = [], []
    for name, m, color in meshes:   # one collection so matplotlib depth-sorts every face together
        t = to_trimesh(m, color)
        base = np.array([int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)])
        shade = np.clip(t.face_normals @ light, 0, 1) * 0.6 + 0.4
        tris.append(t.vertices[t.faces])
        cols.append(np.clip(base * shade[:, None], 0, 1))
    ax.add_collection3d(Poly3DCollection(np.concatenate(tris), facecolors=np.concatenate(cols),
                                         edgecolors=(0, 0, 0, 0.15), linewidths=0.2))
    ax.set_xlim(-6, 66)
    ax.set_ylim(-6, 62)
    ax.set_zlim(-22, 28)
    ax.set_box_aspect((72, 68, 50))
    ax.view_init(elev=28, azim=-58)
    ax.set_axis_off()
    ax.set_title("Seengreat RGB Matrix HUB75 S3 - 3D reference model (speaker side up)")
    plt.tight_layout()
    plt.savefig(path, facecolor="white")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(os.path.join(here, "stl"), exist_ok=True)
    meshes = build()
    scene = trimesh.util.concatenate([to_trimesh(m, c) for _, m, c in meshes])
    scene.export(os.path.join(here, "stl", "seengreat_board.stl"))
    scene.export(os.path.join(here, "stl", "seengreat_board.glb"))  # keeps part colors
    spec = {
        "board": "Seengreat RGB Matrix HUB75 S3",
        "pcb": {"w": PCB_W, "h": PCB_H, "t": PCB_T, "hole_d": HOLE_D, "holes": HOLES},
        "parts": [dict(zip(["name", "side", "x0", "y0", "x1", "y1", "height", "color", "source", "note"], p)) for p in PARTS],
        "thumbwheel": WHEEL,
        "envelope": envelope(),
    }
    with open(os.path.join(here, "board_spec.json"), "w") as f:
        json.dump(spec, f, indent=2)
    drawing(os.path.join(here, "board_drawing.png"))
    render3d(meshes, os.path.join(here, "board_3d.png"))
    print(json.dumps(spec["envelope"], indent=2))


if __name__ == "__main__":
    main()
