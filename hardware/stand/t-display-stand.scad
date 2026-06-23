// ============================================================================
//  T-Display desk stand  —  parametric tilted cradle
//  For the LilyGO T-Display (1.14" ST7789, ESP32) and dimensional clones.
//
//  The board's bottom long edge drops into an angled slot and leans back so the
//  screen faces up toward you on a desk. The right end is left open for the
//  USB-C cable. Designed to print support-free on a Bambu Lab A1 (flat side down).
//
//  Units: millimeters.
// ============================================================================

/* ---------- measured board facts (confirmed) ---------- */
board_len   = 51.49;   // long edge — sits along the slot   [from LilyGO drawing]
board_thick = 1.8;     // PCB thickness used for the slot. Bare PCB ~1.6mm; this
                       // leaves a little room for parts near the bottom edge.

/* ---------- fit & feel — these are the knobs to tweak ---------- */
tilt_deg       = 22;   // lean-back angle from vertical (bigger = screen faces up more)
slot_clearance = 1.6;  // extra slot WIDTH: easy insert + clone tolerance
side_clearance = 1.2;  // extra slot LENGTH so the board drops in without forcing
insert_depth   = 6;    // how deep the bottom edge sits in the slot (grip vs. screen coverage)

/* ---------- stand body ---------- */
left_wall   = 4;       // solid wall on the LEFT end (acts as a locating stop)
base_depth  = 32;      // front-to-back footprint (more = more stable vs. the lean)
base_height = 11;      // height of the block the slot is cut into
corner_r    = 3;       // rounded vertical corners (looks + comfort)
taper       = 2;       // top is inset this much all around (a gentle wedge look)

/* ---------- derived ---------- */
slot_w   = board_thick + slot_clearance;   // slot thickness (front-to-back)
slot_len = board_len + side_clearance;     // slot length (left-to-right)
body_w   = left_wall + slot_len;           // RIGHT end deliberately left open for USB-C
floor_z  = base_height - insert_depth;     // board rests this high above the desk
slot_y   = 11;                             // slot position from the front face

$fn = 64;

// Rounded rectangle (footprint).
module rrect(w, d, r) {
    hull() {
        translate([r,     r    ]) circle(r);
        translate([w - r, r    ]) circle(r);
        translate([r,     d - r]) circle(r);
        translate([w - r, d - r]) circle(r);
    }
}

// Gently tapered block (bottom full size, top inset by `taper`) — prints flat,
// overhang-free, and looks like a stand rather than a brick.
module base() {
    hull() {
        linear_extrude(height = 0.1) rrect(body_w, base_depth, corner_r);
        translate([taper, taper, base_height - 0.1])
            linear_extrude(height = 0.1) rrect(body_w - 2*taper, base_depth - 2*taper, corner_r);
    }
}

// The angled slot. Leans back (top toward the rear) so the screen faces up.
module slot_cut() {
    slab_h = base_height + insert_depth + 30;        // tall enough to cut clear through the top
    translate([left_wall, slot_y, floor_z])          // pocket bottom sits at floor_z
        rotate([-tilt_deg, 0, 0])                    // lean the slot back
            translate([0, -slot_w / 2, 0])
                cube([slot_len + 12, slot_w, slab_h]);  // +12 opens the RIGHT (USB-C) end
}

difference() {
    base();
    slot_cut();
}

/* ---------- preview only (NOT printed) ----------
   Render with:  openscad -D show_board=true ...
   Draws the board + a screen rectangle sitting in the slot so you can see fit. */
show_board   = false;
board_h_vis  = 25.09;   // board short edge (height as it leans)

module board_ghost() {
    translate([left_wall + side_clearance/2, slot_y, floor_z])
        rotate([-tilt_deg, 0, 0]) {
            color([0.12, 0.12, 0.13])                              // dark PCB
                translate([0, -board_thick/2, 0])
                    cube([board_len, board_thick, board_h_vis]);
            color([0.72, 0.47, 0.34])                              // screen (Claude clay)
                translate([3, -board_thick/2 - 0.45, 5.5])
                    cube([board_len - 6, 0.5, 15]);
        }
}

if (show_board) board_ghost();
