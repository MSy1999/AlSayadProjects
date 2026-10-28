# Measurement ESP32 implementation guide

Implement a separate ESP32 project that reads temperature, humidity and relative
brightness, then sends them to the existing SmartDisplay receiver. This document
is an implementation specification; it does not contain completed sender firmware
or claim that either board has passed physical testing.

The receiver contract is defined in [PROTOCOL.md](PROTOCOL.md). Receiver settings
are in [main/Kconfig.projbuild](main/Kconfig.projbuild), and the existing simulated
PC sender is [tools/AsMeasurementSender.py](tools/AsMeasurementSender.py).

## 1. Outcome and scope

The sender must:

- Join the display's Wi-Fi access point as a station using DHCP.
- Attempt all configured measurements every two seconds and publish a full snapshot.
- Represent each metric's success or failure independently.
- Recover automatically when the display restarts or Wi-Fi is lost.
- Keep sampling during network failures and send the newest snapshot after recovery.
- Operate without a router, internet, NTP or synchronized clocks.

Use ESP-IDF and C11, consistent with SmartDisplay. The receiver workspace records
ESP-IDF 6.1; use that environment as the starting point and record the exact SDK
and dependency versions used by the sender. Keep sensor drivers and transmission
logic separate from application wiring.

## 2. Selected reference hardware and wiring

Use the following reference hardware for `MeasurementESP`. This is the documented
design target, not a claim that these parts have been physically connected or that
sensor drivers are implemented. The existing sender's `AsMeasurementBoardConfig.c`
currently registers no sensors and must be updated during hardware implementation.

| Part | Selection / connection |
| --- | --- |
| Measurement board | ESP32-DevKitC V4 with ESP32-WROOM-32E; target `esp32`; USB power |
| Temperature/humidity | KY-015 / DHT11 module; S/DATA to GPIO27; + to 3V3; − to GND |
| Light sensor | KY-018 analog LDR module; + to 3V3; − to GND |
| Light input | KY-018 S → R1 10 kΩ → GPIO34 / ADC1_CH6; R2 10 kΩ from GPIO34 to GND |
| ADC filtering | C1 100 nF from GPIO34 to GND, close to the ESP32 |
| DHT pull-up | R3 10 kΩ from DATA to 3V3 only if no suitable module pull-up exists |

Sensors are selected from cards 26 and 27 of the local reference
`D:/GitHub/AlSayad/35_sensor_photos_inline.html`. Use the actual manufacturer photos
and pinout images in the [offline illustrated hardware guide](Docs/MeasurementESP/MeasurementESP_Hardware.html).
The [hardware notes](Docs/MeasurementESP/HARDWARE.md) include calibration, limitations
and source links. The wiring drawings are logical schematics: connect by printed
pin name, not by a presumed left-to-right connector order.

![MeasurementESP wiring overview](Docs/MeasurementESP/assets/wiring-overview.svg)

![ADC divider and DHT pull-up details](Docs/MeasurementESP/assets/wiring-details.svg)

Record the actual purchased board/module revision and these implementation details
in the sender README before flashing hardware drivers:

| Decision | Required information |
| --- | --- |
| Sender board | Exact ESP32 variant, module, flash size and power supply |
| Temperature/humidity | Sensor model, bus, address/pins and conversion timing |
| Light measurement | Sensor model or analog circuit, bus/pin and calibration |
| Electrical connections | Supply and signal voltages, ground, pull-ups and wiring |
| Sampling | Driver timeouts and whether the sensor supports two-second updates |
| Brightness scale | Dark/bright calibration endpoints and direction |

Verify pins, electrical limits and driver compatibility against the selected
hardware documentation. Do not copy LCD/touch pins from SmartDisplay. If analog
light measurement is chosen, confirm ADC availability while Wi-Fi is active on
the exact ESP32 variant. Avoid selecting a pin before this check.

Until the selected hardware drivers are configured, retain the sensor abstraction's
`unavailable` results. Synthetic test mode must be explicit and mark frames as simulated.
Do not silently substitute synthetic readings for missing hardware.

## 3. Configuration

Expose these sender settings through `menuconfig`; the names below are proposed
sender configuration keys, not existing receiver keys.

