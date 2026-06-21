# Claude usage helper

A tiny local server that surfaces your **Claude usage** to the ESP32 over plain HTTP
on your LAN — no API key, no external calls. It provides two things:

1. **Subscription rate-limit usage** (what the dashboard shows): the 5-hour **session %**
   and 7-day **weekly %** — the same numbers as claude.ai Settings → Usage. These come
   from Claude Code's status-line data via `statusline.sh` (see setup below).
2. **Token tally + estimated cost** (bonus, in the JSON): parsed from your Claude Code
   logs (`~/.claude/projects/**/*.jsonl`).

## Live subscription limits — set up the status line (one-time)

The session/weekly percentages are only exposed to Claude Code's **status line**. The
included `statusline.sh` captures them to `~/.claude/esp32-usage.json` every time Claude
Code updates; the helper then serves them. Enable it by adding this to
**`~/.claude/settings.json`** (merge — keep your other keys):

```json
"statusLine": {
  "type": "command",
  "command": "/absolute/path/to/esp32-display/helper/statusline.sh"
}
```

Use the **absolute path** to your clone (e.g. `$HOME/projects/esp32-display/...`).
Then reload Claude Code (open `/statusline` or restart). You'll see a status bar like
`[Opus 4.8] ctx 12%  sess 33%  wk 9%`, and `~/.claude/esp32-usage.json` will track your
real usage. **Caveat:** it only refreshes while you're actively using Claude Code (when
idle, the % is the last-seen value). Only `session` (5h) and `weekly all-models` (7d) are
exposed — there's no separate "Sonnet only" field.

## Run it

```bash
python3 helper/usage_server.py            # serves on 0.0.0.0:8088
PORT=9000 python3 helper/usage_server.py  # custom port
```

Test it:
```bash
curl http://localhost:8088/usage
```

The ESP32 must point at **this machine's LAN IP**, e.g. `http://192.168.0.163:8088/usage`
(set on the dashboard's web config page). Find your IP with `hostname -I`.

## Running as a service (recommended)

The dashboard only shows usage while this helper is up, so the best setup is a **systemd
user service** that starts on boot and restarts if it crashes. The unit lives at
`~/.config/systemd/user/claude-usage.service` and runs:

```
/usr/bin/python3 /absolute/path/to/esp32-display/helper/usage_server.py
```

Use **system `/usr/bin/python3`** (stdlib only) so it doesn't depend on any per-user
Python/venv setup. Enable linger (`loginctl enable-linger`) so it runs even when logged
out / across reboots. Footprint: ~15 MB RAM, ~0% CPU idle.

### Manage it

```bash
systemctl --user status claude-usage.service     # is it up?
systemctl --user restart claude-usage.service    # after editing usage_server.py
systemctl --user stop claude-usage.service        # stop temporarily
journalctl --user -u claude-usage.service -f      # live logs
```

### Install it

Edit the `ExecStart` path below to point at **your** clone, then run:

```bash
mkdir -p ~/.config/systemd/user
cat > ~/.config/systemd/user/claude-usage.service <<EOF
[Unit]
Description=Claude usage helper for ESP32 dashboard
After=network-online.target
Wants=network-online.target

[Service]
ExecStart=/usr/bin/python3 $(pwd)/helper/usage_server.py
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now claude-usage.service
loginctl enable-linger "$USER"   # so it runs even when logged out
```

> The `$(pwd)/helper/...` expands to your current directory — run it from the repo root,
> or replace it with the absolute path to `helper/usage_server.py`.

## Output shape

```json
{
  "today_date": "2026-06-21",
  "updated": "2026-06-21T10:36:47+02:00",
  "today": { "input": ..., "output": ..., "cache_write": ..., "cache_read": ...,
             "tokens": 57914860, "cost_usd": 49.95 },
  "week":  { ... },
  "total": { ... }
}
```

## Notes

- **Cost is an estimate**: tokens × public API pricing (per million) — Opus 4.8 $5/$25,
  Sonnet 4.6 $3/$15, Haiku 4.5 $1/$5; cache writes 1.25× input, cache reads 0.1× input.
  Claude Code runs on a subscription, so this is the *equivalent* API spend, not a bill.
  Update the `PRICING` table in `usage_server.py` if Anthropic pricing changes.
- Results are cached for 30s; the ESP32 polls every 2 min.
- "today" is in Europe/Berlin time.
