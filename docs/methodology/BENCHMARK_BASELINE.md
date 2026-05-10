# Benchmark Baseline Method

This document defines the initial benchmark baseline process for continuous trend tracking.

## Scope

The baseline currently measures command-level execution duration for:
1. build
2. style checks
3. test suite (unit + integration flow + config validation)

## Local execution

Run:
- ./scripts/benchmark-baseline.py

Output:
- build/benchmarks/baseline.json

## CI execution

Workflow:
- .github/workflows/benchmark.yml

Schedule:
- weekly

Artifacts:
- benchmark-<compiler>-<run-id>/baseline.json

## Usage for thesis milestones

1. Compare baseline trends before and after major subsystem changes.
2. Flag unexpected regressions in test or build duration.
3. Attach baseline snapshot to milestone evidence package.

## Interpretation notes

1. Values are host and environment dependent.
2. Use trend direction, not absolute time, for acceptance decisions.
3. Pair timing data with functional pass/fail evidence.
