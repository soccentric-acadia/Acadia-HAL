#!/usr/bin/env bash
# verdict-integrity-gate.sh — X-002: verdict-integrity test gate.
#
# Scans every HAL test implementation for the false-PASS patterns the audit
# (HAL-037/HAL-038) found:
#   1. tests with no code path that can yield FAIL/ERROR/SKIPPED, and
#   2. tests that return PASS when the hardware they probe is absent.
#
# The gate enforces MONOTONIC PROGRESS: the count of unverifiable tests may
# never exceed the committed baseline (tests/verdict-gate-baseline.txt). The
# per-test items (HAL-101…, HAL-401…) reduce the count; this gate fails CI the
# moment anyone adds a new always-PASS test.
#
# Detection heuristic (documented, best-effort):
#   A test function is "verifiable" when its body contains at least one
#   assignment of a non-PASS status (Status::FAIL / Status::ERROR /
#   Status::SKIPPED / Status::TIMEOUT / Status::ABORTED) OR a `throw`/set_error
#   path wired to the verdict. Bodies that compute a boolean (e.g. xxxFound)
#   but never assign it to `passed` are counted as unverifiable.

set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASELINE="$ROOT/tests/verdict-gate-baseline.txt"

python3 - "$ROOT" <<'PY'
import re, sys, pathlib

root = pathlib.Path(sys.argv[1])
src = root / "src"
unverifiable = []
total = 0

func_re = re.compile(
    r"(?:TestOutput|hal::TestOutput)\s+(\w+::)?(\w+)\s*\(\s*\)\s*\n?\{", re.M)
for path in sorted(src.rglob("*.cpp")):
    text = path.read_text(errors="ignore")
    for m in re.finditer(
            r"(?:TestOutput)\s+\w+::(test\w+)\s*\(\)\s*\{", text):
        name = m.group(1)
        total += 1
        # Extract the function body by brace matching from the match end.
        i = m.end() - 1
        depth = 0
        end = i
        while i < len(text):
            if text[i] == '{': depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    end = i
                    break
            i += 1
        body = text[m.end():end]
        # A test is verifiable when its body can produce a non-PASS verdict:
        # an explicit non-PASS status assignment, a throw, or a failure
        # propagation from a helper call.
        has_fail_path = re.search(
            r"Status::(FAIL|ERROR|SKIPPED|TIMEOUT|ABORTED)|hal::Status::(FAIL|ERROR|SKIPPED|TIMEOUT|ABORTED)"
            r"|throw |runSelectedTests|runAllTests|runSingleTest", body)
        # `passed ? PASS : FAIL` with a `passed` never set false in the body is
        # the classic always-PASS shape.
        assigns_passed_false = re.search(r"passed\s*=\s*false|passed\s*=\s*\w+Found|passed\s*=\s*\w+|!passed", body)
        if not has_fail_path and not assigns_passed_false:
            unverifiable.append(f"{path.relative_to(root)}:{name}")
        elif has_fail_path and not assigns_passed_false and re.search(
                r"Status::PASS|hal::Status::PASS", body) and not re.search(
                r"Status::(FAIL|ERROR|SKIPPED|TIMEOUT|ABORTED)\s*[;,)]|\bthrow\b|SKIPPED", body):
            unverifiable.append(f"{path.relative_to(root)}:{name}")

unverifiable = sorted(set(unverifiable))
baseline_path = root / "tests" / "verdict-gate-baseline.txt"
baseline = 0
if baseline_path.exists():
    for raw in baseline_path.read_text().splitlines():
        raw = raw.strip()
        if raw and not raw.startswith("#"):
            baseline = int(raw.split()[0])
            break

print(f"verdict-integrity: {total} test functions scanned, "
      f"{len(unverifiable)} unverifiable (baseline {baseline})")
if len(unverifiable) > baseline:
    print("verdict-integrity: FAILED — the number of always-PASS tests INCREASED "
          f"({len(unverifiable)} > baseline {baseline}). New tests must define "
          "measurable pass criteria (X-002). Offenders:")
    for line in unverifiable:
        print("  " + line)
    sys.exit(1)
print("verdict-integrity: PASSED (monotonic progress; reduce via HAL-037/HAL-038 items)")
PY