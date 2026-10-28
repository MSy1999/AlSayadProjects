# Software verification results

Run date: 2026-10-28. SDK, dependency and tool versions are recorded in README.md.
These are software checks, not physical acceptance evidence.

## Intermittent DHT11 timeout follow-up

The user's log at 68637 ms reports `raw=unavailable stage=insufficient_budget`,
then `ESP_ERR_TIMEOUT` for both readings. Reads immediately before and after it
decode with valid checksums (25.8 C, 49.0 percent RH). This particular timeout
occurs before physical acquisition: waiting for the driver's two-second minimum
leaves less than its reserved 40 ms within the board's 100 ms read timeout.
Successful capture timestamps advance by about 2010 ms against a 2000 ms sampling
schedule, consistent with tick-rounded waits accumulating until that guard fires.

Changed the active configuration, defaults and Kconfig default to 2500 ms so
normal scheduling jitter has 500 ms of margin above the physical minimum.
The error counters are cumulative, so a nonzero `sensor errors T/H/L` line does
not mean the current readings failed. A flashed-board soak check is still needed
to confirm the intermittent timeout is gone.

Verification: ESP-IDF build and project source/component checks pass. Generated
configuration confirms period=2500 ms and light calibration remains 4095 / 12.

| Check | Result |
| --- | --- |
| MeasurementESP ESP-IDF 6.1.0 build, target esp32 | PASS; `build/MeasurementESP.bin`, 0xe3d00 bytes (933120), 11% app partition free; period=2500 ms, GPIO edge-sync=2, raw input logging, light calibrated at dark=4095 / bright=12 |
| SmartDisplay compatibility firmware rebuild | PASS; existing receiver sources/AP/server API unchanged |
| Sender native C11 checks (`-Wall -Wextra -Werror`) | PASS |
| Existing SmartDisplay core/transport host checks | PASS; rerun after hardware integration |
| Real board + RMT/ADC driver code with injected SDK | PASS in uncalibrated and calibrated (reversed endpoints) configurations |
| DHT11 pulse/checksum decoder | PASS; diagnostic reasons tested for arguments, incomplete response, preamble, timing, checksum and range |
| Resolved component/standard/source checks | PASS |

Run sender tests after dependency resolution:

```powershell
python tests/RunHostTests.py --zig ../SmartDisplay/build/host-tools/ziglang/zig.exe
python tests/CheckProject.py
```

Both scripts accept `--library-root` for a relocated shared tree. Supply the path
to your own Zig 0.16.0 executable if the receiver host-tools directory is absent.
Build this project with `tools/Build.ps1` or the installed SDK's `idf.py build`.
The receiver was rebuilt from its directory using the same SDK environment, and
its existing `tests/RunHostTests.py` was run with its Zig compiler.

The native runner builds/runs twice: uncalibrated light and measured-endpoint-shaped
reversed calibration (test fixture 3000 dark / 1000 bright). Those fixture endpoints
are test inputs only; they have not been applied to the firmware configuration.

Coverage in actual shared implementations:

- cJSON encode/decode round trips through existing `as_measurement_decode`, with
  all three required metrics, nulls, real zero, inclusive endpoints, simulated flag,
  escaped identities, maximum escaped 32-byte identity and exact output byte count.
- Explicit unavailable defaults and synthetic mode; independent read failures,
  recovery, invalid finite ranges, NaN/infinities, failed initialization with bounded
  retry delay and continued operation of other metrics.
- Relative-light clamping, reversed endpoints and rejection of invalid calibration.
- Real DHT11 decoder: preamble/bit timing, malformed widths, incomplete frames, checksum,
  decimal bytes, zero temperature and inclusive selected-module limits.
- Actual RMT driver with injected SDK: two-second boot gating, minimum physical attempt
  spacing, near-boundary RTOS wakes, timeout cancellation/disable, bad checksum/recovery,
  partial-init resource reuse and deinitialization. RMT avoids long CPU critical sections.
- Actual ADC driver with injected SDK: ADC2 pin rejection, ADC1 channel 6 selection,
  12-bit/12 dB configuration, >=5 ms settling despite tick-boundary wakes, fresh 16-read
  averaging, read/config errors, raw bounds and resource cleanup/retry.
- Actual board registration plus snapshot service: one DHT transaction per snapshot,
  shared T/H failure, independent ADC failure, recovery and no previous-pair replay.
  Uncalibrated successful ADC reads stay unavailable and initialized; calibration mode
  with reversed fixture endpoints produces the expected relative percent.
- Small output buffers, invalid identities, stale status and invalid numbers rejected.
  Fault injection fails every cJSON allocation in turn and confirms all trees are freed.
  Valid typed frames cannot reach 1024 bytes even at the maximum escaped identity;
  the transport separately rejects length 0 or 1025 before creating a client.
- Controllable monotonic policy clock: stale/future samples, DHCP gating, no attempted
  snapshot replay, missed periods, slow-request scheduling, 60-second outage model,
  newest snapshot eligibility and capped independent backoff. HTTP results cover
  202, 400, 408, 413, 503, socket failure, 500, 200 and redirects.
