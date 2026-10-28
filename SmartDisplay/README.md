# AlSayad Room Monitor - SmartDisplay

Arabic ESP-IDF receiver and touch UI for the working ESP32-2432S028R/ST7789 board. The display creates a Wi-Fi access point; a separate Measurement ESP32 sends temperature, humidity and relative brightness with independent sensor states.

The receiver, reusable libraries and simulated test sender are implemented. Measurement-board sensor firmware remains a separate deliverable. No hardware was flashed or physically tested by this update.

## Build and configure

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
$env:PYTHONUTF8 = '1'
idf.py menuconfig
# AlSayad Room Monitor: credentials, accepted device ID and optional SD settings
idf.py build
```

Every build prints the ESP-IDF memory table. Firmware: `build/SmartDisplay.bin`; log: `build/room-monitor-build.log`.

Defaults: AP **AlSayad-Room**, example password **AlSayadRoom**, accepted device ID **measurement-01**. Join that AP and send to `http://192.168.4.1/api/v1/measurements`. No router or internet is required. Configure your intended credentials through menuconfig; passwords are neither logged nor displayed.

The installed ESP-IDF 6.1 toolchain and dependency lockfile are retained. The decoder privately depends on `espressif/cjson` version `1.7.19~2`. LVGL 9.3.0, esp_lvgl_port 2.9.0 and the working LCD/touch drivers remain pinned.

The firmware exceeds the previous 1 MiB factory partition. `partitions.csv` provides a **3 MiB factory application** within the configured 4 MiB flash. NVS stays at `0x9000`, size `0x6000`; the application still starts at `0x10000`. There is no OTA slot or internal-flash measurement archive. Flash the generated partition table along with the firmware:

```powershell
idf.py -p COM3 flash monitor
```

## UI and data behavior

- **الرئيسية:** temperature (°C), humidity (%RH), relative brightness (%).
- **المخطط:** select a metric; latest ten minutes or saved SD sessions/windows.
- **الإعدادات:** AP/IP, storage state, dropped-record count and brightness. Brightness saves to NVS when the slider is released.

The dashboard follows `AlSayad Room Monitor Dashboard.png`: navy background, cyan frames, Arabic headings, independent sensor icons and per-sensor status strips. Tap a sensor card to open its chart, or the top-left gear to open settings. Chart/settings pages retain bottom navigation. The footer reports actual reading health and age; unavailable, failed and stale readings keep their distinct states.

The three sensor icons are separate transparent PNG assets in `AlSayadLib/ESP32/as_display/assets/icons`, compiled into alpha-preserving RGB565+A8 images. Supporting status/navigation icons are individual SVGs. See the asset directory's README for prompts and regeneration commands. The small header logo is generated through the canonical guidelines SVG, like the boot logo.

Zero is valid data. Error/unavailable/stale readings display `--` with a status label. Ten seconds without an accepted frame makes all readings stale. AP client connection and received measurement data are shown separately. Test frames explicitly marked `simulated` add a **تجريبي** indicator to the title.

The library supplies the nonblocking two-second boot overlay. Its canonical input is `Guidlines/brand/alsayad-logo.svg`. This SVG currently links a PNG internally; generation resolves it through the SVG and embeds RGB565 data. Runtime SVG/full-resolution image decoding is unnecessary. Licensed Cairo fonts include contextual glyph mappings, RTL labels and separate LTR numeric/unit labels.

## Shared libraries

Reusable implementations live under `D:/GitHub/AlSayad/AlSayadLib/ESP32/`:

| Component | Responsibility |
| --- | --- |
| `as_display` | Hardware, widgets, boot logo, fonts, palette and charts |
| `AsMeasurements` | Portable model, current/stale states and RAM history |
| `AsMeasurementJson` | Bounded cJSON decoding without JSON types in public models |
| `AsTransport` | Transport-neutral frame callback/start/stop contract |
| `AsWifiTransport` | Original ESP-IDF WPA2 AP and HTTP receiver |
| `AsHistoryStorage` | Storage-neutral contract and bounded CSV archive operations |
| `AsSdHistory` | Original SD adapter, allocation checks and asynchronous worker |

`libs/AlSayadLib/AlSayadLib.cmake` registers only these components. Override the CMake cache variable `ALSAYAD_LIB_ROOT` for another shared checkout. The old `AlSayadEmbedded/components/as_display` implementation was moved, not duplicated. Existing widget APIs and `AsDisplay.hpp` remain compatible, with additional `showBootLogo`, `addChart` and `setChartPoints` methods. CAN Logger-specific Wi-Fi/SD code is not compiled into the application.

`main` owns board configuration, screen composition and application wiring. Network callbacks decode/enqueue typed data; the main loop updates the synchronized store. The UI refreshes every 250 ms without sensor reads, JSON, networking or file I/O. Alternative protocols can implement `AsTransport` or submit typed measurements through `as_room_submit()`.

## SD history and board allocation

RAM always retains 120 five-second buckets. On supported hardware, writable FAT SD storage records completed buckets asynchronously and permits older-session/window browsing. The UI offers the newest 16 sessions.

**This CYD profile uses SPI2 for LCD and SPI3 for touch.** The onboard SD wiring (23/19/18/5) cannot become a third independent hardware SPI bus. This profile therefore reports RAM / SD bus unavailable even with a card inserted. It preserves LCD/touch and does not silently remap buses. Actual SD recording requires a verified compatible allocation/backend. The adapter checks host/pin conflicts before mounting.

Supported SD logs rotate at 1 MiB within a configurable 32 MiB budget. Only canonical Room Monitor files can be evicted; unrelated files remain untouched. No automatic formatting occurs. Sync is every 30 seconds and on rotation, so power loss can lose an unsynced tail. Incomplete final rows are ignored. Storage errors fall back to RAM; supported allocations retry every 30 seconds. Recovery starts a new recording session, without fabricating missing samples. Session time is elapsed time, not an assumed real-time clock across reboots.

## Test sender and verification

See [PROTOCOL.md](PROTOCOL.md) for the sender contract and [tests/README.md](tests/README.md) for test commands. The host utility uses clearly marked simulated readings, not sensors:

```powershell
python tools/AsMeasurementSender.py --scenario normal --count 20
python tools/AsMeasurementSender.py --scenario faults --count 20
python tools/AsMeasurementSender.py --scenario outage --count 20
python tools/AsMeasurementSender.py --scenario zero --count 5
python tools/AsMeasurementSender.py --scenario malformed --count 1
python tools/AsMeasurementSender.py --dry-run
```

Real LVGL host renders are under `build/ui-preview/`. Tests cover parsing, independent errors, stale recovery, history gaps/rollover, CSV rotation/sessions/tails, callbacks, deletion and navigation. Compressed-font support is enabled. The pinned LVGL chart implementation computes coordinates for gap sentinels before skipping them; its build uses `-fwrapv` to define that arithmetic without changing managed source.

Physical acceptance remains: cold boots, colors/Arabic, touch, AP/HTTP traffic between boards, burst/queue saturation, NVS brightness, runtime heap and supported SD fault/recovery checks. This update performs no flashing or publication.
