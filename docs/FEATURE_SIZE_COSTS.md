# PortBlaster feature size costs

Measured on 2026-06-13 with the current MSVC release settings from `build.ps1`.

These numbers are best treated as practical linker-output measurements, not pure source-code weights. The Windows PE/linker output moves in coarse chunks, so many small features land at the same `+512 B` boundary. Costs are also not perfectly additive because imports, helper functions, and folded code are shared.

## Standard build sizes

| Build | Size | Delta vs `pb20.exe` |
| --- | ---: | ---: |
| `pb20.exe` | 11,264 B | 0 B |
| `pb50.exe` | 13,824 B | +2,560 B |
| `pb100.exe` | 21,504 B | +10,240 B |

Result: the differentiated builds came out much smaller than their target identities. `pb100.exe` is only about 21 KB while carrying streaming, ranges, worker-pool concurrency, status endpoint, config, tray behavior, access logging, optional bind-all, and optional IPv6.

## Isolated minimum feature costs

Built from the 11,264 B tiny baseline with all optional flags explicitly off, then enabling the feature or its minimum required dependency bundle.

| Feature | Measured flags | Size | Delta |
| --- | --- | ---: | ---: |
| IPv4 all-interface bind | `BIND_ALL` | 11,264 B | +0 B |
| Access log | `ACCESS_LOG` | 11,776 B | +512 B |
| Browse root | `BROWSE` | 11,776 B | +512 B |
| Copy URL | `COPY_URL` | 11,776 B | +512 B |
| IPv6 | `IPV6` | 11,776 B | +512 B |
| Scrolling request log | `LOG` | 11,776 B | +512 B |
| Metrics / uptime / bytes | `METRICS` | 11,776 B | +512 B |
| Expanded MIME table | `MIME_PLUS` | 11,776 B | +512 B |
| Streaming | `STREAM` | 11,776 B | +512 B |
| Thread-per-connection | `THREADS` | 11,776 B | +512 B |
| Socket timeout | `TIMEOUT` | 11,776 B | +512 B |
| Worker pool | `WORKERS` | 11,776 B | +512 B |
| Config file | `CONFIG` | 12,288 B | +1,024 B |
| Directory listing | `DIR_LIST` | 12,288 B | +1,024 B |
| Status endpoint | `METRICS+STATUS_ENDPOINT` | 12,288 B | +1,024 B |
| Tray behavior | `TRAY` | 12,288 B | +1,024 B |
| Range requests | `STREAM+RANGE` | 13,312 B | +2,048 B |

## Marginal costs inside real profiles

Measured by building the full profile, then rebuilding the same profile with one feature removed. Dependency removals are handled conservatively: removing `METRICS` also removes `STATUS_ENDPOINT`; removing `STREAM` also removes `RANGE`.

### `pb50`

| Feature removed | Bytes saved |
| --- | ---: |
| `BROWSE` | 1,024 B |
| `COPY_URL` | 1,024 B |
| `LOG` | 1,024 B |
| `METRICS` | 1,024 B |
| `MIME_PLUS` | 512 B |

### `pb100`

| Feature removed | Bytes saved |
| --- | ---: |
| `STREAM` plus dependent `RANGE` | 2,560 B |
| `RANGE` | 2,048 B |
| `DIR_LIST` | 1,536 B |
| `METRICS` plus dependent `STATUS_ENDPOINT` | 1,536 B |
| `TRAY` | 1,536 B |
| `CONFIG` | 1,024 B |
| `COPY_URL` | 1,024 B |
| `STATUS_ENDPOINT` | 1,024 B |
| `WORKERS` | 1,024 B |
| `ACCESS_LOG` | 512 B |
| `BROWSE` | 512 B |
| `IPV6` | 512 B |
| `LOG` | 512 B |
| `MIME_PLUS` | 512 B |
| `BIND_ALL` | 0 B |
| `TIMEOUT` | 0 B |

Zero-byte marginal costs mean the code fits into space already paid for by the linked binary layout, not that the source has no complexity.

## Takeaways

Best value features:
- `BIND_ALL` is effectively free in bytes, but must stay opt-in for safety.
- `TIMEOUT` is effectively free in the current `pb100` layout and is a strong safety win.
- `MIME_PLUS`, `ACCESS_LOG`, `IPV6`, and `LOG` are cheap relative to usefulness.

Most expensive features:
- `RANGE` is the priciest single feature in the current layout.
- `STREAM+RANGE` is still only about +2.5 KB in the full `pb100` profile, which is excellent for safer large-file behavior.
- `TRAY` and `DIR_LIST` are the biggest polish/convenience costs.

Build strategy implication:
- `pb20` has lots of room under 20 KB, but it should stay intentionally plain to preserve the tiny identity.
- `pb50` is extremely roomy; it could absorb most current convenience features and still stay below 20 KB.
- `pb100` has enormous space left. The real constraint is no longer bytes; it is safety and complexity.
