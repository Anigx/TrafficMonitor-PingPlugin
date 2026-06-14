# TrafficMonitor Ping Monitor Plugin

Ping Monitor is a standalone plugin for [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor). It adds two separately configurable display items for monitoring a server from the desktop window and the taskbar window.

## Features

- `PING:` shows the current ping latency in milliseconds.
- `LOSS:` shows recent packet loss as a percentage.
- The latency and packet loss items can be enabled or disabled separately in TrafficMonitor's display settings.
- Hover information shows the target server, last result, recent loss window, total sent packets, total lost packets, and timeout.
- A plugin options dialog lets you set the server IP or host name, timeout, and packet-loss sample window.

## Screenshots

Ping latency and packet loss display:

![Ping latency and packet loss display](docs/images/ping-monitor-display.svg)

Plugin settings dialog:

![Ping Monitor settings dialog](docs/images/ping-monitor-settings.svg)

## Installation

1. Build `PingMonitorPlugin\PingMonitorPlugin.vcxproj` with Visual Studio 2022 or MSBuild.
2. Copy `PingMonitorPlugin.dll` to the `plugins` folder next to `TrafficMonitor.exe`.
3. Restart TrafficMonitor.
4. Open `More Functions` -> `Plugin Management` to confirm that `Ping Monitor` is loaded.
5. Open the taskbar or main-window display settings and enable `Ping latency` and/or `Packet loss`.

## Settings

Open the plugin options from TrafficMonitor's plugin management dialog. The following values can be changed:

- `Server IP / host`: the target to ping, for example `1.1.1.1`, `8.8.8.8`, or a host name.
- `Timeout (ms)`: ping timeout in milliseconds. Values are clamped to `200` through `5000`.
- `Loss window`: number of recent samples used for packet loss. Values are clamped to `5` through `100`.

The same settings are stored in the plugin INI file:

```ini
[config]
ping_target=1.1.1.1
ping_timeout=1000
ping_window_size=20
```

## Build Output

For an x64 Debug build, the DLL is written to:

```text
Bin\x64\Debug\plugins\PingMonitorPlugin.dll
```

## Notes

- The plugin uses Windows ICMP APIs, so the target network must allow ICMP echo requests.
- A timeout or failed ping is counted as packet loss.
- The plugin is independent from `PluginDemo`; the demo project is not required to use it.
