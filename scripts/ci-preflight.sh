#!/usr/bin/env sh
set -eu

echo "[ci] preflight start"

./scripts/build.sh
./scripts/style-check.sh
./scripts/test.sh
./scripts/check-configs.sh
./scripts/check-traceability.sh

echo "[ci] preflight complete"
