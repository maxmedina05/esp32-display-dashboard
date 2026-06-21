#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASSWORD (gitignored, never committed)

// Stage 6: full dashboard — Berlin time, weather, and Claude Code usage,
// all configurable via a LAN web page (settings persisted to NVS).

TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
Preferences prefs;

static const int SCREEN_W = 240;
static const int SCREEN_H = 135;
static const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char* MDNS_NAME = "esp32-dashboard";
static const uint32_t WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;  // 10 min
static const uint32_t USAGE_INTERVAL_MS   =  2UL * 60UL * 1000UL;  //  2 min

// ---------------- Claude / Anthropic palette ----------------
// Hex values from Anthropic's published brand palette (colors aren't
// copyrightable; no logo/wordmark is reproduced). RGB565 for the ST7789.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static const uint16_t CLAUDE_CLAY  = rgb565(217, 119,  87);  // #D97757 signature coral
static const uint16_t CLAUDE_CREAM = rgb565(240, 238, 230);  // #F0EEE6 ivory (primary text)
static const uint16_t CLAUDE_GREY  = rgb565(140, 130, 120);  // #8C8278 warm grey (secondary)
static const uint16_t CLAUDE_AMBER = rgb565(224, 164,  88);  // #E0A458 gauge warning
static const uint16_t CLAUDE_RED   = rgb565(190,  59,  38);  // #BE3B26 gauge near-limit
static const uint16_t CLAUDE_SLATE = rgb565( 38,  38,  37);  // #262625 near-black base

// ---------------- settings (configurable via the web page) ----------------
struct Settings {
  String label = "Berlin";
  float  lat = 52.52f, lon = 13.41f;
  bool   showClock = true, showDate = true, showWeather = true, use12h = false;
  bool   showUsage = true;
  String usageUrl = "http://192.168.0.163:8088/usage";  // the LAN helper
};
Settings cfg;

void loadSettings() {
  prefs.begin("dash", true);
  cfg.label      = prefs.getString("label", cfg.label);
  cfg.lat        = prefs.getFloat("lat", cfg.lat);
  cfg.lon        = prefs.getFloat("lon", cfg.lon);
  cfg.showClock  = prefs.getBool("clk", cfg.showClock);
  cfg.showDate   = prefs.getBool("date", cfg.showDate);
  cfg.showWeather= prefs.getBool("wx", cfg.showWeather);
  cfg.use12h     = prefs.getBool("h12", cfg.use12h);
  cfg.showUsage  = prefs.getBool("usg", cfg.showUsage);
  cfg.usageUrl   = prefs.getString("uurl", cfg.usageUrl);
  prefs.end();
}

void saveSettings() {
  prefs.begin("dash", false);
  prefs.putString("label", cfg.label);
  prefs.putFloat("lat", cfg.lat);
  prefs.putFloat("lon", cfg.lon);
  prefs.putBool("clk", cfg.showClock);
  prefs.putBool("date", cfg.showDate);
  prefs.putBool("wx", cfg.showWeather);
  prefs.putBool("h12", cfg.use12h);
  prefs.putBool("usg", cfg.showUsage);
  prefs.putString("uurl", cfg.usageUrl);
  prefs.end();
}

// ---------------- live state ----------------
bool   g_haveWeather = false;
float  g_tempC = 0;
String g_condition = "";
bool   g_haveUsage = false;
float  g_sessPct = 0;         // 5-hour session limit used %
float  g_weekPct = 0;         // 7-day weekly limit used %
long   g_sessReset = 0;       // epoch seconds the session window resets
volatile bool g_needFullRedraw = false;
volatile bool g_needWeatherNow = false;
volatile bool g_needUsageNow = false;

const char* wmoText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: case 63: case 65: return "Rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: return "Snow";
    case 77: return "Snow grains";
    case 80: case 81: case 82: return "Rain showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunderstorm+hail";
    default: return "Unknown";
  }
}

// ---------------- helpers ----------------
void centerMsg(const char* l1, uint16_t c1, const char* l2 = nullptr, uint16_t c2 = TFT_WHITE) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM); tft.setTextPadding(0);
  tft.setTextColor(c1, TFT_BLACK);
  tft.drawString(l1, SCREEN_W / 2, l2 ? 55 : 67, 4);
  if (l2) { tft.setTextColor(c2, TFT_BLACK); tft.drawString(l2, SCREEN_W / 2, 90, 2); }
}

