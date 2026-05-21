#!/usr/bin/env bash
# scripts/ci/bench-update-baseline.sh
#
# Regenerate build/benchmarks/latency-baseline.json from the latest
# benchmark run results.  Run this after a deliberate performance change
# to anchor the new expected values.
#
# Usage:
#   ./scripts/ci/bench-update-baseline.sh
#
# The script:
#   1. Runs the full benchmark suite (scripts/dev/test.sh).
#   2. Merges mean_ns values from all build/benchmarks/*.json into
#      build/benchmarks/latency-baseline.json.
#
# Requires: python3, cc (host build).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BASELINE="${ROOT}/tests/benchmarks/latency-baseline.json"
RESULTS_DIR="${ROOT}/build/benchmarks"

echo "[bench-update-baseline] running benchmark suite..."
"${ROOT}/scripts/dev/test.sh" 2>&1 | grep -E "^\[bench\]|\[test\]" || true

echo "[bench-update-baseline] collecting results from ${RESULTS_DIR}/*.json"

python3 - "${RESULTS_DIR}" "${BASELINE}" << 'PYEOF'
import json
import os
import sys
import datetime

results_dir   = sys.argv[1]
baseline_path = sys.argv[2]

collected = {}

for fname in sorted(os.listdir(results_dir)):
    if not fname.endswith(".json"):
        continue
    fpath = os.path.join(results_dir, fname)
    if "baseline" in fname:
        continue

    try:
        with open(fpath) as f:
            data = json.load(f)
    except (json.JSONDecodeError, OSError):
        print(f"  WARNING: cannot parse {fname}, skipping", file=sys.stderr)
        continue

    for entry in data.get("results", []):
        name = entry.get("name") or entry.get("benchmark")
        mean = entry.get("mean_ns")
        if name and mean is not None:
            if name not in collected:
                collected[name] = {"name": name, "mean_ns": mean}
                print(f"  collected: {name} = {mean} ns")

output = {
    "_description": "Haven isolation hot-path latency baseline for regression gating.",
    "_usage": "Run scripts/ci/bench-regression.sh to compare build/benchmarks/*.json against this file.",
    "_update": "Run scripts/ci/bench-update-baseline.sh to refresh from current measurements.",
    "created_utc": datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
    "regression_threshold_pct": 10,
    "benchmarks": list(collected.values())
}

with open(baseline_path, "w") as f:
    json.dump(output, f, indent=2)
    f.write("\n")

print(f"\n[bench-update-baseline] wrote {len(collected)} baseline entries to {baseline_path}")
PYEOF
