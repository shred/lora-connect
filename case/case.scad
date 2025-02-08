/*
 * LoRa-Connect
 *
 * Copyright (C) 2023 Richard "Shred" Körber
 *   https://codeberg.org/shred/lora-connect
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


// Different renderings, only set ONE of them to true.
renderAll       = true;     // Renders the closed case (including the module)
renderTop       = false;    // Renders the top part only
renderBottom    = false;    // Renders the bottom part only
renderOpen      = false;    // Renders both parts of the opened case

// Configuration options
inserts         = false;    // If true, inserts are used for mounting the case screws
oledWindow      = true;     // If true, a window for the OLED is added
openAntenna     = true;     // If true, an opening for the WiFi/BT antenna is added
buttons         = true;     // If true, reset and PRG buttons are inserted


/* ======================================================================
 * NO SERVICABLE PARTS BELOW.
 */

wall = 1.2;
$fn = 60;

module board() {
    import("lora32.stl");
}

module roundSquare(width, radius) {
	hull() {
		translate([width/2 - radius, width/2 - radius, 0]) circle(radius);
		translate([width/2 - radius, radius - width/2, 0]) circle(radius);
		translate([radius - width/2, width/2 - radius, 0]) circle(radius);
		translate([radius - width/2, radius - width/2, 0]) circle(radius);
	}
}

module trapez(w1, w2, h, d) {
    translate([h/2, -w1/2, -d/2]) linear_extrude(height=d) {
        polygon(points=[[0, 0], [0, w1], [h, w1+(w2-w1)/2], [h, -(w2-w1)/2], [0, 0]]);
    }
}

module buttonCutout(h) {
    linear_extrude(height=h) {
        difference() {
            roundSquare(8, 1);
            roundSquare(7, 1);
        }
    }
}

module button(h) {
    plungerDepth = 2.5;
    translate([0, -4, h/2]) cube([4, 4, h], center=true);
    translate([-3.5, 0, -plungerDepth]) cylinder(d=4, h=plungerDepth);
}

module buttonsCutout(h) {
    translate([0, 8, 1]) buttonCutout(h);
    translate([0, -8, 1]) mirror([0, 1, 0]) buttonCutout(h);
}

module buttons(h) {
    translate([0, 8, 1]) button(h);
    translate([0, -8, 1]) mirror([0, 1, 0]) button(h);
}

module window(bottomW, bottomD, topW, topD, height) {
    CubePoints = [
        [ -bottomW/2, -bottomD/2, -height/2 ],  // 0
        [  bottomW/2, -bottomD/2, -height/2 ],  // 1
        [  bottomW/2,  bottomD/2, -height/2 ],  // 2
        [ -bottomW/2,  bottomD/2, -height/2 ],  // 3
        [ -   topW/2, -   topD/2,  height/2 ],  // 4
        [     topW/2, -   topD/2,  height/2 ],  // 5
        [     topW/2,     topD/2,  height/2 ],  // 6
        [ -   topW/2,     topD/2,  height/2 ]   // 7
    ];
    
    CubeFaces = [
        [0,1,2,3],  // bottom
        [4,5,1,0],  // front
        [7,6,5,4],  // top
        [5,6,2,1],  // right
        [6,7,3,2],  // back
        [7,4,0,3]   // left
    ];
    
    polyhedron(CubePoints, CubeFaces);
}

module connector(d, w, h) {
    hull() {
        translate([-w/2, 0, 0]) cylinder(d=d, h=h);
        translate([w/2, 0, 0]) cylinder(d=d, h=h);
    }
}

module screwPost() {
    translate([0, 0, -wall]) cylinder(d=6.5, h=12 + 2*wall);
}

module screwPostCutout() {
    screwLength = 10.0;

    // screw head
    translate([0, 0, 12 + wall - 2.4])
        cylinder(d=4.7, h=2.4);

    // screw thread and insert
    if (inserts) {
        translate([0, 0, 12 + wall - 2.4 - screwLength])
            cylinder(d=2.8, h=screwLength);
        translate([0, 0, 12 + wall - 2.4 - screwLength])
            cylinder(d=4.0, h=5.7 + 0.4);
    } else {
        translate([0, 0, 12 + wall - 2.4 - 4.3])
            cylinder(d=2.8, h=4.3);
        translate([0, 0, 12 + wall - 2.4 - 4.3 - 6])
            cylinder(d=2.8, h=6, $fn=5);
    }
}

