#!/usr/bin/env python3
"""
tools/analysis/latency_analyzer.py — Parse Haven benchmark JSON and print stats.

Usage:
    python3 tools/analysis/latency_analyzer.py [--input FILE] [--platform PLATFORM] [--json OUTPUT]
"""

import argparse
import json
import sys
from datetime import datetime

THRESHOLD_NS = 100_000  # 100 µs


def load_results(path):
    """Load benchmark results from JSON file, handling both array and wrapped formats."""
    with open(path) as f:
        data = json.load(f)
    # Support two JSON shapes:
    #   1. Top-level array: [{"benchmark": ..., "min_ns": ...}, ...]
    #   2. Wrapped object:  {"results": [{"name": ..., "min_ns": ...}, ...]}
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
            "min_ns":    int(entry.get("min_ns", 0)),
            "mean_ns":   int(entry.get("mean_ns", 0)),
            "max_ns":    int(entry.get("max_ns", 0)),
            "iters":     int(entry.get("iters", 0)),
        })
    return normalised


def p99_estimate(mean_ns):
    """Conservative p99 estimate when raw sample data is unavailable."""
    return 3 * mean_ns


def print_table(results, platform):
    col_w = 36
    header = f"{'Benchmark':<{col_w}} {'Min(ns)':>10} {'Mean(ns)':>10} {'Max(ns)':>10} {'p99est(ns)':>12} {'Iters':>8}"
    sep    = "-" * len(header)
    print(f"\nHaven Isolation Latency — Platform: {platform}")
    print(sep)
    print(header)
    print(sep)
    for r in results:
        p99 = p99_estimate(r["mean_ns"])
        flag = "  WARN" if r["max_ns"] >= THRESHOLD_NS else ""
        print(
            f"{r['benchmark']:<{col_w}} "
            f"{r['min_ns']:>10} "
            f"{r['mean_ns']:>10} "
            f"{r['max_ns']:>10} "
            f"{p99:>12} "
            f"{r['iters']:>8}"
            f"{flag}"
        )
    print(sep)


def summarise(results):
    all_pass = all(r["max_ns"] < THRESHOLD_NS for r in results)
    verdict  = "PASS" if all_pass else "FAIL"
    failing  = [r["benchmark"] for r in results if r["max_ns"] >= THRESHOLD_NS]
    print(f"\nSummary: {verdict}  (threshold {THRESHOLD_NS} ns / {THRESHOLD_NS/1000:.0f} µs)")
    if failing:
        for name in failing:
            print(f"  FAIL: {name} exceeded max latency threshold")
    return all_pass


def write_json(results, platform, path):
    output = {
        "tool":      "latency_analyzer",
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "platform":  platform,
        "threshold_ns": THRESHOLD_NS,
        "pass":      all(r["max_ns"] < THRESHOLD_NS for r in results),
        "results": [
            {
                "benchmark": r["benchmark"],
                "min_ns":    r["min_ns"],
                "mean_ns":   r["mean_ns"],
                "max_ns":    r["max_ns"],
                "p99est_ns": p99_estimate(r["mean_ns"]),
                "iters":     r["iters"],
                "pass":      r["max_ns"] < THRESHOLD_NS,
            }
            for r in results
        ],
    }
    with open(path, "w") as f:
        json.dump(output, f, indent=2)
    print(f"Results written to {path}")


def main():
    parser = argparse.ArgumentParser(description="Analyse Haven isolation-latency benchmarks.")
    parser.add_argument("--input",    default="build/benchmarks/isolation-latency.json",
                        metavar="FILE", help="Path to benchmark JSON (default: %(default)s)")
    parser.add_argument("--platform", default="unknown",
                        metavar="PLATFORM", help="Platform label for the report (default: %(default)s)")
    parser.add_argument("--json",     default=None,
                        metavar="OUTPUT", help="Write structured results to this JSON file")
    args = parser.parse_args()

    try:
        results = load_results(args.input)
    except FileNotFoundError:
        print(f"ERROR: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    if not results:
        print("ERROR: No benchmark results found in input file.", file=sys.stderr)
        sys.exit(1)

    print_table(results, args.platform)
    passed = summarise(results)

    if args.json:
        write_json(results, args.platform, args.json)

    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