bool connectWiFi(uint32_t timeoutMs = 20000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi \"%s\" ...\n", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) { delay(300); Serial.print('.'); }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool waitForTime(uint32_t timeoutMs = 15000) {
  configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov", "time.google.com");
  Serial.print("Syncing time via NTP ");
  struct tm t; uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (getLocalTime(&t, 500) && t.tm_year > (2020 - 1900)) { Serial.println(" done."); return true; }
    Serial.print('.');
  }
  Serial.println(" TIMEOUT."); return false;
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(cfg.lat, 4) +
               "&longitude=" + String(cfg.lon, 4) +
               "&current=temperature_2m,weather_code&timezone=auto";
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https; https.setConnectTimeout(8000); https.setTimeout(8000);
  Serial.println("Fetching weather...");
  if (!https.begin(client, url)) return false;
  int code = https.GET();
  if (code != HTTP_CODE_OK) { Serial.printf("  HTTP %d\n", code); https.end(); return false; }
  String payload = https.getString(); https.end();
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  if (doc["current"]["temperature_2m"].isNull()) return false;
  g_tempC = doc["current"]["temperature_2m"].as<float>();
  g_condition = wmoText(doc["current"]["weather_code"].as<int>());
  g_haveWeather = true;
  Serial.printf("  %.1f C (%s)\n", g_tempC, g_condition.c_str());
  return true;
}

bool fetchUsage() {
  if (WiFi.status() != WL_CONNECTED || cfg.usageUrl.length() < 8) return false;
  WiFiClient client;     // plain HTTP to the LAN helper (no TLS)
  HTTPClient http; http.setConnectTimeout(6000); http.setTimeout(6000);
  Serial.println("Fetching Claude usage...");
  if (!http.begin(client, cfg.usageUrl)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { Serial.printf("  HTTP %d\n", code); http.end(); return false; }
  String payload = http.getString(); http.end();
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { Serial.println("  JSON error"); return false; }
  JsonVariant lim = doc["limits"];
  if (lim.isNull() || lim["session_pct"].isNull()) {
    Serial.println("  no limits yet (is the status line configured?)");
    g_haveUsage = false; return false;
  }
  g_sessPct = lim["session_pct"].as<float>();
  g_weekPct = lim["week_pct"].as<float>();
  g_sessReset = lim["session_resets_at"] | 0L;
  g_haveUsage = true;
  Serial.printf("  session %.0f%%, week %.0f%%\n", g_sessPct, g_weekPct);
  return true;
}

// ---------------- dashboard UI ----------------
void drawStaticBackground() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, SCREEN_W, 16, CLAUDE_CLAY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLAUDE_CREAM, CLAUDE_CLAY);
  tft.setTextPadding(0);
  tft.drawString(cfg.label.c_str(), SCREEN_W / 2, 8, 2);
}

void drawClockAndDate() {
  struct tm t;
  if (!getLocalTime(&t, 200)) return;

  if (cfg.showClock) {
    char hm[6], ss[3];
    strftime(hm, sizeof(hm), cfg.use12h ? "%I:%M" : "%H:%M", &t);
    strftime(ss, sizeof(ss), "%S", &t);
    // ~32px clock via the scaled GLCD font — still the biggest element, but
    // smaller than the 48px 7-seg (font 7), which only exists at that one size.
    const int gap = 6, topY = 18;
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
    tft.setTextSize(4);                       // HH:MM -> 32px tall
    int wHM = tft.textWidth(hm, 1);
    String ssStr = String(":") + ss;
    tft.setTextSize(2);                       // seconds -> 16px tall
    int wSS = tft.textWidth(ssStr.c_str(), 1);
    int x0 = (SCREEN_W - (wHM + gap + wSS)) / 2;
    tft.setTextSize(4);
    tft.setTextColor(CLAUDE_CREAM, TFT_BLACK);
    tft.drawString(hm, x0, topY, 1);
    tft.setTextSize(2);
    tft.setTextColor(CLAUDE_CLAY, TFT_BLACK);
    tft.drawString(ssStr, x0 + wHM + gap, topY + 32 - 16, 1);  // bottom-aligned
    tft.setTextSize(1);                       // IMPORTANT: reset scale for other rows
  }

  if (cfg.showDate) {
    char dateStr[28];
    strftime(dateStr, sizeof(dateStr), "%a %d %b %Y", &t);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CLAUDE_GREY, TFT_BLACK);
    tft.setTextPadding(SCREEN_W);
    tft.drawString(dateStr, SCREEN_W / 2, 62, 2);   // date (16px)
  }
  tft.setTextPadding(0);
}

