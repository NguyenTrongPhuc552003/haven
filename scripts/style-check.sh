#!/usr/bin/env sh
set -eu

echo "[style] checking shell scripts with sh -n"
find scripts -type f -name "*.sh" -exec sh -n {} \;

echo "[style] done"
