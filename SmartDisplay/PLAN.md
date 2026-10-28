# AlSayad Room Monitor: Arabic UI and wireless receiver

Status: Implemented in SmartDisplay and the shared AlSayadLib tree. Software build,
portable C tests, real LVGL host rendering/event tests and C++ checks pass.
Physical hardware acceptance remains pending; the current board uses the planned
RAM fallback because LCD and touch reserve both hardware SPI hosts.

Implementation/setup: [README.md](README.md). Sender contract: [PROTOCOL.md](PROTOCOL.md).
Verification details: [VALIDATION.md](VALIDATION.md).

## Summary

Extend SmartDisplay into the Room Monitor receiver, using the working ST7789 board configuration. A separate Measurement ESP32 will connect directly to the display’s Wi-Fi access point and send temperature, humidity and relative brightness.

The first version includes an Arabic dashboard, charts with optional SD-backed history and a ten-minute RAM fallback, connection details, brightness settings and the AlSayad boot logo. Measurement-board firmware is a later deliverable; this work defines its communication contract.

## Library and coding conventions

- Follow `D:/GitHub/AlSayad/Guidlines/C_STYLE_GUIDELINE.md`: C11, `AsUpperCamelCase` types/files, `as_` functions, typed variable prefixes, Doxygen headers, checked errors and no `goto`.
- Store all reusable project-owned libraries under `D:/GitHub/AlSayad/AlSayadLib/ESP32/`: display/widgets/chart helpers, transport interfaces/adapters, measurement decoding/model/history, and SD storage. Keep board configuration, Room Monitor screen composition and application wiring in SmartDisplay.
- Use `SmartDisplay/libs/AlSayadLib/` as the project-local library integration directory. It contains CMake integration/dependency declarations pointing to the shared library root, not duplicate implementations. Register only the required shared components once, before ESP-IDF project configuration. Allow the shared root to be overridden for other checkouts.
- Migrate the existing shared `as_display` implementation from `AlSayadEmbedded/components/as_display` into `AlSayadLib/ESP32/` during implementation, update SmartDisplay dependency paths, and preserve its component identity and public APIs. Do not register or maintain two copies.
- Apply the conventions to new and modified code. Preserve existing public widget APIs and the C++ wrapper; avoid an unrelated breaking rename.
- Use the production AlSayad palette: dark background/surfaces, gold emphasis, cyan indicators, readable foreground text, 8 px spacing/radii and 1 px borders.

## Reusable display features

- Add a library-owned boot-logo API whose canonical source is `D:/GitHub/AlSayad/Guidlines/brand/alsayad-logo.svg`, preserving the complete Arabic wordmark and gear. Do not select the PNG as the asset-pipeline entry point.
- The current SVG contains an `<image href="alsayad-logo.png">` element rather than vector paths. Render the SVG with its relative resources resolved; document this dependency honestly. A completely PNG-independent vector mark would require replacement vector artwork, not merely changing the extension.
- Generate a compact, flash-resident RGB565 asset by rendering the SVG at build/preparation time with the brand background; retain the original asset attribution and a reproducible SVG conversion script. Do not require full-size artwork decoding or an SVG engine on the ESP32.
- Invoke the splash after hardware initialization and landscape rotation. Display it for two seconds at 75% brightness using a nonblocking LVGL timer. Block touch through the overlay, then reveal the application underneath. Clean up the timer and overlay during display shutdown.
- Add reusable chart configuration/update helpers alongside the existing widget API, with internal LVGL locking and support for gaps in missing data.
- Bundle generated Cairo fonts and their license. Enable LVGL Arabic shaping and bidirectional text. Keep numeric values and units LTR inside the Arabic layout.

## Receiver architecture and communication

Separate responsibilities into these modules:

