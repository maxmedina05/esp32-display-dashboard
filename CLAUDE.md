# CLAUDE.md — orientation for AI sessions

Read this first. It captures the non-obvious, hard-won facts about this project so you
don't repeat mistakes from earlier sessions.

## What this project is

A LAN dashboard on an ESP32 + 1.14" ST7789 display. The screen shows info (Berlin
time, weather, Claude usage); a web page served by the device lets the user configure
what's shown. Built in **testable stages** — never advance a stage until the previous
one is verified on the real hardware. The user wants to test each stage himself.

See `README.md` for the roadmap, `docs/HARDWARE.md` for full hardware specs, and
**`docs/STACK.md` for pinned versions + authoritative sources to consult when in doubt**.

## Skills (auto-loaded — use them)
- **esp32-dashboard-dev** — build/flash/serial workflow + toolchain gotchas
- **esp32-display** — TFT_eSPI config, pinout, drawing API, display troubleshooting
- **esp32-data-sources** — NTP/time, Open-Meteo weather, Anthropic usage API

## Pinned stack (never bump without re-verifying on hardware)
`platform = espressif32@7.0.1` → **Arduino-ESP32 core 2.0.17** (a **2.x** line — online
snippets must target 2.x; core 3.x changed many APIs). TFT_eSPI@2.5.43, ArduinoJson@7.4.3.
Full rationale + sources in `docs/STACK.md`.

## The board (key facts)

- **LilyGO T-Display clone**: ESP32-WROOM-32 + ST7789V 135×240 IPS, USB-C.
- USB-serial is a **CH9102**, so it enumerates as **`/dev/ttyACM0`**, NOT `ttyUSB0`.
- Auto-reset works — no need to hold BOOT to flash.

## Confirmed display config (in `platformio.ini` build_flags)

```
ST7789_DRIVER, 135×240, TFT_INVERSION_ON
TFT_MOSI=19  TFT_SCLK=18  TFT_CS=5  TFT_DC=16  TFT_RST=23  TFT_BL=4 (active HIGH)
```
⚠️ The "ideaspark" pinset (13/14/15/26/25/27) does NOT work on this board — tried first,
screen stayed dark. Don't suggest it again.

## Toolchain & commands

- PlatformIO Core CLI: **`~/.platformio/penv/bin/pio`** (the PIO Home GUI server times
  out on this machine — harmless, ignore it; drive everything from the CLI).
- Build: `~/.platformio/penv/bin/pio run`
- Flash: `~/.platformio/penv/bin/pio run -t upload`
- **Serial monitor: `pio device monitor` FAILS in non-interactive shells** (needs a TTY
  console). Read serial with the pyserial snippet in `README.md` instead.

## Claude usage helper (runs as a systemd service)

The "Claude usage" widget is fed by `helper/usage_server.py`, a tiny local HTTP service
the ESP32 polls on the LAN (`http://<laptop-ip>:8088/usage`). It is installed as a
**systemd user service** `claude-usage.service` (unit at `~/.config/systemd/user/`,
runs `/usr/bin/python3`, linger enabled → survives reboot/logout, ~15 MB RAM).
- Manage: `systemctl --user {status,restart,stop} claude-usage.service`
- Logs: `journalctl --user -u claude-usage.service -f`
- After editing `usage_server.py`, **restart the service** (don't `python3` it by hand —
  that double-binds port 8088). Full details + reinstall steps in `helper/README.md`.
- Subscription %s come from `~/.claude/esp32-usage.json`, written by `helper/statusline.sh`
  via the `statusLine` hook in `~/.claude/settings.json` (only refreshes while Claude Code
  is active).

## Gotchas learned

- `ttyACM0` not `ttyUSB0` (CH9102 is CDC-ACM).
- User must be in `dialout` group; required a **reboot** (logout wasn't enough).
- `curl | python` style installers are blocked by the sandbox — have the user run those.
- Flashing over the factory firmware erased the demo (expected; reassure if asked).

## Still unverified (see docs/HARDWARE.md)

- Real flash size (4 vs 8 MB), K1 button GPIO, 80 MHz SPI stability, battery circuit.

## User context

- The dashboard defaults to Berlin (timezone Europe/Berlin) — change `lat`/`lon` and the
  POSIX TZ string in `src/main.cpp` for your own location.
- Built for hands-on testing at each stage, with clear explanations for newcomers.
