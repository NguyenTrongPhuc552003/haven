# Reproducibility Appendix

This appendix provides exact, step-by-step instructions for reproducing
every quantitative result in the Haven thesis from scratch, including
benchmark data, configuration validation outputs, and QEMU smoke-test logs.

---

## Required toolchain versions

| Tool          | Minimum version | Recommended | Notes                                        |
| ------------- | --------------- | ----------- | -------------------------------------------- |
| GCC           | 12.0            | 13.2        | ARM64 cross: `aarch64-unknown-linux-gnu-gcc` |
| Clang         | 15.0            | 17.0        | Alternative; same flags                      |
| QEMU          | 7.2             | 8.2         | `qemu-system-aarch64`                        |
| Coq           | 8.18            | 8.18        | For formal-proof artefacts                   |
| Python        | 3.10            | 3.12        | Evidence comparison script                   |
| GNU Make      | 4.3             | 4.4         |                                              |
| GDB (aarch64) | 12.0            | 14.1        | Optional; for QEMU debug sessions            |

### Docker image (recommended)

A frozen Docker image containing the above toolchain is provided:

```
docker pull ghcr.io/haven-hypervisor/build-env:2026-05-14
# SHA256: <PLACEHOLDER - to be filled at thesis submission>
```

To reproduce results inside the container:

```bash
docker run --rm -v "$(pwd)":/haven -w /haven \
    ghcr.io/haven-hypervisor/build-env:2026-05-14 \
    make test
```

---

## Reproducing all results from scratch

### Step 1 - clone and enter the repository

```bash
git clone https://github.com/NguyenTrongPhuc552003/haven.git
cd haven
git checkout <thesis-tag>   # e.g. v0.4.0-thesis
```

### Step 2 - run the full host test suite and benchmarks

```bash
make test
```

Expected stdout ends with:

```
[test] done
```

All test binaries are placed in `build/tests/`. All benchmark JSON files
are placed in `build/benchmarks/`.

### Step 3 - cross-compile the ARM64 image

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- all
```

Output: `build/haven.elf`.

### Step 4 - QEMU smoke test

```bash
bash scripts/qemu-smoke.sh
```

Expected exit code: 0. The script streams UART output and checks for the
`Haven: EL2 init complete` banner within 10 seconds.

### Step 5 - collect benchmark evidence package

```bash
bash scripts/package-evidence.sh
```

Output: `build/evidence-<date>.tar.gz` containing all JSON results,
build logs, and a SHA256 manifest.

---

## Expected benchmark outputs and acceptable variance

Measurements below are from a reference host: Intel Core i7-1270P @ 2.2 GHz,
Linux 6.6 (Ubuntu 24.04), `CLOCK_MONOTONIC`. QEMU results use
`qemu-system-aarch64 -cpu cortex-a53`.

### bench_isolation_latency (build/benchmarks/isolation-latency.json)

| Benchmark                    | Expected mean (ns) | Acceptable range |
| ---------------------------- | -----------------: | ---------------- |
| stage2_partition_contains_pa |                 15 | 5–100            |
| irq_is_owned_by              |                 10 | 5–80             |
| budget_consume               |                 12 | 5–80             |
| smmu_check_dma_access        |                 18 | 5–120            |
| timer_check_deadline         |                 10 | 5–80             |
| iommu_check_group            |                 10 | 5–80             |

Variance above 5× the expected mean indicates a noisy environment (another
process is competing for CPU cache). Re-run with `taskset -c 0` to pin to
an isolated core.

### bench_temporal_isolation (build/benchmarks/temporal-isolation.json)

Key invariant: `max_ns` for any (period, load) cell must remain below 2×
the `min_ns` of the 0 % load baseline for the same period. This bounds
jitter introduced by the simulated Linux load.

| Period | Load  | Expected mean_ns | Max acceptable max_ns |
| ------ | ----- | ---------------- | --------------------- |
| 1 ms   | 0 %   | 12               | 200                   |
| 1 ms   | 100 % | 15               | 400                   |
| 10 ms  | 0 %   | 12               | 200                   |
| 10 ms  | 100 % | 15               | 400                   |

### bench_stage2_fault (build/benchmarks/stage2-fault.json)

| Case          | Expected mean (ns) | Acceptable range |
| ------------- | -----------------: | ---------------- |
| hot path      |                 12 | 5–80             |
| cold path     |                 14 | 5–100            |
| low boundary  |                 12 | 5–80             |
| high boundary |                 12 | 5–80             |

Cold path slightly higher than hot path is expected (range check falls
through to the not-found branch).

### bench_smmu_policy (build/benchmarks/smmu-policy.json)

| Case                 | Expected mean (ns) | Acceptable range |
| -------------------- | -----------------: | ---------------- |
| check_access_hot     |                 18 | 5–120            |
| window_boundary_low  |                 18 | 5–120            |
| window_boundary_high |                 18 | 5–120            |
| access_denied        |                 16 | 5–100            |
| allocate_streamid    |                 25 | 5–200            |

---

## i.MX95 vs QEMU comparison methodology

Results from the i.MX95-EVK are collected by cross-compiling the benchmark
binaries with:

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- \
     BENCH_EXTRA_CFLAGS="-DHAVEN_BARE_METAL=1" bench
```

and booting the resulting `build/haven-bench.elf` via U-Boot. The UART
output is captured and parsed by `scripts/compare-evidence.py`.

The comparison script flags any result where the i.MX95 mean deviates from
the QEMU mean by more than 10× (expected range: 1×–4× due to real cache
hierarchy vs QEMU's timing model).

---

## Data archival: SHA256 manifest format

Each `build/evidence-<date>.tar.gz` contains a file `SHA256SUMS` with the
format:

```
<sha256hex>  benchmarks/isolation-latency.json
<sha256hex>  benchmarks/temporal-isolation.json
<sha256hex>  benchmarks/stage2-fault.json
<sha256hex>  benchmarks/smmu-policy.json
<sha256hex>  logs/qemu-smoke.log
<sha256hex>  build/haven.elf
```

To verify an archived package:

```bash
tar -xzf build/evidence-<date>.tar.gz
sha256sum -c SHA256SUMS
```

All lines must read `OK`. Any `FAILED` line indicates the artefact was
modified after packaging.
