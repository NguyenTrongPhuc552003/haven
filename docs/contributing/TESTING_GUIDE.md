# Testing Guide

This document explains how to add and run tests in the Haven project.

---

## Test organisation

```
tests/
  unit/          — single-module tests with no cross-subsystem dependencies
  integration/   — multi-module tests exercising combined isolation flows
  isolation/     — boundary tests for spatial and temporal isolation
  selftests/     — hypervisor invariant assertions (run at end of test suite)
  benchmarks/    — latency benchmarks; output JSON to build/benchmarks/
  demos/         — end-to-end demos showing multi-partition behaviour
```

---

## Adding a unit test

1. Create `tests/unit/test_<subsystem>.c`.

2. Include the public header for the module under test:
   ```c
   #include <haven/<subsystem>.h>
   ```

3. Write test functions with names starting `test_`:
   ```c
   static void test_my_invariant(void) {
       hv_my_module_init();
       hv_status_t s = hv_my_function(valid_args);
       assert(s == HV_OK);
   }
   ```

4. Call each function from `main()` and print pass/fail:
   ```c
   int main(void) {
       test_my_invariant();
       printf("[unit] test_<subsystem>: all tests passed\n");
       return 0;
   }
   ```

5. Add a compile-and-run block to `scripts/test.sh`, matching the pattern
   used by existing unit tests (lines 9-16):
   ```sh
   "$CC_BIN" -std=c11 -Wall -Wextra -Werror -Iinclude \
       tests/unit/test_<subsystem>.c \
       src/core/<subsystem>/<subsystem>.c \
       -o build/tests/test_<subsystem>
   ./build/tests/test_<subsystem>
   ```

---

## Adding an integration test

Integration tests exercise two or more subsystems together. They live in
`tests/integration/` and follow the same compile pattern but link multiple
source files.

Checklist before merging an integration test:
- Tests at least one "positive" path (valid cross-partition interaction
  is correctly handled) and one "negative" path (invalid access is
  rejected with the correct error code).
- Uses the exact public API only — no reach into `static` internals.
- Cleans up subsystem state with `*_init()` before each sub-test to
  avoid test-order dependencies.

---

## Adding an isolation test

Isolation tests are boundary-condition tests proving that the isolation
invariants hold at the edges of configured memory / time windows:

- **Spatial**: `tests/isolation/test_spatial_isolation.c` — check that
  PAs at ±1 byte from a partition boundary are handled correctly.
- **Temporal**: `tests/isolation/test_temporal_isolation.c` — check that
  a partition's budget is capped exactly at the configured limit.

Add new cases to the existing files or create a new file named
`test_<property>_isolation.c` with a matching entry in `scripts/test.sh`.

---

## Benchmark methodology

Benchmarks follow this contract:

1. **Warm-up**: at least 200 iterations before timing begins, to populate
   caches and branch-predictor state.
2. **Measurement**: `clock_gettime(CLOCK_MONOTONIC)` around the single hot
   call — no aggregated batches.
3. **Reporting**: min / mean / max in nanoseconds, plus iteration count.
4. **Output**: JSON written to `build/benchmarks/<name>.json` using the
   schema established in `bench_isolation_latency.c`.
5. **Reproducibility**: each benchmark file documents its test matrix and
   the exact PA/IOVA/partition configuration used.

To add a new benchmark:
- Create `tests/benchmarks/bench_<name>.c`.
- Add a compile block in `scripts/test.sh` after the existing benchmark
  blocks (see lines 149–195).
- Document expected variance in `docs/thesis/REPRODUCIBILITY_APPENDIX.md`.

---

## CI matrix explanation

The CI pipeline (`.github/workflows/`) runs the following matrix:

| Job | Runner | What it does |
|-----|--------|-------------|
| `host-tests` | ubuntu-latest | `make test` — compiles and runs all tests |
| `arm64-build` | ubuntu-latest | `make ARCH=arm64 all` — cross-compile to ELF |
| `qemu-smoke` | ubuntu-latest | `scripts/qemu-smoke.sh` — boots ELF in QEMU |
| `ci-preflight` | ubuntu-latest | `scripts/ci-preflight.sh` — lint, headers |
| `traceability` | ubuntu-latest | `scripts/check-traceability.sh` — requirement links |

All jobs must pass for a PR to be mergeable. The `arm64-build` job does
**not** run the test binaries (cross-compiled) — that is the role of
`qemu-smoke`.
