#!/usr/bin/env bash
# ESP32 dashboard status line.
#
# Claude Code pipes session JSON to this script on stdin (see
# https://code.claude.com/docs/en/statusline). We do two things:
#   1. Capture the subscription rate-limit usage to ~/.claude/esp32-usage.json
#      (the same numbers as claude.ai Settings -> Usage: 5-hour session %,
#      7-day weekly %, and their reset timestamps).
#   2. Print a normal status line so the bar stays useful.
#
# The dashboard's usage_server.py reads that file and serves it to the ESP32.

input=$(cat)
OUT="$HOME/.claude/esp32-usage.json"

# 1. Capture usage (atomic write). `now` is current epoch seconds (jq builtin).
printf '%s' "$input" | jq -c '{
  session_pct:       (.rate_limits.five_hour.used_percentage // null),
  session_resets_at: (.rate_limits.five_hour.resets_at // null),
  week_pct:          (.rate_limits.seven_day.used_percentage // null),
  week_resets_at:    (.rate_limits.seven_day.resets_at // null),
  cost_usd:          (.cost.total_cost_usd // null),
  updated:           now
}' > "$OUT.tmp" 2>/dev/null && mv "$OUT.tmp" "$OUT" 2>/dev/null

# 2. Render the visible status line.
printf '%s' "$input" | jq -r '
  "[" + (.model.display_name // "claude") + "] " +
  "ctx " + ((.context_window.used_percentage // 0) | floor | tostring) + "%  " +
  "sess " + ((.rate_limits.five_hour.used_percentage // 0) | floor | tostring) + "%  " +
  "wk " + ((.rate_limits.seven_day.used_percentage // 0) | floor | tostring) + "%"
'