module screwPosts() {
    translate([-64/2, -30/2, -12/2]) screwPost();
    translate([ 64/2, -30/2, -12/2]) screwPost();
    translate([-64/2,  30/2, -12/2]) screwPost();
    translate([ 64/2,  30/2, -12/2]) screwPost();
}

module screwPostCutouts() {
    translate([-64/2, -30/2, -12/2]) screwPostCutout();
    translate([ 64/2, -30/2, -12/2]) screwPostCutout();
    translate([-64/2,  30/2, -12/2]) screwPostCutout();
    translate([ 64/2,  30/2, -12/2]) screwPostCutout();
}

module frame() {
    difference() {
        cube([64, 30, 12], center=true);
        translate([-(64+2)/2, 0, 0]) trapez(13, 9, 1.2, 12);
    }
}

module caseCutouts() {
    // USB connector
    translate([-33.5, 0, 1.7]) rotate([90, 0, 0]) rotate([0, 90, 0]) connector(3.5, 4.7, 4);

    // Display
    if (oledWindow) {
        translate([0.5, 1.5, 6.6]) window(24, 13, 25, 14, 1.2);
    }

    // Make sure board will fit
    translate([-5.3, 0, -1]) board();

    // WiFi/BT Antennta
    if (openAntenna) {
        translate([-18.4, 5, 1.9]) cylinder(d=6.2, h=8);
    } else {
        translate([-18.4, 5, 1.9]) cylinder(d=6.2, h=5);
    }

    // LoRa Antenna
    translate([24, 12, 0]) rotate([0, 90, 90]) difference() {
        cylinder(d=6.5, h=5);                                       // drill hole
        translate([6.0, 0, 2.5]) cube([6.5, 6.5, 5], center=true);  // fixation corner
    }

    // buttons
    if (buttons) {
        translate([-27.5, 0, 0]) buttonsCutout(10);
    }
}

module case() {
    translate([5, 0, 1]) {
        difference() {
            union() {
                difference() {
                    minkowski() {
                        frame();
                        sphere(r=wall);
                    }
                    frame();
                }
                translate([26, 15, 0]) cube([16, 2, 12], center=true);  // antenna reinforcement
                screwPosts();
            }
            caseCutouts();
            screwPostCutouts();
        }
        if (buttons) {
            // buttons
            translate([-25, 0, 5]) buttons(1);
        }
    }
}

module pcbHolder() {
    length = 50;
    difference() {
        translate([(length-64)/2, 0, 0]) difference() {
            cube([length, 30, 12], center=true);                            // outer frame
            cube([length, 20, 12], center=true);                            // inner spare
            translate([-0.3, 0, 0]) cube([48.4, 25.4, 1.8], center=true);   // board
            translate([-21, 0, 6]) cube([10, 25.5, 10.3], center=true);     // room for buttons
            translate([7.5, -10.2, 6]) cube([15, 5, 10.3], center=true);    // room for OLED wire
        }
        screwPostCutouts();
    } 
}

module caseInside() {
    translate([5, 0, 1]) intersection() {
        union() {
            pcbHolder();
        }
        cube([64, 30, 12], center=true);
    }
}

module everything() {
    case();
    caseInside();
}

module shellShape() {
    translate([0, 0, -4.5]) cube([100, 100, 12], center=true);
    translate([5, 0, -4]) difference() {
        cube([90, 90, 13], center=true);
        cube([65, 31, 13], center=true);
        screwPosts();
    }

    translate([-25, 0, 0]) cube([10, 11, 5], center=true);
}

if (renderAll) {
    color("blue") board();
    everything();
}

if (renderTop) {
    rotate([180, 0, 0]) difference() {
        everything();
        shellShape();
    }
}

if (renderBottom) {
    intersection() {
        everything();
        shellShape();
    }
}

if (renderOpen) {
    rotate([180, 0, 0]) translate([0, -40, -2]) difference() {
        everything();
        shellShape();
    }

    intersection() {
        everything();
        shellShape();
    }
}
