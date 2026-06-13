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

## Run

```powershell
.\build\portblaster.exe
```

Default URL:

```text
http://127.0.0.1:8083/
```
