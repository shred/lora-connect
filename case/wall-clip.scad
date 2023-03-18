/*
 * LoRa-Connect
 *
 * Copyright (C) 2023 Richard "Shred" Körber
 *   https://github.com/shred/lora-connect
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

innerW = 30;
innerD = 32.6;
innerH = 14.6;

screwDiam = 3.7;
headDiam = 6.8;
headHeight = 2.0;

clipDiam = 2.4;
base = 3;
wall = 1.6;

module clip() {
    rotate([0, 90, 0]) cylinder(d=clipDiam, h=innerW);
}

effectiveInnerH = innerH - (clipDiam / 1.7)/2;
recess = (clipDiam - wall)/2;

difference() {
    offsetZ = (effectiveInnerH + base + clipDiam/2)/2;
    translate([0, 0, offsetZ]) cube([innerW, innerD + wall * 2, effectiveInnerH + base + clipDiam/2], center=true);
    translate([0, 0, offsetZ + base/2]) cube([innerW, innerD, effectiveInnerH + clipDiam/2], center=true);
    cylinder(d = screwDiam, h=base);
    translate([0, 0, base - headHeight]) cylinder(d1 = screwDiam, d2=headDiam, h=headHeight);
}

translate([-innerW/2,  innerD/2 + recess, effectiveInnerH + base + clipDiam/2]) clip();
translate([-innerW/2, -innerD/2 - recess, effectiveInnerH + base + clipDiam/2]) clip();

$fn = 60;
