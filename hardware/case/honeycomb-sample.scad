// Honeycomb panel prototype — to choose cell size / density for the case back.
// Reusable: the hex_panel() module gets lifted into the real back cover later.
// Render a density with:  openscad -D hole_r=4.5 ...

W        = 56.3;   // back-cover-ish width
H        = 29.9;   // back-cover-ish height
th       = 2;      // panel thickness
hole_r   = 3;      // hex hole circumradius — CHOSEN: fine/small hexes (override with -D hole_r=..)
rib      = 1.2;    // wall left between holes
border   = 4;      // solid frame around the edge
corner_r = 3;

$fn = 64;

module rrect(w, d, r) {
    hull() {
        translate([r,     r    ]) circle(r);
        translate([w - r, r    ]) circle(r);
        translate([r,     d - r]) circle(r);
        translate([w - r, d - r]) circle(r);
    }
}

module hex_holes(w, h, R, gap, t) {
    dx = 1.5 * R + gap;
    dy = sqrt(3) * R + gap;
    for (i = [-1 : ceil(w / dx) + 1])
        for (j = [-1 : ceil(h / dy) + 1]) {
            x = i * dx;
            y = j * dy + ((i % 2 != 0) ? dy / 2 : 0);
            translate([x, y, -1]) cylinder(h = t + 2, r = R, $fn = 6);
        }
}

// A flat panel with a clipped honeycomb field (solid border kept).
module hex_panel(w, h, t, R, gap, b) {
    difference() {
        linear_extrude(t) rrect(w, h, corner_r);
        intersection() {
            hex_holes(w, h, R, gap, t);
            translate([b, b, -1]) cube([w - 2*b, h - 2*b, t + 2]);
        }
    }
}

hex_panel(W, H, th, hole_r, rib, border);
