# Host verification

Tests compile actual shared C implementations and the actual application UI. They do not establish successful hardware operation.

## Portable C modules

Run an ESP-IDF build/reconfigure first to resolve cJSON. The tested native Windows compiler is supplied by the ziglang package:

```powershell
python -m pip install --target build/host-tools ziglang pillow fonttools brotli uharfbuzz
python tests/RunHostTests.py --zig build/host-tools/ziglang/zig.exe
python tests/CheckCpp.py
```

Coverage: valid zero, independent sensor states, malformed/truncated/oversized payloads, wrong device/version, duplicate members, NUL/depth limits, allocation failure, stale/recovery timing, late samples, history boundaries/gaps/rollover, CSV rotation/sessions, missing directories, incomplete tails and preservation of unrelated files. Temporary archives are removed after testing. The actual Wi-Fi/HTTP adapter is also tested with SDK boundary mocks: AP start/cleanup, borrowed event loops, partial body reads, deadlines and HTTP 202/400/408/413/503 responses. This does not test the physical radio or real FreeRTOS queue saturation.

## Actual LVGL UI

Use the ESP-IDF PowerShell environment for CMake/Ninja:

```powershell
python tests/RunUiTests.py --zig build/host-tools/ziglang/zig.exe
$env:PYTHONPATH = (Resolve-Path build/host-tools).Path
python tests/RenderCaptures.py build/ui-preview
```

The first host build compiles LVGL; subsequent builds are incremental. It uses the firmware's 64 KiB LVGL allocation setting and real Cairo/logo assets. Coverage includes navigation, click callbacks/self-deletion, slider/switch events, programmatic updates without callbacks, type rejection, charts, splash timer/manual cleanup and fixed geometry. PNGs under `build/ui-preview/` show test-input screens.

Only the SDK/driver/application-data boundaries are stubbed. The tests do not exercise radio association, actual touch, SD mounting/removal, FreeRTOS scheduling or device heap.

## Asset regeneration

Generated C assets are included, so normal firmware builds require no asset tooling:

```powershell
npm.cmd install --prefix build/asset-tools lv_font_conv@1.5.3 sharp@0.35.5
$env:PYTHONPATH = (Resolve-Path build/host-tools).Path
python D:/GitHub/AlSayad/AlSayadLib/ESP32/as_display/tools/GenerateAssets.py --svg D:/GitHub/AlSayad/Guidlines/brand/alsayad-logo.svg --node-modules build/asset-tools/node_modules
node D:/GitHub/AlSayad/AlSayadLib/ESP32/as_display/tools/GenerateIcons.cjs build/asset-tools/node_modules D:/GitHub/AlSayad/Guidlines/brand/alsayad-logo.svg
```

SVG resources are embedded before rendering. Cairo WOFF2 sources and their OFL license are bundled. HarfBuzz is used only during generation to map contextual glyphs for LVGL; it is not an ESP32 runtime dependency. Generated output excludes temporary paths.

## Physical acceptance

Check cold boot/logo, Arabic/colors/touch, Wi-Fi/HTTP errors and queue bursts, independent sensor failures, stale recovery, brightness persistence and runtime heap. On a board with a supported SD allocation, also exercise card absence, full/read-only media, write/removal/recovery faults and continued UI/network responsiveness. Record actual extended-run/power-cycle results separately from host tests.