- Actual HTTP wrapper with injected SDK initialization/header/open/short-write/
  fetch-header/cleanup failures: 4,900 cycles per configuration confirm no retained client,
  one client at a time, explicit body length and bounded receive configuration.
- Existing receiver host tests pass AP lifecycle/cleanup, borrowed event-loop ownership,
  partial body reads, status mappings, decoder validation, stale/recovery logic,
  history gaps/rollover and CSV/session preservation.
- Resolved build metadata confirms each selected shared component is registered once,
  project source compilation ends with C11, no local implementation copies exist,
  no receiver UI/history/SD components are selected and project C sources contain no goto.

Build logs: `build/AsBuild.log` and
`../SmartDisplay/build/AsSenderCompatibility.log` (generated, ignored artifacts).
Initial SDK configuration reported existing esp_wifi/wpa_supplicant private-include
warnings; neither project required SDK source edits.

Limits: Wi-Fi/DHCP event recovery and real socket timing have been compiled but not
exercised on ESP32 hardware. Task concurrency is enforced by separate workers and
copy/overwrite queue architecture; host tests exercise the pure policy model and
SDK wrappers, actual board callbacks and driver control flow rather than the real
FreeRTOS scheduler, RMT edges, ADC hardware or radio. Actual sampling cadence,
heap stability, sensor electrical behavior/accuracy and receiver display indication
remain pending in PHYSICAL_TESTS.md. The 3000 ms setting is a per-socket-operation
timeout, not an end-to-end wall-clock deadline.

## User-reported runtime investigation

The supplied run confirms HTTP 202 delivery and records cumulative T/H snapshot
errors. The user confirmed the KY-018 is absent. Light acquisition is now disabled
in Kconfig defaults and the current sdkconfig; native driver tests still exercise
both uncalibrated/calibrated light configurations explicitly.

Added bounded DHT initialization/transaction/capture/decoder diagnostics; legacy
`as_dht11_decode` remains compatible. Host tests and the ESP-IDF build pass after
these changes. No root cause for the physical DHT failures has been established;
logs from the diagnostic firmware are needed. No hardware was flashed by the agent.

## Connected light and DHT timeout follow-up

The user connected the KY-018 without the optional C1. Re-enabled light acquisition
in current sdkconfig/defaults, clarified optional filtering in menuconfig/README,
and enabled per-sample raw logs until calibration. Brightness remains unavailable
until actual endpoints are entered. Added successful DHT value logs and verification
that GPIO27 is actually low during the host start pulse before RMT is armed.

Native tests pass with an injected GPIO stuck-high condition, checking that no RMT
transaction starts and DATA is released on failure. Existing timeout, paired-fault,
calibration, transport and decoder checks pass in both native configurations.
ESP-IDF firmware build and project source/component checks pass. These changes
improve observability; they do not establish the root cause of the physical DHT
receive-completion timeout. New raw values and wiring/capture evidence are pending.

## Recurring RMT timeout investigation

User supplied a roughly 22-second RMT timeout pattern after fitting the external
pull-up. Root cause remains unconfirmed. A new selectable GPIO capture backend
bypasses RMT reception through the same driver/model/decoder; it is selected in the
current sdkconfig and defaults. GPIO capture checks a maximum 8 ms response deadline
and per-phase limits while masking interrupts on the sampling core; the 20 ms host
start pulse and all logging run outside the critical section. No RMT resources are
allocated in this mode. The legacy RMT initialization API is preserved.

Fixed read-error scheduling: failed transactions retry at the next scheduled sample;
only failed initialization waits ten seconds. Rate-limited warnings remain separate
from sampling cadence. HTTP 202 logs now identify each metric's actual status so
payload byte count is not used as a proxy for acquisition success.

PASS: native tests in all four GPIO/RMT x calibrated/uncalibrated configurations.
These exercise actual GPIO capture with time-driven level fixtures, valid response,
missing response, bad checksum/recovery, no RMT allocation in GPIO mode, shared
single-transaction board wiring, failure/recovery on the next two-second sample,
and the existing decoder/HTTP/scheduling/resource checks. Actual pin timing, Wi-Fi
behavior during the short critical section and successful physical DHT acquisition
remain to be measured. ESP-IDF build and resolved C11/component checks pass.

## GPIO release-edge alignment regression

The next user log showed completed 82-pulse GPIO captures rejected as
`response_preamble`, HTTP delivery with T/H=error and L=unavailable, and raw light
averages of zero. Added a realistic 3 us pull-up rise interval to the GPIO fixture:
the prior implementation failed its valid-frame assertion. This reproduced an
alignment bug: the residual LOW immediately after host release was mistaken for
the sensor's first response pulse, shifting the fixed-length capture.

Fixed GPIO capture to observe the released bus HIGH before waiting for the sensor
response LOW. Added bounded `gpio_release_high` timeout and up to six pulse diagnostics
on failure. The regression now passes for 0, 3 and 10 us rise times. Stuck LOW,
missing response, bad checksum and recovery remain covered. All four native
configurations, ESP-IDF firmware build and project source/component checks pass.
This is a software-reproduced defect; physical confirmation after flashing remains
pending. ADC raw zero remains a separate observation, not evidence of calibrated
brightness or a proven ADC/wiring fault.

