#!/usr/bin/env sh
set -eu

echo "[qemu] checking qemu-system-aarch64 availability"

if command -v qemu-system-aarch64 >/dev/null 2>&1; then
  qemu-system-aarch64 --version | head -n 1
  echo "[qemu] smoke check passed"
  exit 0
fi

echo "[qemu] qemu-system-aarch64 not found"
exit 1
