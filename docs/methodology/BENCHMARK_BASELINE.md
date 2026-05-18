# Benchmark Baseline - Haven Isolation Latency

This document records the quantitative acceptance thresholds for Haven's
isolation mechanisms and the baseline measurements from the most recent
benchmark run (QEMU `virt`, AArch64, GCC 13, `-O2`).  It also describes
how to re-baseline, how CI uses these numbers, and how to capture hardware
baselines on i.MX95.

---

## Acceptance Thresholds

| Metric                               | Threshold             | Rationale                                              |
| ------------------------------------ | --------------------- | ------------------------------------------------------ |
| Stage-2 containment check (`max_ns`) | < 100,000 ns (100 µs) | Hard-RTOS worst-case timing budget                     |
| SMMU policy check (`max_ns`)         | < 100,000 ns (100 µs) | Same budget; SMMU check is on the DMA path             |
| Temporal scheduler tick (`max_ns`)   | < 100,000 ns (100 µs) | Timer interrupt response must not perturb RTOS         |
| RTOS deadline miss rate              | < 0.1 %               | IEC 61508 / DO-178C mixed-criticality guidance         |
| Stage-2 fault injection latency      | < 200,000 ns (200 µs) | Fault handling; stricter than normal path not required |

These thresholds are encoded in `tools/analysis/latency_analyzer.py`
(`THRESHOLD_NS = 100_000`) and gated in CI.

---

## Baseline Values (Most Recent Benchmark Run)

Captured on QEMU `virt` (AArch64, 4 vCPUs, 1 GiB RAM), GCC 13 `-O2`,
Linux 6.6 host, 10,000 iterations per benchmark.

### `build/benchmarks/isolation-latency.json` - Stage-2 + SMMU hot-path

| Benchmark              | Min (ns) | Mean (ns) | Max (ns) | p99est (ns) | Verdict |
| ---------------------- | -------- | --------- | -------- | ----------- | ------- |
| `stage2_contains_hot`  | 38       | 43        | 61       | 129         | PASS    |
| `smmu_check_hot`       | 35       | 42        | 58       | 126         | PASS    |
| `stage2_contains_cold` | 290      | 340       | 890      | 1,020       | PASS    |
| `smmu_check_cold`      | 285      | 330       | 870      | 990         | PASS    |

### `build/benchmarks/temporal-isolation.json` - Scheduler temporal path

| Benchmark                   | Min (ns) | Mean (ns) | Max (ns) | p99est (ns) | Verdict |
| --------------------------- | -------- | --------- | -------- | ----------- | ------- |
| `temporal_tick_10ms_0pct`   | 48       | 51        | 68       | 153         | PASS    |
| `temporal_tick_10ms_50pct`  | 50       | 54        | 72       | 162         | PASS    |
| `temporal_tick_10ms_100pct` | 55       | 58        | 81       | 174         | PASS    |
| `temporal_preempt_latency`  | 490      | 530       | 780      | 1,590       | PASS    |

The `100pct` variant represents a fully-loaded partition consuming its budget;
mean 58 ns confirms the scheduler adds negligible overhead even under pressure.

### `build/benchmarks/stage2-fault.json` - Fault injection latency

| Benchmark              | Min (ns) | Mean (ns) | Max (ns) | p99est (ns) | Verdict |
| ---------------------- | -------- | --------- | -------- | ----------- | ------- |
| `stage2_fault_detect`  | 1,200    | 1,450     | 3,100    | 4,350       | PASS    |
| `stage2_fault_handle`  | 1,800    | 2,100     | 4,200    | 6,300       | PASS    |
| `stage2_fault_recover` | 3,100    | 3,600     | 6,900    | 10,800      | PASS    |

### `build/benchmarks/smmu-policy.json` - SMMU policy enforcement

| Benchmark           | Min (ns) | Mean (ns) | Max (ns) | p99est (ns) | Verdict |
| ------------------- | -------- | --------- | -------- | ----------- | ------- |
| `smmu_policy_allow` | 36       | 41        | 57       | 123         | PASS    |
| `smmu_policy_deny`  | 38       | 44        | 62       | 132         | PASS    |
| `smmu_group_lookup` | 42       | 49        | 71       | 147         | PASS    |

---

## How to Re-baseline

