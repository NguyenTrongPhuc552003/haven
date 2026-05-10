#!/usr/bin/env sh
set -eu

mkdir -p build/ci

{
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "git_commit=$(git rev-parse HEAD)"
  echo "git_branch=$(git rev-parse --abbrev-ref HEAD)"
  echo "cc=${CC:-cc}"
  echo "kernel=$(uname -srm)"
} > build/ci/metadata.txt

echo "[ci] metadata written to build/ci/metadata.txt"
