# Acadia HAL — Binary Validator

Runs on a **remote x86 laptop**, connects to the Raspberry Pi over SSH, and
**executes + verifies every deployed executable** (binaries only — the source
tree is never compiled or inspected). See `PLAN.md` for the validation plan.

```
hal_server + 57 test_* binaries  →  load check  →  one-shot  →  gRPC server check
                                  (can it run?)   (does it pass?)  (does it listen?)
```

## Why parse stdout?

The HAL test binaries **always exit 0**, even with failures. The harness parses
the summary line (`Total: N Pass: X Fail: Y [Error: Z] [Skip: W]`) and the
result table, plus the simpler `all checks passed` format used by a few
binaries. Exit codes are checked too, but never trusted alone.

## Requirements (laptop)

* Python 3.9+ (stdlib only — no pip packages)
* `ssh` with key auth to the Pi (`ssh-copy-id pi@<IP>` once)

## Quick start

```bash
# 0) self-test the harness against the local x86 build (no Pi needed)
python3 validate/run_validation.py --mode local

# 1) list what would be tested on the Pi
python3 validate/run_validation.py --mode ssh --host pi@192.168.1.42 --list

# 2) full authoritative run
python3 validate/run_validation.py --mode ssh --host pi@192.168.1.42

# 3) faster iteration on one module
python3 validate/run_validation.py --mode ssh --host pi@192.168.1.42 --only test_gpio

# 4) optional 10s soak per binary (~10 min extra)
python3 validate/run_validation.py --mode ssh --host pi@192.168.1.42 --long-term
```

## CLI reference

| Flag | Meaning |
|---|---|
| `--mode ssh\|local` | target connection (local runs `build/bin` directly) |
| `--host user@ip` | SSH target (ssh mode) |
| `--port`, `--key` | SSH port / identity file |
| `--bin-dir` | bin dir(s) on target; default `/opt/hal/bin`, then `~/acadia-hal/build/cross/bin` |
| `--timeout N` | per-binary hard timeout (default 180 s) |
| `--only a,b` / `--skip a,b` | filter binaries by name |
| `--jobs N` | parallel one-shot runs (default 1 — hardware tests are serial by design) |
| `--long-term` | add bounded `long-term 1000 10s` soak phase |
| `--manage-service` | allow stopping/starting the `hal-server` systemd unit (default: if port 50051 is busy, the running instance is validated as-is) |
| `--phase 0..3` | run a single phase for debugging |
| `--list` | only list discovered binaries |

## Outputs

Each run writes `validate/reports/<mode>-<stamp>/`:

* `summary.json` — machine-readable per-binary verdicts
* `summary.md` — human-readable table + failure details
* `system_info.txt` — Pi model, kernel, temp, memory, service state
* `logs/<binary>.log` — raw stdout/stderr of every executed binary

Harness exit codes: `0` all pass · `1` any failure · `2` harness/connection error.

## Verdicts

`PASS` · `FAIL` (parsed failures/crash signatures) · `LOAD_FAILED` (binary did
not even start) · `TIMEOUT` (hard limit) · `SKIP` (filtered).

## Notes

* The `monitoring` test mode runs forever by design — the harness never uses it.
* A `--mode local` run against x86 binaries may show hardware-dependent failures
  (DMA, CAN filters…) that are expected to pass on the Pi; the SSH run is
  authoritative.
* `hal_server`: if TCP 50051 is already listening the harness validates the
  running instance without restarting it; otherwise it starts its own instance
  and shuts it down afterwards.