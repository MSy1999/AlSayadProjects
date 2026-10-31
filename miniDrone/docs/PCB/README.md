# MiniDrone PCB schematic

Open `Esp32SuperMini/Esp32SuperMini.kicad_pro` in KiCad 10, then open its
schematic. If the schematic is already open, close and reopen it so KiCad
loads the updated file. The original empty schematic is saved in
`Esp32SuperMini/references/original-empty.kicad_sch`.

This is a carrier for the existing ESP32-C3 Super Mini and GY-521/MPU6050 boards,
using four BC337 low-side motor switches and your 1S 3.7 V, 400 mAh battery.
The schematic and routed PCB are prototypes for review; module fit and motor
current ratings still require validation before manufacture.

Revision D uses a 70 x 70 mm board with extra space for hand soldering and
through-hole carrier components: ordinary 1/4 W axial resistors,
leaded capacitors and diodes, BC337s, and solderable header/socket holes for both
modules. No SMD assembly is required on the carrier. See
[hand assembly and part sizes](HAND_ASSEMBLY.md). The previous SMD revision is
archived under `Esp32SuperMini/references/revision-B-SMD/`.

The schematic uses one A2 sheet with continuous visible wires between the battery,
power components, controller, IMU and all four motor stages. Labels name the
wires; no connection relies on matching labels. A dot marks a joined branch;
crossing wires without a dot remain separate. The controller symbol now groups
its pins by function to simplify wiring, while preserving the physical header
pad numbers listed below.

## Files

- `Esp32SuperMini.kicad_sch`: editable schematic with all four motor stages.
- `MiniDrone.kicad_sym`: project-local Super Mini and GY-521 module symbols.
- `MiniDrone.pretty/ESP32_C3_SuperMini_THT.kicad_mod`: provisional carrier footprint.
- `sym-lib-table` and `fp-lib-table`: relative library paths; no global installation needed.
- `preview/Esp32SuperMini.svg` and `.png`: rendered schematic previews.
- `BOM.csv`: components, assigned footprints and unresolved design notes.
- `MiniDrone.net.xml`: exported electrical connectivity.
- `erc-report.json`: KiCad electrical rules report.

Paths above are relative to `Esp32SuperMini/`. The project now includes a routed
70 x 70 mm PCB with two copper layers, ground pours, an antenna copper keepout,
four M2 mounting holes and labeled motor connectors at the corners. See
[PCB layout details](PCB_LAYOUT.md) for geometry, previews and validation results.

## If KiCad reports the Super Mini footprint is not found

The project library is `MiniDrone`, with path `${KIPRJMOD}/MiniDrone.pretty`.
If the editors were already open when the library files were created, refresh
the loaded table:

1. Close the **Update PCB from Schematic** dialog.
2. In PCB Editor, open **Preferences > Manage Footprint Libraries** and select
   **Project Specific Libraries**. Confirm that `MiniDrone` is enabled and its
   path is `${KIPRJMOD}/MiniDrone.pretty`.
3. Click **OK** to reload the footprint libraries, then press **F8** again.

This refresh was verified in the running KiCad session: U1 was found and the
update preview reported zero warnings and zero errors. Keep the `.pretty`
directory and `fp-lib-table` alongside the project when copying it elsewhere.

## The Super Mini symbol

The Super Mini is a development board, not the ESP32-C3 IC and not an
ESP32-C3-MINI-1 RF module. Those components have different pinouts and footprints.
Use `MiniDrone:ESP32_C3_SUPER_MINI` in this project. It represents the headers;
the regulator, USB connector, crystal, reset, boot button and antenna remain on
the existing plug-in board.

Looking from the top with the USB connector at the top:

| Left pad, top to bottom | Signal | Right pad, top to bottom | Signal |
| --- | --- | --- | --- |
| 1 | GPIO5 / IMU SCL | 16 | 5V |
| 2 | GPIO6 / M1 front left | 15 | GND |
| 3 | GPIO7 / M2 front right | 14 | 3V3 |
| 4 | GPIO8 / unused | 13 | GPIO4 / IMU SDA |
| 5 | GPIO9 / unused | 12 | GPIO3 / unused |
| 6 | GPIO10 / M3 rear right | 11 | GPIO2 / unused |
| 7 | GPIO20 / M4 rear left | 10 | GPIO1 / unused |
| 8 | GPIO21 / unused | 9 | GPIO0 / battery ADC |

These are carrier pad numbers, not chip pin numbers. They match the project
pinout image and firmware. Before making a board, measure the physical Super
Mini: this draft assumes 2.54 mm header pitch, 15.24 mm row spacing and an
18 x 22.5 mm board outline. Clone dimensions can differ. Check the USB overhang
and socket height. The antenna area is marked on `Dwgs.User`; create an actual
all-layer copper/track/via keepout during layout and verify its extent against
the physical antenna. The library drawing alone is not an enforced keepout;
the routed PCB includes a separate all-copper-layer rule area at the antenna.

U2 uses the assumed GY-521 header order VCC, GND, SCL, SDA, XDA, XCL, AD0, INT.
Verify this order on your board. Its footprint describes the 8-pin header only;
the module outline and mounting holes have not been measured.

## Power circuit

Battery positive -> J1 pin 1 -> external main switch SW1 -> `VBAT_SW`.
The four motor positive terminals connect to `VBAT_SW`; motor negative terminals
connect to their BC337 collectors. All emitters and logic grounds share GND.

