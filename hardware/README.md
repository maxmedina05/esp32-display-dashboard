# Hardware — 3D-printable parts

Parametric OpenSCAD designs for mounting the board. Render/export with OpenSCAD
(`openscad -o out.stl file.scad`). Print on a Bambu Lab A1 (or any FDM), PLA/PETG,
no supports, flat as modeled.

## `stand/` — open tilted desk stand ✅ done
A simple cradle: the board's bottom edge drops into an angled slot and leans back
~22°. Open design, very forgiving. See `stand/t-display-stand.scad` + previews.

## `case/` — enclosed tilted case 🚧 in progress
Wraps the board (only screen + USB-C exposed), with an integrated tilt and a
honeycomb-vented back.

**Status / resume point:**
- **Part 1 — front bezel tray** (`case/t-display-case.scad` → `t-display-case-front.stl`):
  designed. This is the **fit-critical test-fit piece**. ⏳ *Next step: print it, drop
  the board in, and check (a) board seats, (b) screen lines up with the window,
  (c) USB-C cable reaches the cutout.* Then tune `cavity_depth` / `win_off` / window size.
- **Part 2 — honeycomb tilted back:** not built yet. Waiting on the test-fit result.
  Honeycomb density chosen: **fine / small hexes** (`hole_r = 3`, see
  `case/honeycomb-sample.scad`). Goes on the back cover + side walls; front stays solid.

**Board facts used:** LilyGO T-Display footprint, 51.49 × 25.09 mm, bare (no soldered
headers), USB-C centered on one short end with a button each side, screen offset toward
the far-from-USB end. Thickness not measured — using standard T-Display values + a
test-fit to confirm.
