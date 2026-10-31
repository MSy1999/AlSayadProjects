# Ordering the MiniDrone prototype PCB

Upload `MiniDrone_RevC_THT_60x60_Prototype_Gerbers.zip` to a PCB manufacturer's
standard bare-PCB quote page. The ZIP contains seven Gerber layers and separate
plated/non-plated drill files, with matching absolute coordinates. It does not
contain assembly instructions or a component purchase list.

This package is for reviewing quotes and production previews. Before ordering,
confirm the actual Super Mini/GY-521 dimensions, BC337 pinout and drive current,
motor startup/stall current, battery discharge capability and connector ratings.
The clean KiCad DRC report does not establish those physical/electrical ratings.

Suggested quote settings for the existing layout:

| Setting | Value |
| --- | --- |
| Service | Standard PCB fabrication / PCB only |
| Material | FR-4 |
| Copper layers | 2 |
| Finished outline | 60 x 60 mm |
| Board thickness | 1.0 mm |
| Finished copper | 1 oz per side, prototype assumption |
| Quantity | 5 prototypes, or another small batch offered by the vendor |
| Solder mask / silkscreen | Green / white |
| Finish | Lead-free HASL, or ENIG if preferred |
| Delivery format | Individual boards; no customer panel |
| Stencil | Not needed for hand soldering |
| Controlled impedance / blind vias / filled vias | Not required |

The Gerber outline's centerline is exactly 60 x 60 mm. Some tools include its
0.05 mm drawing width in the detected dimensions and may show 60.05 mm. Check
the finished outline dimensions in the production preview rather than scaling
or modifying the Gerbers.

Verify that the manufacturer's preview shows both copper layers, the antenna
copper-free region at the rear edge, and four 2.2 mm non-plated mounting holes.
The exported drill files contain 106 plated component holes and four non-plated holes.
Do not mirror the back copper or drill files before uploading; their coordinates
are already prepared for fabrication.

PCB-only fabrication delivers the empty board. Purchase and solder the
resistors, capacitors, diodes, BC337s and connectors separately, then fit the
existing Super Mini and GY-521 boards. Revision C uses only through-hole carrier
components and header sockets; no SMD tools or stencil are needed. The previous
revision B SMD package is obsolete and archived under `references/revision-B-SMD/`.
See `../../HAND_ASSEMBLY.md` for the required physical part sizes.
A component-assembly service additionally
requires exact manufacturer part numbers, an assembly-ready BOM and placement
files; the existing generic `BOM.csv` is not a complete turnkey assembly order.

Useful official pages:

- [PCBWay standard PCB quote](https://www.pcbway.com/orderonline.aspx)
- [JLCPCB Gerber preparation](https://jlcpcb.com/help/article/gerber-files-preparation)
- [AISLER KiCad support](https://community.aisler.net/t/kicad/88)

Compare the full delivered cost, including shipping and applicable VAT/taxes.
Review the manufacturer's generated production preview before paying or
approving production.

`package-validation.json` records source/ZIP hashes, outline dimensions and
drill counts. Drill-map SVGs and the drill report are available alongside the
exports for local review and are intentionally excluded from the upload ZIP.
