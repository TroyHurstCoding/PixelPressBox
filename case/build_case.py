#!/usr/bin/env python3
"""PixelPressbox case - 64x32 P2.5 HUB75 panel + Seengreat RGB Matrix HUB75 S3 board.
Tuned for an Ender-3 S1 Pro (0.4 mm nozzle). The board's geometry comes from board_spec.json
(written by board_model.py), so update the board numbers there, not here.

    pip install manifold3d trimesh numpy
    python3 board_model.py && python3 build_case.py      # -> stl/*.stl + checks

Parts (print orientation):
  shell.stl     black PLA   BACK edge down, no supports. Slicer: 5 wall lines (makes the 2 mm wall solid)
  lid.stl       black PLA   flat, standoffs up, no supports
  stand.stl     black PLA   as exported
  fit_test.stl              the shell's first 10 mm: push the panel in to check the fit

The panel sits FLUSH with the shell's front edge: no lip, no louver, no diffuser. The shell is an open frame
2 mm wide around the panel (0.3 mm gap). The lid's 4 columns are the panel's stop: the M3 screws pull the
panel back against them, so column length (from PANEL_D) decides flushness. Measure the panel's thickness.
Hardware: 4x M3x8 countersunk/flat head (back cover -> shell corner bosses), 4x M2.5x8 (board -> lid standoffs),
          4x M3 button head through the lid columns into the panel's corner holes (length printed by check())

Layout (seen from the FRONT): board in the right half, speaker side facing the back cover.
Walls: no openings. The power supply's cable plugs into the board and snaps into a slot in the back cover's bottom edge.
Back cover: speaker grille + speaker gasket ring, two mic holes, vents. Left wall: air intake.
Board <-> panel: the HUB75 ribbon cable (board header is right-angle, cable heads toward the middle).

Coordinates (right-handed): Y up, Z = depth (front face at 0, back at OUT_D). Facing the display from the
front, +X points to your LEFT, so "right side" = -X. RIGHT below handles that.
Lid "distance" d = how far in front of the lid's inner face something is.
"""
import json
import os

import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType, Manifold, OpType

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = json.load(open(os.path.join(HERE, "board_spec.json")))
PARTS = {p["name"]: p for p in SPEC["parts"]}
PCB = SPEC["pcb"]
WHEEL = SPEC["thumbwheel"]

# ----------------------------------------------------------------- panel / shell (mm)
PANEL_W, PANEL_H, PANEL_D = 160.0, 80.0, 14.7   # MEASURE YOUR PANEL'S THICKNESS
PITCH, COLS, ROWS = 2.5, 64, 32
CLR = 0.3                 # gap between the panel's edge and the shell, per side
WALL = 2.0                # = the visible bezel around the panel (5 lines of a 0.4 mm nozzle)
FRONT_CHAMFER = 0.5       # small bevel on the outer front edge; hides first-layer squish ("elephant foot")
FLUSH = 0.0               # how far the panel's face sits behind the front edge (0 = flush; 0.3 protects LEDs)
LID_T = 2.4
CORNER_R = 3.0            # outer corner radius (keeps >= 1.5 mm of wall at the corners)
RIM_T, RIM_D = 2.0, 6.0
STAND_ANGLE = 12.0
# Back cover -> shell: 4 countersunk M3x8 from the back into bosses in the shell's inner corners. No screws in any
# side wall, so all four sides stay flat (portrait or landscape).
BOSS = 8.0                # corner boss size (square, in the corner of the cavity)
BOSS_D = 10.0             # boss depth from the back edge
PANEL_PARTS_H = 17.0      # tallest thing on the panel's back: HUB75 IN header + plugged ribbon + bend
BOARD_UNDER = 3.0         # board underside SMD parts (the 8.5 mm socket is checked separately)

# ----------------------------------------------------------------- board placement
STANDOFF = 10.5           # lid inner face -> board's top (speaker-side) surface; clears the 9.5 mm VH-4P
USB_EDGE_GAP = 35.0       # board's USB edge -> inside of the right wall: room for the power plug + cable bend
                          # (also puts the thumbwheel well inside the case; the accelerometer replaces it)