void drawWeather() {
  const int y = 82, fNum = 2, fCond = 2;
  tft.setTextSize(1);
  tft.fillRect(0, y - 10, SCREEN_W, 20, TFT_BLACK);
  if (!cfg.showWeather) return;
  if (!g_haveWeather) {
    tft.setTextDatum(MC_DATUM); tft.setTextPadding(0);
    tft.setTextColor(CLAUDE_GREY, TFT_BLACK);
    tft.drawString("weather: --", SCREEN_W / 2, y, fNum); return;
  }
  char num[8]; snprintf(num, sizeof(num), "%.1f", g_tempC);
  String rest = String("C  ") + g_condition;
  const int degR = 2, degGap = 2;
  int wNum = tft.textWidth(num, fNum), wRest = tft.textWidth(rest.c_str(), fCond);
  int total = wNum + degGap + degR * 2 + 2 + wRest;
  int x = (SCREEN_W - total) / 2;
  tft.setTextDatum(ML_DATUM); tft.setTextPadding(0);
  tft.setTextColor(CLAUDE_CLAY, TFT_BLACK);
  tft.drawString(num, x, y, fNum); x += wNum + degGap;
  tft.drawCircle(x + degR, y - tft.fontHeight(fNum) / 2 + degR + 1, degR, CLAUDE_CLAY);
  x += degR * 2 + 2;
  tft.setTextColor(CLAUDE_CREAM, TFT_BLACK);
  tft.drawString(rest, x, y, fCond);
}

// A compact horizontal gauge: outlined track with a proportional fill that
// shifts green -> orange -> red as it fills (so a near-limit bar reads at a glance).
void drawBar(int x, int y, int w, int h, float pct) {
  if (pct < 0) pct = 0; else if (pct > 100) pct = 100;
  // Warm Claude ramp: clay normally, amber as it fills, terracotta near the cap.
  uint16_t col = pct >= 90 ? CLAUDE_RED : (pct >= 70 ? CLAUDE_AMBER : CLAUDE_CLAY);
  tft.drawRect(x, y, w, h, CLAUDE_GREY);
  int inner = w - 2;
  int fillW = (int)(inner * pct / 100.0f + 0.5f);
  if (fillW > 0) tft.fillRect(x + 1, y + 1, fillW, h - 2, col);
  if (fillW < inner) tft.fillRect(x + 1 + fillW, y + 1, inner - fillW, h - 2, TFT_BLACK);
}

void drawUsage() {
  const int y = 114;
  tft.setTextSize(1);
  tft.fillRect(0, y - 11, SCREEN_W, 24, TFT_BLACK);  // clear the whole usage band
  if (!cfg.showUsage) return;
  if (!g_haveUsage) {
    tft.setTextDatum(MC_DATUM); tft.setTextPadding(SCREEN_W);
    tft.setTextColor(CLAUDE_GREY, TFT_BLACK);
    tft.drawString("Claude: --", SCREEN_W / 2, y, 2);
    tft.setTextPadding(0);
    return;
  }
  const int barH = 16, barY = y - barH / 2, f = 2;
  char pc[6];
  tft.setTextDatum(ML_DATUM); tft.setTextPadding(0);
  // Session (5h) on the left, weekly (7d) on the right.
  tft.setTextColor(CLAUDE_CLAY, TFT_BLACK); tft.drawString("S", 2, y, f);
  drawBar(16, barY, 50, barH, g_sessPct);
  snprintf(pc, sizeof(pc), "%.0f%%", g_sessPct);
  tft.setTextColor(CLAUDE_CREAM, TFT_BLACK); tft.drawString(pc, 70, y, f);

  tft.setTextColor(CLAUDE_CLAY, TFT_BLACK); tft.drawString("W", 122, y, f);
  drawBar(136, barY, 50, barH, g_weekPct);
  snprintf(pc, sizeof(pc), "%.0f%%", g_weekPct);
  tft.setTextColor(CLAUDE_CREAM, TFT_BLACK); tft.drawString(pc, 190, y, f);
}

void redrawAll() {
  drawStaticBackground();
  drawClockAndDate();
  drawWeather();
  drawUsage();
}

