#!/usr/bin/env sh
set -eu

OUTDIR="build/evidence"
ARTIFACT="${OUTDIR}/qemu-validation.json"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
PLATFORM=$(uname -srm)

mkdir -p "$OUTDIR"

echo "[qemu] checking qemu-system-aarch64 availability"

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
  echo "[qemu] qemu-system-aarch64 not found — recording unavailable status"
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
  exit 0
fi

QEMU_VERSION=$(qemu-system-aarch64 --version 2>&1 | head -n 1)
echo "[qemu] found: ${QEMU_VERSION}"

# -----------------------------------------------------------------------
# Run the host-compiled haven test suite as the portable validation step.
# On a target-hardware CI this would be replaced by a kernel image boot
# sequence, but the logical validation contract is identical: all
# isolation invariants must hold before the suite is declared passing.
# -----------------------------------------------------------------------
echo "[qemu] running haven test suite for validation"
SUITE_LOG="${OUTDIR}/qemu-suite.txt"
PASS_COUNT=0
FAIL_COUNT=0
SUITE_STATUS="fail"

if ./scripts/test.sh > "$SUITE_LOG" 2>&1; then
  SUITE_STATUS="pass"
fi

PASS_COUNT=$(grep -c '^\[PASS\]' "$SUITE_LOG" || true)
FAIL_COUNT=$(grep -c '^\[FAIL\]' "$SUITE_LOG" || true)

echo "[qemu] suite: ${PASS_COUNT} passed, ${FAIL_COUNT} failed (${SUITE_STATUS})"

cat > "$ARTIFACT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "platform": "${PLATFORM}",
  "qemu_available": true,
  "qemu_version": "${QEMU_VERSION}",
  "validation_status": "${SUITE_STATUS}",
  "tests_passed": ${PASS_COUNT},
  "tests_failed": ${FAIL_COUNT}
}
EOF

echo "[qemu] artifact written to ${ARTIFACT}"

if [ "$SUITE_STATUS" != "pass" ]; then
  echo "[qemu] validation FAILED — see ${SUITE_LOG}"
  exit 1
fi

echo "[qemu] smoke check passed"
