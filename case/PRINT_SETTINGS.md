# Print settings: Creality Ender-3 S1 Pro (0.4 mm nozzle, PLA)

Works in Cura or Creality Print. Matte black PLA hides layer lines best.

## Order
1. `fit_test.stl` (about 30 min): the shell's front 10 mm. Press the panel in: it should slide in with a hair of play.
2. `shell.stl`, front edge down, no supports. PETG if you'll snap it open often; PLA is fine for occasional opening.
3. `lid.stl` (back cover), outside face down, no supports. Measure the power cable first and set `CABLE_D`.
4. `stand.stl`, as exported.

## Settings
| Setting | Shell | Back cover + stand |
| --- | --- | --- |
| Layer height | 0.12 to 0.16 mm (smoothest sides) | 0.2 mm |
| Wall line count | 5 (makes the 2 mm wall solid) | 3 |
| Infill | 20% | 20% |
| Outer wall speed | 25 to 30 mm/s | 40 mm/s |
| Inner wall / infill speed | normal (60 to 80 mm/s) | normal |
| Seam position | Sharpest corner (or a back corner) | Sharpest corner |
| Supports | None | None |

**For every part:**
- Nozzle 205 to 210 °C, bed 60 °C, part fan 100% after the first layer.
- Retraction 0.8 mm at 40 mm/s (direct drive needs very little). Z-hop off.
- Elephant foot compensation: Cura "Initial Layer Horizontal Expansion" -0.15 mm.
- Bed: the textured PEI sheet gives the front bezel and the back cover a fine matte finish.
- Dry the filament first if the roll has been sitting out (about 4 h at 50 °C).
  Damp PLA prints fuzzy, stringy walls.

## Expect
- The front bezel and the back cover's outside face print against the bed, so they come out the smoothest.
- The shell's sides show faint layer lines. The slower outer wall and thinner layers above shrink them.
- The shell's walls have no openings, so there's nothing to clean up on the sides.

## Screws (no tapping or inserts needed)
- The back cover snaps into the shell: no screws.
- 4x M3x35 button head: back cover's columns into the panel's threaded corner holes.
- 4x M2.5x8: board onto the standoffs. They cut their own threads.
- 2x M3x6: accelerometer under the fin on the back cover. They cut their own threads.
- Drive the self-cutting screws slowly the first time and don't overtighten.