// ---------------- web server ----------------
String htmlPage() {
  String chkClk = cfg.showClock ? "checked" : "";
  String chkDat = cfg.showDate ? "checked" : "";
  String chkWx  = cfg.showWeather ? "checked" : "";
  String chk12  = cfg.use12h ? "checked" : "";
  String chkUsg = cfg.showUsage ? "checked" : "";

  String p = F(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 Dashboard</title><style>"
    "body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:18px}"
    ".card{max-width:420px;margin:auto;background:#1c1c1c;padding:20px;border-radius:12px}"
    "h1{font-size:20px;margin:0 0 14px}label{display:block;margin:12px 0 4px}"
    "input[type=text],input[type=number]{width:100%;padding:8px;border-radius:8px;border:1px solid #333;background:#222;color:#eee;box-sizing:border-box}"
    ".row{display:flex;align-items:center;gap:8px;margin:10px 0}.row input{width:auto}"
    "button{margin-top:16px;width:100%;padding:11px;border:0;border-radius:8px;background:#2563eb;color:#fff;font-size:16px}"
    ".muted{color:#888;font-size:12px;margin-top:14px}</style></head><body><div class='card'>"
    "<h1>ESP32 Dashboard</h1><form method='POST' action='/save'>");
  p += "<label>Header label</label><input type='text' name='label' value='" + cfg.label + "'>";
  p += "<label>Latitude</label><input type='number' step='0.0001' name='lat' value='" + String(cfg.lat, 4) + "'>";
  p += "<label>Longitude</label><input type='number' step='0.0001' name='lon' value='" + String(cfg.lon, 4) + "'>";
  p += "<label>Claude usage helper URL</label><input type='text' name='uurl' value='" + cfg.usageUrl + "'>";
  p += "<div class='row'><input type='checkbox' name='clk' " + chkClk + "><span>Show clock</span></div>";
  p += "<div class='row'><input type='checkbox' name='date' " + chkDat + "><span>Show date</span></div>";
  p += "<div class='row'><input type='checkbox' name='wx' " + chkWx + "><span>Show weather</span></div>";
  p += "<div class='row'><input type='checkbox' name='usg' " + chkUsg + "><span>Show Claude usage</span></div>";
  p += "<div class='row'><input type='checkbox' name='h12' " + chk12 + "><span>12-hour time</span></div>";
  p += F("<button type='submit'>Save</button></form>");
  p += "<div class='muted'>Device: " + WiFi.localIP().toString() + " &middot; http://" + String(MDNS_NAME) + ".local</div>";
  p += F("</div></body></html>");
  return p;
}

void handleRoot() { server.send(200, "text/html", htmlPage()); }

void handleSave() {
  if (server.hasArg("label")) cfg.label = server.arg("label");
  if (server.hasArg("lat"))   cfg.lat = server.arg("lat").toFloat();
  if (server.hasArg("lon"))   cfg.lon = server.arg("lon").toFloat();
  if (server.hasArg("uurl"))  cfg.usageUrl = server.arg("uurl");
  cfg.showClock   = server.hasArg("clk");
  cfg.showDate    = server.hasArg("date");
  cfg.showWeather = server.hasArg("wx");
  cfg.showUsage   = server.hasArg("usg");
  cfg.use12h      = server.hasArg("h12");
  saveSettings();
  Serial.println("Settings saved via web.");
  g_needFullRedraw = true;
  g_needWeatherNow = true;
  g_needUsageNow = true;
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Saved");
}

void startWebServer() {
  if (MDNS.begin(MDNS_NAME)) { MDNS.addService("http", "tcp", 80); Serial.printf("mDNS: http://%s.local\n", MDNS_NAME); }
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.printf("Web server up: http://%s/\n", WiFi.localIP().toString().c_str());
}

// ---------------- main ----------------
void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
  tft.init(); tft.setRotation(1);

  loadSettings();
  centerMsg("Starting...", CLAUDE_CREAM);

  if (!connectWiFi()) { centerMsg("WiFi FAILED", CLAUDE_RED, "check secrets.h / 2.4GHz", CLAUDE_GREY); return; }
  Serial.print("WiFi OK, IP "); Serial.println(WiFi.localIP());

  centerMsg("Syncing time...", CLAUDE_CLAY);
  if (!waitForTime()) { centerMsg("Time sync FAILED", CLAUDE_RED, "no NTP reply", CLAUDE_GREY); return; }

  startWebServer();
  redrawAll();
  fetchWeather(); drawWeather();
  fetchUsage();   drawUsage();
}

void loop() {
  server.handleClient();

  static int lastSec = -1;
  static uint32_t lastWeather = 0, lastUsage = 0, lastWifiTry = 0;

  if (g_needFullRedraw) { g_needFullRedraw = false; redrawAll(); }
  if (g_needWeatherNow) { g_needWeatherNow = false; if (fetchWeather()) drawWeather(); }
  if (g_needUsageNow)   { g_needUsageNow = false;   if (fetchUsage())   drawUsage(); }

  struct tm t;
  if (getLocalTime(&t, 50) && t.tm_sec != lastSec) { lastSec = t.tm_sec; drawClockAndDate(); }

  if (millis() - lastWeather > WEATHER_INTERVAL_MS) { lastWeather = millis(); if (fetchWeather()) drawWeather(); }
  if (millis() - lastUsage   > USAGE_INTERVAL_MS)   { lastUsage = millis();   if (fetchUsage())   drawUsage(); }

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTry > 30000) {
    lastWifiTry = millis();
    Serial.println("WiFi dropped, reconnecting...");
    connectWiFi(10000);
  }
  delay(20);
}
