# MeasurementESP

ESP-IDF C11 sender for the existing [SmartDisplay protocol](../SmartDisplay/PROTOCOL.md).
Joins the display AP using DHCP and posts a full temperature/humidity/relative-light
snapshot every 2500 ms. No router, internet, NTP or synchronized clock is used.

The hardware reference now selects **ESP32-DevKitC V4 / ESP32-WROOM-32E**,
**KY-015 / DHT11**, and **KY-018 / LDR**. Temperature/humidity use real DHT
transactions. The user has now connected the light sensor without C1, so light
acquisition is enabled and logs fresh raw ADC averages every sample during calibration.
Brightness stays `unavailable` until measured calibration endpoints are enabled.
The agent did not flash either board. User-reported logs confirm HTTP 202 delivery
and valid DHT readings, with intermittent preamble failures; sensor acceptance
after the decoder fix is confirmed by the user. The sensor-directory refactor
has software validation; a new board run remains pending. Actual module revisions
and physical flash capacity still need inspection.

Serial logging defaults to warnings and errors for both the application and
bootloader. Info messages, including raw inputs and successful HTTP/sensor
readings, are suppressed. To resume those diagnostics, select Info under
menuconfig > Component config > Log > Default log verbosity and rebuild.

Reusable sensor APIs and configuration examples are documented in
[AlSayadLib sensors](../../AlSayad/AlSayadLib/ESP32/sensors/README.md).
`main/main.c` supplies pins, driver configuration and read timeouts to the board
adapter, which calls the reusable init/read APIs. Application
sampling, paired readings, light calibration and HTTP delivery remain here.

## Hardware decisions and wiring

The user supplied [HARDWARE.md](../Docs/MeasurementESP/HARDWARE.md) and the
[illustrated offline guide](../Docs/MeasurementESP/MeasurementESP_Hardware.html).
That reference's original implementation-status paragraph predates the drivers;
this README and the test records describe the current firmware.

| Item | Selected configuration |
| --- | --- |
| Board / module / power | ESP32-DevKitC V4 / ESP32-WROOM-32E, USB powered |
| Build target / flash | `esp32`; conservative 2 MB image setting retained; actual module capacity not recorded |
| Temperature/humidity | KY-015 / DHT11, bidirectional GPIO27 with DATA pulled up to 3.3 V |
| Light | KY-018 analog module, GPIO34 / ADC1 channel 6, oneshot 12-bit at 12 dB attenuation |
| DHT timing | First snapshot after 2-second startup; >=2 seconds between physical attempts, including failures; 100 ms callback budget |
| Light timing | >=5 ms RC settle, 16 fresh ADC samples per attempt, 100 ms callback budget |
| Brightness endpoints | Actual dark/bright measurements pending; conversion disabled until explicit calibration |

| From | To |
| --- | --- |
| ESP32 3V3 | Both modules' +/VCC |
| ESP32 GND | Both modules' -/GND, R2 lower end, C1 lower end |
| GPIO27 | KY-015 S/DATA; required R3 10 kohm to 3V3 (user confirmed no module pull-up) |
| KY-018 S | R1 10 kohm upper end |
| R1 lower end | GPIO34, R2 10 kohm upper end, C1 100 nF upper end |

[Wiring overview](../Docs/MeasurementESP/assets/wiring-overview.svg) and
[circuit details](../Docs/MeasurementESP/assets/wiring-details.svg) show the exact
reference circuit. Check actual connector **S/+/-** labels rather than photograph
orientation. Both sensors and DATA pull-ups use 3.3 V; the two ESP boards communicate
over Wi-Fi and are powered separately. C1 (100 nF from the ADC node to ground) is optional noise filtering; the user does
not currently have it. Retain both 10 kohm resistors when omitting C1. Firmware still
averages 16 fresh ADC readings; calibrate and assess variation on the actual circuit.
The light divider limits the ADC node to
half the module output (at most 1.65 V for a 3.3 V supply). Confirm it with a meter
before connecting IO34. GPIO34 is input-only and uses ADC1, avoiding ADC2/Wi-Fi
contention. GPIO27 is digital here. No LCD/touch pin assignments are reused.

