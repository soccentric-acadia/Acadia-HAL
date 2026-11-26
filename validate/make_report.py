#!/usr/bin/env python3
"""make_report.py — re-render a human-readable summary.md from a summary.json
produced by run_validation.py (no need to re-run the tests).

Usage: python3 validate/make_report.py validate/reports/ssh-20250101-120000/summary.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from run_validation import render_markdown  # noqa: E402


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    jpath = Path(sys.argv[1])
    summary = json.loads(jpath.read_text())
    md = render_markdown(summary)
    out = jpath.with_name("summary.md")
    out.write_text(md)
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())