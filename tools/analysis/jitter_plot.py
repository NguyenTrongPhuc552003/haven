#!/usr/bin/env python3
"""
tools/analysis/jitter_plot.py — Bar chart comparing isolation latencies across benchmarks.

Uses matplotlib when available; falls back to an ASCII chart for minimal CI environments.

Usage:
    python3 tools/analysis/jitter_plot.py [--input FILE] [--output FILE] [--platform PLATFORM]
"""

import argparse
import json
import sys


# ---------------------------------------------------------------------------
# Data loading (shared with latency_analyzer)
# ---------------------------------------------------------------------------

def load_results(path):
    with open(path) as f:
        data = json.load(f)
    if isinstance(data, list):
        results = data
    elif isinstance(data, dict) and "results" in data:
        results = data["results"]
    else:
        print(f"ERROR: Unrecognised JSON structure in {path}", file=sys.stderr)
        sys.exit(1)

    normalised = []
    for entry in results:
        name = entry.get("benchmark") or entry.get("name") or "<unnamed>"
        normalised.append({
            "benchmark": name,
            "min_ns":  int(entry.get("min_ns", 0)),
            "mean_ns": int(entry.get("mean_ns", 0)),
            "max_ns":  int(entry.get("max_ns", 0)),
        })
    return normalised


# ---------------------------------------------------------------------------
# Matplotlib plot
# ---------------------------------------------------------------------------

def plot_matplotlib(results, platform, output_path):
    import matplotlib
    matplotlib.use("Agg")  # non-interactive backend for CI
    import matplotlib.pyplot as plt
    import numpy as np

    names  = [r["benchmark"] for r in results]
    mins   = [r["min_ns"]   for r in results]
    means  = [r["mean_ns"]  for r in results]
    maxs   = [r["max_ns"]   for r in results]

    x      = np.arange(len(names))
    width  = 0.25
    fig, ax = plt.subplots(figsize=(max(10, len(names) * 1.6), 6))

    ax.bar(x - width, mins,  width, label="Min",  color="#4caf50")
    ax.bar(x,         means, width, label="Mean", color="#2196f3")
    ax.bar(x + width, maxs,  width, label="Max",  color="#f44336")

    ax.set_xlabel("Benchmark")
    ax.set_ylabel("Latency (ns)")
    ax.set_title(f"Haven Isolation Latency — {platform}")
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=20, ha="right", fontsize=9)
    ax.legend()
    ax.yaxis.grid(True, linestyle="--", alpha=0.5)
    ax.set_axisbelow(True)

    # Draw 100 µs threshold line
    ax.axhline(y=100_000, color="orange", linestyle="--", linewidth=1.2, label="100 µs limit")
    ax.legend()

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    print(f"Plot saved to {output_path}")


# ---------------------------------------------------------------------------
# ASCII fallback
# ---------------------------------------------------------------------------

def plot_ascii(results, platform):
    BAR_WIDTH   = 40
    MAX_VAL     = max(r["max_ns"] for r in results) or 1

    print(f"\nHaven Isolation Latency — {platform}  (ASCII chart)")
    print("=" * 72)

    for r in results:
        name = r["benchmark"]
        print(f"\n  {name}")
        for label, val in [("min ", r["min_ns"]), ("mean", r["mean_ns"]), ("max ", r["max_ns"])]:
            bar_len = int(val / MAX_VAL * BAR_WIDTH)
            bar     = "#" * bar_len
            print(f"    {label} |{bar:<{BAR_WIDTH}}| {val:>8} ns")

    print("\n" + "=" * 72)
    print(f"  Scale: each '#' ≈ {MAX_VAL / BAR_WIDTH:.0f} ns   (max value = {MAX_VAL} ns)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate a bar chart of Haven isolation latencies."
    )
    parser.add_argument("--input",    default="build/benchmarks/isolation-latency.json",
                        metavar="FILE",     help="Benchmark JSON input (default: %(default)s)")
    parser.add_argument("--output",   default="build/benchmarks/latency_plot.png",
                        metavar="FILE",     help="Output PNG path (default: %(default)s)")
    parser.add_argument("--platform", default="QEMU",
                        metavar="PLATFORM", help="Platform label (default: %(default)s)")
    args = parser.parse_args()

    try:
        results = load_results(args.input)
    except FileNotFoundError:
        print(f"ERROR: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    if not results:
        print("ERROR: No benchmark results found.", file=sys.stderr)
        sys.exit(1)

    try:
        import matplotlib  # noqa: F401
        plot_matplotlib(results, args.platform, args.output)
    except ImportError:
        print("WARNING: matplotlib not available — falling back to ASCII chart.", file=sys.stderr)
        plot_ascii(results, args.platform)


if __name__ == "__main__":
    main()
