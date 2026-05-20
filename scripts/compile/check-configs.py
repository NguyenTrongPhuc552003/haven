#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys


REQUIRED_TOP_LEVEL = ["target", "description", "cpus", "memory", "interrupts", "timing"]
REQUIRED_TIMING = ["period_us", "budgets_us"]


def has_top_level_key(text: str, key: str) -> bool:
  pattern = rf"^\s*{re.escape(key)}\s*:"
  return re.search(pattern, text, flags=re.MULTILINE) is not None


def validate_file(path: pathlib.Path) -> list[str]:
  content = path.read_text(encoding="utf-8")
  errors: list[str] = []

  for key in REQUIRED_TOP_LEVEL:
    if not has_top_level_key(content, key):
      errors.append(f"missing top-level key '{key}'")

  timing_block_match = re.search(r"^timing:\n((?:^[ ]{2}.+\n?)*)", content, flags=re.MULTILINE)
  if timing_block_match is None:
    errors.append("missing timing block")
  else:
    timing_block = timing_block_match.group(1)
    for key in REQUIRED_TIMING:
      pattern = rf"^[ ]{{2}}{re.escape(key)}\s*:"
      if re.search(pattern, timing_block, flags=re.MULTILINE) is None:
        errors.append(f"missing timing.{key}")

    period_match = re.search(r"^[ ]{2}period_us:\s*(\d+)\s*$", timing_block, flags=re.MULTILINE)
    if period_match:
      period_us = int(period_match.group(1))
      budget_values = [int(v) for v in re.findall(r"^[ ]{4}[A-Za-z0-9_]+:\s*(\d+)\s*$", timing_block, flags=re.MULTILINE)]
      if budget_values and sum(budget_values) > period_us:
        errors.append(
            f"timing budget sum {sum(budget_values)} exceeds period_us {period_us}"
        )

  return errors


def main() -> int:
  root = pathlib.Path("configs")
  candidates = sorted(root.rglob("*.yaml"))
  if not candidates:
    print("[config-check] no yaml files found under configs/")
    return 1

  print(f"[config-check] validating {len(candidates)} config file(s)")
  failures = 0
  for path in candidates:
    errors = validate_file(path)
    if errors:
      failures += 1
      print(f"[config-check] FAIL {path}")
      for err in errors:
        print(f"  - {err}")
    else:
      print(f"[config-check] PASS {path}")

  if failures:
    return 2

  return 0


if __name__ == "__main__":
  sys.exit(main())
