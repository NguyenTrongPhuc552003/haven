#!/usr/bin/env sh
set -eu

OUTDIR="build/evidence"
ARTIFACT="${OUTDIR}/qemu-validation.json"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
PLATFORM=$(uname -srm)

mkdir -p "$OUTDIR"

echo "[qemu] checking haven.bin availability"

HAVEN_BIN="build/haven.bin"
if [ ! -f "$HAVEN_BIN" ]; then
  echo "[qemu] $HAVEN_BIN not found - recording skipped status"
  cat > "$ARTIFACT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "platform": "${PLATFORM}",
  "qemu_available": null,
  "qemu_version": null,
  "validation_status": "skipped",
  "note": "build/haven.bin not found; run 'make ARCH=arm64 all' to build the hypervisor image before running QEMU validation"
}
EOF
  echo "[qemu] artifact written to ${ARTIFACT}"
  echo "[qemu] exiting with code 0 (binary not built - skipped)"
  exit 0
fi

echo "[qemu] checking qemu-system-aarch64 availability"

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
  echo "[qemu] qemu-system-aarch64 not found - recording unavailable status"
  cat > "$ARTIFACT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "platform": "${PLATFORM}",
  "qemu_available": false,
  "qemu_version": null,
  "validation_status": "skipped",
  "note": "qemu-system-aarch64 not found on this host; install QEMU to enable virtual platform validation"
}
EOF
  echo "[qemu] artifact written to ${ARTIFACT}"
  echo "[qemu] exiting with code 0 (QEMU unavailable - skipped)"
  exit 0
fi

QEMU_VERSION=$(qemu-system-aarch64 --version 2>&1 | head -n 1)
echo "[qemu] found: ${QEMU_VERSION}"

echo "[qemu] launching ARM64 image under QEMU"
SMOKE_LOG="${OUTDIR}/qemu-smoke.log"
SUITE_STATUS="fail"
BOOT_MARKER="Haven hypervisor starting"

set +e
timeout 20s ./scripts/qemu-run.sh > "$SMOKE_LOG" 2>&1
QEMU_RC=$?
set -e

if [ "$QEMU_RC" -eq 124 ]; then
  if grep -q "$BOOT_MARKER" "$SMOKE_LOG"; then
    SUITE_STATUS="pass"
  fi
elif [ "$QEMU_RC" -eq 0 ]; then
  SUITE_STATUS="pass"
fi

echo "[qemu] smoke: status=${SUITE_STATUS} (rc=${QEMU_RC})"

cat > "$ARTIFACT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "platform": "${PLATFORM}",
  "qemu_available": true,
  "qemu_version": "${QEMU_VERSION}",
  "validation_status": "${SUITE_STATUS}",
  "qemu_exit_code": ${QEMU_RC}
}
EOF

echo "[qemu] artifact written to ${ARTIFACT}"

if [ "$SUITE_STATUS" != "pass" ]; then
  echo "[qemu] validation FAILED - see ${SMOKE_LOG}"
  exit 1
fi

echo "[qemu] smoke check passed"
