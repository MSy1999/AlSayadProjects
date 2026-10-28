# Wiring — conditional two-wire BRUSHED motor backend

## Internet pinout
[Original PNG — Chiptron.cz](https://chiptron.cz/wp-content/plugins/chiptron-pinouts/boards/esp32-c3-super-mini/esp32-c3-super-mini-pinout-cs.png?v=1791750871)
Source: https://chiptron.cz/pinouty/esp32-c3-super-mini-pinout/ . CC BY 4.0; unmodified source image. Compare the board labels with your physical variant. The local downloader could not resolve the source host, so the original is linked rather than copied.
GPIO functions are routed in firmware; printed default SDA/SCL need not be used.

| C3 connection | Destination |
|---|---|
| GPIO6 | M1 front-left MOSFET gate via 100 ohm |
| GPIO7 | M2 front-right MOSFET gate via 100 ohm |
| GPIO10 | M3 rear-right MOSFET gate via 100 ohm |
| GPIO20 | M4 rear-left MOSFET gate via 100 ohm |
| GPIO4 | MPU6050 SDA |
| GPIO5 | MPU6050 SCL |
| GPIO0 | Battery divider midpoint |
| 3V3 | Regulated 3.3 V and IMU supply |
| GND | Battery negative, regulator ground, MOSFET sources, IMU ground |

For EACH motor: battery+ -> motor+; motor- -> N-MOSFET drain; MOSFET source -> battery-. Gate from GPIO through 100 ohm; 10 kohm gate-source pulldown. Flyback Schottky diode cathode at battery+, anode at drain. Select diode and 3.3 V gate MOSFET against measured motor stall current. Add 100 nF directly across brushed motor terminals. Never assume a generic MOSFET turns on adequately at 3.3 V: verify Rds(on) at 2.5/3.3 V gate voltage. Motors may be direction-specific; swap wires only if their specifications allow.

M1/M3 CW, M2/M4 CCW viewed from above. Install matching propellers only after direction and stabilization signs are verified. Front = IMU +X. Positive roll = right side down; positive pitch = nose up; positive yaw = clockwise viewed from above. Mixer corrections raise FL/RL for positive roll torque, FL/FR for positive pitch torque, and CCW rotors for positive yaw torque.

Battery: 1S 3.7 V nominal / 4.2 V full. Connect battery through rated switch to motor positive and a regulated 3.3 V buck-boost supply for logic. Rate regulator for radio current transients with margin. Add local 100 nF plus suitable bulk capacitance to logic and motor supply. Do not use solderless breadboards for motor current. Do not connect battery to 3V3 or assume its voltage is enough for the 5V input regulator. Verify USB/external power isolation for your board before simultaneous connection.

Divider: battery+ -> 100 kohm -> GPIO0 -> 100 kohm -> GND; 100 nF from GPIO0 to GND. ADC calibration is required; verify actual battery voltage with a multimeter. No charger is included: use an appropriate 1S lithium charger. Discharge rating, motor current and aircraft weight remain unverified.

IMU: use a 3.3 V compatible MPU6050 breakout; SDA/SCL need pullups to 3.3 V (typically 4.7 kohm, accounting for existing board pullups). AD0 to GND selects 0x68. Leave XDA/XCL unconnected. This implementation polls; INT is unused. Test regulator and breakout behavior rather than assuming all GY-521 boards are equivalent.

If the motors truly have internal brushless commutation, stop: this MOSFET PWM arrangement is not verified compatible. A bare three-phase brushless motor requires an ESC backend, not this circuit.
