#!/usr/bin/env python3
"""run_validation.py — Acadia HAL binary validation orchestrator.

Runs on a remote x86 laptop; connects to the Raspberry Pi over SSH and
executes + verifies EVERY deployed executable (binaries only).

Phases
  0  preflight       connectivity, system snapshot, discovery, ELF arch check
  1  load check      every test binary: `--help` must start and print usage
  2  one-shot        every test binary: run once, parse summary, zero failures
  3  hal_server      start gRPC server, verify TCP listen, clean shutdown
  4  (opt) long-term `<exe> long-term 1000 10s` bounded soak — --long-term flag

Usage
  # harness self-test against the local x86 build
  python3 validate/run_validation.py --mode local

  # authoritative run against the Pi
  python3 validate/run_validation.py --mode ssh --host pi@192.168.1.42

Exit codes: 0 = all PASS/SKIP · 1 = any failure · 2 = harness/connection error
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from hal_ssh import CommandRunner, ExecResult
from hal_parser import parse as parse_output, load_check, oneshot_verdict
from hal_discovery import (Binary, discover, check_arch, snapshot_system,
                           DEFAULT_REMOTE_BIN_DIRS, DEFAULT_LOCAL_BIN_DIRS)

DEFAULT_CONFIG = Path(__file__).parent / "config.json"
GRPC_PORT = 50051
SERVER_NAME = "hal_server"


# --------------------------------------------------------------------- config
def load_config(path: str | None) -> dict:
    cfg: dict = {}
    if path:
        p = Path(path)
        if p.exists():
            cfg = json.loads(p.read_text())
    return cfg


# ------------------------------------------------------------------ reporting
class Report:
    def __init__(self, report_dir: Path):
        self.dir = report_dir
        self.logs = report_dir / "logs"
        self.logs.mkdir(parents=True, exist_ok=True)
        self.started = datetime.now(timezone.utc)

    def log_path(self, name: str) -> Path:
        return self.logs / f"{name}.log"

    def write_log(self, name: str, stdout: str, stderr: str, cmd: str) -> str:
        p = self.log_path(name)
        p.write_text(f"$ {cmd}\n--- stdout ---\n{stdout}"
                     + (f"\n--- stderr ---\n{stderr}" if stderr.strip() else ""))
        return str(p)

    def finish(self, results: list[dict], sysinfo: SystemInfo, meta: dict) -> Path:
        stamp = self.started.strftime("%Y%m%d-%H%M%S")
        summary = {
            "meta": {**meta, "started_utc": self.started.isoformat(),
                     "finished_utc": datetime.now(timezone.utc).isoformat()},
            "system": sysinfo.raw,
            "results": results,
        }
        jpath = self.dir / "summary.json"
        jpath.write_text(json.dumps(summary, indent=2))
        mpath = self.dir / "summary.md"
        mpath.write_text(render_markdown(summary))
        (self.dir / "system_info.txt").write_text(sysinfo.render() + "\n")
        return jpath


def render_markdown(summary: dict) -> str:
    meta, sysinfo, results = summary["meta"], summary["system"], summary["results"]
    n_pass = sum(1 for r in results if r["verdict"] == "PASS")
    n_fail = sum(1 for r in results if r["verdict"] in ("FAIL", "LOAD_FAILED", "TIMEOUT"))
    n_skip = sum(1 for r in results if r["verdict"] == "SKIP")
    out = [
        "# Acadia HAL Binary Validation Report",
        "",
        f"* **Target**: {meta.get('target')} | **Mode**: {meta.get('mode')} | "
        f"**Started**: {meta.get('started_utc')}",
        f"* **Binaries**: {len(results)} total — "
        f"**PASS {n_pass}** · **FAIL {n_fail}** · **SKIP {n_skip}**",
        f"* **Verdict**: {'✅ ALL GREEN' if n_fail == 0 else '❌ FAILURES PRESENT'}",
        "",
        "## Environment",
        "",
        "```",
    ]
    out += [f"{k}: {v}" for k, v in sysinfo.items()]
    out += ["```", "", "## Results", "",
            "| # | Binary | Verdict | Load | One-shot | Total | Pass | Fail | Error | Skip | Time(s) | Notes |",
            "|--:|--------|---------|------|----------|------:|-----:|-----:|------:|-----:|--------:|-------|"]
    for i, r in enumerate(results, 1):
        os_ = r.get("oneshot", {})
        notes = "; ".join(r.get("reasons", [])[:3]).replace("|", "/")
        out.append(
            f"| {i} | `{r['name']}` | **{r['verdict']}** | "
            f"{r.get('load', {}).get('status', '-')} | {os_.get('status', '-')} | "
            f"{os_.get('total', '-')} | {os_.get('pass', '-')} | "
            f"{os_.get('fail', '-')} | {os_.get('error', '-')} | "
            f"{os_.get('skip', '-')} | {os_.get('time_s', '-') if os_.get('time_s') is not None else '-'} | "
            f"{notes} |")
    fails = [r for r in results if r["verdict"] in ("FAIL", "LOAD_FAILED", "TIMEOUT")]
    if fails:
        out += ["", "## Failures detail", ""]
        for r in fails:
            out.append(f"### `{r['name']}` — {r['verdict']}")
            out += ["```"] + [f"- {x}" for x in r.get("reasons", [])] + ["```",
                    f"Log: `{r.get('log', '')}`", ""]
    return "\n".join(out) + "\n"


# ------------------------------------------------------------------ checks
def phase0(runner: CommandRunner, cfg: dict) -> tuple[list[Binary], SystemInfo, list[str]]:
    print(f"\n=== Phase 0: preflight ({runner.describe()}) ===")
    ping = runner.ping()
    if not ping.ok:
        raise ConnectionError(
            f"cannot reach target: rc={ping.rc} stderr={ping.stderr.strip()[:200]}")
    print(f"  connectivity ......... OK ({ping.duration_s:.1f}s)")

    sysinfo = snapshot_system(runner)
    for k in ("model", "uname", "os", "cpu_temp", "mem_total", "systemd_hal_server"):
        if sysinfo.raw.get(k):
            print(f"  {k:<20} {sysinfo.raw[k]}")

    bin_dirs = cfg["bin_dirs"]
    binaries, problems = discover(runner, bin_dirs)
    for p in problems:
        print(f"  discovery warning: {p}")
    if not binaries:
        raise RuntimeError(f"no executables found under: {bin_dirs}")
    print(f"  discovered ........... {len(binaries)} executables in {bin_dirs}")

    check_arch(runner, binaries)
    want = "x86_64" if runner.mode == "local" else "aarch64"
    bad = [b for b in binaries if b.arch_ok is not True]
    if bad:
        print(f"  arch check ........... {len(bad)} unexpected (want {want}): "
              + ", ".join(f"{b.name}={b.arch}" for b in bad[:5]))
    else:
        print(f"  arch check ........... all {len(binaries)} are {want}")
    return binaries, sysinfo, []


def load_phase(runner: CommandRunner, bins: list[Binary], timeout: float,
               report: Report) -> dict[str, dict]:
    print(f"\n=== Phase 1: load check (--help) on {len(bins)} test binaries ===")
    out: dict[str, dict] = {}

    def one(b: Binary) -> tuple[str, dict]:
        cmd = f"'{b.path}' --help"
        r = runner.run(cmd, timeout=timeout)
        ok, reasons, has_usage = load_check(r.stdout, r.rc)
        log = report.write_log(b.name + ".load", r.stdout, r.stderr, cmd)
        return b.name, {"status": "PASS" if ok else "FAIL",
                        "rc": r.rc, "duration_s": round(r.duration_s, 2),
                        "usage_printed": has_usage, "reasons": reasons, "log": log}

    with ThreadPoolExecutor(max_workers=4) as pool:      # load checks are safe
        for name, d in pool.map(one, bins):
            out[name] = d
    n_ok = sum(1 for d in out.values() if d["status"] == "PASS")
    print(f"  {n_ok}/{len(out)} binaries load and execute")
    for name, d in out.items():
        if d["status"] != "PASS":
            print(f"    LOAD FAIL {name}: {'; '.join(d['reasons'])}")
    return out


def oneshot_phase(runner: CommandRunner, bins: list[Binary], timeout: float,
                  report: Report, jobs: int) -> dict[str, dict]:
    print(f"\n=== Phase 2: one-shot run on {len(bins)} test binaries "
          f"(timeout {timeout:.0f}s, jobs={jobs}) ===")
    out: dict[str, dict] = {}

    def one(b: Binary) -> tuple[str, dict]:
        cmd = f"'{b.path}' one-shot"
        r = runner.run(cmd, timeout=timeout, env_export={"LC_ALL": "C"})
        # some binaries (platform_detection, typed_module_handler, …) print
        # their results on stderr — parse the combined stream
        parsed = parse_output(r.stdout + "\n" + r.stderr)
        ok, reasons = oneshot_verdict(parsed, r.rc, r.stdout + "\n" + r.stderr,
                                      r.stderr, r.timed_out)
        status = "TIMEOUT" if r.timed_out else ("PASS" if ok else "FAIL")
        log = report.write_log(b.name, r.stdout, r.stderr, cmd)
        d = {"status": status, "rc": r.rc, "duration_s": round(r.duration_s, 1),
             "parse_mode": parsed.parse_mode, "total": parsed.total,
             "pass": parsed.passed, "fail": parsed.failed,
             "error": parsed.errored, "skip": parsed.skipped,
             "time_s": parsed.time_s, "reasons": reasons, "log": log}
        return b.name, d

    done = 0
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        for name, d in pool.map(one, bins):
            done += 1
            mark = {"PASS": "✓", "FAIL": "✗", "TIMEOUT": "⏱"}[d["status"]]
            print(f"  [{done:>2}/{len(bins)}] {mark} {name:<42} {d['status']:<7} "
                  f"total={d['total']:<3} pass={d['pass']:<3} fail={d['fail']:<2} "
                  f"err={d['error']:<2} skip={d['skip']:<2} {d['duration_s']:>6.1f}s")
            out[name] = d
    return out


def server_phase(runner: CommandRunner, server: Binary | None, timeout: float,
                 report: Report, manage_service: bool) -> dict:
    print(f"\n=== Phase 3: hal_server gRPC check (port {GRPC_PORT}) ===")
    res: dict = {"status": "SKIP", "reasons": []}
    if server is None:
        res["reasons"].append("hal_server not found in bin dirs")
        res["status"] = "SKIP"
        print("  hal_server not present — skipped")
        return res

    # who holds the port? (owner name + busy flag; ss primary, /dev/tcp fallback)
    r = runner.run(
        f"ss -tlnp 2>/dev/null | grep ':{GRPC_PORT} ' || "
        f"(command -v ss >/dev/null 2>&1 || "
        f"(timeout 1 bash -c '</dev/tcp/127.0.0.1/{GRPC_PORT}' 2>/dev/null "
        f"&& echo busy || echo free))", timeout=10)
    port_busy = "LISTEN" in r.stdout or "busy" in r.stdout
    owner = "unknown"
    m = re.search(r'users:\(\("?([^,")]+)', r.stdout)
    if m:
        owner = m.group(1).strip('"')

    stopped_service = False
    chosen_port = GRPC_PORT

    if port_busy:
        if "hal_server" in owner:
            print(f"  port {GRPC_PORT} listening — owned by hal_server; "
                  "validating running instance")
            res["status"] = "PASS"
            res["already_running"] = True
            res["owner"] = owner
            res["reasons"].append(
                f"validated pre-existing hal_server listener on {GRPC_PORT}")
            return res
        # port held by an unrelated process — find a free alternate port
        print(f"  port {GRPC_PORT} held by '{owner}' (not hal_server) — "
              "using alternate port")
        r2 = runner.run(
            "for p in " + " ".join(str(GRPC_PORT + i) for i in range(1, 6)) + "; do "
            "ss -tln 2>/dev/null | grep -q \":$p \" || { echo $p; break; }; "
            "done", timeout=10)
        alt = r2.stdout.strip().splitlines()[0] if r2.stdout.strip() else ""
        if not alt.isdigit():
            res["status"] = "FAIL"
            res["reasons"].append(
                f"port {GRPC_PORT} held by '{owner}' and no free alternate port found")
            print("  FAIL no free port available for hal_server check")
            return res
        chosen_port = int(alt)
        res["alternate_port"] = chosen_port
        res["owner_of_default_port"] = owner

    port = chosen_port

    if manage_service:
        r = runner.run(
            "systemctl is-active hal-server >/dev/null 2>&1 && "
            "systemctl stop hal-server && echo stopped || echo none", timeout=30)
        stopped_service = "stopped" in r.stdout
        if stopped_service:
            print("  stopped existing hal-server systemd unit")
            res["service_was_stopped"] = True

    cmd = (f"nohup '{server.path}' 0.0.0.0:{port} "
           f"</dev/null >/tmp/hal_val_server.log 2>&1 & echo $!")
    r = runner.run(cmd, timeout=15)
    pid = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
    if not pid.isdigit():
        res["status"] = "FAIL"
        res["reasons"].append(f"could not start hal_server: {r.stdout[:120]} {r.stderr[:120]}")
        log = report.write_log("hal_server", r.stdout, r.stderr, cmd)
        res["log"] = log
        print("  FAIL could not start hal_server")
        return res

    # wait for listen
    listening = False
    t0 = time.monotonic()
    while time.monotonic() - t0 < 12:
        rr = runner.run(
            f"kill -0 {pid} 2>/dev/null && echo alive || echo dead; "
            f"ss -tln 2>/dev/null | grep ':{port} ' || true", timeout=10)
        if "dead" in rr.stdout.splitlines()[0]:
            break
        if f":{port} " in rr.stdout:
            listening = True
            break
        time.sleep(0.5)

    srv_log = runner.run("cat /tmp/hal_val_server.log 2>/dev/null", timeout=10).stdout
    log = report.write_log("hal_server", srv_log,
                           f"pid={pid} listening={listening}", cmd)

    if not listening:
        alive = runner.run(f"kill -0 {pid} 2>/dev/null && echo alive || echo dead",
                           timeout=10).stdout.strip()
        res["status"] = "FAIL"
        res["reasons"].append(
            f"server pid {pid} did not listen on {port} within 12s "
            f"(alive={alive == 'alive'})")
        print(f"  FAIL hal_server did not listen on {port}")
    else:
        res.update({"status": "PASS", "pid": pid, "listening": True,
                    "port": port,
                    "startup_s": round(time.monotonic() - t0, 1)})
        print(f"  ✓ hal_server pid {pid} LISTENING on {port} "
              f"(startup {time.monotonic() - t0:.1f}s)")

    # shutdown
    if pid.isdigit():
        runner.run(f"kill {pid} 2>/dev/null; sleep 1; "
                   f"kill -0 {pid} 2>/dev/null && kill -9 {pid} 2>/dev/null; "
                   f"kill -0 {pid} 2>/dev/null && echo still || echo gone", timeout=15)
        gone = runner.run(
            f"sleep 1; kill -0 {pid} 2>/dev/null && echo alive || echo gone; "
            f"ss -tln 2>/dev/null | grep -q ':{port} ' && echo portbusy || echo portfree",
            timeout=15)
        lines = gone.stdout.splitlines()
        exited = len(lines) > 0 and "gone" in lines[0]
        port_free = len(lines) > 1 and "portfree" in lines[1]
        res["shutdown"] = {"exited": exited, "port_released": port_free}
        if listening and res["status"] == "PASS":
            if not exited:
                res["status"] = "FAIL"
                res["reasons"].append("server did not exit after SIGTERM")
            elif not port_free:
                res["reasons"].append("port still busy after exit (informational)")
        print(f"  shutdown ............. {'clean' if exited else 'FORCED/KILLED'}; "
              f"port released: {port_free}")

    if stopped_service:
        r = runner.run("systemctl start hal-server && echo restarted || echo failed",
                       timeout=30)
        print(f"  service restore ...... {r.stdout.strip()}")
        res["service_restored"] = "restarted" in r.stdout
    return res


def longterm_phase(runner: CommandRunner, bins: list[Binary], timeout: float,
                   report: Report, duration: str = "10s") -> dict[str, dict]:
    print(f"\n=== Phase 4 (optional): long-term {duration} soak ===")
    out: dict[str, dict] = {}
    for b in bins:
        cmd = f"'{b.path}' long-term 1000 {duration}"
        r = runner.run(cmd, timeout=timeout)
        parsed = parse_output(r.stdout + "\n" + r.stderr)
        ok, reasons = oneshot_verdict(parsed, r.rc, r.stdout + "\n" + r.stderr,
                                      r.stderr, r.timed_out)
        status = "TIMEOUT" if r.timed_out else ("PASS" if ok else "FAIL")
        log = report.write_log(f"{b.name}.longterm", r.stdout, r.stderr, cmd)
        out[b.name] = {"status": status, "duration_s": round(r.duration_s, 1),
                       "total": parsed.total, "fail": parsed.failed,
                       "reasons": reasons, "log": log}
        print(f"  {b.name:<44} {status} {out[b.name]['duration_s']}s")
    return out


# ---------------------------------------------------------------------- main
def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", choices=("ssh", "local"), help="connection mode")
    ap.add_argument("--host", help="ssh target, e.g. pi@192.168.1.42")
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--key", help="ssh identity file")
    ap.add_argument("--config", default=str(DEFAULT_CONFIG))
    ap.add_argument("--bin-dir", action="append", dest="bin_dirs",
                    help="bin dir(s) on target (repeatable; overrides config)")
    ap.add_argument("--timeout", type=float, help="per-binary timeout seconds")
    ap.add_argument("--only", help="comma-separated names to include")
    ap.add_argument("--skip", help="comma-separated names to exclude")
    ap.add_argument("--jobs", type=int, help="parallel one-shot runs (default 1)")
    ap.add_argument("--report-dir", help="report output dir")
    ap.add_argument("--long-term", action="store_true", help="add phase-4 soak")
    ap.add_argument("--long-term-duration", default="10s")
    ap.add_argument("--manage-service", action="store_true",
                    help="allow systemctl stop/start of hal-server (default off)")
    ap.add_argument("--list", action="store_true", help="only list discovered binaries")
    ap.add_argument("--phase", help="run a single phase (0,1,2,3) for debugging")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    cfg = load_config(args.config if Path(args.config).exists() else None)

    mode = args.mode or cfg.get("mode", "local")
    if mode == "ssh" and not args.host:
        print("ERROR: --mode ssh requires --host (e.g. pi@192.168.1.42)", file=sys.stderr)
        return 2
    host = args.host or cfg.get("host", "")
    project_root = Path(__file__).resolve().parent.parent
    bin_dirs = (args.bin_dirs
                or cfg.get(f"bin_dirs_{mode}")
                or cfg.get("bin_dirs")
                or (DEFAULT_LOCAL_BIN_DIRS if mode == "local"
                    else DEFAULT_REMOTE_BIN_DIRS))
    if mode == "local":  # resolve relative paths against the project root
        bin_dirs = [str(d if Path(d).is_absolute() else project_root / d)
                    for d in bin_dirs]
    timeout = args.timeout or cfg.get("timeout_s", 180)
    jobs = args.jobs or cfg.get("jobs", 1)
    only = set((args.only or cfg.get("only", "")).split(",")) - {""}
    skip = set((args.skip or cfg.get("skip", "")).split(",")) - {""}
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    report_dir = Path(args.report_dir or cfg.get("report_dir", "validate/reports") or
                      "validate/reports") / f"{mode}-{stamp}"
    report = Report(report_dir)

    runner = CommandRunner(mode=mode, host=host, port=args.port,
                           ssh_key=args.key or "")
    meta = {"mode": mode, "host": runner.describe(), "bin_dirs": bin_dirs,
            "target": (runner.describe() if mode == "ssh" else "localhost (x86 self-test)"),
            "timeout_s": timeout, "jobs": jobs}

    try:
        binaries, sysinfo, _ = phase0(runner, cfg | {"bin_dirs": bin_dirs})
    except (ConnectionError, RuntimeError) as exc:
        print(f"FATAL phase 0: {exc}", file=sys.stderr)
        return 2

    selected = [b for b in binaries
                if (not only or b.name in only) and (not skip or b.name not in skip)]
    if args.list:
        for b in selected:
            print(f"{b.kind:<6} {b.name:<44} {b.path}  [{b.arch}]")
        return 0
    print(f"  selected ............. {len(selected)} binaries "
          f"(of {len(binaries)} discovered)")

    server = next((b for b in selected if b.name == SERVER_NAME), None)
    tests = [b for b in selected if b.kind == "test"]
    results: list[dict] = []

    phase_filter = args.phase or ""
    loadres: dict[str, dict] = {}
    onesres: dict[str, dict] = {}
    ltres: dict[str, dict] = {}
    srvres: dict = {"status": "SKIP", "reasons": []}
    if not phase_filter or phase_filter == "1":
        loadres = load_phase(runner, tests, timeout, report)
    if not phase_filter or phase_filter == "2":
        runnable = tests if phase_filter == "2" else [
            b for b in tests if loadres.get(b.name, {}).get("status") == "PASS"]
        onesres = oneshot_phase(runner, runnable, timeout, report, jobs)
    if not phase_filter or phase_filter == "3":
        srvres = server_phase(runner, server, timeout, report, args.manage_service)
    if args.long_term and not phase_filter:
        ltres = longterm_phase(runner, runnable, max(timeout, 120),
                               report, args.long_term_duration)

    # ------------------------------------------------------- assemble verdicts
    for b in selected:
        r: dict = {"name": b.name, "path": b.path, "arch": b.arch,
                   "arch_ok": b.arch_ok, "kind": b.kind, "verdict": "SKIP",
                   "reasons": []}
        if only and b.name not in only or b.name in skip:
            r["verdict"] = "SKIP"
            r["reasons"].append("excluded by --only/--skip")
        elif b.kind == "server":
            r["verdict"] = srvres["status"]
            r["server"] = srvres
            r["reasons"] = srvres.get("reasons", [])
        else:
            ld = loadres.get(b.name)
            os_ = onesres.get(b.name)
            lt = ltres.get(b.name) or None
            if ld:
                r["load"] = ld
            if os_:
                r["oneshot"] = os_
            if lt:
                r["longterm"] = lt
            if ld and ld["status"] == "FAIL" and not os_:
                r["verdict"] = "LOAD_FAILED"
                r["reasons"] = ld["reasons"]
            elif os_ is None:
                r["verdict"] = "SKIP"
                r["reasons"].append("skipped after load failure or phase filter")
            else:
                r["verdict"] = os_["status"]
                r["reasons"] = os_["reasons"]
                if lt and lt["status"] != "PASS":
                    r["verdict"] = lt["status"]
                    r["reasons"] = lt["reasons"]
        log = (r.get("oneshot") or r.get("load") or r.get("server") or {}).get("log")
        if log:
            r["log"] = log
        results.append(r)

    jpath = report.finish(results, sysinfo, meta)
    n_pass = sum(1 for r in results if r["verdict"] == "PASS")
    n_fail = sum(1 for r in results if r["verdict"] in ("FAIL", "LOAD_FAILED", "TIMEOUT"))
    n_skip = len(results) - n_pass - n_fail
    print(f"\n{'=' * 72}")
    print(f"TOTAL {len(results)} | PASS {n_pass} | FAIL {n_fail} | SKIP {n_skip}")
    print(f"Report: {report_dir}/summary.md")
    print(f"JSON:   {jpath}")
    print(f"{'=' * 72}")
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())