## Edge-stream synchronization (edge-sync=2)

The next supplied board log failed the prior `gpio_release_high` check. Added a
fixture in which the CPU first observes the bus 70 us after release, during the
sensor LOW response: the old implementation rejected a valid waveform. The prior
mandatory release-HIGH check is superseded, not retained as a required phase.

GPIO capture now explicitly switches DATA to input, records a bounded edge stream
through final HIGH idle, and locates the preamble. A first LOW may be partially
observed; complete response HIGH, every data-bit timing, checksum and range checks
remain required. It restores open-drain output afterward. The current startup
marker is `edge-sync=2`. Compared release direction handling with the esp-idf-lib
reference source linked in README; no third-party code/dependency was imported.

PASS: native GPIO/RMT x calibrated/uncalibrated checks with initial 0/3/10 us pull-up
rise, 70 us delayed first observation, incomplete response rejected after 180 us
observation delay, stuck LOW, no reply, checksum failure, recovery and unchanged
outputs on failure. Firmware build and resolved source/component checks pass.
Root cause and successful acquisition on the real board remain unconfirmed until
this build is run. No additional electrical changes were made by the agent.

## Separate raw-input serial logging

Added `AsRawInputs` tag for every physical ADC/DHT attempt: light sample array and
average, DHT digital levels/capture status/full pulse stream, and complete raw bytes
with checksum/decoder outcome. Incomplete bytes and failed ADC reads are explicitly
unavailable rather than numeric zero placeholders. Printing occurs after sampling,
outside DHT critical sections and ISR callbacks, using bounded formatting buffers.

ESP-IDF firmware build, all four native configurations and project C11/component
checks pass. Verified the installed monitor LineMatcher accepts AsRawInputs and
rejects MeasurementESP with `AsRawInputs:I *:N`. README documents filtering and
PowerShell Tee-Object capture to a separate raw-inputs.log file. No physical raw
readings were obtained by the agent; sample logs in explanations are format examples.


## HIGH-first board capture regression

Reproduced the latest response_preamble failure using the exact 82 pulses from
user log timestamp 27598 ms; the existing decoder rejected the fixture. Added
support only for exactly 82 pulses consisting of a 60..100 us initial HIGH,
forty complete LOW/HIGH pairs and a 35..75 us trailing LOW. No data bits are
skipped and all existing validation remains. This frame decodes bytes
30/00/19/08/51, 48.0 %RH and 25.8 C.

PASS: all four native GPIO/RMT x calibrated/uncalibrated configurations, including
exact-board-waveform decoding, GPIO capture beginning at response HIGH, truncated
frame rejection, short initial HIGH, invalid trailing level, ambiguous bit timing,
checksum corruption and unchanged outputs on failure. ESP-IDF firmware build and
resolved component/C11 checks pass. This is a shared-library decoder change under
D:/GitHub/AlSayad/AlSayadLib/ESP32/AsDht11. No board was flashed by the agent;
physical confirmation of the correction is pending.


## Reusable sensor directory and configuration API

Moved the existing sensor sources into AlSayadLib/ESP32/sensors/DHT11 and
sensors/Light, registered as the DHT11 and Light ESP-IDF components. Updated
MeasurementESP registration, board initialization and host/project-check paths.
The public headers retain existing names and pin-only APIs; new init_config
functions accept explicit sensor config structs. Defaults preserve GPIO27 GPIO
capture, GPIO34 ADC1, 16 samples and 5 ms settling in MeasurementESP.

DHT11 config selects GPIO and capture backend. Light config selects GPIO,
1..16 averaging samples and 0..100 ms settling. Reinitializing with different
configuration requires deinit. Both components now use esp_timer directly and
depend only on ESP-IDF, with no TimeManager or measurement-framework dependency.
Raw light calibration and paired measurement coordination remain in the app.
Usage and integration examples live in the shared sensors/README.md.

PASS: native GPIO/RMT x calibrated/uncalibrated runs, including invalid configs,
same-config init, backend/config change rejection, four-sample averaging with
zero settling, and settling deadline failure without output mutation or ADC reads.
Existing pulse/checksum, retry/recovery, paired sensor, calibration, scheduling
and HTTP checks pass. ESP-IDF firmware build and resolved component/C11 checks
pass; warnings/errors-only logging remains configured. The user confirms the
pre-refactor sensors work; physical confirmation of this refactor is pending.


## Explicit sensor configuration in main.c

main.c now supplies DHT11/light pin and driver configs plus separate read
budgets to the board adapter. The adapter copies these settings and uses them
for init/read callbacks instead of hardcoded GPIO27/GPIO34 settings. Capture
selection still honors menuconfig; defaults preserve the working setup.
Configuration errors terminate the sampling task with an error log.

PASS: all four native configurations, including forwarding a caller-selected
eight-sample light average through board sampling and rejecting invalid read
budgets. Existing shared DHT transaction, retry and calibration checks pass.
Firmware build and resolved component/C11 checks pass. No board was flashed.
