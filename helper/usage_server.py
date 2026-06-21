#!/usr/bin/env python3
"""
Claude Code usage helper for the ESP32 dashboard.

Parses ~/.claude/projects/**/*.jsonl (your local Claude Code logs), tallies token
usage + estimated cost, and serves a tiny JSON summary on the LAN for the ESP32
to fetch over plain HTTP. No API key, no external calls — fully local.

Run:
    python3 helper/usage_server.py            # serves on 0.0.0.0:8088
    PORT=9000 python3 helper/usage_server.py  # custom port

Endpoint:
    GET /usage  -> {"today": {...}, "week": {...}, "total": {...}, ...}

Cost is an ESTIMATE: tokens x public API pricing (per million). Since Claude Code
runs on a subscription, this is the equivalent API spend, not a real bill.
"""
import glob
import json
import os
import time
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from zoneinfo import ZoneInfo

LOG_GLOB = os.path.expanduser("~/.claude/projects/**/*.jsonl")
LIMITS_FILE = os.path.expanduser("~/.claude/esp32-usage.json")  # written by statusline.sh
TZ = ZoneInfo("Europe/Berlin")
PORT = int(os.environ.get("PORT", "8088"))
CACHE_TTL = 30  # seconds; recompute at most this often

# Public API pricing per 1M tokens: (input, output). Cache write = 1.25x input
# (5-min ephemeral), cache read = 0.1x input. Update if Anthropic pricing changes.
PRICING = {
    "claude-opus-4-8": (5.0, 25.0),
    "claude-opus-4-7": (5.0, 25.0),
    "claude-opus-4-6": (5.0, 25.0),
    "claude-fable-5": (10.0, 50.0),
    "claude-sonnet-4-6": (3.0, 15.0),
    "claude-haiku-4-5": (1.0, 5.0),
}
DEFAULT_PRICE = (5.0, 25.0)  # fall back to Opus-tier for unknown models
CACHE_WRITE_MULT = 1.25
CACHE_READ_MULT = 0.10

_cache = {"at": 0.0, "data": None}


def _bucket():
    return {"input": 0, "output": 0, "cache_write": 0, "cache_read": 0,
            "tokens": 0, "cost_usd": 0.0}


def _add(bucket, u, model):
    inp = int(u.get("input_tokens", 0) or 0)
    out = int(u.get("output_tokens", 0) or 0)
    cw = int(u.get("cache_creation_input_tokens", 0) or 0)
    cr = int(u.get("cache_read_input_tokens", 0) or 0)
    in_price, out_price = PRICING.get(model, DEFAULT_PRICE)
    cost = (inp * in_price
            + cw * in_price * CACHE_WRITE_MULT
            + cr * in_price * CACHE_READ_MULT
            + out * out_price) / 1_000_000.0
    bucket["input"] += inp
    bucket["output"] += out
    bucket["cache_write"] += cw
    bucket["cache_read"] += cr
    bucket["tokens"] += inp + out + cw + cr
    bucket["cost_usd"] += cost


def _round(bucket):
    bucket["cost_usd"] = round(bucket["cost_usd"], 4)
    return bucket


def compute():
    now = datetime.now(TZ)
    today = now.date()
    week_ago = today - timedelta(days=6)  # last 7 calendar days incl. today

    total, this_week, this_day = _bucket(), _bucket(), _bucket()
    seen = set()  # dedupe by (requestId, message.id)

    for path in glob.glob(LOG_GLOB, recursive=True):
        try:
            with open(path, encoding="utf-8") as fh:
                for line in fh:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        o = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if o.get("type") != "assistant":
                        continue
                    msg = o.get("message") or {}
                    u = msg.get("usage")
                    if not isinstance(u, dict):
                        continue
                    model = msg.get("model", "")
                    if model == "<synthetic>":
                        continue
                    key = (o.get("requestId"), msg.get("id"))
                    if key != (None, None):
                        if key in seen:
                            continue
                        seen.add(key)

                    _add(total, u, model)

                    ts = o.get("timestamp")
                    if not ts:
                        continue
                    try:
                        dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
                    except ValueError:
                        continue
                    d = dt.astimezone(TZ).date()
                    if d >= week_ago:
                        _add(this_week, u, model)
                    if d == today:
                        _add(this_day, u, model)
        except OSError:
            continue

    return {
        "today_date": today.isoformat(),
        "updated": now.isoformat(timespec="seconds"),
        "today": _round(this_day),
        "week": _round(this_week),
        "total": _round(total),
    }


def get_cached():
    now = time.time()
    if _cache["data"] is None or now - _cache["at"] > CACHE_TTL:
        _cache["data"] = compute()
        _cache["at"] = now
    return _cache["data"]


def read_limits():
    """Subscription rate-limit usage captured by statusline.sh (session % over the
    5-hour window, weekly % over 7 days, and reset epochs). None until Claude Code
    has run with the status line configured. Read fresh on every request."""
    try:
        with open(LIMITS_FILE, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, json.JSONDecodeError):
        return None


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.split("?")[0] not in ("/usage", "/"):
            self.send_error(404, "use /usage")
            return
        try:
            data = dict(get_cached())
            data["limits"] = read_limits()  # fresh each request
            payload = json.dumps(data).encode()
        except Exception as e:  # never crash the server on a bad read
            self.send_error(500, str(e))
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *args):
        pass  # quiet


if __name__ == "__main__":
    print(f"Claude usage helper on http://0.0.0.0:{PORT}/usage  (TZ {TZ})")
    # Warm the cache once at startup so the first ESP32 request is instant.
    get_cached()
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
