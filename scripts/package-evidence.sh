#!/usr/bin/env sh
set -eu

# -----------------------------------------------------------------------
# Argument parsing
# --platform <name>   Select evidence target (default: qemu)
#                     Supported values: qemu, imx95
# -----------------------------------------------------------------------
TARGET_PLATFORM="qemu"
while [ $# -gt 0 ]; do
  case "$1" in
    --platform)
      shift
      TARGET_PLATFORM="${1:?--platform requires an argument}"
      ;;
    *)
      echo "[evidence] unknown argument: $1" >&2
      exit 1
      ;;
  esac
  shift
done

./scripts/ci-preflight.sh
./scripts/ci-metadata.sh

mkdir -p build/evidence
mkdir -p build/evidence/imx95/logs
mkdir -p build/evidence/imx95/metrics
mkdir -p build/evidence/imx95/captures

cp -r docs/methodology build/evidence/methodology
cp -r docs/porting build/evidence/porting
cp build/ci/metadata.txt build/evidence/metadata.txt

# -----------------------------------------------------------------------
# QEMU virtual platform validation
# -----------------------------------------------------------------------
echo "[evidence] running QEMU smoke check"
set +e
./scripts/qemu-smoke.sh
QEMU_SMOKE_EXIT=$?
set -e
if [ "$QEMU_SMOKE_EXIT" -ne 0 ]; then
  if [ "$QEMU_SMOKE_EXIT" -eq 2 ] && [ "$TARGET_PLATFORM" = "imx95" ]; then
    echo "[evidence] qemu smoke skipped for imx95 packaging (qemu unavailable)"
  else
    echo "[evidence] qemu smoke failed (exit=${QEMU_SMOKE_EXIT})"
    exit "$QEMU_SMOKE_EXIT"
  fi
fi
if [ -f build/evidence/qemu-validation.json ]; then
  echo "[evidence] qemu-validation.json included"
fi

# -----------------------------------------------------------------------
# Structured V1/V2/V3 validation transcript
# -----------------------------------------------------------------------
echo "[evidence] running V1/V2/V3 validation transcript"
sh scripts/ci-validate.sh || true   # non-zero warns but does not abort packaging
if [ -f build/evidence/validation-transcript.json ]; then
  echo "[evidence] validation-transcript.json included"
fi

# -----------------------------------------------------------------------
# Capture live test results
# -----------------------------------------------------------------------
echo "[evidence] running test suite to capture results"
set +e
./scripts/test.sh > build/evidence/test-results.txt 2>&1
TEST_EXIT=$?
set -e

PASS_COUNT=$(grep -c '^\[PASS\]' build/evidence/test-results.txt || true)
FAIL_COUNT=$(grep -c '^\[FAIL\]' build/evidence/test-results.txt || true)

if [ "$TEST_EXIT" -ne 0 ]; then
  TEST_STATUS="fail"
else
  TEST_STATUS="pass"
fi

echo "[evidence] tests: ${PASS_COUNT} passed, ${FAIL_COUNT} failed (status: ${TEST_STATUS})"

# -----------------------------------------------------------------------
# Include benchmark baseline and latency results
# -----------------------------------------------------------------------
if [ -f build/benchmarks/baseline.json ]; then
  cp build/benchmarks/baseline.json build/evidence/benchmark-baseline.json
  echo "[evidence] benchmark baseline included"
fi

if [ -f build/benchmarks/isolation-latency.json ]; then
  cp build/benchmarks/isolation-latency.json build/evidence/isolation-latency.json
  echo "[evidence] isolation-latency.json included"
fi

# -----------------------------------------------------------------------
# Regression check against previous summary (if present)
# -----------------------------------------------------------------------
PREV_SUMMARY="build/evidence/summary-previous.json"
if [ -f build/evidence/summary.json ]; then
  cp build/evidence/summary.json "$PREV_SUMMARY"
fi

