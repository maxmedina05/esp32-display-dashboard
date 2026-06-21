# Tech Stack — pinned versions, decisions & sources

> Researched 2026-06-21. This is the **single source of truth** for which versions we use
> and *why*, plus the authoritative places to consult when something is unclear. The goal:
> never get bitten by APIs changing between versions or examples written for the wrong core.

## ⚠️ The #1 ESP32 gotcha: Arduino-ESP32 core 2.x vs 3.x

Arduino-ESP32 had a **major breaking release at 3.0** (peripheral APIs changed: `ledc*`,
`analogWrite`, `dac*`, ADC, etc.). Tutorials and snippets written for 2.x often **do not
compile on 3.x** and vice-versa. So the version we pin is not cosmetic — it determines
which examples are valid.

**We are on core 2.0.17.** When you find example code online, check it targets **2.x**.

## Pinned toolchain (verified installed & working)

| Component | Pinned version | Notes |
|---|---|---|
| PlatformIO platform `espressif32` | **7.0.1** | Bundles Arduino-ESP32 **2.0.17** |
| Arduino-ESP32 core | **2.0.17** | PlatformIO shows it as `framework-arduinoespressif32 @ 3.20017.x` (the `3.` is PlatformIO packaging; `20017` = core 2.0.17) |
| Based on ESP-IDF | 4.4.x | (what core 2.0.17 ships) |
| Toolchain | xtensa-esp32 8.4.0 | auto-selected by platform |
| esptool | 4.11.x | flashing |

Pinned in [`../platformio.ini`](../platformio.ini) as `platform = espressif32@7.0.1`.

### Why 2.0.17 and not core 3.x?
- It's what already **works on our board** (verified end-to-end in Stage 1).
- The vast majority of ESP32 + TFT_eSPI + WiFi/NTP/HTTPS tutorials target 2.x.
- It's frozen → zero version drift, exactly the project's goal.
- We don't need any 3.x-only feature for this dashboard.

### If we ever DO need core 3.x
The official `platformio/espressif32` lagged on 3.x; the community fork **pioarduino** is the
de-facto way to get core 3.x in PlatformIO:
```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/<ver>/platform-espressif32.zip
```
Only switch if a needed library requires core 3.x. Re-verify the display pins/init if we do.

## Pinned libraries

| Library | Pinned | Used for | Stage |
|---|---|---|---|
| `bodmer/TFT_eSPI` | **2.5.43** | ST7789 display driver + graphics/fonts | 1 ✅ |
| `bblanchon/ArduinoJson` | **7.4.3** | Parse weather + usage JSON | 4+ |

Built-in to the core (no external dep, frozen with 2.0.17) — **prefer these to avoid more deps**:
- `WiFi.h` — station mode networking
- `time.h` + `configTzTime()` — NTP + timezone (Stage 3)
- `WiFiClientSecure` + `HTTPClient` — HTTPS calls to APIs (Stage 4+)
- `WebServer.h` — the config web page (Stage 5). Simple, synchronous, zero extra deps.

### Web server choice (Stage 5)
- **Default: built-in `WebServer`** — fewest dependencies, frozen with the core, fine for a
  small config page.
- **Optional upgrade: `ESP32Async/ESPAsyncWebServer`** (the *maintained* fork — NOT the
  abandoned `me-no-dev` one) if we need non-blocking serving or WebSockets so display/network
  work never stalls the page. Pin it + its `ESP32Async/AsyncTCP` dep if adopted.

## External APIs

### Weather — Open-Meteo (Stage 4)
- **No API key, no signup.** Free for non-commercial ≤ 10,000 calls/day.
- Endpoint: `https://api.open-meteo.com/v1/forecast`
- Berlin: `latitude=52.52&longitude=13.41&timezone=Europe%2FBerlin`
- Ask for `current=temperature_2m,weather_code,wind_speed_10m` and/or `daily=...`.
- `weather_code` is WMO codes → map to icons/text ourselves.

### Time — NTP (Stage 3)
- `configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov")`
- That POSIX TZ string is **Europe/Berlin** with automatic DST. Then use `getLocalTime()`.

### Claude usage — Anthropic Admin API (Stage 6)
- Endpoint: `GET https://api.anthropic.com/v1/organizations/usage_report/messages`
  (and `/v1/organizations/cost_report`).
- Auth headers: `x-api-key: <ADMIN_KEY>` + `anthropic-version: 2023-06-01`.
- Needs an **Admin API key** (org-level), distinct from a normal API key.
- ⚠️ **Architecture note:** putting an admin key on the ESP32 + doing heavy HTTPS to
  api.anthropic.com is risky and memory-tight. **Plan:** a tiny helper/proxy on an
  always-on machine holds the key, calls Anthropic, and exposes a small summarized JSON on
  the LAN that the ESP32 fetches over plain HTTP. Decide details at Stage 6.
- Local reference: the **`claude-api` skill** in this environment has API specifics.

## 📚 Where to consult when in doubt (authoritative sources)

| Topic | Source |
|---|---|
| Arduino-ESP32 core (API, releases) | https://github.com/espressif/arduino-esp32/releases · https://docs.espressif.com/projects/arduino-esp32/en/latest/ |
| 2.x → 3.x migration | https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html |
| PlatformIO espressif32 platform | https://github.com/platformio/platform-espressif32/releases · https://docs.platformio.org/en/latest/platforms/espressif32.html |
| pioarduino fork (core 3.x) | https://github.com/pioarduino/platform-espressif32 |
| TFT_eSPI | https://github.com/Bodmer/TFT_eSPI · examples in its `examples/` folder |
| ArduinoJson v7 | https://arduinojson.org/v7/ (has an Assistant that generates code) |
| ESPAsyncWebServer (maintained) | https://github.com/ESP32Async/ESPAsyncWebServer |
| Open-Meteo | https://open-meteo.com/en/docs |
| Anthropic Usage/Cost API | https://platform.claude.com/docs/en/manage-claude/usage-cost-api |
| ESP32-WROOM-32 / ESP32 / ST7789V datasheets | [`datasheets/`](datasheets/) |
| General ESP32 tutorials (target 2.x — verify) | https://randomnerdtutorials.com/ |

> When you find a snippet online: confirm it targets **Arduino-ESP32 2.x**, and prefer
> built-in libraries over adding new dependencies.