The shared inventory was checked: it had no DHT or ADC-light drivers. New reusable
implementations live under `sensors/DHT11` and `sensors/Light` in the shared root;
`main/AsMeasurementBoardConfig.*` holds only board registration and paired-result
coordination. The default diagnostic build captures DHT timing by GPIO polling,
masking interrupts on the sampling core only during the response (8 ms capture
deadline). The 20 ms host start pulse runs outside that critical section. The
original RMT backend remains selectable for comparison. The combined
checksum/range-validated result is shared only within one snapshot,
then invalidated before the next one. Both metrics become `error` after a failed
transaction. The selected KY-015 range is 0-50 C and 20-90 %RH, narrower than the
receiver's limits; out-of-sensor-range results are rejected.

Hardware references checked: [Espressif DevKitC pin mapping](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html),
[Joy-IT KY-015 specifications](https://sensorkit.joy-it.net/en/sensors/ky-015),
[Joy-IT KY-018 circuit](https://sensorkit.joy-it.net/en/sensors/ky-018),
and [Aosong DHT11 timing/data sheet](https://www.electronicoscaldas.com/datasheet/DHT11_Aosong.pdf).
Driver API compatibility was checked against the installed ESP-IDF 6.1 sources.
Input-mode release was also checked against the [esp-idf-lib DHT reference](https://github.com/UncleRus/esp-idf-lib/blob/master/components/dht/dht.c).
The local driver remains an independent implementation with no added third-party dependency.

## Build and configure

Validated SDK: ESP-IDF **6.1.0**, tag `v6.1`, commit
`fff9895c82d744c7237be8847347bdd1b07c6643`. Managed dependency:
`espressif/cjson` **1.7.19~2**, pinned in the shared JSON manifest and resolved in
[dependencies.lock](dependencies.lock). Toolchain: GCC 15.2.0
(`esp-15.2.0_20251204`), Python 3.11.15, component manager 3.1.1,
CMake 4.0.3, Ninja 1.12.1. Host tests use Zig 0.16.0.

Activate the installed ESP-IDF environment, then run:

```powershell
idf.py menuconfig
idf.py build
```

This machine's EIM installation can also build with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1
```

That helper uses the installed virtual environment and disables the *missing*
EIM Python constraints-file check; installed package requirements are still checked.
It makes process-local environment changes only. Adjust its installation paths on
another machine. Do not use the helper as a portable SDK installer.

Override the external shared source root when needed:

```powershell
idf.py -DALSAYAD_LIB_ROOT=D:/GitHub/AlSayad/AlSayadLib/ESP32 build
```

The local `libs/AlSayadLib` directory contains CMake integration only, so copying
this directory alone is insufficient: include the shared modules listed below.
Only sender components and their SDK dependencies are registered. Project-owned
sources explicitly compile as GNU C11 because IDF 6.1 otherwise appends GNU C23.

Under **MeasurementESP sender** in menuconfig:

| Setting | Default |
| --- | --- |
| `AS_SENDER_WIFI_SSID` | `AlSayad-Room` |
| `AS_SENDER_WIFI_PASSWORD` | `AlSayadRoom` (example; match receiver) |
| `AS_SENDER_DEVICE_ID` | `measurement-01` |
| `AS_SENDER_URL` | `http://192.168.4.1/api/v1/measurements` |
| `AS_SENDER_PERIOD_MS` | `2500` (100-60000); leaves margin above the DHT11's 2000 ms minimum |
| `AS_SENDER_HTTP_TIMEOUT_MS` | `3000` (100-30000) |
| `AS_SENDER_SIMULATED` | Disabled |
| `AS_SENDER_DHT_ENABLED` | Enabled, GPIO27 fixed by reference wiring |
| `AS_SENDER_DHT_GPIO_CAPTURE` | Enabled to investigate RMT receive-completion timeouts; disable for RMT comparison |
| `AS_SENDER_LIGHT_ENABLED` | Enabled for the connected KY-018; GPIO34 fixed by reference wiring |
| `AS_SENDER_LIGHT_CALIBRATED` | Enabled using the user's covered/bright-light readings |
| `AS_SENDER_LIGHT_DARK_RAW` / `AS_SENDER_LIGHT_BRIGHT_RAW` | 4095 / 12 (bright mean 12.375 rounded to nearest integer) |

Startup validates SSID 1-32 bytes, password 8-63 bytes and device ID 1-32 bytes,
HTTP URL and timing limits. DHT hardware mode additionally requires a sampling period
of at least 2000 ms; enabled light calibration requires unequal endpoints. Passwords are never logged. Credentials are copied
into SDK RAM, not persisted by the station. The string device ID must match the
receiver; it is metadata, not authentication. HTTP is carried over the WPA2 link.

For communication bring-up, enable `AS_SENDER_SIMULATED`: every frame then contains
`simulated:true` and fixed test values 23.4 C, 56.7 %RH and 0 % brightness. Disable
it before real acquisition. Missing hardware never enables simulation automatically.

After exact target, flash capacity, board and COM port are verified, use `idf.py
set-target` with the selected target and `idf.py -p` with the sender's serial port
for `flash monitor`. No flash operation is included in the build helper.

## Acquisition, handoff and delivery

The sampling worker creates a new frame from independent sensor callbacks and
publishes a copy into a one-slot overwrite queue. It uses monotonic acquisition
start time, and skips missed periods rather than running catch-up conversions.
The HTTP worker copies the queue, checks DHCP readiness and rejects snapshots older
than two configured periods. At most one request runs, and an attempted acquisition
is never replayed, including after uncertain timeout acceptance. Sampling continues
while the HTTP worker blocks. Network recovery uses the latest queued snapshot.

`AsMeasurementSensor` callbacks borrow their context for the worker lifetime and
must honor their timeout and return fresh conversions. Disabled metrics remain
`unavailable`; enabled init/read failures become `error`. Failed initialization
is retried after 10 seconds; read failures retry initialization and fresh acquisition
at the next scheduled sample. Invalid numbers become `error` without clamping temperature/humidity.
Init failure in one metric never blocks light acquisition. Board wiring coordinates
one DHT frame per snapshot, including combined failures, and invalidates the pair
before every cycle. DHT and ADC callbacks each use a 100 ms budget (with the second
DHT callback consuming that same cycle's result). Failed reads no longer impose
10 seconds of error-only snapshots: a fresh transaction is eligible at the next
scheduled sample, while physical DHT attempts remain at least two seconds apart. An intentionally uncalibrated light callback returns
`ESP_ERR_NOT_SUPPORTED`, which the service maps to `unavailable` without reinitializing
or discarding raw calibration diagnostics.

Wi-Fi events update an event group only. A dedicated worker attempts association
and DHCP, abandons attempts after 15 seconds, and reconnects indefinitely using
1/2/4/8/10-second backoff plus 0-250 ms jitter. DHCP success resets its backoff.
A disconnect clears readiness immediately. Pending HTTP eligibility is polled every
20 ms. An in-flight synchronous SDK request completes/fails under its socket timeout;
it is not cancelled concurrently: the SDK cancellation API closes **and reconnects**
the client and offers no cross-task ownership guarantee. Sampling is independent.
Timeout is per blocking socket operation, not a hard wall-clock request deadline.

The HTTP adapter sends a positive fixed content length of 1-1024 bytes, without NUL,
using SDK open/write/fetch-headers APIs. It checks short writes, disables redirects,
and always cleans up its client. It does not drain or parse response bodies; the
SDK receive buffer is 512 bytes. `202` means receiver queue acceptance only.

| Result | Next eligible attempt |
| --- | --- |
| 202 | Next future acquisition cadence deadline; missed slots skipped |
| 400 / 413 | At least 10 seconds and one sample period; log schema/config/size defect |
| 408 / 503 / socket failure / unexpected status | Capped 1/2/4/8/10-second HTTP backoff, at least one sample period |

HTTP backoff is separate from Wi-Fi backoff. Every retry uses a newer snapshot.
Repeated HTTP failure diagnostics are limited to once per 10 seconds and include
status, error, failure count and time since last acceptance. Sampling logs per-metric
error counters and minimum free heap every 30 samples. Network initialization errors
preserve NVS and leave acquisition running; allocation/initialization errors are
logged without reboot loops. Fatal startup network initialization needs operator
repair/reboot; ordinary disconnects and receiver restarts recover automatically.

## Sensor troubleshooting

`HTTP 202` confirms frame acceptance, including frames with sensor error statuses.
The T/H/L counters count failed **scheduled snapshots**, not physical transactions;
older firmware used a ten-second read-error recovery delay while publishing error
snapshots every two seconds. Current firmware retries transactions on the next sample;
only initialization failures retain the ten-second delay. A zero ADC average with no ADC error does not establish that a light
sensor is attached. Leave `AS_SENDER_LIGHT_ENABLED` disabled while it is absent.

DHT warnings now include initialization errors, transaction error code and capture
stage, idle/current DATA level, captured symbol/pulse counts, decoder failure reason,
decoded bytes when available, and at most six leading pulses. Share the `AsDht11`
and `AsBoard: DHT11` lines after running this build. The driver additionally checks
that the host start pulse pulls GPIO27 low before arming RMT; a high pad produces
`stage=start_level_low`. Successful DHT values are logged every five successes. Repeated capture/transaction
warnings are limited to once per ten seconds independently of the sampling cadence.

The user observed RMT completion timeouts recurring roughly every 22 seconds despite
adding R3. This does not establish a periodic electrical fault. To isolate the RMT
path, `AS_SENDER_DHT_GPIO_CAPTURE=y` now selects a separate, bounded polling capture
through the same shared driver and decoder. Confirm `DHT11 GPIO27 capture=GPIO` at
startup after flashing; the current GPIO version additionally prints `edge-sync=2`.
GPIO capture releases DATA, switches to input and records the edge stream through
end-of-frame idle. It locates the response preamble rather than requiring a short
release-HIGH window to be observed. A first captured LOW may be truncated by
observation latency. An exactly 82-pulse stream beginning with a 60..100 us
response HIGH is also accepted when it contains forty complete data pairs and
a valid trailing LOW. All data timing, checksum and sensor ranges remain checked. It restores open-drain host output after capture.
Host tests cover finite rise times and first observation after the reply has begun;
physical success still requires a new board run. `gpio_release_high` belongs to
the earlier implementation and is absent from the edge-sync=2 capture path.
HTTP acceptance logs now include explicit T/H/L statuses;
do not infer success from 222 versus 234 payload bytes. Both backends still require
the external pull-up, correct wiring and actual sensor responses. Physical GPIO
mode results are pending; passing host tests do not prove the board's sensor works. These diagnostics distinguish
missing/incomplete response, response preamble, bit timing, checksum, encoding and
sensor-range failures without substituting synthetic values. Failed attempts remain
bounded and the old decoder API is preserved.

Verify KY-015 S/DATA actually reaches IO27, +/VCC reaches 3V3, -/GND reaches the
sender ground, and the DATA pull-up goes to 3V3 (add the documented external 10 kohm
if absent from the module). Check connector labels on the actual PCB. The current
log alone cannot distinguish a wiring/pull-up problem from a capture/decoding problem.

## Brightness calibration procedure

Current calibration uses the user's covered reading of 4095 and bright-light
mean of 12.375 (16 samples), rounded to 12. These define relative brightness,
not lux. The covered reading saturates the ADC; changes beyond that endpoint
cannot be distinguished. Recheck these endpoints after wiring or supply changes.

1. Assemble the complete KY-018 plus R1/R2/C1 circuit above. Verify the ADC node with
   a meter across expected lighting conditions; confirm it stays within 0-1.65 V.
2. Enable `AS_SENDER_LIGHT_ENABLED` after connecting the sensor. Leave
   `AS_SENDER_LIGHT_CALIBRATED` disabled. Build/flash the verified measurement
   board and monitor `KY-018 raw average=...` diagnostics (every sample while uncalibrated;
   every five samples after calibration).
   Temperature/humidity acquisition continues independently; transmitted brightness
   remains `unavailable`.
3. Record several averaged ADC readings in representative dark conditions and then
   in representative bright conditions. Record geometry, supply voltage, mean,
   variation and any saturation. Calibrate the loaded assembled circuit, including
   the external divider, rather than applying the unloaded module resistance equation.
4. Enter representative rounded raw endpoints (0-4095) in menuconfig and enable
   `AS_SENDER_LIGHT_CALIBRATED`. Endpoints must differ; their order is unrestricted.
   Defaults are placeholders, not physical calibration. Rebuild/reflash after changes.
5. Confirm dark/bright results approach 0/100 percent and intermediate lighting varies
   monotonically in the correct direction. Repeat after wiring/supply/filter changes.

```text
percent = clamp(100 * (raw - dark) / (bright - dark), 0, 100)
```

Only successful ADC averages are converted. Nonfinite/equal endpoints are rejected;
reversed raw direction works without a separate inversion flag. Relative brightness
is not lux. A disconnected analog lead may still produce plausible ADC readings:
range checks do not establish that a KY-018 is connected. Accuracy, endpoint values
and physical direction remain pending measurement and must be recorded in
[the physical test record](tests/PHYSICAL_TESTS.md).

## Shared-library decisions

All shared implementations live in `D:/GitHub/AlSayad/AlSayadLib/ESP32/`, referenced
through overridable `ALSAYAD_LIB_ROOT`. No second source tree is kept locally.

| Module | Decision and compatibility |
| --- | --- |
| `AsMeasurements` | Reused model, status enums and validation limits unchanged; sender never starts its history store |
| `AsMeasurementJson` | Added bounded `as_measurement_encode`, keeping decoder signature/behavior and private cJSON; allocation and serialization failures return no body |
| `AsMeasurementSensors` (new) | Missing reusable fresh-snapshot acquisition, per-metric validation/init recovery and relative-light conversion; model/history module has no driver boundary |
| `DHT11` (`sensors/DHT11`) | Native ESP-IDF DHT11 acquisition with selectable GPIO/RMT capture, >=2-second physical attempt spacing, checksum/timing/module-range decoder; no Arduino dependency |
| `Light` (`sensors/Light`) | Wi-Fi-compatible ADC1 acquisition; fresh 16-sample average with bounded waits and retry-safe resource ownership |
| `AsMeasurementSender` (new) | Missing portable retry/freshness/cadence policy; no task scheduler exists in `StateMachine` |
| `AsWifiStation` (new) | Separate station/DHCP/reconnect owner because legacy `WifiManager` has shared globals, immediate reconnects and CAN Logger/certificate coupling; no legacy API changes |
| `AsMeasurementHttp` (new) | Configurable bounded fixed-length POST; legacy upload API targets CAN Logger and `AsWifiTransport` is an AP/server receiver |
| `TimeManager` | Reused `MICRO_SEC_TO_MS` behind application's clock boundary; added header-only component CMake metadata, header unchanged |
| `StateMachine` | Evaluated transition types; event flags/deadlines suffice, so no unused dependency |
| `Device` | Numeric stored identity is unsuitable for protocol string; menuconfig needs no metadata persistence |
| `AsTransport` / `AsWifiTransport` | Receiver callback/AP/server contracts unchanged and excluded from sender registration |
| Display/history/SD and other legacy modules | Excluded: sender has no UI, history, CAN, MQTT or storage requirement |

The C and dark-style guidelines were read; this headless project adds no visual UI.
See [software results](tests/RESULTS.md) and the separate
[physical test record](tests/PHYSICAL_TESTS.md).

### Checking the light input without a meter

If raw ADC stays zero under a flashlight, test the divider/input separately from
the module. Power off, disconnect KY-018 S from the upper end of R1, and temporarily
connect that R1 input to 3V3. Keep R1 and R2 in place, with R2 to GND and their
junction to IO34. On power-up, the 10k/10k divider supplies about 1.65 V to IO34;
its raw reading should be nonzero. A zero result points to the divider/IO34/ground
path or ADC acquisition and does not establish a bad KY-018. Power off and restore
the original S connection after testing. This is a diagnostic, not a brightness
calibration endpoint, and has not been physically tested by the agent.

### Separate raw input log

The `AsRawInputs` serial-log tag is separate from `MeasurementESP` HTTP messages
and `AsBoard` converted-value diagnostics. On every physical read it reports:

- Light: all SDK-successful raw ADC samples, count, result code and average before
  brightness calibration. Failed reads never print an invented numeric sample.
- DHT: GPIO idle/after levels, backend, capture result, all captured pulses in
  bounded `level:duration_us` groups, and five raw bytes in hexadecimal only when
  a complete frame was decoded. Checksum failures still expose the received bytes
  and expected checksum. Incomplete captures say `bytes=unavailable`.

Pulse logging happens after capture/interrupt restoration, not in the critical
section or ISR. This verbose diagnostic output uses UART bandwidth after acquisition.
Raw bytes, ADC codes and pin levels are not calibrated environmental measurements.

In an activated ESP-IDF terminal, select the **sender's actual serial port** and
filter the monitor with:

```powershell
idf.py -p <sender-COM-port> monitor --print-filter "AsRawInputs:I *:N"
```

Replace the port placeholder before running. To keep a separate text file, append
`| Tee-Object -FilePath .\raw-inputs.log` in PowerShell. Other firmware logs are
filtered out of this monitor view. The filter syntax was checked against the
installed ESP-IDF 6.1 monitor parser.
