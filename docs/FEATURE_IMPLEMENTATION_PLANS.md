# PortBlaster Feature Implementation Plans

This file breaks down the major candidate features for PortBlaster into goals, build fit, safety notes, and likely implementation steps.

Priority order remains:

1. Safety
2. Size
3. Function
4. Polish

The server should stay one source file, `portblaster.c`, with compile-time feature flags. `pbjelly.c` stays separate as the tester/reporting tool.

## 1. Browse Root Button

Status: Complete

Build fit: `pb50`, `pb100`

Goal:
- Let users choose the document root without manually typing a path.
- Keep `pb20` text-only to protect its size identity.

Implementation goals:
- Add a `Browse` button beside the root edit field.
- Use the smallest practical Win32 folder picker.
- Write the selected path into the existing root edit control.
- Preserve existing root validation through `set_root_full`.
- Feature gate as `PB_FEAT_BROWSE`.

Safety notes:
- Do not auto-start after path selection.
- Do not allow file selection; root must be a directory.
- Keep path validation at server start unchanged.

Acceptance:
- Selecting a valid folder lets the server start.
- Cancelling the dialog leaves the root unchanged.
- `pb20` size is unaffected.
- The selected value is only written to the existing root edit field and remains validated by the normal start path.

## 2. Copy URL Button

Status: Complete

Build fit: `pb50`, `pb100`

Goal:
- Let users copy the active local URL quickly after start.

Implementation goals:
- Add a small `Copy URL` button near Start/Stop or status.
- Enable it only when a valid URL is known.
- Use Win32 clipboard APIs directly.
- Copy `http://127.0.0.1:<port>/`.
- Feature gate as `PB_FEAT_COPY_URL`.

Safety notes:
- Do not expose non-loopback URLs unless a future bind option exists.
- Do not copy stale ports after invalid start attempts.

Acceptance:
- Starting on port `8083` copies `http://127.0.0.1:8083/`.
- Failed starts do not update the copied URL.
- Jelly validation builds support `--copy-url-test` to prove copy happens only after the running transition.

## 3. Better Status Codes

Status: Complete

Build fit: all builds

Goal:
- Improve correctness without meaningful size cost.

Implementation goals:
- Keep `400 Bad Request` for malformed request lines.
- Keep `403 Forbidden` for rejected paths.
- Keep `404 Not Found` for missing files.
- Keep `405 Method Not Allowed` for unsupported methods.
- Use `413 Payload Too Large` for the non-streaming file cap.
- Use `500 Internal Server Error` for actual read/send/server errors.

Safety notes:
- Never weaken path rejection to make status codes prettier.
- Do not leak filesystem details in response bodies.

Acceptance:
- `pbjelly` explicitly checks `400`, `403`, `404`, `405`, and load success.
- `pbjelly` can generate a large file and validate the expected large-file status for each target build.

## 4. Larger MIME Table

Status: Complete

Build fit: `pb50`, `pb100`

Goal:
- Serve common static web assets with useful content types.

Implementation goals:
- Keep the current tiny suffix check style.
- Add only high-value extensions first: `.json`, `.webp`, `.wasm`, `.mjs`, `.map`, `.pdf`.
- Feature gate expanded MIME types as `PB_FEAT_MIME_PLUS`.
- Keep the minimal core MIME set always available.

Safety notes:
- MIME type is not authorization.
- Default unknown files to `application/octet-stream`.

Acceptance:
- `pbjelly` confirms representative content types for `.json`, `.webp`, `.wasm`, and `.pdf`.
- Size delta is recorded for each added extension group.

## 5. Per-Request Timeout

Status: Complete

Build fit: `pb100`, possibly `pb50`

Goal:
- Reduce exposure to slow or stalled clients.

Implementation goals:
- Set `SO_RCVTIMEO` and `SO_SNDTIMEO` on accepted client sockets.
- Start with a conservative fixed timeout, such as `5000 ms`.
- Feature gate as `PB_FEAT_TIMEOUT`.
- Consider exposing timeout in `pb100` only after the fixed version is proven.

Safety notes:
- Avoid indefinite blocking in `recv` and `send`.
- Do not introduce complex retry loops.
- Keep close-after-response behavior.

Acceptance:
- Normal `pbjelly` requests still pass.
- `pbjelly` validates that an idle accepted connection is closed by `pb100` within the timeout window.

## 6. Thread-Per-Connection

Status: Complete

Build fit: `pb100`

Goal:
- Prevent one slow or large request from blocking all other requests.

Implementation goals:
- Move per-request buffers out of global shared state or protect shared state carefully.
- Create a small client context containing socket and request/file buffers.
- Spawn one worker thread per accepted client.
- Close thread handles immediately after creation.
- Feature gate as `PB_FEAT_THREADS`.

Safety notes:
- Do this only after buffer ownership is clean.
- Metrics/log UI updates must remain thread-safe.
- Cap or revisit thread creation if abuse becomes easy.

Acceptance:
- Concurrent requests complete without corrupting logs, paths, or responses.
- `pbjelly` load run remains green.
- A slow client does not block a normal request.
- `pbjelly` validates this by holding one idle connection while a normal request completes quickly.

