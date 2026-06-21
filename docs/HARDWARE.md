# Hardware Reference — ESP32 1.14" ST7789 Display Board

> There is **no official datasheet** for this specific clone. This document is the
> authoritative reference for *our* board: every value here was either read off the
> device or **confirmed empirically by flashing it**. See `docs/datasheets/` for the
> official datasheets of the individual components (ESP32-WROOM-32, ST7789).

## Identity

| Field | Value |
|---|---|
| Seller / brand | TENSTAR ROBOT (AliExpress) |
| Listing | item `1005009890213912` (de.aliexpress.com) |
| Form factor | ESP32 dev board with **integrated 1.14" IPS display** |
| Closest known design | **LilyGO/TTGO T-Display clone** (confirmed by working pinout) |
| Also marketed as | "ideaspark ESP32 1.14 inch ST7789" family |

## MCU / Module

| Field | Value |
|---|---|
| Module | ESP32-WROOM-32 (Xtensa dual-core LX6) |
| Wireless | WiFi 802.11 b/g/n, Bluetooth v4.2 BR/EDR + BLE |
| PlatformIO board id | `esp32dev` |
| Flash | 4 MB assumed by `esp32dev` ⚠️ listing claims 8 MB — **verify before relying on >4 MB** |

## USB / Serial — IMPORTANT

| Field | Value |
|---|---|
| USB connector | USB-C |
| USB-serial chip | **QinHeng CH9102** (not CH340!) |
| USB VID:PID | `1a86:55d4` |
| Device serial | `5B09016499` |
| **Enumerates as** | **`/dev/ttyACM0`** (CDC-ACM) — NOT `/dev/ttyUSB0` |
| Auto-reset | Works via RTS/DTR — no need to hold BOOT to flash |
| Linux driver | Built into kernel (cdc_acm) — no install needed |

> ⚠️ Because it's a CH9102 (CDC-ACM), the port is `ttyACM0`, which trips up guides that
> assume `ttyUSB0`. User must be in the `dialout` group to access it.

## Display — CONFIRMED WORKING

| Field | Value |
|---|---|
| Panel | 1.14" IPS, full color |
| Controller | **ST7789V** |
| Resolution | **135 × 240** |
| Interface | 4-wire SPI |
| Color inversion | **ON** (`TFT_INVERSION_ON`) |
| Backlight | active HIGH |
| Library | Bodmer **TFT_eSPI** |

### Confirmed pin map (these are the ones that WORK — T-Display clone wiring)

| Signal | GPIO |
|---|---|
| TFT_MOSI | **19** |
| TFT_SCLK | **18** |
| TFT_CS   | **5** |
| TFT_DC   | **16** |
| TFT_RST  | **23** |
| TFT_BL (backlight) | **4** (active HIGH) |
| SPI clock | 40 MHz (works; 80 MHz untested) |

> ❌ Pins that do **NOT** work for this board (tried first, screen stayed dark) —
> the "ideaspark" set: MOSI=13, SCLK=14, CS=15, DC=26, RST=25, BL=27.

## Buttons — TODO (not yet confirmed)

- One labelled user button **"K1"**, plus BOOT and RST.
- T-Display clones typically wire the two user buttons to **GPIO0** and **GPIO35**.
- ⏳ Not yet verified on this board — confirm in a later stage if we need button input.

## Open questions to verify later

- [ ] Real flash size (4 MB vs 8 MB)
- [ ] Exact GPIO(s) for the K1 button
- [ ] Whether 80 MHz SPI is stable
- [ ] Is there a battery/charging circuit? (some T-Display clones have one)
