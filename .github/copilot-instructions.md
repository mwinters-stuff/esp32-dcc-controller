# Copilot Instructions — esp32-dcc-controller

Touchscreen DCC-EX Throttle controller firmware for ESP32-S3, using ESP-IDF, LVGL, and LovyanGFX. Runs as a single
firmware image (no OS/host build); there is no unit test suite for `main/` — validation is build + on-device flash.

## Build

Requires ESP-IDF v5.5.4 sourced into the shell (`source /home/mathew/.esp/v5.5.4/esp-idf/export.sh` or the
`~/.espressif` path in `.vscode/settings.json`).

```bash
idf.py set-target esp32s3   # only needed once / after a clean
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # flash + serial monitor (Ctrl+] to exit)
```

There is no `idf.py test`/host unit test target for this app. Treat a clean `idf.py build` as the correctness check
for any change. `idf.py menuconfig` edits `sdkconfig` (e.g. rotary encoder GPIO pins/defaults come from
`main/Kconfig.projbuild`).

Formatting: `.clang-format` (LLVM-based, 2-space indent, 120 col limit) — VS Code has `editor.formatOnSave` on.
There is no separate lint step beyond clang-format/clangd diagnostics.

## Architecture

Source lives under `main/`, organized by concern (`file(GLOB_RECURSE ...)` in `main/CMakeLists.txt` picks up
`*.cpp` from `main/`, `display/`, `utilities/`, `ui/`, `connections/`, and `*.c` from `images/`):

- **`main/main.cpp`** — entry point. `app_main()` inits NVS then calls `setup()`, then runs the LVGL tick loop
  (`lv_timer_handler()` every 10ms) forever. Also owns global concerns that don't fit one screen: touch input
  callback (`my_touchpad_read`), display sleep/wake via brightness fade after `INACTIVITY_TIMEOUT_MS`, and
  global `lv_msg_subscribe` handlers for Wi-Fi failure / DCC disconnect popups.
- **`main/utilities/WifiHandler.*`** — Wi-Fi STA connection (saved/manual SSID) and mDNS discovery of
  `_withrottle._tcp` DCC-EX command stations into `withrottle_devices`.
- **`main/connection/`** — the DCC-EX TCP link, layered as: `wifi_connection.*` (`TCPSocketStream` over lwIP TCP +
  heartbeat timeout), `wifi_control.*` (`WifiControl` singleton owning the `DCCEXProtocol` instance and its
  polling task), `dcc_delegate.*` (translates `DCCEXProtocol` callbacks — roster/turnout/route/turntable/power/loco
  — into app events).
- **`main/display/`** — one `Screen` subclass (see `Screen.h`) per UI screen (Wi-Fi connect/list, DCC connect,
  roster/turnout/route/turntable lists, calibration, message boxes, waiting screen, first screen/menu). List
  screens follow a `XxxList` + `XxxListItem` + shared `RotaryListScreenBase` pattern for rotary-encoder-navigable
  lists.
- **`main/ui/`** — `lv_msg` pub/sub wrapper (message IDs are the app's event bus) and LVGL theme/style.
- **`main/utilities/`** — rotary encoder driver, calibration/NVS helpers, HTTP screenshot server
  (`ScreenshotHttp`, port 8080 — see `Screenshot.*`).
- **`main/LGFX_ILI9488_S3.hpp`** — LovyanGFX panel/bus/touch pin config; this is the single source of truth for
  display/touch GPIO wiring (see README hardware table).
- **`main/definitions.h`** — all cross-module `MSG_*` LVGL message IDs and `NVS_*` namespace/key constants. Add
  new inter-module events/persisted keys here rather than hardcoding strings/ints at call sites.

Communication between modules is mostly event-driven via `lv_msg_subscribe`/publish (see `main/ui/lv_msg.h`) using
the IDs in `definitions.h`, not direct method calls — check `definitions.h` first when tracing how a Wi-Fi/DCC
event reaches the UI.

External dependencies (LVGL, LovyanGFX, DCCEXProtocol, mdns, button) are pulled by the IDF Component Manager into
`managed_components/` per `main/idf_component.yml`; don't hand-edit files under `managed_components/` — change the
version/git ref in `idf_component.yml` instead (DCCEXProtocol currently pins the `devel` branch of
`mwinters-stuff/DCCEXProtocol`).

## Conventions

- Singletons use a `static std::shared_ptr<T> instance()` accessor (see `WifiControl::instance()`,
  `WifiHandler::instance()`, `FirstScreen::instance()`, `ManualCalibration::instance()`) — follow this pattern for
  new manager/controller classes rather than raw globals or `getInstance()` naming.
- Screens derive from `display::Screen` (`main/display/Screen.h`) and implement `show(parent, parentScreen)`;
  `showScreen()` is the public entry point and tracks `parentScreen_` for back-navigation.
- Cross-module signaling goes through `lv_msg_subscribe`/publish with IDs from `main/definitions.h`
  (`MSG_WIFI_*`, `MSG_DCC_*`) rather than callbacks passed across module boundaries.
- Persisted state (Wi-Fi credentials, DCC host/port, touch calibration) uses NVS namespaces/keys defined in
  `main/definitions.h` (`NVS_NAMESPACE*`, `NVS_*`) — reuse these constants, don't inline new key strings.
- Rotary-encoder-navigable list screens (roster/turnout/route/turntable) extend `RotaryListScreenBase` and pair a
  `XxxList` screen with a `XxxListItem` row type; follow this pair when adding a new list-based screen.