BOARD_Y0 = -28.8          # board's bottom edge (front view); centered between the panel screw columns
# Hard-wired power: the power supply's own USB-C cable (iUniker XSG-0503500HUS, 5.25 V 4 A) plugs straight into
# the board and leaves through a snap-in slot in the back cover's bottom edge. MEASURE THE CABLE and set CABLE_D.
CABLE_D = 4.0             # cable diameter (EST, typical for a 4 A USB-C lead)
CABLE_X = -52.0           # front-view x of the exit: right side, clear of the panel screw column and the board
CABLE_SNAP = 0.6          # slot entry this much narrower than the cable, so it clicks in and stays

# derived
CAV_W, CAV_H = PANEL_W + 2 * CLR, PANEL_H + 2 * CLR
BOSS_X, BOSS_Y = CAV_W / 2 - BOSS / 2, CAV_H / 2 - BOSS / 2   # back-cover screw positions
OUT_W, OUT_H = CAV_W + 2 * WALL, CAV_H + 2 * WALL
BOT_H = SPEC["envelope"]["tallest_bottom"]
# room behind the panel: board stack + clearance over the panel's own connectors
# room behind the panel: board stack + the panel's own plugged connectors, wherever they end up
BACK_SPACE = max(STANDOFF + PCB["t"] + BOARD_UNDER + PANEL_PARTS_H, STANDOFF + PCB["t"] + BOT_H + 3.0)
OUT_D = FLUSH + PANEL_D + BACK_SPACE
PANEL_BACK_Z = FLUSH + PANEL_D
RIGHT = -1                               # -X is the right side when you face the display
BX1 = CAV_W / 2 - USB_EDGE_GAP           # distance from center to the board's USB edge (board x = 0)
SEG = 40


def bx(x):
    """board x (photo / speaker-side view) -> case X. USB edge (x=0) sits against the right wall."""
    return RIGHT * (BX1 - x)


def by(y):
    return BOARD_Y0 + y


def fx(x):
    """case X -> lid X (the lid is printed plate-down and flipped about Y when installed)."""
    return -x


def z_of(d):
    """lid distance -> shell z."""
    return OUT_D - d


# part positions (front view)
def part_rect(name):
    p = PARTS[name]
    xs = sorted([bx(p["x0"]), bx(p["x1"])])
    return xs[0], by(p["y0"]), xs[1], by(p["y1"])


USB = PARTS["usb_c_board"]
PORT_Y = by((USB["y0"] + USB["y1"]) / 2)
PORT_D = STANDOFF - 0.8                  # mid-mount USB-C: center ~0.8 above the PCB top surface
WHEEL_Y = by(WHEEL["cy"])
WHEEL_D = STANDOFF - (WHEEL["z_center"] - PCB["t"])
HOLES = [(bx(x), by(y)) for x, y in PCB["holes"]]
BOARD_RECT = (min(bx(0), bx(PCB["w"])), by(0), max(bx(0), bx(PCB["w"])), by(PCB["h"]))
SPK = part_rect("speaker")
MICS = [part_rect("mic1"), part_rect("mic2")]
# Panel screw columns: the panel's 4 corner mounting holes (from the panel's STEP model,
# 64x32x2.5mm-rgb-matrix: holes at x 16.96/17.08 and 143.23/143.24, y 6.76 and 73.02/73.13 on 160x80).
# Center-to-center 126.2 x 66.3 mm. Clearance holes (3.4 mm) absorb the ~0.2 mm spread.
PX, PY = 126.2 / 2, 66.3 / 2
# ADXL345 accelerometer (GY-291 style board, from GY-291_ADXL345_module.step): 20.4 x 16.1 x 1.6 mm PCB,
# two 3.6 mm holes 15.5 mm apart, 2.3 mm in from one long edge; 8-pin header along the other long edge
# (pins stick out 8.5 mm on the component side, 1.4 mm on the other). It hangs from the INSIDE of the shell's
# top wall, components and pins facing down into the case, pin edge toward the back cover.
ACC_X = 40.0               # left half seen from the front, clear of the Seengreat board
ACC_Z = 30.0               # board center depth (front face at 0); keeps the post ramps behind the panel
ACC_HOLE_DX, ACC_HOLE_OFF = 15.5, 5.75   # hole spacing; hole row 5.75 mm from the board's center line
ACC_POST_L, ACC_POST_R = 5.5, 3.0        # posts stand 5.5 mm off the wall (clears the 1.4 mm pin stubs)
ACC_PILOT_R = 1.3                         # 2.6 mm pilot, 6.6 mm deep: an M3x8 bites 6.4 mm; 0.9 mm skin outside
ACC_HOLES = [(ACC_X - ACC_HOLE_DX / 2, ACC_Z - ACC_HOLE_OFF), (ACC_X + ACC_HOLE_DX / 2, ACC_Z - ACC_HOLE_OFF)]
POST_R = 3.3              # column radius (1.6 mm wall around an M3 clearance hole)
POST_HOLE_R = 1.7         # M3 clearance
PANEL_SCREW = 35          # M3x35 button head, from the back
CBORE_R, CBORE_D = 3.1, 4.0   # head recess (6.2 mm, fits a 5.7 mm button head)
POST_BASE_R, POST_BASE_H = 4.3, 6.0   # thicker column foot so the recess has walls
POSTS = [(-PX, PY), (PX, PY), (-PX, -PY), (PX, -PY)]
CABLE_Y = -CAV_H / 2 + 2.3 + CABLE_D / 2   # cable center: just inside the lid's rim, close to the bottom edge
# Panel stop tabs: one per panel screw, reaching in from the shell's top/bottom wall right behind the panel. The
# panel's back rests on them and the M3x35 screws pull it tight against them, so the panel is held by the SHELL:
# take the 4 screws out from the back and the panel comes out the front, back cover still on. The tab's back is
# a 45 deg slope down to the wall (the shell prints back-edge down, so the slope faces the bed: no supports).
TAB_W, TAB_T = 10.0, 3.0                  # width along the wall; thickness at the tip
TAB_REACH = (CAV_H / 2 - PY) + 4.5        # wall -> 4.5 mm past the screw
def tab_back_z(d):                        # z of the tab's back face at distance d from the wall
    return PANEL_BACK_Z + TAB_T + max(TAB_REACH - d, 0.0)