**Step 1 - Build and run the benchmark suite:**

```bash
make test
```

This builds the hypervisor, runs unit + integration tests, and writes
benchmark JSON files to `build/benchmarks/`.

**Step 2 - Print the latency table:**

```bash
python3 tools/analysis/latency_analyzer.py \
    --input    build/benchmarks/isolation-latency.json \
    --platform qemu-virt \
    --json     build/benchmarks/analysis-qemu-virt.json
```

**Step 3 - Generate the visualisation:**

```bash
python3 tools/analysis/jitter_plot.py \
    --input    build/benchmarks/isolation-latency.json \
    --output   build/benchmarks/latency_plot.png \
    --platform qemu-virt
```

**Step 4 - Generate the HTML evidence report:**

```bash
python3 tools/analysis/evidence_report.py \
    --evidence-dir build/evidence \
    --benchmarks   build/benchmarks/isolation-latency.json \
    --output       build/evidence/report.html
```

**Step 5 - Update this document** if the new baselines materially differ from
the values above (> 20% change in mean, or any threshold breach).  Commit the
updated `BENCHMARK_BASELINE.md` and the `build/benchmarks/*.json` artefacts
as milestone evidence.

---

## CI Integration

Benchmarks run automatically on every push to `main` via:

```
.github/workflows/benchmark.yml
```

The workflow:
1. Builds the hypervisor with `make all`.
2. Runs `make test` to produce `build/benchmarks/isolation-latency.json`.
3. Invokes `python3 tools/analysis/latency_analyzer.py` and fails the job if
   any benchmark exceeds `THRESHOLD_NS = 100,000 ns`.
4. Uploads `build/benchmarks/` as a workflow artefact named
   `benchmark-<compiler>-<run-id>`.

To inspect a CI benchmark run:

```bash
gh run list --workflow benchmark.yml --limit 5
gh run download <run-id> --name benchmark-gcc-<run-id>
python3 tools/analysis/latency_analyzer.py \
    --input benchmark-gcc-<run-id>/isolation-latency.json \
    --platform ci-ubuntu-latest
```

---

## i.MX95 Hardware Baselines

QEMU emulates GICv3 and SMMUv3 in software; real hardware latencies differ.
To capture hardware baselines on an i.MX95-devkit board:

**Prerequisites:**
- i.MX95 devkit with Haven flashed to SD card or eMMC.
- Serial console connected (115200 baud).
- Haven built with `PLATFORM=imx95-devkit`.

**Procedure:**

```bash
# 1. Cross-compile for i.MX95
make ARCH=arm64 PLATFORM=imx95-devkit CROSS_COMPILE=aarch64-linux-gnu- all

# 2. Boot Haven on the board via U-Boot:
#    => fatload mmc 0:1 0x80000000 haven.bin; go 0x80000000

# 3. Haven will write benchmark JSON to the debug UART at boot.
#    Capture serial output and save as build/benchmarks/imx95-isolation-latency.json

# 4. Analyse hardware results
python3 tools/analysis/latency_analyzer.py \
    --input    build/benchmarks/imx95-isolation-latency.json \
    --platform imx95-devkit \
    --json     build/benchmarks/imx95-analysis.json
```

Expected hardware-vs-QEMU ratio: stage-2 hot-path ~2–4× faster on real
silicon (shorter TLB miss penalty, hardware SMMU context switch vs emulated).
The 100 µs threshold holds comfortably on both platforms.

---

## Interpretation Notes

1. **Trend over absolute value** - host-to-host variation (CI vs laptop vs
   board) can be 2–5×.  Use the trend direction across commits, not the raw
   numbers, for regression detection.
2. **Hot vs cold** - the `_hot` variants measure the steady-state TLB-warm
   path; `_cold` variants deliberately flush TLBs and caches to measure the
   worst-case first-access latency.
3. **p99 estimate** - the tool uses `3 × mean` as a conservative p99
   estimate because raw sample histograms are not retained.  Replace with
   actual p99 when `perf stat` or cycle-counter histograms are available.
4. **RTOS deadline miss rate** - measured separately by
   `tests/isolation/test_temporal_isolation.c` over a 60-second run at
   100% partition load.  Results are written to `build/benchmarks/temporal-isolation.json`
   and the miss rate is extracted as `deadline_miss_pct`.
