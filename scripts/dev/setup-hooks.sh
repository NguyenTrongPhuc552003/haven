#!/usr/bin/env sh
set -eu

git config core.hooksPath .githooks
echo "[hooks] core.hooksPath set to .githooks"