# lid columns now only guide the screws: they stop 0.5 mm short of the tab's sloped back
POST_LEN = OUT_D - tab_back_z((CAV_H / 2 - PY) - POST_R) - 0.5
PANEL_BITE = PANEL_SCREW - (OUT_D + LID_T - CBORE_D - PANEL_BACK_Z)
VENT_H, VENT_Y = 12.0, OUT_H / 2 - 23     # top vents end below the upper screw columns


def box(w, h, d, x=0.0, y=0.0, z=0.0):
    return Manifold.cube((w, h, d)).translate((x - w / 2, y - h / 2, z))


def rect_box(x0, y0, x1, y1, z0, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def rounded_slab(w, h, d, r, z=0.0):
    cs = CrossSection.square((w - 2 * r, h - 2 * r)).translate((-(w - 2 * r) / 2, -(h - 2 * r) / 2))
    return cs.offset(r, JoinType.Round, 2.0, SEG).extrude(d).translate((0, 0, z))


def cyl(r, h, x=0.0, y=0.0, z=0.0):
    return Manifold.cylinder(h, r, r, SEG).translate((x, y, z))


def y_cyl(r, length, x, z):
    return Manifold.cylinder(length, r, r, SEG).rotate((90, 0, 0)).translate((x, length / 2, z))


def x_cyl(r, length, y, z):
    return Manifold.cylinder(length, r, r, SEG).rotate((0, 90, 0)).translate((-length / 2, y, z))


def union(parts):
    return Manifold.batch_boolean(parts, OpType.Add)


def stadium_x(length_y, height_z, y, z, depth_x, x):
    """Rounded slot through a wall that faces +/-X."""
    r = height_z / 2
    core = box(depth_x, max(length_y - height_z, 0.01), height_z, x=x, y=y, z=z - r)
    return core + x_cyl(r, depth_x, y - (length_y - height_z) / 2, z).translate((x, 0, 0)) \
        + x_cyl(r, depth_x, y + (length_y - height_z) / 2, z).translate((x, 0, 0))


# ----------------------------------------------------------------- parts
def shell():
    c = FRONT_CHAMFER
    body = Manifold.batch_hull([rounded_slab(OUT_W - 2 * c, OUT_H - 2 * c, 0.01, CORNER_R - c),
                                rounded_slab(OUT_W, OUT_H, OUT_D - c, CORNER_R, z=c)])
    s = body - box(CAV_W, CAV_H, OUT_D + 2, z=-1)   # open front: the panel fills the whole opening
    cuts = []
    # corner bosses for the back-cover screws, flush with the back edge (they sit on the bed; the slope toward the
    # walls just saves plastic)
    bosses = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * CAV_W / 2, sy * CAV_H / 2                  # the cavity corner
            blk = rect_box(min(cx, cx - sx * BOSS), min(cy, cy - sy * BOSS), max(cx, cx - sx * BOSS),
                           max(cy, cy - sy * BOSS), OUT_D - BOSS_D, OUT_D)
            z0 = OUT_D - BOSS_D - BOSS
            strip_a = rect_box(min(cx, cx - sx * BOSS), min(cy, cy - sy * 0.5), max(cx, cx - sx * BOSS),
                               max(cy, cy - sy * 0.5), z0, z0 + 0.1)
            strip_b = rect_box(min(cx, cx - sx * 0.5), min(cy, cy - sy * BOSS), max(cx, cx - sx * 0.5),
                               max(cy, cy - sy * BOSS), z0, z0 + 0.1)
            bosses.append(Manifold.batch_hull([blk, strip_a, strip_b]))
            cuts.append(cyl(1.3, BOSS_D + 1, sx * BOSS_X, sy * BOSS_Y, OUT_D - BOSS_D + 1))   # 2.6 mm M3 pilot
    s = s + union(bosses)
    # panel stop tabs (see TAB_* above): flat front face, sloped back, M3 clearance hole
    tabs, tab_holes = [], []
    for x, y in POSTS:
        sy = 1 if y > 0 else -1
        wy = sy * CAV_H / 2
        tip_y = wy - sy * TAB_REACH
        prism = Manifold.batch_hull([
            rect_box(x - TAB_W / 2, min(wy, tip_y), x + TAB_W / 2, max(wy, tip_y), PANEL_BACK_Z, PANEL_BACK_Z + TAB_T),
            rect_box(x - TAB_W / 2, min(wy, wy - sy * 0.5), x + TAB_W / 2, max(wy, wy - sy * 0.5),
                     PANEL_BACK_Z, tab_back_z(0.0))])
        tabs.append(prism + rect_box(x - TAB_W / 2, min(wy, wy + sy * 0.5), x + TAB_W / 2, max(wy, wy + sy * 0.5),
                                     PANEL_BACK_Z, tab_back_z(0.0)))   # fused into the wall
        tab_holes.append(cyl(POST_HOLE_R, 40, x, y, PANEL_BACK_Z - 1))
    s = s + union(tabs)
    cuts += tab_holes
    # accelerometer posts on the inside of the top wall. The shell prints back-edge down, so each post gets a
    # 45 deg ramp on its back side and needs no supports.
    wall_y = CAV_H / 2
    posts = []
    for hx, hz in ACC_HOLES:
        cylp = y_cyl(ACC_POST_R, ACC_POST_L + 0.5, hx, hz).translate((0, wall_y - (ACC_POST_L + 0.5) / 2, 0))
        foot = box(2 * ACC_POST_R, 0.5, 2 * ACC_POST_R + ACC_POST_L, x=hx, y=wall_y + 0.25,
                   z=hz - ACC_POST_R)
        posts.append(Manifold.batch_hull([cylp, foot]))
    s = s + union(posts)
    pilots = [y_cyl(ACC_PILOT_R, ACC_POST_L + 1.6, hx, hz).translate((0, wall_y - ACC_POST_L / 2 + 0.3, 0))
              for hx, hz in ACC_HOLES]
    return s - union(cuts + pilots)


def fit_test():
    """Quick test print: the shell's first 10 mm, to check how the panel fits."""
    return shell().trim_by_plane((0, 0, -1), -10.0)


def lid():
    """Modeled seen from the back: plate on the bed (z=0), features rise toward the panel (z = LID_T + d)."""
    plate = rounded_slab(OUT_W, OUT_H, LID_T, CORNER_R)
    rim_w, rim_h = CAV_W - 0.4, CAV_H - 0.4
    rim = box(rim_w, rim_h, RIM_D, z=LID_T) - box(rim_w - 2 * RIM_T, rim_h - 2 * RIM_T, RIM_D + 2, z=LID_T - 1)
    parts = [plate, rim]
    for hx, hy in HOLES:                    # board standoffs (M2.5)
        parts.append(cyl(2.4, STANDOFF, fx(hx), hy, LID_T))
    for x, y in POSTS:                     # panel screw columns: the panel's back rests on their ends
        parts.append(cyl(POST_R, POST_LEN, fx(x), y, LID_T))
        parts.append(cyl(POST_BASE_R, POST_BASE_H, fx(x), y, LID_T))
    # speaker gasket ring: channels sound to the grille, stops 1 mm short of the speaker (no buzz)
    sx0, sy0, sx1, sy1 = SPK
    cx, cy = (sx0 + sx1) / 2, (sy0 + sy1) / 2
    ring_h = STANDOFF - PARTS["speaker"]["height"] - 1.0
    gw, gh = 33.0, 25.0
    parts.append(box(gw + 2.4, gh + 2.4, ring_h, x=fx(cx), y=cy, z=LID_T) - box(gw, gh, ring_h + 2, x=fx(cx), y=cy, z=LID_T - 1))
    l = union(parts)
    cuts = []
    for sx in (-1, 1):                      # back-cover screws: countersunk M3 (90 deg), heads flush with the back
        for sy in (-1, 1):
            hx, hy = fx(sx * BOSS_X), sy * BOSS_Y
            cuts.append(cyl(1.7, LID_T + 2, hx, hy, -1))
            cuts.append(Manifold.cylinder(1.6, 3.3, 1.7, SEG).translate((hx, hy, -0.1)))
            # rim stops short of the corner bosses
            cuts.append(box(BOSS + 1.5, BOSS + 1.5, RIM_D + 2, x=fx(sx * (CAV_W / 2 - BOSS / 2)),
                            y=sy * (CAV_H / 2 - BOSS / 2), z=LID_T))
    cuts += [cyl(1.05, STANDOFF + 2, fx(hx), hy, LID_T - 0.6) for hx, hy in HOLES]  # 2.1 mm pilot for M2.5
    cuts += [cyl(POST_HOLE_R, LID_T + POST_LEN + 2, fx(x), y, -1) for x, y in POSTS]   # M3 through the column
    cuts += [cyl(CBORE_R, CBORE_D + 1, fx(x), y, -1) for x, y in POSTS]                # head recess
    # power cable: a round hole just inside the wall line, with a narrower neck open to the bottom edge to snap
    # the cable through; the rim gets a matching gap so the cable passes into the case
    cr = CABLE_D / 2 + 0.1
    cut_x = fx(CABLE_X)
    cuts.append(cyl(cr, LID_T + 2, cut_x, CABLE_Y, -1))
    neck = CABLE_D - CABLE_SNAP
    cuts.append(rect_box(cut_x - neck / 2, -OUT_H / 2 - 1, cut_x + neck / 2, CABLE_Y, -1, LID_T + 1))
    cuts.append(box(CABLE_D + 1.0, RIM_T * 3, RIM_D + 2, x=cut_x, y=-rim_h / 2 + RIM_T / 2, z=LID_T - 0.5))
    # speaker grille (inside the gasket ring)
    x = -gw / 2 + 3
    while x <= gw / 2 - 3:
        y = -gh / 2 + 3
        while y <= gh / 2 - 3:
            cuts.append(cyl(1.4, LID_T + 2, fx(cx + x), cy + y, -1))
            y += 4.5
        x += 4.5
    # mic holes: a small cluster over each microphone
    for mx0, my0, mx1, my1 in MICS:
        mx, my = (mx0 + mx1) / 2, (my0 + my1) / 2
        for ox, oy in ((0, 0), (2.2, 0), (-2.2, 0)):
            cuts.append(cyl(0.8, LID_T + 2, fx(mx + ox), my + oy, -1))
    # vents, away from the board: cool air in along the bottom, warm air out along the top (no wall slots)
    for i in range(-7, -2):                 # x 31..67: the left side, clear of the board
        for vy in (VENT_Y, -VENT_Y):
            cuts.append(box(3.0, VENT_H, LID_T + 2, x=fx(RIGHT * (i * 9 - 4)), y=vy, z=-1))
    return l - union(cuts)


def stand():
    """Desk cradle, 12 deg lean-back. Modeled upright, then tilted: the case's top leans toward -y, so the
    viewer stands at +y. Front lip is only 2 mm tall so it never covers the flush panel's LEDs (the bezel is 2.3);
    the back lip carries the lean and has the slot for the power cable."""
    wall, floor, height, width = 3.0, 3.0, 14.0, 120.0
    depth_in = OUT_D + LID_T + 0.6
    outer = box(width, depth_in + 2 * wall, height)
    slot = box(width + 2, depth_in, height, z=floor)
    front_cut = rect_box(-width, depth_in / 2 - 0.1, width, depth_in / 2 + wall + 1, floor + 2.0, height + 1)
    # back lip slot: seen from the front, case +X is your left; looking from +y, stand +X is your left too
    cable_z = floor + (CABLE_Y + OUT_H / 2)                    # cable center height above the cradle floor
    notch = rect_box(CABLE_X - (CABLE_D + 2) / 2, -depth_in / 2 - wall - 1, CABLE_X + (CABLE_D + 2) / 2,
                     -depth_in / 2 + 0.1, cable_z - CABLE_D / 2 - 0.5, height + 1)
    channel = (outer - slot - front_cut - notch).rotate((STAND_ANGLE, 0, 0))
    channel = channel.translate((0, 0, -channel.bounding_box()[2]))
    base = box(width, depth_in + 40, 4.0, y=-10)
    return (channel + base).trim_by_plane((0, 0, 1), 0.0)


# ----------------------------------------------------------------- board placed in the case (for previews)
def placed_board():
    """The board model positioned inside the case (shell coordinates)."""
    meshes = []
    pcb_z0, pcb_z1 = z_of(STANDOFF + PCB["t"]), z_of(STANDOFF)   # PCB spans these depths
    x0, y0, x1, y1 = BOARD_RECT
    meshes.append((rect_box(x0, y0, x1, y1, pcb_z0, pcb_z1), "#1d4d2e"))
    for p in SPEC["parts"]:
        X0, Y0, X1, Y1 = part_rect(p["name"])
        h = p["height"]
        if p["side"] == "top":                     # toward the lid (larger z)
            a, b = pcb_z1, pcb_z1 + h
            if p["name"].startswith("usb_c"):
                a, b = pcb_z1 - 0.8, pcb_z1 - 0.8 + h
        else:                                      # toward the panel
            a, b = pcb_z0 - h, pcb_z0
        meshes.append((rect_box(X0, Y0, X1, Y1, a, b), p["color"]))
    wz = z_of(WHEEL_D)
    meshes.append((Manifold.cylinder(WHEEL["thick"], WHEEL["d"] / 2, WHEEL["d"] / 2, 48).rotate((0, 0, 0))
                   .translate((bx(WHEEL["cx"]), WHEEL_Y, wz - WHEEL["thick"] / 2)), "#2a2a2a"))
    return meshes


# ----------------------------------------------------------------- checks
def check():
    errors = []
    bx0, by0, bx1, by1 = BOARD_RECT
    # 1 posts clear the board
    for x, y in POSTS:
        if bx0 - POST_R - 0.5 < x < bx1 + POST_R + 0.5 and by0 - POST_R - 0.5 < y < by1 + POST_R + 0.5:
            errors.append("panel screw column at (%.0f,%.0f) hits the board" % (x, y))
        if abs(y) - POST_R < VENT_Y + VENT_H / 2 + 1 and y > 0 and -8 < x < 72:
            errors.append("panel screw column at (%.0f,%.0f) hits the vents" % (x, y))
    # 2 board fits inside the cavity (incl. wheel + USB overhang)
    if by0 < -CAV_H / 2 + 1 or by1 > CAV_H / 2 - 1 or bx0 < -CAV_W / 2 or bx1 > CAV_W / 2:
        errors.append("board outside the cavity")
    # 3 lid screw bosses vs board footprint is irrelevant in depth (rim only 6 deep < standoff), but check tall parts
    for p in SPEC["parts"]:
        if p["side"] == "top" and p["height"] > STANDOFF - 0.5:
            errors.append("%s (%.1f) touches the lid" % (p["name"], p["height"]))
    # 4 standoffs vs top-side parts
    for hx, hy in HOLES:
        for p in SPEC["parts"]:
            if p["side"] != "top":
                continue
            X0, Y0, X1, Y1 = part_rect(p["name"])
            if X0 - 2.4 < hx < X1 + 2.4 and Y0 - 2.4 < hy < Y1 + 2.4:
                dx = max(X0 - hx, 0, hx - X1)
                dy = max(Y0 - hy, 0, hy - Y1)
                if (dx * dx + dy * dy) ** 0.5 < 2.4:
                    errors.append("standoff at (%.1f,%.1f) hits %s" % (hx, hy, p["name"]))
    # 5 room behind the panel
    socket_tip = STANDOFF + PCB["t"] + BOT_H
    if BACK_SPACE - socket_tip < 3:
        errors.append("board's underside socket too close to the panel")
    # 6 wall openings sit in solid wall below the lid rim
    for x, y in POSTS:
        if abs(x - CABLE_X) < POST_BASE_R + CABLE_D / 2 + 1 and abs(y - CABLE_Y) < POST_BASE_R + CABLE_D / 2 + 1:
            errors.append("power cable exit hits a panel screw column")
    if abs(abs(CABLE_X) - BOSS_X) < BOSS / 2 + CABLE_D:
        errors.append("power cable exit hits a corner boss")
    for x, y in POSTS:
        if abs(abs(x) - BOSS_X) < BOSS / 2 + POST_BASE_R + 0.5 and abs(abs(y) - BOSS_Y) < BOSS / 2 + POST_BASE_R + 0.5:
            errors.append("panel screw column hits a corner boss")
    if not 3.0 <= PANEL_BITE <= 5.5:
        errors.append("panel screw bite %.1f mm (want 3-5.5): change PANEL_SCREW or CBORE_D" % PANEL_BITE)
    for x, y in POSTS:
        if bx0 - POST_BASE_R < x < bx1 + POST_BASE_R and by0 - POST_BASE_R < y < by1 + POST_BASE_R:
            errors.append("screw column foot at (%.0f,%.0f) reaches under the board" % (x, y))
    # 7 accelerometer: inside the depth between the panel's back and the back cover's rim, clear of the board
    if ACC_HOLES[0][1] - ACC_POST_R < PANEL_BACK_Z + 0.5:
        errors.append("accelerometer posts reach the panel (move ACC_Z back)")
    if ACC_HOLES[0][1] + ACC_POST_R + ACC_POST_L > OUT_D - RIM_D - 0.5:
        errors.append("accelerometer post ramps reach the back cover's rim")
    for x, y in POSTS:                     # tabs vs accelerometer board footprint (top wall)
        if y > 0 and abs(x - ACC_X) < 10.2 + TAB_W / 2 + 1:
            errors.append("panel stop tab hits the accelerometer")
    if POST_LEN < 8:
        errors.append("lid columns too short to guide the screws")
    if not (PANEL_BACK_Z + 1 < ACC_Z - 8.05 and ACC_Z + 8.05 < OUT_D - RIM_D - 1):
        errors.append("accelerometer board doesn't fit between the panel and the back cover's rim")
    if ACC_X - 10.2 < bx1 + 3 and ACC_X + 10.2 > bx0 - 3:
        errors.append("accelerometer hangs over the Seengreat board")
    if errors:
        raise SystemExit("CHECK FAILED:\n  " + "\n  ".join(errors))
    print("checks ok")
    print("  panel screws: M3x%d button head, %.1f mm into the panel's holes; panel rests on 4 shell tabs" % (PANEL_SCREW, PANEL_BITE))
    print("  lid columns %.1f mm (screw guides), tabs %.1f mm thick at the screw" % (POST_LEN, tab_back_z(CAV_H / 2 - PY) - PANEL_BACK_Z))
    print("  board: X %.1f..%.1f (right half seen from the front), Y %.1f..%.1f, speaker side facing the back" % (bx0, bx1, by0, by1))
    print("  board's USB-C %.0f mm in from the right wall; power cable leaves the back cover's bottom edge at x %.0f" %
          (USB_EDGE_GAP, CABLE_X))
    print("  accelerometer: hangs from the top wall at x %.0f, posts at depth %.1f mm (2x M3x8)" % (ACC_X, ACC_HOLES[0][1]))
    print("  room behind panel %.1f mm; underside socket ends %.1f mm in front of the lid -> %.1f mm to the panel" %
          (BACK_SPACE, socket_tip, BACK_SPACE - socket_tip))


def save(m, path):
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=True)
    tm.export(path)
    size = tm.bounds[1] - tm.bounds[0]
    print("%-14s %6.1f x %6.1f x %5.1f mm  watertight=%s" % (os.path.basename(path), *size, tm.is_watertight))
    return tm


def main():
    check()
    out = os.path.join(HERE, "stl")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("shell", shell), ("lid", lid), ("stand", stand)):
        save(fn(), os.path.join(out, name + ".stl"))
    save(fit_test(), os.path.join(out, "fit_test.stl"))
    print("\nOuter size: %.1f x %.1f mm front, %.1f mm deep + %.1f mm back cover" % (OUT_W, OUT_H, OUT_D, LID_T))


if __name__ == "__main__":
    main()
