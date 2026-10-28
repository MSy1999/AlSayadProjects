# AlSayad MiniDrone â€” ESP32-C3

Bench firmware, not flight-validated. Motors default OFF. Two-wire motors must be identified: ordinary two-wire coreless drone motors are brushed; internally commutated two-wire brushless devices may not support this PWM driver. Never connect motors to GPIO.

## Hardware
See [wiring](docs/WIRING.md) and [wiring PNG](docs/images/drone-wiring.png).
MPU6050 is an explicit example sensor, not confirmed owned hardware. It must be mounted X forward, Y left, Z up; driver maps to flight axes X forward, Y right, Z down. Keep flat and still on boot for 2 seconds. A different IMU requires another adapter implementing imu.h.

## Build
Use an ESP-IDF 6.1 terminal:
```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```
In AlSayad MiniDrone, change the Wi-Fi password. Leave motor enable OFF for initial tests. Power motors from the battery through correctly rated drivers; never USB. Do not flash with propellers fitted.
For ESP-NOW select that mode, enter the remote STA MAC and a unique nonzero 16-byte key as 32 hex digits. Configure the same key and drone STA MAC in controller/esp32_remote using its menuconfig. Channel 6 and encrypted unicast are used. Read STA MAC with esptool read-mac or esp_wifi_get_mac(WIFI_IF_STA). No automatic pairing. Controller target is another C3; see remote.c for joystick/button pins. ADC mapping is provisional and requires calibration to actual joystick travel; a spring-centered throttle is unsuitable without a deliberate throttle mapping.

## Phone
In default phone mode join AlSayad-MiniDrone and open http://192.168.4.1. Hold ENABLE, leave throttle zero, and press ARM using another finger. Holding ENABLE sends inputs at up to 50 Hz; release sends latched STOP. Arm succeeds only with healthy IMU/battery and hardware motor opt-in. Roll/pitch/yaw sliders return to zero on release. State and battery are polled. This is a bench interface, with no altitude/position hold. Change credentials before enabling outputs. The browser acknowledges packet enqueue only; /status reports actual safety state. TCP buffering and iOS suspension must be tested before flight.

## Architecture
components/as_flight has no ESP-IDF dependency: commands, arbitration, angle estimation, cascaded PID and X mixer. as_protocol handles 24-byte little-endian serialization independently of transport. main/inputs.c adapts WebSocket or ESP-NOW into queues; only the main task owns flight state and motor writes. Input uses a one-element latest-value mailbox; discrete events use a bounded queue. Only fresh INPUT packets refresh the control watchdog. Controller/session ownership changes only while disarmed after 500 ms idle. FAULT requires reboot and never auto-rearms.
components/AlSayadLib contains a project-local radio-only WifiManager and new AnalogOutput LEDC PWM library. See its README for provenance. The external AlSayadLib folder remains unchanged; this is not a linked upstream dependency.

## Limits and verification
Rate loop target 250 Hz; local dt outside 1â€“12 ms faults. Stale commands >250 ms, failed IMU, ADC failure/low battery, excessive tilt or output errors stop outputs and latch FAULT. Battery thresholds are provisional (3.4 V loaded cutoff), not chemistry-independent. A 100 ms task watchdog resets a stalled controller; MOSFET gate pulldowns are mandatory. Hardware cutoff is needed for stronger protection against firmware hangs. Fixed gains are placeholders and output is capped at 65%; mixer saturation is simply clipped, with conservative PID integral limits, not a fully desaturating allocator. No flight-readiness claim.

Tests in tests/flight_test.c cover packet validation, replay, ownership, arming, timeout, invalid sensors/timing and mixer signs. Build with a native C compiler against components/as_flight/as_flight.c and as_protocol.c, linking libm where needed. First verify motors without propellers, sensor axes, every correction sign, supply stability, and radio-load timing. No hardware tests were performed.

## Validation on this workstation
On 2026-10-28, the ESP-IDF 6.1 build for ESP32-C3 passed and generated `build/miniDrone.bin` (918,528 bytes, 12% app partition space free). Host flight/protocol tests also passed using MSVC. No hardware tests were performed.

For this workstation's EIM installation, initialize PowerShell with its installed profile before building:
```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
$env:PYTHONUTF8 = '1'
idf.py build
```
This profile selects the installed Python environment at `C:\Espressif\tools\python\v6.1\venv`. The generic `export.ps1` otherwise looks for a missing environment under `C:\Espressif\python_env`.
