# AlSayad MiniDrone - motors off and MPU6050 monitor

The current firmware reads a real MPU6050/GY-521 using ESP-IDF 6.1 on an
ESP32-C3 Super Mini. It logs acceleration, angular velocity and temperature
every 100 ms over USB Serial/JTAG. All four brushed motor driver outputs on
GPIO6, GPIO7, GPIO10 and GPIO20 are initialized at 0% PWM duty and remain off.
Existing flight and radio source files remain inactive.

The automatic motor ramp is disabled, and the maximum accepted duty is 0%.
Motor output initialization runs independently of MPU6050 initialization and
does not use the `AS_ENABLE_MOTORS` option. It requires active-high transistor drivers for
two-wire brushed motors; never connect motors directly to GPIOs. See
[motor wiring](docs/WIRING.md) for the driver and power connections.

## Wiring

| MPU6050 / GY-521 | ESP32-C3 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO4 |
| SCL | GPIO5 |
| AD0 | GND (address 0x68) |

INT, XDA and XCL are unused. Internal SDA/SCL pull-ups are enabled. External
pull-ups to **3.3 V** may be required (typically 4.7 kohm on each line,
accounting for pull-ups already on the breakout). See the
[ESP-IDF I2C guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/i2c.html)
for pull-up and probe behavior. The [drone wiring](docs/WIRING.md)
also describes the motor driver connections used by the bench test.

## Build, flash and monitor

Initialize the installed ESP-IDF 6.1 environment on this workstation:

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
$env:PYTHONUTF8 = '1'
idf.py set-target esp32c3
idf.py build
idf.py -p COM4 flash monitor
```

COM4 is the detected Espressif USB Serial/JTAG device. If the port changes,
find it with `python -m serial.tools.list_ports -v` and substitute that port.
Exit the IDF monitor with Ctrl+]. To monitor the already flashed board:

```powershell
idf.py -p COM4 monitor
```

The project already targets ESP32-C3; `set-target` is needed only for a fresh
configuration or when switching targets. It clears the build and regenerates
sdkconfig. The PowerShell profile selects the working Python environment at
`C:\Espressif\tools\python\v6.1\venv`.

## Driver and API

- `main/main.c`: motor outputs held at 0% plus the sensor initialization/retry
  loop and three ESP_LOGI lines per sample.
- `main/mpu6050.c`: modern `driver/i2c_master.h` driver at 100 kHz,
  address probe, WHO_AM_I verification (0x75 must return 0x68), wake-up,
  all axes enabled, +/-2 g accelerometer and +/-250 degrees/s gyroscope.
- `main/mpu6050.h`: reusable `mpu6050_init()`, `mpu6050_read()` and
  `mpu6050_data_t`. Fields `accel_x/y/z` are in g, `gyro_x/y/z` in
  degrees/s, and `temperature` in degrees Celsius.
- `main/board.h`: shared pin and sensor address definitions.
- `main/CMakeLists.txt`: builds the monitor and driver with
  `esp_driver_i2c`, `esp_driver_ledc`, `esp_common`, `freertos` and `log`.

Each read uses one repeated-start transaction for all 14 bytes beginning at
0x3B. Signed big-endian 16-bit values are converted using raw / 16384.0 for
acceleration, raw / 131.0 for angular velocity, and raw / 340.0 + 36.53 for
temperature. Axes follow the sensor markings without remapping, calibration
or fusion. Temperature is the sensor die temperature.

Transactions have a 100 ms timeout. Missing devices, unexpected identities
and transfer failures produce readable errors. Failed reads never print
measurements or overwrite the caller's output. After failures the application
waits one second, resets the I2C bus, probes the address and reapplies sensor
configuration. Bus/device handles are reused across retries. The API owns
I2C port 0 and must be called from one task or serialized externally.

When stationary, acceleration should be approximately 1 g in total according
to orientation; gyro readings may have a bias. Repeated detection failures
call for checking power, common ground, SDA/SCL, AD0 and pull-ups.

## Validation

On 2026-10-29, the ESP-IDF 6.1 ESP32-C3 build passed and generated
`build/miniDrone.bin` (170,352 bytes, 84% app partition space free).
Flashing COM4 succeeded with hash verification on an ESP32-C3 revision v0.4.
The IDF monitor displayed continuous real sensor frames approximately every
102-103 ms, including positive and negative values. WHO_AM_I verification
passed before sampling. The monitor was closed after validation.

One actual captured frame:

```text
I (19028) MPU6050: ACCEL X: -0.05 g | Y: 0.04 g | Z: 1.15 g
I (19029) MPU6050: GYRO  X: -2.08 °/s | Y: 1.65 °/s | Z: 0.19 °/s
I (19029) MPU6050: TEMP  19.45 °C
```

These are uncalibrated measurements. Physical disconnection/reconnection
recovery has not been tested. The existing 2 MB flash configuration was
retained; the board reports 4 MB and logs a size warning at boot.
