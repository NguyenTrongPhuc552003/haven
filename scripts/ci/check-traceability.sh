#!/usr/bin/env sh
# check-traceability.sh - Chapter traceability artifact verification
#
# Verifies that every repository deliverable referenced in the
# docs/methodology/CHAPTER_TRACEABILITY.md matrix exists on disk.
# Produces a structured JSON report at build/evidence/traceability-report.json.
#
# Exit codes:
#   0  All mandatory artifacts present.
#   1  One or more mandatory artifacts missing.
set -eu

OUTDIR="build/evidence"
REPORT="${OUTDIR}/traceability-report.json"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")

mkdir -p "$OUTDIR"

PASS=0
FAIL=0
RESULTS=""

# -----------------------------------------------------------------------
# Helper: check a single artifact path (file or directory)
# $1 = chapter label  $2 = artifact path  $3 = description
# -----------------------------------------------------------------------
check_artifact() {
  CHAPTER="$1"
  ARTIFACT="$2"
  DESC="$3"

  if [ -e "$ARTIFACT" ]; then
    STATUS="present"
    PASS=$((PASS + 1))
    printf "[TRACE] %-10s PRESENT  %s\n" "$CHAPTER" "$ARTIFACT"
  else
    STATUS="missing"
    FAIL=$((FAIL + 1))
    printf "[TRACE] %-10s MISSING  %s  (%s)\n" "$CHAPTER" "$ARTIFACT" "$DESC"
  fi

  if [ -n "$RESULTS" ]; then
    RESULTS="${RESULTS},"
  fi
  RESULTS="${RESULTS}
    { \"chapter\": \"${CHAPTER}\", \"artifact\": \"${ARTIFACT}\", \"description\": \"${DESC}\", \"status\": \"${STATUS}\" }"
}

echo "[traceability] checking chapter artifact matrix"

# -----------------------------------------------------------------------
# Chapter 1: Problem and Motivation
# -----------------------------------------------------------------------
check_artifact "Ch1" "README.md"                              "project overview"
check_artifact "Ch1" "docs/architecture/OVERVIEW.md"         "architecture overview"

# -----------------------------------------------------------------------
# Chapter 2: Related Work
# -----------------------------------------------------------------------
check_artifact "Ch2" "docs/architecture/ISOLATION_MODEL.md"  "isolation model analysis"

# -----------------------------------------------------------------------
# Chapter 3: System Design
# -----------------------------------------------------------------------
check_artifact "Ch3" "include/haven/stage2.h"                "stage-2 API contract"
check_artifact "Ch3" "include/haven/irq_ownership.h"         "IRQ ownership API contract"
check_artifact "Ch3" "include/haven/budget_sched.h"          "budget scheduler API contract"
check_artifact "Ch3" "include/haven/smmu.h"                  "SMMU/DMA API contract"
check_artifact "Ch3" "include/haven/timer.h"                 "timer API contract"
check_artifact "Ch3" "include/haven/iommu.h"                 "IOMMU policy API contract"
check_artifact "Ch3" "include/haven/el2_exceptions.h"        "EL2 exceptions API contract"
check_artifact "Ch3" "docs/architecture/REPOSITORY_STRUCTURE.md" "repository structure"

# -----------------------------------------------------------------------
# Chapter 4: Spatial Isolation
# -----------------------------------------------------------------------
check_artifact "Ch4" "src/core/mm/stage2.c"                  "stage-2 mapping implementation"
check_artifact "Ch4" "src/core/iommu/iommu_policy.c"         "IOMMU group ownership"
check_artifact "Ch4" "src/core/dma/smmu.c"                   "SMMU DMA enforcement"
check_artifact "Ch4" "tests/isolation/test_spatial_isolation.c" "spatial isolation evidence tests"
check_artifact "Ch4" "tests/unit/test_core_stubs.c"          "stage-2/IRQ unit contracts"
check_artifact "Ch4" "tests/unit/test_smmu_dma.c"            "SMMU unit contracts"
check_artifact "Ch4" "tests/unit/test_iommu_policy.c"        "IOMMU unit contracts"

# -----------------------------------------------------------------------
# Chapter 5: Temporal Isolation
# -----------------------------------------------------------------------
check_artifact "Ch5" "src/core/sched/budget.c"               "budget accounting implementation"
check_artifact "Ch5" "src/core/irq/ownership.c"              "IRQ ownership implementation"
check_artifact "Ch5" "src/core/time/timer.c"                 "per-partition timer implementation"
check_artifact "Ch5" "tests/isolation/test_temporal_isolation.c" "temporal isolation evidence tests"
check_artifact "Ch5" "tests/unit/test_timer.c"               "timer unit contracts"
check_artifact "Ch5" "tests/benchmarks/bench_isolation_latency.c" "hot-path latency benchmark"

# -----------------------------------------------------------------------
# Chapter 6: Implementation (Platform)
# -----------------------------------------------------------------------
check_artifact "Ch6" "configs/arm64/qemu-virt.yaml"          "QEMU virtual platform config"
check_artifact "Ch6" "configs/arm64/imx95-devkit.yaml"       "i.MX95 board config"
check_artifact "Ch6" "configs/arm64/imx8qm-mek.yaml"         "secondary platform config"
check_artifact "Ch6" "src/core/exc/el2_exceptions.c"         "EL2 exception handling"
check_artifact "Ch6" "docs/porting/IMX95_HARDWARE_SETUP.md"  "i.MX95 hardware setup runbook"
check_artifact "Ch6" "docs/porting/IMX95_VALIDATION_RUNBOOK.md" "i.MX95 validation runbook"

# -----------------------------------------------------------------------
# Chapter 7: Evaluation
# -----------------------------------------------------------------------
check_artifact "Ch7" "docs/methodology/EVALUATION_PLAN.md"   "evaluation plan and protocol"
check_artifact "Ch7" "tests/integration/test_isolation_flow.c"   "integration happy-path flow"
check_artifact "Ch7" "tests/integration/test_isolation_negative.c" "integration negative flow"
check_artifact "Ch7" "tests/selftests/test_hypervisor_invariants.c" "module invariant self-tests"
check_artifact "Ch7" "tests/demos/demo_two_partition.c"       "two-partition coexistence demo"
check_artifact "Ch7" "build/benchmarks/isolation-latency.json" "measured hot-path latency data"
check_artifact "Ch7" "tests/integration/test_fault_injection.c" "fault-injection pass/fail matrix"
check_artifact "Ch7" "build/evidence/summary.json"           "evidence bundle summary"

# -----------------------------------------------------------------------
# Chapter 8: Conclusions and Future Work
# -----------------------------------------------------------------------
check_artifact "Ch8" "docs/safety/ASSUMPTIONS.md"            "system assumptions"
check_artifact "Ch8" "docs/safety/THREAT_MODEL.md"           "threat model"
check_artifact "Ch8" "docs/roadmap/MILESTONES.md"            "milestone plan"
check_artifact "Ch8" "CHANGELOG.md"                          "changelog"

# -----------------------------------------------------------------------
# Emit report
# -----------------------------------------------------------------------
OVERALL="pass"
if [ "$FAIL" -gt 0 ]; then
  OVERALL="fail"
fi

cat > "$REPORT" << EOF
{
  "timestamp_utc": "${TIMESTAMP}",
  "git_commit": "${GIT_COMMIT}",
  "overall": "${OVERALL}",
  "artifacts_present": ${PASS},
  "artifacts_missing": ${FAIL},
  "artifacts": [${RESULTS}
  ]
}
EOF

echo "[traceability] ${PASS} present, ${FAIL} missing - overall: ${OVERALL}"
echo "[traceability] report written to ${REPORT}"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
