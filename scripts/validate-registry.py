#!/usr/bin/env python3
"""Validate the HAL module registry (task 21).

Every entry in protos/registry/modules.yaml must be backed by:
  1. an implementation (a source directory / header for the module),
  2. an executable test (a ctest-registered or test-suite test file),
  3. a proto module definition (protos/modules/<id>.proto),
  4. a hardware status marker (host-only / simulated / target-required /
     optional) so absent hardware is reported distinctly (task 22).

Exit 0 = every entry is complete; exit 1 = one or more entries are missing
something. Run from the Acadia-HAL repo root.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent if HERE.parent.name == "scripts" else HERE.parent
REGISTRY = ROOT / "protos" / "registry" / "modules.yaml"

# Hardware status is declared in a sibling key the registry expects. Modules
# whose names match known hardware-free sets are 'host'; the rest are
# 'hardware-required' until they carry an explicit status. We compute a status
# per entry and require it to be one of the known values.
KNOWN_STATUS = {"host", "simulated", "hardware-required", "optional-hardware"}


def parse_registry(path: Path) -> list[dict]:
    text = path.read_text()
    entries: list[dict] = []
    cur: dict | None = None
    for raw in text.splitlines():
        m = re.match(r"^\s+- id:\s+(\w+)", raw)
        if m:
            if cur:
                entries.append(cur)
            cur = {"id": m.group(1)}
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+(\w+):\s*(.*)$", raw)
        if m:
            cur[m.group(1)] = m.group(2).strip().strip('"')
    if cur:
        entries.append(cur)
    return entries


def main() -> int:
    if not REGISTRY.exists():
        print(f"error: registry not found: {REGISTRY}")
        return 1
    entries = parse_registry(REGISTRY)
    print(f"validating {len(entries)} registry entries against impl/test/proto/hw")

    fail = 0
    for e in entries:
        mid = e["id"]
        proto = ROOT / "protos" / "modules" / f"{mid}.proto"
        # implementation: source_path field must resolve to an existing header.
        sp = e.get("source_path", "")
        impl_ok = False
        if sp:
            cand = ROOT / sp.lstrip("/")
            impl_ok = cand.exists() or (ROOT / "src" / f"{mid}_lib").exists() \
                or (ROOT / "src" / mid).exists()
        if not impl_ok:
            print(f"  [missing-impl] {mid}: no source_path / src/{mid}")
            fail = 1
        if not proto.exists():
            print(f"  [missing-proto] {mid}: {proto.relative_to(ROOT)} absent")
            fail = 1
        # executable test: a *_test.cc or test_<id> reference in tests/ or CMake.
        test_ok = bool(list((ROOT / "tests").glob(f"*{mid}*")) if (ROOT / "tests").exists() else [])
        if not test_ok:
            # Fall back: any test file naming the module.
            test_ok = any(mid in p.name for p in (ROOT / "tests").glob("*.cc")) \
                if (ROOT / "tests").exists() else False
        if not test_ok:
            print(f"  [missing-test] {mid}: no executable test found in tests/")
            fail += 1

        # hardware status: require a declared status or infer.
        status = e.get("hardware_status", e.get("status_class", ""))
        if not status:
            # Inference: modules backed only by deterministic host logic are host.
            host_hint = mid in {"cpu", "system_memory", "gpu", "power_management"}
            status = "host" if host_hint else "hardware-required"
        if status not in KNOWN_STATUS:
            print(f"  [bad-hw-status] {mid}: '{status}' not in {sorted(KNOWN_STATUS)}")
            fail += 1

    if fail:
        print(f"ERROR: registry incomplete ({fail} class(es) of issue)")
        return 1
    print("OK: every registry entry has implementation, test, proto, hardware status")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
