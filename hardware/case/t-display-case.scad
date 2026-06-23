// ============================================================================
//  T-Display enclosed case  —  PART 1: front bezel tray  (also the TEST-FIT piece)
//
//  Holds the bare LilyGO T-Display (51.49 x 25.09 mm). The board drops in from
//  the back, screen facing out through the window; USB-C exits one short end.
//  This is the fit-critical part — print THIS first, verify, then we add the
//  tilted enclosed back.
//
//  Print bezel-face DOWN on the bed → clean screen-side, no supports.
//  Units: mm.  Numbers marked (TUNE) are first estimates to confirm on the test print.
// ============================================================================

/* ---------- board (confirmed) ---------- */
board_len    = 51.49;   // long edge (X)
board_w      = 25.09;   // short edge (Y)

/* ---------- fit (TUNE on the test print) ---------- */
cavity_depth = 7.0;     // (TUNE) bezel back-face to the rear opening: PCB + back parts + room
clear        = 0.4;     // per-side gap around the board (drop-in fit)

/* ---------- shell ---------- */
wall    = 2.0;          // side wall thickness
bezel_t = 1.6;          // front bezel thickness (frames the screen)
corner_r= 3;            // outer rounded corners

/* ---------- screen window (TUNE position on the test print) ---------- */
win_x   = 27;           // window along board LENGTH (active ~25.6 + margin)
win_y   = 16;           // window along board WIDTH  (active ~14.5 + margin)
win_off = -3;           // window centre offset along length, toward the far-from-USB end

/* ---------- USB-C cutout (on the +X short end) ---------- */
usb_w   = 11;           // opening width  (USB-C ~9 + clearance)
usb_z   = 5.5;          // opening height (through the board-thickness direction)

/* ---------- preview only ---------- */
show_board = false;     // render with: openscad -D show_board=true ...

$fn = 64;

cav_x = board_len + 2*clear;
cav_y = board_w   + 2*clear;
out_x = cav_x + 2*wall;
out_y = cav_y + 2*wall;
out_z = bezel_t + cavity_depth;

module rrect(w, d, r) {
    hull() {
        translate([r,     r    ]) circle(r);
        translate([w - r, r    ]) circle(r);
        translate([r,     d - r]) circle(r);
        translate([w - r, d - r]) circle(r);
    }
}

module front_tray() {
    difference() {
        // outer shell
        linear_extrude(out_z) rrect(out_x, out_y, corner_r);

        // board cavity (open at the back, +Z)
        translate([wall, wall, bezel_t]) cube([cav_x, cav_y, cavity_depth + 1]);

        // screen window through the bezel
        translate([out_x/2 + win_off - win_x/2, out_y/2 - win_y/2, -1])
            cube([win_x, win_y, bezel_t + 2]);

        // USB-C cutout in the +X end wall
        translate([out_x - wall - 1, out_y/2 - usb_w/2, bezel_t])
            cube([wall + 2, usb_w, usb_z]);
    }
}

module board_ghost() {
    translate([wall + clear, wall + clear, bezel_t]) {
        color([0.12, 0.12, 0.13]) cube([board_len, board_w, 1.6]);        // PCB
        // lit screen, drawn under the window opening (preview coherence)
        color([0.72, 0.47, 0.34])
            translate([out_x/2 + win_off - (wall + clear) - 25.6/2,
                       out_y/2 - (wall + clear) - 14.5/2, -bezel_t - 0.2])
                cube([25.6, 14.5, bezel_t]);
    }
}

front_tray();
if (show_board) board_ghost();