J2 is a **wired interface to an external regulated 5 V boost module**, not the
footprint of a particular regulator. Pin 1 = battery input positive, 2 = input
ground, 3 = regulated 5 V output, 4 = output ground. Choose a module verified to
deliver at least 0.5 A at 5 V across the usable 1S battery range. Confirm its pin
order and connect with wires as needed. Its internal circuit is not implemented
on this carrier.

JP1 connects the boost output to the Super Mini's 5V input; its existing onboard
regulator supplies 3V3 to the controller and sensor. This deliberately uses the
development board's 5V input rather than assuming its regulator tolerates power
injected into 3V3. It differs from the earlier generic buck-boost proposal in
`../WIRING.md`.

- Battery operation: USB unplugged, JP1 removable shunt installed.
- USB programming: battery switch OFF, JP1 shunt REMOVED, then connect USB.
- JP1 is a manual isolation link, not automatic USB/battery power OR-ing.

The battery must have appropriate protection and discharge capability. Capacity
alone does not specify discharge current. No charger, automatic low-voltage
cutoff or reverse-polarity protection is included. The current sensor-monitor
firmware does not sample the battery divider or implement battery protection.

## BC337 motor drivers: values to verify

Each channel has a 330 ohm series base resistor (provisional), 10 kohm
base-emitter pulldown, leaded 1N5819 Schottky flyback diode (provisional), and 100 nF
ceramic suppression capacitor. The diode cathode/striped end goes to battery
positive and its anode goes to the collector/motor negative. Place the 100 nF
at the physical motor terminals; a distant PCB capacitor is not equivalent.

The selected BC337 footprint assumes onsemi's pin numbering: 1 collector,
2 base, 3 emitter. Verify the datasheet for your actual supplier. Do not reuse
a MOSFET pin assignment for a BC337.

At 3.3 V, 330 ohm gives roughly 6-8 mA of base drive depending on the base
voltage. This does not guarantee saturation at the unknown motor startup or
stall current. The onsemi BC337 datasheet specifies its saturation test at
500 mA collector current with 50 mA base current; the 800 mA absolute maximum
collector rating is not a practical design target. Do not reduce the resistor
to demand that base current directly from an ESP32 pin. Measure startup/stall
current, transistor voltage drop and temperature; use a suitable 3.3 V
logic-level MOSFET or proper driver if the BC337 stage is insufficient.

The 1N5819 selection, main switch, battery/motor connectors, copper widths and
battery discharge rating must also be checked against measured current. The
2.54 mm header footprints are placeholders for prototype wiring; they are not
verified high-current connector selections. C1's 470 uF value is a starting
point for measuring motor-induced supply transients, not a validated result.

R9/R10 are marked DNP in KiCad. Populate the optional 4.7 kohm I2C pull-ups only
if the GY-521 does not already supply suitable pull-ups to 3.3 V.

## Validation and regeneration

Validated with KiCad CLI 10.0.7: schematic loaded, SVG exported, and ERC produced
zero errors and zero warnings. `validate_schematic.py` checks the exported nets
against `main/board.h`, all four driver topologies, diode polarity, controller
power, sensor connections and battery divider. It also checks that every
assigned footprint exists. These checks do not validate thermal ratings or fit.
Revision B also compares every exported net's component/pin membership against
revision A and checks that there are no repeated or off-sheet labels, so the
connections are preserved using the visible wires.

`generate_schematic.py` regenerates the schematic, custom library, footprint
and BOM. **It overwrites the generated schematic**, so save manual edits
elsewhere before running it. `inspect_symbols.py` supplies its library reader.
`wired_layout.py` defines the fully connected page arrangement. The generator
uses KiCad's standard symbol libraries at the installed Windows
KiCad 10 path; edit `BASE` if using another installation.

```powershell
python docs/PCB/generate_schematic.py
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' sch erc --format json --output docs/PCB/Esp32SuperMini/erc-report.json docs/PCB/Esp32SuperMini/Esp32SuperMini.kicad_sch
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' sch export netlist --format kicadxml --output docs/PCB/Esp32SuperMini/MiniDrone.net.xml docs/PCB/Esp32SuperMini/Esp32SuperMini.kicad_sch
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' sch export svg --output docs/PCB/Esp32SuperMini/preview docs/PCB/Esp32SuperMini/Esp32SuperMini.kicad_sch
python docs/PCB/validate_schematic.py
python docs/PCB/render_preview.py
```

The optional PNG renderer needs `resvg-py`; the editable schematic and SVG do
not depend on it. The generator makes no firmware changes.

## Circuit references

- [Super Mini pinout and board notes in Apache NuttX](https://nuttx.incubator.apache.org/docs/latest/platforms/risc-v/esp32c3/boards/esp32c3-supermini/index.html).
- [Supplier-hosted Super Mini pinout/schematic PDF](https://dl.artronshop.co.th/ESP32-C3%20SuperMini%20datasheet.pdf): a reference link for comparing your board variant; the remote PDF download timed out in this session, so it was not used to verify internal circuitry.
- [ded811's editable KiCad Super Mini circuit](https://github.com/ded811/ESP32-C3-Super-Mini-Kicad): a third-party redesign with antenna changes, not guaranteed identical to the purchased board.
- [Espressif ESP32-C3 hardware design checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/schematic-checklist.html): supply and GPIO guidance.
- [onsemi BC337 datasheet](https://www.onsemi.com/pdf/datasheet/bc337-fsc-d.pdf): assumed transistor pinout and saturation conditions.
- [Vishay 1N5819 datasheet](https://www.vishay.com/docs/88525/1n5817.pdf): leaded DO-41 Schottky package and cathode stripe.

Local module symbols and the carrier footprint were created for this project.
Cached standard component symbols originate from the installed KiCad standard
libraries; their library license/exception applies.
