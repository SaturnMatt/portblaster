# PortBlaster HTTP Server

Tiny Win32 GUI wrapper around the barebones C HTTP server.

## Features

- Start / stop button.
- Copy URL button in `pb50` and `pb100`.
- Browse root button in `pb50` and `pb100`.
- Tray minimize/menu behavior in `pb100`.
- Built-in Windows icon, menu bar, grouped panels, and classic status styling in `pb100`.
- Safety limits panel in `pb100` with timeout, worker, queue, active, rejected, and timeout counters.
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
- `pb50.exe` - adds request log, uptime, bytes served, title state, Copy URL, and Browse root.
- `pb100.exe` - adds chunked streaming, bounded worker-pool serving, configurable safety limits, defense counters, and tray behavior.
- `pb100.exe` also supports `portblaster.ini` beside the exe with `port=`, `root=`, `timeout_ms=`, `workers=`, `queue=`, `dir_list=1`, explicit `bind=all`, and explicit `ipv6=1` lines.

Trial builds can mix features for byte-cost experiments:

```powershell
.\build.ps1 -TrialName pblab -TrialFeatures LOG,STREAM
.\build.ps1 -TrialName pblab -TrialFeatures LOG,METRICS,STREAM -TrialJelly
```

TLS is intentionally not in the trial feature list yet. A native SChannel build needs a correct certificate workflow and its own validation before it belongs in the tiny-build matrix.

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

Launching `pbjelly.exe` without arguments opens the Win32 attack tester GUI. It can start `pbj20`, `pbj50`, or `pbj100`, run the current suite, stream results, and open `jelly-report.html`.

`pbjelly` sends normal requests, method/path attacks, malformed-path checks, slow-client probes, queue-saturation probes, and a small load run with average, p50, p95, and max latency. Its CLI and HTML report label HTTP probes as served, blocked, or unexpected and include defense snapshots from `pb100`.
Add a third argument to write an HTML report that PortBlaster can serve:

```powershell
.\build\pbjelly\pbjelly.exe 127.0.0.1 8083 build\public\jelly-report.html
```

Pass extra arguments to validate large-file, MIME, timeout, status endpoint, access-log, directory-listing, and range behavior:

```powershell
.\build\pbjelly\pbjelly.exe 127.0.0.1 8083 build\pbjelly\public\jelly-report.html 200 1 1 200 build\pbjelly\portblaster.log 1 200 206
```

Add a final `1` after the bind-mode argument to validate that concurrent `pb100` serving keeps a normal request moving while another accepted client stays idle. Use `2` for the standard worker-pool build to also validate queue saturation returns `503 Service Unavailable`.

Add a final `0` or `1` argument to validate the listener bind mode through the Windows TCP table: `0` expects loopback-only, `1` expects all IPv4 interfaces. `bind=all` is `pb100`-only and must be set explicitly in `portblaster.ini`.

For IPv6 validation, set `ipv6=1` in the `pb100` config and run the same jelly command against `::1`.

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

## Intentional omissions

Compression and keep-alive are intentionally not part of the standard builds. PortBlaster prioritizes safety and tiny size, so responses stay uncompressed and close after each request.