## 7. Worker Pool

Status: Complete

Build fit: future `pb100` or later

Goal:
- Add bounded concurrency after thread-per-connection lessons are clear.

Implementation goals:
- Create a fixed number of worker threads.
- Queue accepted sockets.
- Use simple Win32 synchronization primitives.
- Feature gate as `PB_FEAT_WORKERS`.

Safety notes:
- More moving parts than thread-per-connection.
- Avoid this until there is evidence that unbounded thread creation is a real problem.

Acceptance:
- Queue saturation behaves predictably.
- Requests are not dropped silently.
- `pbjelly` can report queue/load behavior later.
- `pbjelly` fills the fixed worker queue and validates that overflow receives `503 Service Unavailable`.

## 8. Access Log File

Status: Complete

Build fit: `pb100`

Goal:
- Persist request evidence outside the GUI.

Implementation goals:
- Add optional access log file writing.
- Keep one compact line per request: status, method/path, body bytes.
- Use a fixed filename beside the exe or under selected root only if explicitly chosen.
- Feature gate as `PB_FEAT_ACCESS_LOG`.

Safety notes:
- Do not log full headers by default.
- Avoid logging secrets in query strings if query support expands later.
- Make failures non-fatal to serving.

Acceptance:
- `pbjelly` validates that smaller builds do not create the log and `pb100` writes request evidence to `portblaster.log`.
- Normal serving continues if log write fails.

## 9. Config File

Status: Complete

Build fit: `pb100`

Goal:
- Allow repeatable local startup settings without registry or installer state.

Implementation goals:
- Use a tiny line-based config, not JSON.
- Example keys: `port=8083`, `root=public`, `timeout_ms=5000`.
- Load from a fixed local file beside the exe, such as `portblaster.ini`.
- GUI values still show loaded settings.
- Feature gate as `PB_FEAT_CONFIG`.

Safety notes:
- Validate config exactly like GUI input.
- Invalid config must fail closed or fall back visibly.
- Do not support environment expansion or includes.

Acceptance:
- Valid `portblaster.ini` pre-fills port/root and auto-starts through the jelly build.
- Invalid `port=` shows a clear status and does not start.

## 10. Directory Listing

Status: Complete

Build fit: `pb100`, off by default

Goal:
- Make local static browsing more useful when no `index.html` exists.

Implementation goals:
- Only generate listing when explicitly enabled.
- Escape file names in HTML.
- Link only entries resolved under the selected root.
- Feature gate as `PB_FEAT_DIR_LIST`.

Safety notes:
- Default must be off.
- Never list outside root.
- Avoid exposing hidden/system files unless explicitly allowed later.

Acceptance:
- Directory without `index.html` returns `404` unless `dir_list=1` is configured.
- `pbjelly` validates that enabled listing returns an HTML response containing the generated file name.

## 11. Status Endpoint

Status: Complete

Build fit: `pb100`

Goal:
- Provide machine-readable local status for `pbjelly` and humans.

Implementation goals:
- Add a local-only path such as `/__pb/status`.
- Return compact plain text or minimal JSON.
- Include request count, bytes served, uptime, build target, and enabled features.
- Feature gate as `PB_FEAT_STATUS_ENDPOINT`.

Safety notes:
- Keep loopback bind as default.
- Do not expose filesystem root unless we explicitly choose to.
- Keep endpoint read-only.

Acceptance:
- `GET /__pb/status` returns current counters.
- `pbjelly` validates that smaller builds return `404` and `pb100` returns a status body with target, counters, uptime, and features.

## 12. HTTP Range Requests

Status: Complete

Build fit: `pb100`, later

Goal:
- Improve behavior for media and download clients.

Implementation goals:
- Parse a single `Range: bytes=start-end` header.
- Support only one range.
- Return `206 Partial Content` or `416 Range Not Satisfiable`.
- Feature gate as `PB_FEAT_RANGE`.

Safety notes:
- Avoid multi-range support.
- Validate numeric bounds carefully.
- Do not let ranges bypass root/file checks.

Acceptance:
- `pbjelly` validates that `pb100` returns `206 Partial Content` for a valid single byte range.
- `pbjelly` validates that invalid ranges return `416`.
- Normal full-file requests remain unchanged.

## 13. IPv4 All-Interfaces Bind

Status: Complete

Build fit: `pb100`, optional and never default

Goal:
- Allow LAN testing when the user intentionally opts in.

Implementation goals:
- Add an explicit bind option, such as loopback vs all interfaces.
- Keep default as `127.0.0.1`.
- Make all-interface status visually obvious.
- Feature gate as `PB_FEAT_BIND_ALL`.

Safety notes:
- Never silently expose the server beyond loopback.
- Consider requiring a checkbox or config value, not automatic behavior.

Acceptance:
- Default bind remains loopback.
- Opt-in bind listens on all IPv4 interfaces.
- `pbjelly` checks the Windows TCP listener table for loopback vs all-interface mode.

## 14. IPv6

