# Acadia HAL — Binary Validation Plan (Remote x86 → Raspberry Pi 5)

> Goal: from a remote **x86 laptop**, connect to the Raspberry Pi over SSH and
> **execute and verify every deployed executable** (binaries only — no source,
> no compilation). Produce a pass/fail verdict per binary and a consolidated
> report.

---

## 1. Scope

| Item | Detail |
|---|---|
| System under test | Raspberry Pi 5 (aarch64), Acadia HAL deployed binaries |
| Harness host | Remote x86 laptop (this is where the scripts run) |
| Transport | SSH (`ssh` binary, BatchMode/key auth; zero pip dependencies) |
| Binaries under test | `hal_server` + 57 `test_*` executables (e.g. `/opt/hal/bin`) |
| What is verified | That each **binary** loads, runs, and reports success on the Pi |
| What is NOT tested | Source code, compilation, unit-test internals |

## 2. Ground truth about the binaries (from reverse-engineering the build)

1. All `test_*` binaries share one CLI (from `src/test_runner.h`):
   ```
   <exe>                    one-shot (default)
   <exe> one-shot           run all tests once
   <exe> --help             usage, exit 0
   <exe> monitoring ...     runs forever  → NEVER used by the harness
   <exe> long-term <ms> <dur>
   <exe> monitor-test <ms> <dur>
   <exe> event <ms> <criteria> [dur]
   ```
2. **Exit code is always 0** for one-shot, even with failures → verification
   MUST parse stdout, never trust the return code alone.
3. Two output formats exist:
   * **Table format** (most binaries):
     ```
     Test Name                Result   Duration(s)
     ------------------------------------------------
     testGpioPwmOutput        PASS     5.002438
     ------------------------------------------------
     Total: 22  Pass: 22  Fail: 0  Time: 5.2306s
     GpioTesting completed.
     ```
     Statuses: PASS / FAIL / ERROR / TIMEOUT / SKIP / ABORT
     Summary may include `Error:` and `Skip:` fields.
   * **Simple format** (e.g. typed_module_handler, vision_accelerator_metrics,
     platform_detection): lines of `ok   <check>` ending with
     `all checks passed` or `all platform detection checks passed`.
4. `hal_server` takes an address as argv (`hal_server 0.0.0.0:50051`), has no
   `--help`, binds TCP 50051 by default and prints a TLS warning in insecure
   mode. A systemd unit (`hal-server.service`) may already hold the port.
5. Known slow binaries: `test_gpio` (~5 s, PWM), `test_system_memory` (~8 s),
   `test_reliability` (~6 s). Hardware-touching tests can legitimately take
   longer on the Pi.
6. Some binaries (platform_detection, typed_module_handler,
   vision_accelerator_metrics) print their results on **stderr** — the harness
   always parses the combined stdout+stderr stream.
7. If TCP 50051 is occupied by an unrelated process (e.g. a dev service), the
   hal_server check automatically uses an alternate port (50052–50056) and
   records it in the report.

## 3. Validation phases

### Phase 0 — Preflight (per run)
| # | Check | Pass criteria |
|---|---|---|
| 0.1 | Connectivity | `echo ok` over SSH returns rc 0 |
| 0.2 | System snapshot | collect uname, `/proc/device-tree/model`, uptime, mem, temp |
| 0.3 | Binary discovery | `find` all executables under configured dirs |
| 0.4 | Arch check | every binary is ELF **aarch64** on the Pi (x86-64 in local mode) |
| 0.5 | Dedup | same basename in multiple dirs → first dir in config wins |

### Phase 1 — Load check (each `test_*` binary)
| # | Check | Pass criteria |
|---|---|---|
| 1.1 | Dynamic-link / launch | `--help` → rc 0 (binary loaded & ran). `Usage` in stdout is informational — three binaries (platform_detection, typed_module_handler, vision_accelerator_metrics) print nothing on `--help` but exit 0 |

A binary that cannot even start (missing `.so`, wrong arch, corrupt ELF) fails
here and is skipped in later phases (marked `LOAD_FAILED`).

### Phase 2 — One-shot run (each `test_*` binary)
| # | Check | Pass criteria |
|---|---|---|
| 2.1 | Completion | rc 0 AND footer `<App> completed.` OR simple-format pass marker |
| 2.2 | Summary consistency | `Pass+Fail+Error+Skip == Total` and `Total > 0` |
| 2.3 | Zero failures | `Fail == 0` and `Error == 0` (SKIP is allowed, recorded) |
| 2.4 | No crash signatures | no segfault / double-free / terminate-called / cannot-open-shared-object |
| 2.5 | Row audit | table rows parsed; row FAIL/ERROR/TIMEOUT/ABORT count must equal summary |

Timeout (default 180 s) → verdict `TIMEOUT` (fail). Output captured to per-binary log.

### Phase 3 — hal_server (gRPC service)
| # | Check | Pass criteria |
|---|---|---|
| 3.1 | Port pre-check | if 50051 already listening → validate the running instance (INFO, no restart) |
| 3.2 | Startup | process stays alive ≥ 3 s, no crash text in log |
| 3.3 | Listen | TCP 50051 in LISTEN state (verified via `ss` on the Pi) |
| 3.4 | Shutdown | SIGTERM → process exits; port released |

`systemctl` management is opt-in (`--manage-service`) — never touched by default.

### Phase 4 — Optional long-term smoke (off by default, `--long-term`)
`<exe> long-term 1000 10s` per binary: completes, cycles > 0, summary consistent.

## 4. Verdicts

```
PASS          all applicable phase checks green
FAIL          any applicable check red (details in reason list)
LOAD_FAILED   Phase 1 failed (binary did not start) — Phases 2/4 skipped
TIMEOUT       Phase 2 hit the per-binary timeout
SKIP          excluded via --only/--skip or missing arch check
```

Exit codes of the harness: `0` = all PASS/SKIP, `1` = any FAIL/LOAD_FAILED/TIMEOUT,
`2` = harness/connection error.

## 5. Deliverables (`validate/`)

```
validate/
├── PLAN.md             ← this plan
├── README.md           ← usage guide
├── config.json         ← default config (dirs, timeouts, options)
├── hal_ssh.py          ← exec layer: SSH (laptop→Pi) + local mode
├── hal_parser.py       ← output parsers + verdict rules
├── hal_discovery.py    ← discovery, arch check, system snapshot
├── run_validation.py   ← orchestrator (phases 0–4, reports, exit codes)
├── make_report.py      ← re-render summary.md from summary.json
└── reports/<stamp>/    ← summary.json, summary.md, system_info.txt, logs/*.log
```

## 6. Execution procedure

1. **Dry run (harness self-test, x86):** `python3 validate/run_validation.py --mode local`
   runs the same logic against the host-built `build/bin` to prove the harness.
2. **Real run:** `python3 validate/run_validation.py --mode ssh --host pi@<PI_IP>`
   (bin dirs default to `/opt/hal/bin`, `~/acadia-hal/build/cross/bin`).
3. Review `summary.md`; triage FAILs with `logs/<binary>.log`.

## 7. Known/expected deviations

* Running the harness in `--mode local` against x86 binaries may report
  hardware-dependent failures (e.g. `test_cpu` DMA, `test_communication_interface`
  CAN filters) that are **expected to pass on the real Pi**. This is why the Pi
  run over SSH is authoritative.
* Binaries in `monitoring` mode never exit by design → the harness only uses
  `one-shot` (and bounded `long-term` in the optional phase).