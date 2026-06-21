---
name: esp32-data-sources
description: Fetch the dashboard's data on the ESP32 — Berlin time via NTP, weather via Open-Meteo, and Claude usage via the Anthropic Admin API. Use when implementing or debugging network data fetching (WiFi, HTTPS, NTP/timezone, JSON parsing) for Stages 3, 4, and 6.
---

# ESP32 data sources

How to fetch each widget's data. Parse JSON with **ArduinoJson 7.4.3** (v7 API:
`JsonDocument doc; deserializeJson(doc, payload);`). HTTPS via `WiFiClientSecure`
(use `client.setInsecure()` to skip cert pinning for these public APIs unless we add certs).

## Time — NTP + Europe/Berlin (Stage 3)  [no network lib beyond built-in]
```cpp
#include <time.h>
configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
struct tm t;
if (getLocalTime(&t)) { /* t.tm_hour, t.tm_min, strftime(...) */ }
```
The TZ string encodes Berlin DST automatically. No API key.

## Weather — Open-Meteo (Stage 4)  [no API key, free non-commercial]
```
GET https://api.open-meteo.com/v1/forecast
      ?latitude=52.52&longitude=13.41&timezone=Europe%2FBerlin
      &current=temperature_2m,weather_code,wind_speed_10m,relative_humidity_2m
      &daily=temperature_2m_max,temperature_2m_min,weather_code
```
- Response is JSON. `current.weather_code` is a **WMO code** → map to text/icon ourselves
  (0=clear, 1-3=partly cloudy, 45/48=fog, 51-67=rain, 71-77=snow, 80-82=showers,
  95-99=thunderstorm).
- Docs: https://open-meteo.com/en/docs

## Claude usage — Anthropic Admin API (Stage 6)
- `GET https://api.anthropic.com/v1/organizations/usage_report/messages`
  and `/v1/organizations/cost_report`.
- Headers: `x-api-key: <ADMIN_KEY>`, `anthropic-version: 2023-06-01`.
- Needs an **org Admin API key** (not a normal key).
- ⚠️ **Don't put the admin key on the ESP32.** Plan: a tiny helper/proxy on an always-on
  machine holds the key, calls Anthropic, and serves a small summarized JSON on the LAN over
  plain HTTP for the ESP32 to fetch. Finalize at Stage 6.
- Consult the **`claude-api`** skill for exact request/response shapes.
- Docs: https://platform.claude.com/docs/en/manage-claude/usage-cost-api

## Secrets handling
WiFi creds + any keys go in a **gitignored** `include/secrets.h` (or `build_flags` from a
local `.env`), never committed. See README/STACK for the convention.

## Memory/stability tips
- Reuse a single `WiFiClientSecure`; don't allocate per request.
- Fetch on a timer (e.g. weather every 10–15 min, usage every few min — respect the
  Anthropic "poll ≤ once/min" guidance), not every loop.
- Parse only the fields you need (ArduinoJson filters) to save RAM.
