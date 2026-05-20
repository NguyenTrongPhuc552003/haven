#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys


def parse_kv(path: pathlib.Path) -> dict[str, str]:
  pairs: dict[str, str] = {}
  for line in path.read_text(encoding="utf-8").splitlines():
    if not line or "=" not in line:
      continue
    key, value = line.split("=", 1)
    pairs[key.strip()] = value.strip()
  return pairs


def main() -> int:
  if len(sys.argv) != 3:
    print("usage: compare-metadata.py <old-metadata.txt> <new-metadata.txt>")
    return 2

  old_path = pathlib.Path(sys.argv[1])
  new_path = pathlib.Path(sys.argv[2])

  if not old_path.exists() or not new_path.exists():
    print("error: metadata file not found")
    return 2

  old = parse_kv(old_path)
  new = parse_kv(new_path)
  keys = sorted(set(old.keys()) | set(new.keys()))

  print("[metadata-compare] begin")
  changes = 0
  for key in keys:
    old_value = old.get(key, "<missing>")
    new_value = new.get(key, "<missing>")
    if old_value != new_value:
      changes += 1
      print(f"- {key}: {old_value} -> {new_value}")

  if changes == 0:
    print("[metadata-compare] no differences")
  else:
    print(f"[metadata-compare] differences: {changes}")

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
