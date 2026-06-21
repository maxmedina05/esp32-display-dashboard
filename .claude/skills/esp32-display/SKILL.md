---
name: esp32-display
description: Drive the 1.14" ST7789 (135x240) display on the ESP32 dashboard board using TFT_eSPI. Use when writing or debugging anything that draws to the screen — pin config, init, text, colors, layout, rotation, or fixing a blank/garbled/inverted display.
---

# ESP32 display (ST7789 135×240 via TFT_eSPI)

Library: **TFT_eSPI 2.5.43**. The panel is configured entirely through `build_flags` in
`platformio.ini` (NOT a `User_Setup.h`).

## Confirmed working pin map (T-Display clone — do not change without reason)
```
ST7789_DRIVER, TFT_WIDTH=135, TFT_HEIGHT=240, TFT_INVERSION_ON
TFT_MOSI=19  TFT_SCLK=18  TFT_CS=5  TFT_DC=16  TFT_RST=23  TFT_BL=4 (active HIGH)
SPI_FREQUENCY=40000000
```
❌ The "ideaspark" pinset (13/14/15/26/25/27) does NOT work on this board — tried first,
screen stayed completely dark. Do not suggest it.

## Minimal usage
```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup() {
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH); // belt-and-suspenders backlight
  tft.init();
  tft.setRotation(1);        // 1 or 3 = landscape (240 wide x 135 tall)
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM); // anchor (TL_DATUM, MC_DATUM, etc.)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Hello", 120, 67, 4); // x, y, font (2/4/6/7/8 loaded)
}
```

## Layout facts
- Rotation 1/3 → **240×135** (landscape, what we use). Rotation 0/2 → 135×240 (portrait).
- Fonts loaded: GLCD(1), 2, 4, 6, 7(7-seg), GFXFF, smooth fonts.
- `weather_code`/digits look great in font 7 (7-segment) for clocks.

## Troubleshooting a bad display
| Symptom | Fix |
|---|---|
| Totally dark, no glow | wrong `TFT_BL` pin → check backlight GPIO |
| Backlight glows but blank/garbled | wrong data pins (MOSI/SCLK/DC/CS/RST) |
| Colors look like a photo negative | toggle `TFT_INVERSION_ON` / `_OFF` |
| Red↔Blue swapped | add/remove `TFT_RGB_ORDER=TFT_BGR` |
| Image shifted / cut off | 135×240 offset; ensure TFT_WIDTH/HEIGHT correct |

## To avoid flicker
Draw into a `TFT_eSprite` (offscreen buffer) and push it, rather than clearing+redrawing
the whole screen each update. Good for the clock seconds tick.
