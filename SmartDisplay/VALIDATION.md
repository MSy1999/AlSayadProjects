# Room Monitor implementation verification

Recorded 25 October 2026. No board flashing or physical test claims are made.

## Firmware build

`idf.py build` passed with ESP-IDF 6.1.0, target ESP32 and the retained pinned
drivers/LVGL dependencies. The decoder uses `espressif/cjson` 1.7.19~2 privately.
No compiler warnings were reported. SDK configuration/include-validation notes
can appear during reconfiguration; they did not prevent the build.

Log: `build/dashboard-build.log`. Firmware: `build/SmartDisplay.bin`.

| Build-time allocation | Used bytes | Remaining bytes | Total bytes |
| --- | ---: | ---: | ---: |
| Factory application partition | 1,442,640 | 1,703,088 | 3,145,728 |
| DRAM | 120,911 | 59,825 | 180,736 |
| IRAM | 99,663 | 31,409 | 131,072 |
| RTC SLOW | 64 | 8,128 | 8,192 |

The app image is `0x160350` bytes and leaves 54% of its partition free. The
configured physical flash is 4 MiB. These numbers describe static allocation,
not the runtime free heap. Startup logs report device free heap after networking
starts. Flash the generated partition table together with the application.

## Software checks performed

- **Portable C and archive tests passed:** actual model/decoder/CSV code compiled
  as C11 with warnings treated as errors. Cases cover valid zero, independent
  failures, wrong/truncated/oversized/duplicate/embedded-NUL/deep JSON, allocation
  failure, stale boundaries/recovery, late timestamps, 120-bucket rollover/gaps,
  file rotation/budget behavior, separate sessions, simulated-data persistence,
  incomplete final records and preservation of unrelated files.
- **HTTP adapter tests passed with SDK boundary mocks:** actual request handler
  and cJSON decoder handle partial reads and produce 202/400/408/413/503 results.
  AP lifecycle tests cover already-started/stopped DHCP states, borrowed event
  loops and cleanup after registration failure. These do not exercise a radio
  or actual FreeRTOS queue saturation.
- **Real LVGL host tests passed:** actual UI/widgets/assets render boot, waiting,
  valid, sensor-error, stale, chart, settings and archived-session screens.
  Tests exercise navigation, click/self-deletion, slider/switch events, copied
  updates without callbacks, wrong widget types, chart ownership and splash
  cleanup. Image creation/source replacement/type validation and clickable card
  navigation are covered. Archived simulated values remain marked as test data.
- **Host UI memory check:** the firmware's 64 KiB LVGL allocation setting was
  used. The latest host monitor reported 44,952 bytes used, 12,904 bytes free,
  57,856 bytes managed pool and 48,552 bytes peak. This excludes SDK/driver
  allocations and is not a measurement of the ESP32 runtime heap.
- **ESP32 C++17 checks passed:** compatible `AsDisplay.hpp` facade, image/chart
  methods and public shared-library headers compile with the configured SDK.
- **Asset generation checked:** sensor icons are separate transparent PNGs,
  with original alpha retained. Re-running the icon converter produced
  byte-identical C output. SVG-derived boot/header logos and Cairo 12/14/16/20/28
  fonts are embedded. Font source/license notices are retained. The current SVG's
  linked PNG is resolved through the SVG.
- **Test sender dry-run passed:** emits a version-1 frame with `simulated:true`;
  it does not read sensors or claim a successful connection.

Commands are documented in [tests/README.md](tests/README.md). Logs are under
`build/dashboard-core-tests.log`, `build/dashboard-ui-tests.log`,
`build/dashboard-cpp-check.log` and `build/dashboard-build.log`.

## Host-rendered previews

These are injected test inputs, not physical measurements:

- [Dashboard](build/ui-preview/dashboard.png)
- [Sensor error](build/ui-preview/sensor-error.png)
- [Stale readings](build/ui-preview/stale.png)
- [Chart](build/ui-preview/chart.png)
- [Settings](build/ui-preview/settings.png)
- [Archived test session](build/ui-preview/archive.png)
- [SVG-derived boot logo](build/ui-preview/boot.png)

## Physical acceptance still pending

The working ST7789/LCD/touch configuration is preserved, but this updated firmware
has not been installed or observed on the board. Verify cold boots, Arabic/colors,
touch navigation/calibration, actual AP association/HTTP traffic, sender failure
and recovery, bursts, brightness persistence, runtime heap and extended operation.

The current board reserves SPI2 for LCD and SPI3 for touch. Its SD allocation is
therefore unsupported and intentionally selects the ten-minute RAM history,
without initializing conflicting buses/pins or disabling touch. The SD adapter,
worker and file backend are implemented for a verified compatible allocation.
Actual card mounting, full/read-only media, removal/write failures and recovery
must be tested on such hardware before claiming concurrent SD recording support.
