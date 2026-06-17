# Codex Limit Monitor Plugin

TrafficMonitor plugin that shows OpenAI Codex usage limits in the taskbar.

## What It Shows

- `Codex 5h`: remaining 5-hour Codex limit percentage and remaining hours.
- `Codex W`: remaining weekly Codex limit percentage and remaining hours.
- Tooltip: reset timestamp and full countdown for both limits.

## Requirements

- TrafficMonitor for Windows.
- OpenAI Codex CLI installed and available in `PATH`.
- Codex CLI logged in with ChatGPT auth.
- The plugin DLL must match the TrafficMonitor architecture, for example x64 TrafficMonitor needs the x64 plugin DLL.

Check Codex from a terminal:

```powershell
codex doctor
```

The plugin uses Codex's local app-server protocol:

```text
codex app-server --stdio
account/rateLimits/read
```

No access token is read or stored by the plugin. Codex CLI handles authentication itself.

## Installation

1. Copy `CodexLimitPlugin.dll` into TrafficMonitor's `plugins` folder.
2. Restart TrafficMonitor.
3. Open the taskbar window display settings.
4. Enable both plugin items:
   - `Codex 5-hour limit`
   - `Codex weekly limit`

## How Updates Work

- TrafficMonitor calls the plugin regularly.
- The plugin starts a background worker at most once every 5 minutes.
- The worker asks Codex for `account/rateLimits/read`.
- TrafficMonitor is not blocked while Codex is queried.
- If Codex is slow or unavailable, the last known values remain visible.

## Fallback Data

If automatic Codex querying fails, the plugin can still read fallback values.

Supported fallback file:

```text
%USERPROFILE%\.codex\codex_status.txt
```

Example content:

```text
5h limit: 99% left (resets 19:09)
Weekly limit: 94% left (resets 09:15 on 23 Jun 2026)
```

The plugin options dialog can also store manual values in its INI file.

## Troubleshooting

- If the taskbar shows `--% --h`, run `codex doctor` and make sure Codex is logged in.
- If values do not update immediately, wait up to 5 minutes or restart TrafficMonitor.
- If TrafficMonitor is x64, use the x64 DLL. If it is Win32, build and use the Win32 DLL.
- If colors look wrong, configure plugin item colors in TrafficMonitor's taskbar color settings.
