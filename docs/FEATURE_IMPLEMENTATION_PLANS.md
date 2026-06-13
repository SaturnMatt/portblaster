# PortBlaster Feature Implementation Plans

This file breaks down the major candidate features for PortBlaster into goals, build fit, safety notes, and likely implementation steps.

Priority order remains:

1. Safety
2. Size
3. Function
4. Polish

The server should stay one source file, `portblaster.c`, with compile-time feature flags. `pbjelly.c` stays separate as the tester/reporting tool.

## 1. Browse Root Button

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

## 2. Copy URL Button

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

## 3. Better Status Codes

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
- Add a `413` check with a generated large file when practical.

## 4. Larger MIME Table

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
- `pbjelly` or a small manual check confirms representative content types.
- Size delta is recorded for each added extension group.

## 5. Per-Request Timeout

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
- A slow/incomplete request eventually closes instead of holding the server forever.

## 6. Thread-Per-Connection

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

## 7. Worker Pool

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

## 8. Access Log File

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
- Log file is created only when feature is enabled.
- Normal serving continues if log write fails.

## 9. Config File

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
- Valid config pre-fills port/root.
- Invalid port/root shows a clear status and does not start.

## 10. Directory Listing

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
- Directory without `index.html` returns `404` unless enabled.
- Enabled listing escapes names and links correctly.

## 11. Status Endpoint

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
- `pbjelly` can optionally include status endpoint data in reports.

## 12. HTTP Range Requests

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
- Valid range returns expected byte count.
- Invalid range returns `416`.
- Normal full-file requests remain unchanged.

## 13. IPv4 All-Interfaces Bind

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

## 14. IPv6

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

## 15. Tray / Minimize Behavior

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

## 16. GUI Attack Tester

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

## 17. Real-Time Defense Reactions

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
- Users have a clear path if TLS is needed.

## 20. Compression

Build fit: skip for now

Goal:
- Not planned for standard builds.

Reason:
- Compression adds complexity, memory pressure, and likely dependency/code size.
- It does not fit current safety/size priorities.

Revisit only if:
- A tiny static implementation is proven safe and useful.
- `pbjelly` can measure a real benefit.

## 21. Keep-Alive

Build fit: skip for now

Goal:
- Not planned for standard builds.

Reason:
- Keep-alive adds parser loops, timeouts, and denial-of-service surface.
- PortBlaster currently closes each connection, which is simple and safe.

Revisit only if:
- Connection setup overhead becomes a measured problem.
- Per-request timeout and concurrency are already solid.
