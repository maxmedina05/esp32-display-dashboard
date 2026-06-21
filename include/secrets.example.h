// secrets.example.h  —  TEMPLATE (safe to commit)
//
// Copy this file to `secrets.h` in the same folder and fill in real values:
//     cp include/secrets.example.h include/secrets.h
//
// `include/secrets.h` is gitignored AND blocked from Claude — it never gets
// committed or read by the assistant. Only edit it yourself.

#pragma once

// ---- WiFi (Stage 2) ----
#define WIFI_SSID      "your-network-name"
#define WIFI_PASSWORD  "your-network-password"

// ---- Anthropic usage proxy (Stage 6) ----
// The ESP32 talks to a small LAN helper, not Anthropic directly.
// #define USAGE_PROXY_URL "http://192.168.x.x:port/usage"
