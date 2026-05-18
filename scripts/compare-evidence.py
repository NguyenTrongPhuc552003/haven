#!/usr/bin/env python3
"""
Compare two haven evidence summary.json files for regressions.

Usage:
    compare-evidence.py <baseline-summary.json> <current-summary.json>

Exit codes:
    0  No regressions detected.
    1  One or more regressions detected (test count dropped, status worsened,
       modules removed, or new failures introduced).
    2  Usage error or file not found.
"""
from __future__ import annotations

import json
import pathlib
import sys


def load(path: pathlib.Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"error: cannot read {path}: {exc}")
        sys.exit(2)


def compare(baseline: dict, current: dict) -> int:
    """Return number of regressions found."""
    regressions = 0

    print("[compare-evidence] begin")
    print(f"  baseline : {baseline.get('timestamp_utc', '?')} "
          f"commit={baseline.get('git_commit', '?')[:12]}")
    print(f"  current  : {current.get('timestamp_utc', '?')} "
          f"commit={current.get('git_commit', '?')[:12]}")

    # Status regression (pass → fail).
    b_status = baseline.get("test_status", "unknown")
    c_status = current.get("test_status", "unknown")
    if b_status == "pass" and c_status != "pass":
        print(f"[REGRESSION] test_status: {b_status} -> {c_status}")
        regressions += 1
    else:
        print(f"[OK] test_status: {c_status}")

    # Test count regression.
    b_passed = int(baseline.get("tests_passed", 0))
    c_passed = int(current.get("tests_passed", 0))
    if c_passed < b_passed:
        print(f"[REGRESSION] tests_passed dropped: {b_passed} -> {c_passed} "
              f"(-{b_passed - c_passed})")
        regressions += 1
    elif c_passed > b_passed:
        print(f"[OK] tests_passed increased: {b_passed} -> {c_passed} "
              f"(+{c_passed - b_passed})")
    else:
        print(f"[OK] tests_passed unchanged: {c_passed}")

    # New failures introduced.
    b_failed = int(baseline.get("tests_failed", 0))
    c_failed = int(current.get("tests_failed", 0))
    if c_failed > b_failed:
        print(f"[REGRESSION] tests_failed increased: {b_failed} -> {c_failed} "
              f"(+{c_failed - b_failed})")
        regressions += 1
    else:
        print(f"[OK] tests_failed: {c_failed}")

    # Module coverage regression (modules removed).
    b_modules = set(baseline.get("modules_covered", []))
    c_modules = set(current.get("modules_covered", []))
    removed = b_modules - c_modules
    added = c_modules - b_modules
    if removed:
        print(f"[REGRESSION] modules removed: {sorted(removed)}")
        regressions += 1
    if added:
        print(f"[OK] modules added: {sorted(added)}")
    if not removed and not added:
        print(f"[OK] module coverage unchanged ({len(c_modules)} modules)")

    # Platform change (informational, not a regression).
    b_platform = baseline.get("platform", "")
    c_platform = current.get("platform", "")
    if b_platform != c_platform:
        print(f"[INFO] platform changed: {b_platform!r} -> {c_platform!r}")

    print(f"[compare-evidence] done — {regressions} regression(s) found")
    return regressions


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compare-evidence.py <baseline.json> <current.json>")
        return 2

    baseline_path = pathlib.Path(sys.argv[1])
    current_path = pathlib.Path(sys.argv[2])

    if not baseline_path.exists():
        print(f"error: baseline not found: {baseline_path}")
        return 2
    if not current_path.exists():
        print(f"error: current summary not found: {current_path}")
        return 2

    baseline = load(baseline_path)
    current = load(current_path)

    regressions = compare(baseline, current)
    return 1 if regressions > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
