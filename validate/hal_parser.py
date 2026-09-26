#!/usr/bin/env python3
"""hal_parser.py — parse Acadia HAL test-binary output and derive verdicts.

The binaries share src/test_runner.h which ALWAYS exits 0 on one-shot, so the
verdict comes from stdout. Two formats are recognised:

  1. table format   (most binaries)
       Test Name                Result   Duration(s)
       ------------------------------------------------
       testGpioout1             PASS     0.000026
       ------------------------------------------------
       Total: 22  Pass: 22  Fail: 0  Time: 5.2306s
       GpioTesting completed.

  2. simple format  (typed_module_handler, vision_accelerator_metrics,
                     platform_detection, ...)
       ok   some check name
       all checks passed                 (or: all platform detection checks
                                          passed)

Statuses in table rows: PASS FAIL ERROR TIMEOUT SKIP ABORT
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

STATUSES = ("PASS", "FAIL", "ERROR", "TIMEOUT", "SKIP", "ABORT")

RE_SUMMARY = re.compile(
    r"Total:\s*(\d+)\s+Pass:\s*(\d+)\s+Fail:\s*(\d+)"
    r"(?:\s+Error:\s*(\d+))?(?:\s+Skip:\s*(\d+))?(?:\s+Time:\s*([\d.]+)s)?")
RE_ROW = re.compile(
    r"^\s*(\S.*?)\s{2,}(PASS|FAIL|ERROR|TIMEOUT|SKIP|ABORT)\s+([\d.]+)\s*$",
    re.MULTILINE)
RE_FOOTER = re.compile(r"^\s*\S+\s+completed\.\s*$", re.MULTILINE)
RE_USAGE = re.compile(r"Usage:")
RE_OK_LINE = re.compile(r"^\s*ok\b", re.MULTILINE)

SIMPLE_PASS_MARKERS = (
    "all checks passed",
    "all platform detection checks passed",
)

CRASH_SIGNATURES = (
    "segmentation fault", "core dumped", "double free", "corrupted",
    "terminate called", "std::bad_alloc", "bus error", "illegal instruction",
    "cannot open shared object", "version `", "GLIBC_",
)


@dataclass
class ParsedResult:
    parse_mode: str = "none"        # table | simple | none
    total: int = 0
    passed: int = 0
    failed: int = 0
    errored: int = 0
    skipped: int = 0
    time_s: float | None = None
    rows: list[dict] = field(default_factory=list)   # {name,status,duration}
    footer: bool = False
    simple_pass: bool = False
    ok_lines: int = 0
    crash_hits: list[str] = field(default_factory=list)

    # ---------------------------------------------------------------- summary
    def failing_rows(self) -> list[dict]:
        return [r for r in self.rows
                if r["status"] not in ("PASS", "SKIP")]


def parse(text: str) -> ParsedResult:
    """Parse stdout of a one-shot run. Never raises."""
    res = ParsedResult()
    low = text.lower()
    res.crash_hits = [s for s in CRASH_SIGNATURES if s in low]

    m = RE_SUMMARY.search(text)
    if m:
        res.parse_mode = "table"
        res.total = int(m.group(1))
        res.passed = int(m.group(2))
        res.failed = int(m.group(3))
        res.errored = int(m.group(4) or 0)
        res.skipped = int(m.group(5) or 0)
        res.time_s = float(m.group(6)) if m.group(6) else None
        res.rows = [{"name": n, "status": s, "duration": float(d)}
                    for n, s, d in RE_ROW.findall(text)]
        res.footer = bool(RE_FOOTER.search(text))
    else:
        res.ok_lines = len(RE_OK_LINE.findall(text))
        res.simple_pass = any(mk in low for mk in SIMPLE_PASS_MARKERS)
        res.parse_mode = "simple" if (res.ok_lines or res.simple_pass) else "none"
        res.footer = bool(RE_FOOTER.search(text))
        # a bare "<App> completed." with nothing failing also counts as simple
        if res.parse_mode == "none" and res.footer and not any(
                s in low for s in ("fail", "error", "timeout", "abort")):
            res.parse_mode = "simple"
            res.simple_pass = True
    return res


def load_check(stdout: str, rc: int) -> tuple[bool, list[str], bool]:
    """Phase-1 verdict for `<exe> --help`.

    rc == 0 is the real criterion: the binary loaded, linked and ran. Most
    binaries print a usage block, but three (platform_detection,
    typed_module_handler, vision_accelerator_metrics) print nothing and still
    exit 0 — that counts as a successful load. Empty stdout is recorded as
    informational only.
    """
    reasons: list[str] = []
    has_usage = bool(RE_USAGE.search(stdout))
    if rc != 0:
        reasons.append(f"exit code {rc} (expected 0) — binary failed to load/run")
    elif not stdout.strip():
        # tolerated: binary runs but does not implement --help output
        pass
    return (rc == 0), reasons, has_usage


def oneshot_verdict(parsed: ParsedResult, rc: int, stdout: str, stderr: str,
                    timed_out: bool) -> tuple[bool, list[str]]:
    """Phase-2 verdict for `<exe> one-shot`."""
    reasons: list[str] = []
    if timed_out:
        return False, ["binary timed out (hard limit hit)"]
    if rc != 0:
        reasons.append(f"exit code {rc} (expected 0)")
    if parsed.crash_hits:
        reasons.append("crash signature(s): " + ", ".join(parsed.crash_hits))

    if parsed.parse_mode == "table":
        if parsed.total <= 0:
            reasons.append("summary reports Total=0 (no tests executed?)")
        accounted = parsed.passed + parsed.failed + parsed.errored + parsed.skipped
        if accounted != parsed.total:
            reasons.append(
                f"summary inconsistent: Pass+Fail+Error+Skip={accounted} != Total={parsed.total}")
        row_bad = [r["name"] for r in parsed.failing_rows()]
        if parsed.failed > 0:
            reasons.append(f"Fail={parsed.failed}: {row_bad[:10]}")
        if parsed.errored > 0:
            reasons.append(f"Error={parsed.errored}: {row_bad[:10]}")
        # audit rows vs summary (SKIP rows are fine, counted separately)
        row_fail = sum(1 for r in parsed.rows if r["status"] in ("FAIL", "ERROR", "TIMEOUT", "ABORT"))
        if row_bad and row_fail != (parsed.failed + parsed.errored):
            reasons.append(
                f"row audit mismatch: table rows failing={row_fail} vs summary Fail+Error={parsed.failed + parsed.errored}")
        if not parsed.footer and stdout.strip():
            reasons.append("footer '<App> completed.' not found")
        ok = not reasons
        return ok, reasons

    if parsed.parse_mode == "simple":
        if not parsed.simple_pass:
            reasons.append("no 'all checks passed' marker found")
        if any(s in stdout.lower() + stderr.lower()
               for s in ("fail", "error:", "timeout", "abort")) \
                and not parsed.simple_pass:
            reasons.append("failure keyword present without pass marker")
        ok = (rc == 0) and (not reasons) and (not parsed.crash_hits)
        return ok, reasons

    reasons.append("could not parse output: neither summary line nor pass marker")
    return False, reasons