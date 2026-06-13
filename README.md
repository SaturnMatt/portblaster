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
- `build\pbcheck\pb20.exe` - validation build for `pb20.exe`.
- `build\pbcheck\pb50.exe` - validation build for `pb50.exe`.
- `build\pbcheck\pb100.exe` - validation build for `pb100.exe`.
- `build\pbcheck\pbprobe.exe` - local safety and performance probe utility.

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
.\build\pbcheck\pbprobe.exe 127.0.0.1 8083
```

`pbprobe` sends normal requests, method/path attacks, malformed-path checks, and a small load run with latency totals.
Add a third argument to write an HTML report that PortBlaster can serve:

```powershell
.\build\pbcheck\pbprobe.exe 127.0.0.1 8083 build\public\probe-report.html
```

For automated local validation, the check build has a test hook:

```powershell
.\build\pbcheck\pb20.exe --start
```

That starts the GUI and automatically presses Start. The release builds do not use this hook.
When testing a validation build from `build\pbcheck`, write reports under `build\pbcheck\public` so that validation server can serve them.
