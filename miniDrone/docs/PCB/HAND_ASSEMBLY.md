# Revision D: through-hole hand assembly

The 70 x 70 mm carrier uses ordinary leaded components. Insert their leads through
the plated holes from the top and solder on the underside. The existing ESP32
and MPU boards retain their own factory-fitted components.

| Parts | Type and value | Physical footprint / lead spacing |
| --- | --- | --- |
| R1-R4 | 330 ohm, 1/4 W axial, provisional motor base resistance | Common kit resistor, about 6.3 x 2.5 mm body; leads bent to 7.62 mm |
| R5-R8 | 10 kohm, 1/4 W axial | Same 7.62 mm spacing |
| R9-R10 | Optional 4.7 kohm, 1/4 W axial | Same spacing; leave empty if GY-521 already has suitable 3.3 V pull-ups |
| R11-R12 | 100 kohm, 1%, 1/4 W axial | Same 7.62 mm spacing |
| C1 | 470 uF, at least 10 V, radial electrolytic | 6.3 mm diameter, 2.5 mm lead spacing |
| C3 | 22 uF, at least 16 V, radial electrolytic | 5 mm diameter, 2 mm lead spacing |
| C5 | 10 uF, at least 10 V, radial electrolytic | 5 mm diameter, 2 mm lead spacing |
| C2, C4, C6-C10 | 100 nF (often marked 104), at least 16 V, ceramic disc | About 5 mm body, 5 mm lead spacing; no polarity |
| D1-D4 | 1N5819 Schottky, leaded DO-41 | 10.16 mm lead spacing; stripe goes to the K end of the PCB outline |
| Q1-Q4 | BC337, TO-92 | Spread leads to match the wide footprint; pads 1=C, 2=B, 3=E; verify supplier pinout |
| U1 | Existing ESP32-C3 Super Mini | Two 1 x 8 female socket strips, 2.54 mm pitch; rows 15.24 mm apart |
| U2 | Existing GY-521 / MPU6050 | One 1 x 8 female socket strip, 2.54 mm pitch |

U1 and U2 have 1.0 mm plated holes for standard header pins. Solder male pin
strips to the modules and female socket strips to the carrier for removable
modules. You may instead solder module pins directly into the carrier holes.
Use sockets high enough to clear components on the modules' undersides.

Print the KiCad assembly drawing at 1:1 and compare it with your actual parts
before ordering. Header order and dimensions vary between module clones. The
current ESP32 assumption is 15.24 mm between rows and 2.54 mm between pins;
the MPU header is assumed to read VCC, GND, SCL, SDA, XDA, XCL, AD0, INT.
The gyro body extends toward the center of the carrier; verify that orientation
and the firmware's sensor-axis convention with your module.

Start with resistors and diodes, then the transistors and capacitors, followed
by headers/sockets. Resistors and ceramic capacitors work either way around.
For C1, C3 and C5, put the positive lead in the pad marked +; the stripe on an
electrolytic capacitor normally marks its negative lead. For D1-D4, match the
diode's cathode stripe with the PCB's K/stripe marking. The
[Vishay 1N5819 datasheet](https://www.vishay.com/docs/88525/1n5817.pdf) identifies
this leaded package and polarity.

J1 is battery, SW1 is the wired main switch, J2 is the external 5 V boost
interface, JP1 is a removable power shunt, and J3-J6 are motors. These use
through-hole 2.54 mm headers; verify their current rating for your motors.
Fit additional 100 nF suppression capacitors directly at motor terminals when
motor wires run away from the PCB.

The schematic, PCB, BOM, previews and revision D Gerber ZIP all use the new
through-hole footprints. Earlier SMD files are archived in
`Esp32SuperMini/references/revision-B-SMD/` and should not be ordered.
The superseded 60 x 60 mm through-hole board is archived in
`Esp32SuperMini/references/revision-C-60x60-THT/`.
Electrical checks pass, but physical fit and motor startup/stall current still
need confirmation. Motor firmware has not been changed by this revision.
