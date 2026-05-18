#!/usr/bin/env bash
# Wrapper — delegates to scripts/qemu-run.sh
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
exec "$ROOT/scripts/qemu-run.sh" "$@"