| Proposed sender setting | Initial value | Requirement |
| --- | --- | --- |
| `AS_SENDER_WIFI_SSID` | `AlSayad-Room` | Match receiver `AS_ROOM_AP_SSID` |
| `AS_SENDER_WIFI_PASSWORD` | `AlSayadRoom` | Example only; match receiver password |
| `AS_SENDER_DEVICE_ID` | `measurement-01` | Match receiver `AS_ROOM_DEVICE_ID` |
| `AS_SENDER_URL` | `http://192.168.4.1/api/v1/measurements` | Display endpoint |
| `AS_SENDER_PERIOD_MS` | `2000` | Default snapshot interval |
| `AS_SENDER_HTTP_TIMEOUT_MS` | `3000` | Proposed bounded client timeout |
| `AS_SENDER_SIMULATED` | Disabled | Explicit synthetic-data mode |

Validate configuration at startup: SSID 1-32 bytes, WPA2 password 8-63 bytes,
device ID 1-32 bytes, and positive bounded timings. Never log passwords. The
display accepts one configured device ID and permits two Wi-Fi clients, allowing
a test PC alongside the sender. The device ID is routing/validation metadata,
not an authentication token. The current transport is HTTP over the WPA2 link.

## 4. Wire contract

Send a fixed-length HTTP request:

```http
POST /api/v1/measurements HTTP/1.1
Host: 192.168.4.1
Content-Type: application/json
```

Set `Content-Length` to the actual UTF-8 body length. Send 1-1024 body bytes,
excluding the terminating C NUL. Avoid chunked request encoding because the
receiver uses the request content length. Field names and status strings are
case-sensitive. All three metric objects are required in every frame.

```json
{
  "version": 1,
  "device_id": "measurement-01",
  "readings": {
    "temperature_c": {"value": 23.4, "status": "valid"},
    "humidity_rh": {"value": 56.7, "status": "valid"},
    "brightness_pct": {"value": 0, "status": "valid"}
  }
}
```

| Metric | Unit | Receiver range for `valid` |
| --- | --- | --- |
| `temperature_c` | Degrees Celsius | -40 through 125, inclusive |
| `humidity_rh` | Percent relative humidity | 0 through 100, inclusive |
| `brightness_pct` | Relative brightness percent | 0 through 100, inclusive |

These bounds describe receiver acceptance, not sensor accuracy. Numeric zero is
valid. Do not send numbers as strings, NaN, infinity or duplicate required fields.

| Status | JSON value | Sender meaning |
| --- | --- | --- |
| `valid` | Finite number in range | Latest scheduled attempt succeeded |
| `error` | `null` | Attempt failed, timed out, or produced an invalid result |
| `unavailable` | `null` | No measurement is available, including disabled/absent hardware or initial startup |

The receiver generates `stale` locally; never transmit it as a sensor status.
Initialization failure must not prevent other sensors or networking from running.
Report an enabled sensor's failed initialization as `error` and retry with bounded
delays; an intentionally disabled or unconfigured sensor remains `unavailable`.

Example with independent failures:

```json
{
  "version": 1,
  "device_id": "measurement-01",
  "readings": {
    "temperature_c": {"value": null, "status": "error"},
    "humidity_rh": {"value": 55, "status": "valid"},
    "brightness_pct": {"value": null, "status": "unavailable"}
  }
}
```

For real hardware, omit `simulated` or set it to `false`. Set `"simulated": true`
at the top level for any frame containing synthetic test values. Never resend a
previous successful value as current after a new measurement attempt fails.

## 5. Sensor acquisition and brightness conversion

At each sampling deadline, initialize a new snapshot, attempt the configured
sensors with bounded timeouts, validate results, and replace the latest snapshot
atomically. Do not clear the only shared snapshot while the HTTP task is reading it.

Temperature and humidity may come from one physical sensor. A failed shared
transaction can legitimately mark both as `error`; a light-sensor failure must
not invalidate them. If a driver offers independent results, preserve that detail.
Never publish a cached driver value as a new successful measurement unless a
fresh conversion or sensor-supported fresh sample was obtained.

For a relative light reading, define calibrated raw endpoints `dark` and `bright`:

```text
brightness_pct = clamp(100 * (raw - dark) / (bright - dark), 0, 100)
```

Use floating-point arithmetic, reject equal endpoints, and allow reversed raw
direction through the endpoint configuration. Apply clamping only to a successful
relative-light conversion. Do not clamp invalid temperature/humidity results into
apparently valid measurements. If the selected sensor measures lux, document the
chosen lux-to-relative-percent mapping; never send raw lux as `brightness_pct`.