Status: Complete

Build fit: `pb100`, later

Goal:
- Support modern local networking without affecting smaller builds.

Implementation goals:
- Add IPv6 socket setup separately from IPv4.
- Decide whether dual-stack or separate sockets is smaller/safer.
- Feature gate as `PB_FEAT_IPV6`.

Safety notes:
- Preserve loopback default.
- Avoid accidentally exposing on public IPv6 addresses.

Acceptance:
- `http://[::1]:8083/` works when enabled.
- IPv4 behavior remains unchanged.
- `pbjelly` validates the same safety/load suite through `::1` when `ipv6=1` is configured.

## 15. Tray / Minimize Behavior

Status: Complete

Build fit: `pb100`

Goal:
- Make longer local server sessions less intrusive.

Implementation goals:
- Add minimize-to-tray behavior.
- Add tray menu with open, copy URL, stop, exit.
- Feature gate as `PB_FEAT_TRAY`.

Safety notes:
- Avoid hidden-running confusion.
- Show clear state in tray tooltip/title.

Acceptance:
- User can restore and stop the server.
- Exiting closes listener cleanly.
- Tray menu includes open, copy URL, stop, and exit where the enabled actions match current state.

## 16. GUI Attack Tester

Status: Complete

Build fit: `pbjelly`, not server builds

Goal:
- Turn `pbjelly.exe` into the visible attack/defense cockpit.

Implementation goals:
- Preserve command-line mode for automation.
- Add a Win32 GUI mode when launched without probe arguments.
- Let user select `pbj20`, `pbj50`, or `pbj100`.
- Buttons: Start Target, Run Safety, Run Load, Run All, Open Report.
- Live table: probe, expected, actual, latency, bytes, pass/fail.
- Generate `jelly-report.html`.

Safety notes:
- Keep attack traffic local by default.
- Do not require elevated privileges.
- Do not remove CLI validation path.

Acceptance:
- GUI can start a target and run the current suite.
- CLI mode continues to pass automated validation.
- `pbjelly --gui-smoke` creates and closes the GUI for automated build validation.

## 17. Real-Time Defense Reactions

Status: Complete

Build fit: `pbjelly` first, `pb100` maybe later

Goal:
- Make security behavior visible and satisfying.

Implementation goals:
- In `pbjelly`, show attack/defense pairs:
  - sent `/../portblaster.c`
  - got `403 Forbidden`
  - verdict `blocked`
- Add grouped categories: path safety, method safety, protocol safety, load.
- Optionally read `/__pb/status` if `pb100` adds it later.

Safety notes:
- Keep this as reporting/visualization, not server logic.
- Avoid teaching unsafe defaults by making external targets too easy.

Acceptance:
- Current jelly suite displays pass/fail reactions in plain language.
- HTML report includes a defense summary.

## 18. HTTPS via SChannel

Build fit: future separate experiment, not current `pb100`

Goal:
- Explore native Windows TLS without OpenSSL.

Implementation goals:
- Keep as an experimental branch/profile until byte cost is known.
- Use SChannel directly.
- Require explicit cert configuration.
- Feature gate as `PB_FEAT_TLS_SCHANNEL`.

Safety notes:
- TLS is easy to get wrong.
- Certificate UX can dominate complexity.
- Do not include in standard builds until proven.

Acceptance:
- Experimental build serves HTTPS locally with documented cert setup.
- Size, complexity, and security tradeoffs are recorded.

## 19. Reverse Proxy TLS Docs

Status: Complete

Build fit: docs for all builds

Goal:
- Provide safe TLS guidance without increasing exe size.

Implementation goals:
- Document using an external reverse proxy for TLS.
- Keep PortBlaster bound to loopback behind the proxy.
- Include one minimal example once a preferred proxy is chosen.

Safety notes:
- Avoid recommending public exposure without clear warnings.
- Keep this separate from built-in HTTPS claims.

Acceptance:
- README explains that built-in TLS is not part of standard tiny builds.
- README documents the loopback reverse-proxy shape for TLS.

## 20. Compression

Status: Complete - deferred by design

Build fit: skip for now

Goal:
- Not planned for standard builds.

Reason:
- Compression adds complexity, memory pressure, and likely dependency/code size.
- It does not fit current safety/size priorities.

Revisit only if:
- A tiny static implementation is proven safe and useful.
- `pbjelly` can measure a real benefit.

Acceptance:
- Standard builds do not advertise or emit compressed responses.
- `pbjelly` verifies that `Accept-Encoding: gzip` does not produce `Content-Encoding`.

## 21. Keep-Alive

Status: Complete - deferred by design

Build fit: skip for now

Goal:
- Not planned for standard builds.

Reason:
- Keep-alive adds parser loops, timeouts, and denial-of-service surface.
- PortBlaster currently closes each connection, which is simple and safe.

Revisit only if:
- Connection setup overhead becomes a measured problem.
- Per-request timeout and concurrency are already solid.

Acceptance:
- Standard builds continue to close each connection.
- `pbjelly` verifies that a `Connection: keep-alive` request still receives `Connection: close`.
