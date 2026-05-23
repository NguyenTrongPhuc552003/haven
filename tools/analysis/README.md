# Haven Analysis Tools

This directory contains three Python 3 analysis scripts that consume the
JSON output produced by Haven's benchmark suite (`ctest --test-dir build-host` writes results
to `build-host/`).

---

## `latency_analyzer.py`

Parses a benchmark JSON file and prints a formatted statistics table with
pass/fail verdict against the 100 µs acceptance threshold.

### Usage

```bash
python3 tools/analysis/latency_analyzer.py \
    --input  build/benchmarks/isolation-latency.json \
    --platform qemu-virt \
    [--json  build/benchmarks/analysis-out.json]
```

### Arguments

| Flag         | Default                                   | Description                               |
| ------------ | ----------------------------------------- | ----------------------------------------- |
| `--input`    | `build/benchmarks/isolation-latency.json` | Path to benchmark JSON                    |
| `--platform` | `unknown`                                 | Platform label printed in the report      |
| `--json`     | *(none)*                                  | Optional: write structured output to file |

### Input Format

The tool accepts two JSON shapes:

```jsonc
// Shape 1 - top-level array
[{"benchmark": "stage2_contains_hot", "min_ns": 38, "mean_ns": 43, "max_ns": 61, "iters": 10000}]

// Shape 2 - wrapped object
{"results": [{"name": "stage2_contains_hot", ...}]}
```

### Output

```
Haven Isolation Latency - Platform: qemu-virt
-------------------------------------------------------------------
Benchmark                            Min(ns)   Mean(ns)    Max(ns)   p99est(ns)    Iters
-------------------------------------------------------------------
stage2_contains_hot                       38         43         61          129    10000
smmu_check_hot                            35         42         58          126    10000
-------------------------------------------------------------------
Summary: PASS  (threshold 100000 ns / 100 µs)
```

The exit code is `0` if all benchmarks pass, `1` otherwise - making it
suitable for CI gating.

---

## `jitter_plot.py`

Generates a bar chart (min / mean / max per benchmark) from the same JSON
input format as `latency_analyzer.py`.  Uses `matplotlib` when available;
falls back to an ASCII chart in minimal CI environments.

### Usage

```bash
python3 tools/analysis/jitter_plot.py \
    --input    build/benchmarks/isolation-latency.json \
    --output   build/benchmarks/latency_plot.png \
    --platform qemu-virt
```

### Arguments

| Flag         | Default                                   | Description                             |
| ------------ | ----------------------------------------- | --------------------------------------- |
| `--input`    | `build/benchmarks/isolation-latency.json` | Benchmark JSON input                    |
| `--output`   | `build/benchmarks/latency_plot.png`       | Output PNG path (ignored in ASCII mode) |
| `--platform` | `QEMU`                                    | Platform label for chart title          |

### Matplotlib Dependency

```bash
pip install matplotlib numpy
```

If `matplotlib` is not importable, the script automatically falls back to an
ASCII bar chart printed to stdout.  The 100 µs threshold is drawn as a dashed
orange line in the matplotlib version.

---

## `evidence_report.py`

Generates a self-contained HTML evidence report combining benchmark results
and any test-result JSON files found in the evidence directory.  Embeds
git commit hash for traceability.

### Usage

```bash
python3 tools/analysis/evidence_report.py \
    --evidence-dir build/evidence \
    --benchmarks   build/benchmarks/isolation-latency.json \
    --output       build/evidence/report.html
```

### Arguments

| Flag             | Default                                   | Description                                    |
| ---------------- | ----------------------------------------- | ---------------------------------------------- |
| `--evidence-dir` | `build/evidence`                          | Directory containing test JSON files           |
| `--benchmarks`   | `build/benchmarks/isolation-latency.json` | Benchmark JSON file                            |
| `--output`       | `build/evidence/report.html`              | Output HTML path (directory created if needed) |

### Output

A self-contained HTML page with:
- Benchmark results table (colour-coded pass/fail, p99 estimate)
- Per-benchmark JSON evidence sections from `--evidence-dir/*.json`
- Git commit hash footer for thesis traceability

### Git Integration

The report footer automatically captures the current `HEAD` commit hash via
`git rev-parse HEAD`.  If git is unavailable, it prints `unknown`.

---

## Example: Full Analysis Pipeline

```bash
# 1. Run benchmarks (requires built host-test suite)
cmake --preset host-tests && cmake --build build-host
ctest --test-dir build-host --output-on-failure

# 2. Print latency table
python3 tools/analysis/latency_analyzer.py \
    --input build/benchmarks/isolation-latency.json \
    --platform qemu-virt

# 3. Generate plot
python3 tools/analysis/jitter_plot.py \
    --input  build/benchmarks/isolation-latency.json \
    --output build/benchmarks/latency_plot.png

# 4. Generate full HTML evidence report
python3 tools/analysis/evidence_report.py \
    --evidence-dir build/evidence \
    --output       build/evidence/report.html
```