## 6. Architecture and source layout

The project name is **`MeasurementESP`**. Keep board choices and task composition
in that project. Existing AlSayad libraries and guidelines are implementation
inputs: inspect and reuse them before creating equivalent functionality.

### Required guidelines

Read these files before implementation:

- [C style guideline](D:/GitHub/AlSayad/Guidlines/C_STYLE_GUIDELINE.md): apply to all
  new and modified project-owned C sources and public APIs.
- [AlSayad dark style guideline](D:/GitHub/AlSayad/Guidlines/ALSAYAD_DARK_STYLE_GUIDLINE.md):
  apply to any visual documentation or interface added within the agreed scope.
  The sender is headless; a display or web UI is not required.
- [Receiver protocol](PROTOCOL.md): preserve the existing wire contract.

Use C11, `AsUpperCamelCase` module/type names, `as_` functions and `AS_` constants.
Use the guideline's scalar prefixes, `st_` for structs and `pst_` for struct
pointers. Keep `*` next to the type, use explicit object pointers rather than
pointer typedefs, four-space indentation, braces for all control-flow bodies,
bounded buffers and checked return values. Document ownership, units and public
APIs. Add accurate Doxygen file headers with author `As`; do not copy unrelated
project descriptions or copyright text from a sample header. Never use `goto`,
including error macros that expand to it. Keep framework-required names such as
`app_main` and `main.c` unchanged.

For this project, retain SmartDisplay's established shared-library layout:
implementations in `D:/GitHub/AlSayad/AlSayadLib/ESP32/` and local integration in
`MeasurementESP/libs/AlSayadLib/`. This is the project-specific adaptation of the
generic guideline's `libs/As/` layout; do not create a second implementation tree.

### Existing library reuse plan

The following assessment is based on the existing headers and sources. Recheck
them when implementation starts, since shared components may have changed.

| Existing module | Required use or integration decision |
| --- | --- |
| `AsMeasurements` | Reuse `AsMeasurement`, `AsReading`, metric/status enums and shared limits. Do not define a second measurement model. Do not start its receiver history store on the sender. |
| `AsMeasurementJson` | Reuse `AS_MEASUREMENT_FRAME_BYTES` and the decoder in encoder round-trip tests. Prefer adding a bounded encoding API to this existing module while preserving its decoder API; keep cJSON private. |
| `WifiManager` | Inspect the existing station and HTTP APIs before implementing networking. Its current source has a fixed CAN Logger upload URL, an embedded-certificate dependency, immediate reconnects and shared globals. Extract/generalize reusable behavior behind configurable APIs, or document why a separate shared sender adapter is needed. Do not call its existing upload function unchanged for SmartDisplay. |
| `TimeManager` | Reuse its monotonic time conversion/deadline helpers where suitable, behind a clock boundary that tests can replace. Keep units and 64-bit arithmetic explicit; do not duplicate equivalent time macros. |
| `StateMachine` | Evaluate its transition types if a table-driven sender lifecycle is needed. It is a type definition header, not a ready-to-run scheduler; avoid adding it solely as an unused dependency. |
| `Device` | Evaluate only if persistent device metadata is required. Its numeric `u32Id` does not replace the protocol's string `device_id`; the current menuconfig identity needs no metadata storage dependency. |
| `AsTransport` / `AsWifiTransport` | Preserve their receiver-facing APIs. The existing Wi-Fi adapter hosts an AP and HTTP server; sender station/client behavior requires a suitable separate adapter or a compatible extension. |
| `as_display`, `AsHistoryStorage`, `AsSdHistory`, `SDCardController` | Keep receiver display/history responsibilities on SmartDisplay; include only if a later sender requirement needs them. |
| `MQTTManager`, `WebServer`, `CanController`, `DigitalOutput`, `eeprom` | No current sender requirement needs these modules. Do not register unrelated components. |

Reuse must preserve existing callers. Make shared changes additive where practical,
keep existing APIs compatible, and verify affected consumers when modifying shared
code. Do not rename legacy public symbols merely to apply new naming conventions.
For every new shared component, document the missing capability it provides and
why extending an existing component was unsuitable. Sensor drivers must also be
checked against the shared library inventory before adding a new implementation.

