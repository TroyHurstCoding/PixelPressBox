#!/usr/bin/env python3
"""Preview renders of the final case with the board model placed inside (matplotlib, no GPU needed)."""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import trimesh
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_case as bc  # noqa: E402

LIGHT = np.array([0.35, -0.55, 0.75])
LIGHT /= np.linalg.norm(LIGHT)


def tm(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)


def draw(ax, items, elev, azim, title, lim=None):
    tris, cols = [], []
    for mesh, color, alpha in items:
        base = np.array(matplotlib.colors.to_rgb(color))
        shade = np.clip(mesh.face_normals @ LIGHT, 0, 1) * 0.6 + 0.4
        c = np.clip(base * shade[:, None], 0, 1)
        tris.append(mesh.vertices[mesh.faces])
        cols.append(np.hstack([c, np.full((len(c), 1), alpha)]))
    ax.add_collection3d(Poly3DCollection(np.concatenate(tris), facecolors=np.concatenate(cols), edgecolors="none"))
    pts = np.vstack([m.vertices for m, _, _ in items])
    lo, hi = pts.min(0), pts.max(0)
    if lim:
        lo, hi = np.array(lim[0]), np.array(lim[1])
    c, r = (lo + hi) / 2, (hi - lo).max() / 2 * 0.62
    ax.set_xlim(c[0] - r, c[0] + r)
    ax.set_ylim(c[1] - r, c[1] + r)
    ax.set_zlim(c[2] - r, c[2] + r)
    ax.view_init(elev=elev, azim=azim)
    ax.set_axis_off()
    ax.set_title(title, fontsize=12)


def main(out):
    shell = tm(bc.shell())
    lid_raw = tm(bc.lid())
    lid = lid_raw.copy()  # lid modeled from the back: flip about Y, move to the back of the shell
    lid.apply_transform(trimesh.transformations.rotation_matrix(np.pi, (0, 1, 0)))
    lid.apply_translation((0, 0, bc.OUT_D + bc.LID_T))
    board = [(tm(m), c, 1.0) for m, c in bc.placed_board()]
    louver = tm(bc.louver())
    louver.apply_translation((0, 0, bc.FRONT + bc.DIFF_T))

    fig = plt.figure(figsize=(18, 12), dpi=100)
    # 1 open from the back: board sitting in the case
    ax = fig.add_subplot(2, 2, 1, projection="3d")
    draw(ax, [(shell, "#3a3d42", 1.0)] + board, elev=89, azim=-90,
         title="From the back, cover off: board in place (speaker side facing you)")
    # 2 back cover outside: grille, mic holes, vents
    ax = fig.add_subplot(2, 2, 2, projection="3d")
    draw(ax, [(lid_raw, "#55595f", 1.0)], elev=-89, azim=-90,
         title="Back cover, outside: speaker grille, mic holes, vents, 4 panel screws")
    # 3 right side: USB-C window + thumbwheel
    ax = fig.add_subplot(2, 2, 3, projection="3d")
    up = trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0))  # make Y point up
    side = []
    for m, c, a in [(shell, "#3a3d42", 1.0), (lid, "#55595f", 1.0)] + board:
        m2 = m.copy()
        m2.apply_transform(up)
        side.append((m2, c, a))
    draw(ax, side, elev=4, azim=180, title="Right side: USB-C window (lower) + thumbwheel slot (upper)")
    # 4 back cover inside: standoffs, gasket ring, posts
    ax = fig.add_subplot(2, 2, 4, projection="3d")
    draw(ax, [(lid_raw, "#7a7f86", 1.0)], elev=62, azim=-70,
         title="Back cover, inside: board standoffs, speaker gasket ring, panel screw columns")
    plt.tight_layout()
    plt.savefig(out, facecolor="white")
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "case_preview.png"))
