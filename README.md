# PortBlaster HTTP Server

Tiny Win32 GUI wrapper around the barebones C HTTP server.

## Features

- Start / stop button.
- Editable port.
- Editable root directory.
- Auto-filled root directory relative to the exe.
- Exact running URL in the status line.
- Request counter.
- Last request path and status.
- Scrolling request log.
- Uptime and bytes-served status.
- Common static asset MIME types.
- Static file serving from the selected root.
- Basic `GET` and `HEAD`.
- Fixed buffers and direct Win32/Winsock calls.

## Build

```powershell
.\build.ps1
```

The build script creates:

- `build\pb20.exe` - 20 KB target release build.
- `build\pb50.exe` - 50 KB target release build.
- `build\pb100.exe` - 100 KB target release build.
- `build\pbjelly\pbj20.exe` - jelly validation build for `pb20.exe`.
- `build\pbjelly\pbj50.exe` - jelly validation build for `pb50.exe`.
- `build\pbjelly\pbj100.exe` - jelly validation build for `pb100.exe`.
- `build\pbjelly\pbjelly.exe` - local safety and performance jelly utility.

The three release builds come from the same `portblaster.c` source with compile-time feature flags:

- `pb20.exe` - safety-first minimum build.
- `pb50.exe` - adds request log, uptime, bytes served, and title state.
- `pb100.exe` - adds chunked streaming for larger files.
- `pb100.exe` also supports `portblaster.ini` beside the exe with `port=` and `root=` lines.

Trial builds can mix features for byte-cost experiments:

```powershell
.\build.ps1 -TrialName pblab -TrialFeatures LOG,STREAM
.\build.ps1 -TrialName pblab -TrialFeatures LOG,METRICS,STREAM -TrialJelly
```

## Run

```powershell
.\build\pb20.exe
```

Default URL:

```text
http://127.0.0.1:8083/
```

## Probe

Start PortBlaster, then run:

```powershell
.\build\pbjelly\pbjelly.exe 127.0.0.1 8083
```

`pbjelly` sends normal requests, method/path attacks, malformed-path checks, and a small load run with latency totals.
Add a third argument to write an HTML report that PortBlaster can serve:

```powershell
.\build\pbjelly\pbjelly.exe 127.0.0.1 8083 build\public\jelly-report.html
```

Pass extra arguments to validate large-file, MIME, timeout, status endpoint, and access-log behavior:

```powershell
.\build\pbjelly\pbjelly.exe 127.0.0.1 8083 build\pbjelly\public\jelly-report.html 200 1 1 200 build\pbjelly\portblaster.log 1
```

For automated local validation, the check build has a test hook:

```powershell
.\build\pbjelly\pbj20.exe --start
```

That starts the GUI and automatically presses Start. The release builds do not use this hook.
When testing a jelly build from `build\pbjelly`, write reports under `build\pbjelly\public` so that validation server can serve them.

## TLS

PortBlaster's standard builds are HTTP-only to preserve size and reduce security complexity.
If TLS is needed, put a reverse proxy in front of PortBlaster and keep PortBlaster bound to loopback:

```text
browser -> https reverse proxy -> http://127.0.0.1:8083/
```

Do not expose PortBlaster directly on a public interface unless a future build explicitly adds and validates that mode.