| Module | Responsibility |
| --- | --- |
| Transport interface | Start/stop reception and deliver bounded frames through a callback |
| Wi-Fi/HTTP adapter | ESP-IDF access point, connection events and HTTP requests |
| Measurement decoder | Validate versioned JSON and produce typed measurements |
| Measurement store | Latest values, independent sensor states, receipt age and RAM history |
| History storage interface | Append/query records independently of SD/FAT filesystem APIs |
| SD history adapter | Card probing, mounting, asynchronous persistence and historical reads |
| Room Monitor UI | Render snapshots and handle navigation/settings |

The store and UI must not include Wi-Fi, socket, HTTP, JSON, SD-driver or filesystem types. A replacement transport can submit frames through the same decoder or submit typed measurements directly.

### JSON library

- Use the C library `espressif/cjson`, pinned initially to `1.7.19~2` through the IDF Component Manager, and retain the resolved application lockfile.
- Declare it as a private dependency of the reusable measurement decoder; no `cJSON*` types escape into public measurement, transport or UI interfaces.
- Use length-bounded parsing and explicit field/type/range/status checks; release parse trees on every success/error path and reject trailing non-whitespace data. Do not hand-write a JSON parser.
- ESP-IDF 6 no longer bundles the old `json` component. Reference: [ESP-IDF migration guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/migration-guides/release-6.x/6.0/protocols.html), [cJSON component](https://components.espressif.com/components/espressif/cjson/versions/1.7.19~2/readme).

### Initial Wi-Fi transport

- Display operates as a WPA2 access point at `192.168.4.1`.
- Configure SSID/password through ESP-IDF menuconfig; provide clearly documented example credentials.
- Measurement ESP32 sends a full snapshot every two seconds to `POST /api/v1/measurements`.
- Define protocol version 1 with a device identifier and separate temperature, humidity and relative-brightness entries. Each entry has a numeric value when valid and an explicit valid/error/unavailable status.
- Units are °C, %RH and relative brightness percent—not lux. Zero remains valid.
- Limit request bodies to 1 KiB. Reject malformed, oversized or unsupported-version messages without changing displayed measurements.
- Return success only after validation and acceptance into the bounded processing queue. Report temporary capacity failures so the sender can retry.
- Support one configured Measurement device in this release. Document the complete payload and response examples for its future firmware.
- Use original ESP-IDF networking code rather than the existing CAN Logger-specific `WifiManager`.

### State and timing

- Use receiver monotonic time; no internet, NTP or synchronized board clocks are required.
- Start in “waiting for readings.” Mark data stale after ten seconds without an accepted snapshot.
- Show sensor errors independently and distinguish “Wi-Fi device connected” from “valid measurement data received.”
- Retain previous successful values internally, but never present them as current after an error or timeout.
- Network callbacks never draw the UI or write files. A processing task updates the store; the UI refreshes snapshots every 250 ms. A separate storage worker handles bounded SD write/read requests so slow or failed storage cannot stall reception or display updates.

## Screens, history and validation

### Screens

- **Dashboard:** Arabic title, temperature, humidity and brightness cards, units, per-sensor status and connection/data-age indicators.
- **Charts:** selectable metric with units and an explicitly relative time axis. Show the latest ten minutes by default. With usable SD history, allow browsing older ten-minute windows and stored sessions, loading/decimating only the selected window. Without usable SD storage, expose only the latest ten-minute RAM window.
- **Settings:** brightness slider, AP name, IP address, sender/data status and storage state (SD recording / RAM only / storage error). Credentials remain build-time settings.
- Use text alongside status colors. Missing readings display an unavailable indicator rather than zero.

### History and SD-card fallback

- Always keep 120 five-second buckets in RAM, storing the most recent accepted value/status in each bucket. This guarantees a responsive ten-minute chart even while SD operations are pending.
- At startup, check the configured SD hardware/backend, attempt a non-destructive FAT mount and verify that the card is writable. A usable card enables persistent recording; no card, an unsupported bus configuration, mount failure, read-only media or a write failure leaves the application in ten-minute RAM mode.
- Keep SD operations behind the reusable history-storage interface. Store each completed five-second bucket, including sensor statuses and explicit gaps, through a bounded worker queue. Storage failures and queue overflow must be surfaced as recording gaps/storage status, never block the measurement task, reboot the device or invent saved samples.
- Use versioned CSV records with session identifier, monotonic elapsed time, metric values and statuses. Create a new session on each reboot; do not assume elapsed times from different boots form a continuous wall-clock timeline. Leave invalid/stale chart points as gaps.
- Batch writes and sync at most every 30 seconds; document that a sudden power loss can lose the unsynced tail. Ignore an incomplete final CSV row when reading a previous session.
- Rotate project-owned history files at 1 MiB and enforce a configurable recording budget, initially 32 MiB. Remove only the oldest complete Room Monitor history files when that budget is reached; never delete unrelated files or format a card automatically. On other full-card/write failures, continue in RAM mode and display the storage error.
- Retain saved sessions across reboots and query older chart windows from SD without loading the entire archive into RAM. The RAM window itself resets on reboot. Missing or unreadable archived windows show a clear unavailable state.
- Retry unavailable storage every 30 seconds using the storage worker. If a card becomes usable, begin recording from that point; do not fabricate history for time when recording was unavailable. Manual card insertion/removal still requires physical verification on the selected board.
- Persist user-selected brightness in NVS when the interaction finishes. Measurement history goes to SD only, not internal flash. Cloud services and alarm functionality remain excluded.

### SD hardware allocation prerequisite

- Verify the actual board revision, SD wiring and peripheral ownership before claiming SD availability. The current profile uses SPI2 for LCD and SPI3 for touch. The commonly used onboard SD wiring is MOSI 23, MISO 19, SCLK 18, CS 5; it cannot be added as a third independent hardware SPI bus or attached to an existing bus with different signal pins without a supported board/backend change.
- Preserve the working LCD and touch configuration. The storage adapter must report an unsupported allocation and select RAM fallback if concurrent SD operation is not supported. Do not silently remap an active SPI host, disable touch, or use an SDMMC pin assignment that conflicts with the LCD.
- Concurrent onboard SD support requires a verified compatible allocation/backend before implementation can be accepted as working on this board. Card presence alone does not prove storage is usable; document this hardware dependency in setup instructions and test results.
- The existing `AlSayadLib/ESP32/SDCardController` is not a drop-in backend: it has CAN Logger-specific restrictions and fixed SDMMC pins that overlap the LCD. Implement an original board-configurable adapter using ESP-IDF FATFS/SD APIs where the verified allocation permits it; retain the abstract storage interface for other supported backends.

### Checks and acceptance

- Test decoding, valid zero, independent sensor failures, malformed/oversized payloads, unsupported versions, queue saturation, stale transitions and recovery.
- Test history rollover, gaps and bucket timing with an injectable clock.
- Test SD absent, unsupported pin/bus allocation, mount failure, read-only/full media, write failure, storage-queue overflow and recovery without blocking the UI or reception.
- Test SD file rotation/budget enforcement, preservation of unrelated files, session separation, archived-window queries and recovery from an incomplete final record. Verify that no automatic formatting occurs.
- Verify that all reusable code lives under `AlSayadLib/ESP32/`, that project integration uses `SmartDisplay/libs/AlSayadLib/`, and that each component is registered exactly once.
- Verify that logo generation uses the SVG as its input and resolves its current linked resources reproducibly.
- Verify that injected typed measurements update the UI without starting Wi-Fi.
- Build the C application and check C++ compatibility; retain the automatic memory report.
- Provide a host sender utility with clearly marked test scenarios for normal readings, errors and reconnection.
- On the board, verify Arabic shaping/RTL, logo timing, touch navigation, chart gaps, brightness persistence, AP reconnection and continued UI responsiveness during network/storage failures. Confirm actual simultaneous LCD/touch/SD operation before marking the SD backend supported; otherwise record the hardware limitation and verify RAM fallback.
- Record build results separately from physical test results. No flashing, publication or claims of successful hardware testing are implied.
