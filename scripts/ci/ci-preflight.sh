#!/usr/bin/env sh
set -eu

echo "[ci] preflight start"

./scripts/compile/build.sh
./scripts/dev/style-check.sh
./scripts/dev/test.sh
./scripts/compile/check-configs.sh

echo "[ci] preflight complete"
