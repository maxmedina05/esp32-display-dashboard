---
name: esp32-dashboard-dev
description: Build, flash, and monitor the ESP32 display dashboard board. Use whenever working on this project's firmware — compiling, uploading to the device, reading serial output, or troubleshooting the toolchain/port. Covers the pinned toolchain and the board's quirks (CH9102 = /dev/ttyACM0, dialout group, non-interactive serial).
---

# ESP32 Dashboard — dev workflow

Operational guide for building/flashing this project. For hardware specs see
`docs/HARDWARE.md`; for pinned versions see `docs/STACK.md`.

## The board (essentials)
- LilyGO **T-Display clone**: ESP32-WROOM-32 + ST7789V 135×240 IPS, USB-C.
- USB-serial is a **CH9102** → port is **`/dev/ttyACM0`** (NOT `ttyUSB0`).
- Auto-reset works; no need to hold BOOT to flash.
- User must be in the `dialout` group (needed a **reboot**, not just logout).

## Pinned toolchain (do not bump without re-verifying on hardware)
- `platform = espressif32@7.0.1` → **Arduino-ESP32 core 2.0.17** (a 2.x line!).
- Libs exact-pinned in `platformio.ini`. When using online snippets, confirm they
  target **core 2.x** (3.x changed many peripheral APIs).

## Commands
```bash
PIO=~/.platformio/penv/bin/pio
$PIO run                # build
$PIO run -t upload      # build + flash to /dev/ttyACM0
$PIO run -t clean       # clean
```
The PIO Home GUI server times out on this machine — harmless, ignore it; use the CLI.

## Reading serial output  (IMPORTANT)
`pio device monitor` **fails in non-interactive shells** (needs a TTY console). Read the
port directly with pyserial instead:
```bash
~/.platformio/penv/bin/python - <<'PY'
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False); time.sleep(0.1)
s.reset_input_buffer()
end = time.time() + 6
while time.time() < end:
    line = s.readline().decode(errors='replace').rstrip()
    if line: print(line)
s.close()
PY
```
(The VSCode PlatformIO monitor button works fine for the user interactively.)

## Workflow philosophy
Build in **testable stages** (see README roadmap). Never advance a stage until the user
has verified the current one on the real device. The user wants to test each step.

## If flashing/port fails
- Confirm port: `ls -l /dev/ttyACM*` and `lsusb | grep 1a86`.
- Confirm access: `test -w /dev/ttyACM0 && echo ok` (else fix `dialout` + reboot).
- `curl | python` style installers are sandbox-blocked — have the user run those.