# -----------------------------------------------------------------------
# Emit structured summary.json
# -----------------------------------------------------------------------
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
PLATFORM=$(uname -srm)

cat > build/evidence/summary.json << SUMMARY_EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "git_branch": "${GIT_BRANCH}",
  "platform": "${PLATFORM}",
  "test_status": "${TEST_STATUS}",
  "tests_passed": ${PASS_COUNT},
  "tests_failed": ${FAIL_COUNT},
  "modules_covered": [
    "stage2",
    "irq_ownership",
    "budget_sched",
    "smmu_dma",
    "timer",
    "iommu_policy",
    "el2_exceptions"
  ],
  "evidence_files": [
    "metadata.txt",
    "test-results.txt",
    "benchmark-baseline.json",
    "isolation-latency.json",
    "qemu-validation.json",
    "validation-transcript.json",
    "summary.json"
  ]
}
SUMMARY_EOF

echo "[evidence] summary.json written"

# Run regression comparison if a previous summary was preserved.
if [ -f "$PREV_SUMMARY" ]; then
  echo "[evidence] comparing against previous summary"
  python3 scripts/compare-evidence.py "$PREV_SUMMARY" build/evidence/summary.json || {
    echo "[evidence] WARNING: regressions detected — review compare-evidence output above"
  }
fi

# Write a placeholder README into the i.MX95 evidence directory so the
# directory structure is self-documenting even before hardware runs occur.
if [ ! -f build/evidence/imx95/README.md ]; then
  cat > build/evidence/imx95/README.md << 'EOF'
# i.MX95 Evidence Bundle

Place campaign artefacts here before running:
  ./scripts/package-evidence.sh --platform imx95

- logs/      : UART boot and runtime logs
- metrics/   : latency and deadline CSV files
- captures/  : SMMU dumps, fault audit logs, serial captures
EOF
fi

# -----------------------------------------------------------------------
# i.MX95 board evidence (included when --platform imx95 is passed)
# -----------------------------------------------------------------------
IMX95_EVIDENCE_DIR="build/evidence/imx95"
IMX95_EVIDENCE_COUNT=0

if [ "$TARGET_PLATFORM" = "imx95" ]; then
  echo "[evidence] collecting i.MX95 board evidence from ${IMX95_EVIDENCE_DIR}"
  if [ -d "${IMX95_EVIDENCE_DIR}/logs" ]; then
    IMX95_EVIDENCE_COUNT=$(find "${IMX95_EVIDENCE_DIR}/logs" -type f | wc -l | tr -d ' ')
    echo "[evidence] imx95: ${IMX95_EVIDENCE_COUNT} log file(s) found"
  else
    echo "[evidence] WARNING: ${IMX95_EVIDENCE_DIR}/logs not populated; no hardware runs captured yet"
  fi

  # Update summary.json with an imx95_evidence_count field.
  # Append before the closing brace using a portable sed replacement.
  if [ -f build/evidence/summary.json ]; then
    # Remove last closing brace and re-add with the new field appended.
    sed -e '$d' -e '$ s/$/,/' build/evidence/summary.json > build/evidence/summary.json.tmp
    cat >> build/evidence/summary.json.tmp << IMXEOF
  "imx95_evidence_count": ${IMX95_EVIDENCE_COUNT}
}
IMXEOF
    mv build/evidence/summary.json.tmp build/evidence/summary.json
    echo "[evidence] imx95_evidence_count written to summary.json"
  fi

  # Produce a SHA256 manifest for the i.MX95 evidence directory.
  if command -v sha256sum > /dev/null 2>&1; then
    find "${IMX95_EVIDENCE_DIR}" -type f ! -name manifest.sha256 | sort | xargs sha256sum \
      > "${IMX95_EVIDENCE_DIR}/manifest.sha256"
    echo "[evidence] imx95 manifest.sha256 written"
  fi
fi

echo "[evidence] package created at build/evidence"
