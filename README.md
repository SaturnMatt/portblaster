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

- `build\portblaster.exe` - smallest release build.
- `build\portblaster-check.exe` - validation/debug-friendly build.
- `build\portblaster-check.pdb` - debug symbols for the validation build.
- `build\pbprobe.exe` - local safety and performance probe utility.

## Run

```powershell
.\build\portblaster.exe
```

Default URL:

```text
http://127.0.0.1:8083/
```

## Probe

Start PortBlaster, then run:

```powershell
.\build\pbprobe.exe 127.0.0.1 8083
```

`pbprobe` sends normal requests, method/path attacks, malformed-path checks, and a small load run with latency totals.
Add a third argument to write an HTML report that PortBlaster can serve:

```powershell
.\build\pbprobe.exe 127.0.0.1 8083 build\public\probe-report.html
```

For automated local validation, the check build has a test hook:

```powershell
.\build\portblaster-check.exe --start
```

That starts the GUI and automatically presses Start. The size-optimized `portblaster.exe` does not use this hook.
