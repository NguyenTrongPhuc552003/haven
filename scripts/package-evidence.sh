#!/usr/bin/env sh
set -eu

./scripts/ci-preflight.sh
./scripts/ci-metadata.sh

mkdir -p build/evidence
cp -r docs/methodology build/evidence/methodology
cp -r docs/porting build/evidence/porting
cp build/ci/metadata.txt build/evidence/metadata.txt

echo "[evidence] package created at build/evidence"
