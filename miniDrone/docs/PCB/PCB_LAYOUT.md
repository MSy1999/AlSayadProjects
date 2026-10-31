# MiniDrone PCB layout - revision D through-hole prototype

Open `Esp32SuperMini/Esp32SuperMini.kicad_pro`, then the PCB file with the same
name. The routed board is `Esp32SuperMini/Esp32SuperMini.kicad_pcb`.

## Board and placement

- 70 x 70 mm, two copper layers, nominal 1.0 mm board thickness.
- Four 2.2 mm M2 non-plated mounting holes on a 64 x 64 mm square pattern.
- Component centres are spread across the larger area for easier hand assembly;
  actual resistor lead spacings and module header dimensions are unchanged.
- Corners have 2 mm chamfers. The top edge is marked FRONT.
- J3/M1 front left, J4/M2 front right, J5/M3 rear right, J6/M4 rear left.
- Each BC337 and flyback diode sits close to its motor connector.
- The Super Mini is near the rear edge with its antenna facing outwards.
- The GY-521's nominal body envelope is marked on the drawings layer, near the
  center of the board and separated from the motor driver groups.
- Battery and switch connections are at the front; J2 is the wired interface
  to an external boost module, not a complete onboard boost converter.
- JP1 remains the removable battery/USB power link from the schematic.

The board uses the exact 40 electrical footprints from the schematic, plus
four board-only mounting holes. Existing imported footprints were backed up in
`Esp32SuperMini/references/before-layout.kicad_pcb` before placement changed.

## Copper and routing

Fixed 1.8 mm battery trunks run down the left and right perimeter on the front.
Battery connections from the switch and bulk capacitor use a 1.2 mm
target width; motor-negative routing uses 0.8 mm, logic supply routing 0.5 mm,
and control/I2C/ADC routing 0.25 mm. Local neckdowns may occur at component pads.
These widths are layout choices, not a certified current rating.

Ground is poured on both copper layers. High-current connector/transistor
ground pads use solid plane connections. Module ground headers and other
small through-hole ground pads use thermal reliefs to make hand soldering easier.
Motor current paths stay near their
drivers and the board edges rather than crossing through the sensor area.

An actual rule area prohibits tracks, vias and copper fills on both layers in
the central antenna region: x = 98.167-110.167 mm, y = 133.583-140.1 mm in KiCad's board
coordinates. It lies between the Super Mini header rows and reaches the rear
board edge. Its extent must be checked against the physical clone's antenna
and matching network. Keep the battery, frame metal and wiring clear of that
antenna region as well.

All 106 component holes
are plated through-holes; the four mounting holes are non-plated. Routing was performed
locally with Freerouting 2.1.0 from a KiCad DSN export, after manual placement
and fixed battery distribution. Both ground planes were then filled.
See `pcb-validation.json` for the final track and via counts. The preceding
60 x 60 mm through-hole board and its fabrication files are archived in
`references/revision-C-60x60-THT/`; use the revision D manufacturing ZIP.

## Previews and reports

All paths below are relative to `Esp32SuperMini/`:

- `preview/pcb-3d.png`: top view of the carrier assembly, with representative
  connector models. The plug-in ESP32 and GY-521 boards themselves are not
  modeled; U1 shows two carrier sockets instead.
- `preview/pcb-top.svg` / `.png`: front copper and silkscreen.
- `preview/pcb-bottom.svg` / `.png`: mirrored back copper view.
- `preview/pcb-assembly.svg` / `.png`: assembly markings and nominal envelopes.
- `placement.json`: electrical component positions and rotations.
- `drc-report.json`: final KiCad DRC and schematic-parity report.
- `pcb-validation.json`: machine-readable board validation summary.

KiCad 10.0.7 final checks report **zero DRC violations, zero unconnected items,
and zero schematic-parity issues**. The native-board validator also checks all
component/pad net assignments against the schematic export, board dimensions,
layer count, thickness, ground pours and antenna keepout.

## Physical and electrical details still to confirm

The Super Mini footprint assumes 2.54 mm header pitch, 15.24 mm row spacing and
an 18 x 22.5 mm board. The GY-521 envelope is nominal, and its exact sensor
orientation, header order, underside clearance and mounting holes have not
been measured. Confirm the sensor's +X direction against the drone front and
the firmware's axis convention when assembling the actual board.

Use two 1 x 8 female socket strips for the Super Mini and one for the GY-521.
Both use 2.54 mm pitch and 1.0 mm plated header holes. Male module pins plug
into these sockets. Confirm module orientation and plug-in height.
All resistors use the common 1/4 W axial size with 7.62 mm lead spacing;
capacitors and 1N5819 diodes are leaded as well. See [assembly details](HAND_ASSEMBLY.md).
The external boost module is connected through wires and has no reserved
mounting footprint on this carrier.

The BC337 manufacturer/pinout, motor startup/stall current, base resistance,
diode rating, switch/connector ratings and battery discharge capability remain
unverified. No thermal or motor-current test has been performed on this PCB.
The layout does not add charging, reverse-polarity protection or automatic
battery cutoff. C7-C10 are close to the motor connectors; additional suppression
capacitors should be soldered directly at the motors if wires run to them.

A prototype Gerber/drill ZIP is available in `Esp32SuperMini/fabrication/`,
with ordering instructions and file-validation details. No fabrication order
has been placed. A clean DRC checks the implemented geometry and connectivity;
it does not prove module fit or the unknown motor-current ratings.

## Editing and regeneration

Use KiCad normally to edit the routed board. The schematic generator does not
replace PCB placement or routing. `layout_pcb.py prepare`, however, resets the
board to the backed-up imported footprints and regenerates the layout, so do
not run it over manual layout work without preserving that work first.

The layout scripts require KiCad's bundled Python, for example:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' docs/PCB/layout_pcb.py prepare
```

That exports `routing/MiniDrone.dsn`. Route it locally to
`routing/MiniDrone.ses` using a compatible Freerouting installation. The tool
used here was its 2.1.0 JAR with Java 21, GUI/API server disabled and anonymous
analytics disabled (`-da`). Then import, refill, run DRC and remove any reported
unused trace ends:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' docs/PCB/layout_pcb.py finish
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' pcb drc --format json --schematic-parity --output docs/PCB/Esp32SuperMini/drc-report.json docs/PCB/Esp32SuperMini/Esp32SuperMini.kicad_pcb
& 'C:\Program Files\KiCad\10.0\bin\python.exe' docs/PCB/clean_pcb.py
```

Re-run DRC after cleanup; repeat only if it identifies further unused trace
ends. Finally run `finalize_pcb.py` for the socket models/title metadata and
`validate_pcb.py` after the last DRC report. Do not use the intermediate
placement report as the final routing result.