```text
MeasurementESP/
  CMakeLists.txt
  sdkconfig.defaults
  dependencies.lock
  README.md
  main/
    CMakeLists.txt
    Kconfig.projbuild
    main.c
    AsMeasurementBoardConfig.h
    AsMeasurementBoardConfig.c
  libs/AlSayadLib/
    AlSayadLib.cmake
  tests/
```

Put new reusable sensor, JSON-encoding and HTTP-sender modules under the existing
shared root `D:/GitHub/AlSayad/AlSayadLib/ESP32/`; reference them through the local
CMake integration with an overridable shared-root path. Register only components
needed by the sender. Do not copy the display, UI or SD-storage dependencies.

| Responsibility | Implementation guidance |
| --- | --- |
| Sensor drivers | Hardware-specific acquisition, conversion and bounded timeouts |
| Sensor service | Build full snapshots and map individual results to statuses |
| Latest snapshot | Mutex-protected copy or a one-slot overwrite queue |
| JSON encoder | Typed snapshot to bounded protocol-v1 JSON |
| Wi-Fi station | Association, DHCP, disconnect events and reconnect scheduling |
| HTTP sender | One request at a time, response classification and retry scheduling |
| `main` | Configuration, initialization and task composition |

The shared status enum includes receiver-local `AS_READING_STALE`; reject that
state before encoding. Preserve the decoder and AP/HTTP receiver behavior while
adding encoding and station/client functionality according to the reuse plan.

Set `project(MeasurementESP)` in the top-level CMake project. Adapt
[the existing shared-library integration](libs/AlSayadLib/AlSayadLib.cmake) with an
overridable `ALSAYAD_LIB_ROOT` and register selected components before ESP-IDF's
project configuration. Initially select `AsMeasurements` and `AsMeasurementJson`;
add the chosen station/client and sensor components once their integration is
defined. Existing header-only or legacy directories may need component CMake
metadata before registration. Do not assume every shared directory is already an
ESP-IDF component. Declare direct `REQUIRES`/`PRIV_REQUIRES` dependencies, register
each component once and keep implementation copies out of the local integration
directory.

Use cJSON for serialization with a declared, pinned dependency compatible with
the sender SDK. The receiver currently pins `espressif/cjson` to `1.7.19~2`.
Keep JSON types private to the encoder. Check allocation/serialization failures,
validate finite numeric values before encoding, and enforce the 1024-byte limit
before network transmission. Free JSON trees and HTTP resources on every path.

## 7. Task scheduling and connection recovery

Use separate acquisition and HTTP workers so a network timeout cannot stop
sampling. Wi-Fi callbacks should only update connection state and schedule work.
Start HTTP requests only after the station has an IP address.

```text
Sampling worker, every 2000 ms:
    create a fresh full snapshot from bounded sensor attempts
    publish an atomic copy with its monotonic acquisition time
    notify sender; overwrite pending older snapshots

HTTP worker:
    wait until connected, a snapshot is ready, and send/retry deadline is due
    copy the latest snapshot; reject it if it has become too old
    encode and POST with a bounded timeout
    classify result; schedule next eligible attempt
    after a failure, use a fresh latest snapshot at the next attempt
```

Use monotonic deadlines and skip missed periods rather than sending catch-up
bursts. Keep at most one request in flight. As a proposed local freshness policy,
do not begin sending a snapshot older than two sample periods; wait for new
acquisition instead. This prevents old data being shown as fresh after reconnection.
Timestamps are local bookkeeping and are not required in the wire payload.

On disconnect, clear the IP-ready flag and cancel/wake pending work as supported
by the client implementation. Reconnect with capped backoff, for example 1, 2,
4, 8, then 10 seconds, with small jitter. Retry indefinitely without reboot loops.
Reset Wi-Fi backoff after obtaining an IP. Use a separate HTTP retry schedule;
normal accepted traffic should return to the two-second cadence. Do not erase
NVS automatically to recover from initialization errors.

## 8. HTTP result handling

| Result | Action |
| --- | --- |
| `202` | Snapshot accepted into the receiver queue; resume normal cadence |
| `400` | Log bounded diagnostic and inspect schema, device ID, statuses and ranges; avoid immediate retries |
| `408` | Temporary receive timeout; retry later with the newest snapshot |
| `413` | Encoding/size defect; fix it and avoid immediately repeating the same body |
| `503` | Receiver busy; wait at least one normal interval, then send the newest snapshot |
| Client timeout/socket failure | Keep sampling; retry with backoff and current connection state |
| Unexpected HTTP status | Log it, apply capped backoff, and check endpoint/configuration |

