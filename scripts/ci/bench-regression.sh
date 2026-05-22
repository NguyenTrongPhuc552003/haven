#!/usr/bin/env bash
# scripts/ci/bench-regression.sh
#
# Benchmark regression check for Haven isolation hot-path latencies.
#
# Reads all JSON files from build/benchmarks/ (excluding the baseline itself)
# and compares each named benchmark's mean_ns against the corresponding entry
# in build/benchmarks/latency-baseline.json.
#
# Exit 0 if every benchmark is within tolerance.
# Exit 1 if any benchmark regresses beyond the threshold.
#
# Environment variables:
#   BASELINE                 Path to the baseline JSON (default: build/benchmarks/latency-baseline.json)
#   RESULTS_DIR              Directory containing benchmark result JSONs (default: build/benchmarks)
#   REGRESSION_THRESHOLD_PCT Integer percentage tolerance (default: 10).
#                            A benchmark is a regression only when BOTH conditions hold:
#                              1. mean_ns > baseline_ns * (1 + threshold/100)
#                              2. absolute delta (mean_ns - baseline_ns) > MIN_ABS_NS
#   MIN_ABS_NS               Minimum absolute increase (ns) before the percentage check
#                            applies (default: 8).  Suppresses false positives from
#                            timer-granularity noise (~30 ns) in virtualised CI runners.
#
# Dependencies: python3 (standard library only — no third-party packages needed).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BASELINE="${BASELINE:-${ROOT}/tests/benchmarks/latency-baseline.json}"
RESULTS_DIR="${RESULTS_DIR:-${ROOT}/build/benchmarks}"
REGRESSION_THRESHOLD_PCT="${REGRESSION_THRESHOLD_PCT:-10}"
MIN_ABS_NS="${MIN_ABS_NS:-8}"

# -----------------------------------------------------------------------
# Preflight
# -----------------------------------------------------------------------

if ! command -v python3 &>/dev/null; then
    echo "[bench-regression] ERROR: python3 is required but not found." >&2
    exit 1
fi

if [ ! -f "${BASELINE}" ]; then
    echo "[bench-regression] ERROR: baseline not found: ${BASELINE}" >&2
    echo "  Run scripts/ci/bench-update-baseline.sh to create it." >&2
    exit 1
fi

echo "[bench-regression] threshold=${REGRESSION_THRESHOLD_PCT}% min_abs=${MIN_ABS_NS}ns baseline=${BASELINE}"

# -----------------------------------------------------------------------
# Python3 comparison script (executed inline for portability)
# -----------------------------------------------------------------------
python3 - "${BASELINE}" "${RESULTS_DIR}" "${REGRESSION_THRESHOLD_PCT}" "${MIN_ABS_NS}" << 'PYEOF'
import json
import os
import sys

baseline_path = sys.argv[1]
results_dir   = sys.argv[2]
threshold_pct = int(sys.argv[3])
min_abs_ns    = int(sys.argv[4]) if len(sys.argv) > 4 else 8

# Load baseline: flat dict {name: mean_ns}
with open(baseline_path) as f:
    bl_data = json.load(f)

bl_map = {}
for entry in bl_data.get("benchmarks", []):
    key = entry.get("name") or entry.get("benchmark")
    if key:
        bl_map[key] = entry["mean_ns"]

if not bl_map:
    print("[bench-regression] WARNING: baseline contains no benchmark entries",
          file=sys.stderr)
    sys.exit(0)

passed  = []
failed  = []
skipped = []

# Scan all JSON files in results_dir, skip the baseline itself
for fname in sorted(os.listdir(results_dir)):
    if not fname.endswith(".json"):
        continue
    fpath = os.path.join(results_dir, fname)
    if os.path.realpath(fpath) == os.path.realpath(baseline_path):
        continue

    try:
        with open(fpath) as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError):
        print(f"[bench-regression] WARNING: cannot parse {fname}, skipping",
              file=sys.stderr)
        continue

    results = data.get("results", [])
    for entry in results:
        name = entry.get("name") or entry.get("benchmark")
        if not name:
            continue
        measured = entry.get("mean_ns")
        if measured is None:
            continue

        if name not in bl_map:
            skipped.append((fname, name))
            continue

        baseline_val = bl_map[name]
        if baseline_val == 0:
            # Baseline is 0 ns on host — skip ratio check
            skipped.append((fname, name))
            continue

        ratio_pct = ((measured - baseline_val) / baseline_val) * 100.0
        limit_ns  = int(baseline_val * (1 + threshold_pct / 100.0))
        abs_delta = measured - baseline_val

        # A regression requires BOTH a percentage increase above threshold AND
        # an absolute delta above min_abs_ns.  The absolute guard suppresses
        # false positives from timer-granularity noise in virtualised CI runners.
        if measured > limit_ns and abs_delta > min_abs_ns:
            failed.append((fname, name, baseline_val, measured, ratio_pct))
        else:
            passed.append((fname, name, baseline_val, measured, ratio_pct))

# ---- Report ----
print(f"\n{'Benchmark':<50} {'Baseline':>10} {'Measured':>10} {'Delta':>8}  {'Result'}")
print("-" * 90)

for fname, name, bl, m, pct in passed:
    tag = "PASS"
    sign = "+" if pct >= 0 else ""
    print(f"  {name:<48} {bl:>10} {m:>10} {sign}{pct:>6.1f}%   {tag}")

for fname, name, bl, m, pct in failed:
    tag = f"FAIL (>{threshold_pct}%)"
    sign = "+"
    print(f"  {name:<48} {bl:>10} {m:>10} {sign}{pct:>6.1f}%   {tag}")

for fname, name in skipped:
    print(f"  {name:<48} {'N/A':>10} {'N/A':>10} {'N/A':>7}    SKIP (no baseline)")

print()
print(f"[bench-regression] passed={len(passed)}  failed={len(failed)}  skipped={len(skipped)}")

if failed:
    print(f"[bench-regression] REGRESSION DETECTED — {len(failed)} benchmark(s) exceed "
          f"{threshold_pct}% threshold", file=sys.stderr)
    sys.exit(1)

print("[bench-regression] all benchmarks within tolerance")
PYEOF