Success normally returns `{"accepted":true}`. Acceptance does not confirm a screen
refresh or SD write. Bound response buffering and do not assume all error bodies
are JSON. After a timeout, acceptance may be uncertain: do not replay the old
request. Keep retries at a bounded rate; a later frame may recover from a transient
fault or configuration change.

The display marks readings stale after ten seconds without an accepted frame.
Continuing to deliver frames with explicit `error` statuses communicates a live
sender with sensor faults; losing all delivery produces the separate stale state.
There is no health-check endpoint or application-level sequence acknowledgment
in the current contract.

## 9. Implementation order

1. Read the guidelines, inspect existing shared APIs, and record reuse/extension
   decisions. Create `MeasurementESP` with shared-library integration and configuration.
2. Implement the sensor abstraction with `unavailable` defaults and unit-testable
   status/range mapping. Record the selected board, sensors and wiring.
3. Implement bounded JSON encoding and verify all three fields, nulls and real zeros.
4. Implement Wi-Fi station/DHCP handling and reconnect scheduling.
5. Implement the HTTP worker and latest-snapshot handoff.
6. Add an explicit simulated mode for communication bring-up.
7. Implement the selected sensor drivers and relative-light calibration.
8. Add diagnostics: connection state, last HTTP status, acceptance age, per-sensor
   errors, failure counters and minimum free heap. Rate-limit repeated failures.
9. Build, perform software checks, then perform the two-board acceptance checks.

## 10. Verification and acceptance

Test encoder and scheduling logic with injected sensor results, transport outcomes
and a controllable clock. Cover:

- Guideline compliance and one registered implementation per shared component.
- Encoder output decoded by the existing `as_measurement_decode()` API, preserving
  device ID, simulated marker, values and statuses.
- Existing receiver checks if shared measurement/JSON/transport code is modified.
- Normal values, valid zero and inclusive range endpoints.
- NaN, infinity and out-of-range sensor results becoming `error`/`null`.
- Each sensor failing independently, all sensors unavailable, and recovery.
- Correct escaping of the configured device ID and the final body byte count.
- All snapshots containing all required metrics; no transmitted `stale` status.
- Serialization failure and oversize output producing no request.
- No overlapping requests and no backlog replay after a prolonged outage.
- HTTP `400`, `408`, `413`, `503`, socket failures and unexpected responses.
- Sampling continuing during slow requests; expired snapshots skipped on recovery.
- Repeated failures releasing resources and keeping retries bounded.

Before involving sender hardware, join a PC to the display AP and use the existing
utility from the SmartDisplay directory:

```powershell
python tools/AsMeasurementSender.py --scenario normal --count 10
python tools/AsMeasurementSender.py --scenario faults --count 10
python tools/AsMeasurementSender.py --scenario outage --count 10
python tools/AsMeasurementSender.py --scenario zero --count 5
```

These are simulated PC tests; use the configured `--device` if the receiver ID
has changed. They verify the receiver path, not sender sensor acquisition.

After creating the sender project, build and flash from its directory using the
selected target and the sender's actual serial port:

```powershell
# Activate the installed ESP-IDF environment first.
idf.py set-target <selected-esp32-target>
idf.py menuconfig
idf.py build
idf.py -p <sender-COM-port> flash monitor
```

Replace the angle-bracket placeholders before running these commands. Confirm
the port belongs to the measurement board.

Physical acceptance requires both boards:

- Cold boot with the display already running, then boot the sender first.
- Confirm DHCP and HTTP `202`, correct units and plausible real sensor readings.
- Confirm the simulated indicator appears only during explicit synthetic tests.
- Disconnect each sensor and verify independent errors and subsequent recovery.
- Stop sender delivery for over ten seconds and verify stale readings on the display.
- Power-cycle the display and verify sender reconnects and sends fresh measurements.
- Run an extended session and record request cadence, recovery time and heap behavior.
- Confirm temperature/humidity accuracy and light calibration against suitable
  references for the selected hardware.

Deliver the sender sources, build configuration/lockfile, exact wiring diagram,
calibration procedure, software-check results and a separate physical-test record.
Mark unperformed hardware checks as pending. No receiver firmware change is needed
to implement the protocol described here.
