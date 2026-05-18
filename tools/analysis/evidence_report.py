#!/usr/bin/env python3
"""
tools/analysis/evidence_report.py - Generate an HTML evidence report.

Reads benchmark JSON and any test-results JSON files from the evidence directory,
then writes a self-contained HTML page.

Usage:
    python3 tools/analysis/evidence_report.py \
        [--evidence-dir DIR] [--benchmarks FILE] [--output FILE]
"""

import argparse
import glob
import json
import os
import subprocess
import sys
from datetime import datetime

THRESHOLD_NS = 100_000


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def git_commit():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True
        )
        return result.stdout.strip()
    except Exception:
        return "unknown"


def load_json_safe(path):
    try:
        with open(path) as f:
            return json.load(f)
    except Exception as exc:
        return {"_error": str(exc), "_path": path}


def load_benchmarks(path):
    data = load_json_safe(path)
    if "_error" in data:
        return None, data["_error"]
    if isinstance(data, list):
        results = data
    elif isinstance(data, dict) and "results" in data:
        results = data["results"]
    else:
        return None, "Unrecognised benchmark JSON structure"

    normalised = []
    for entry in results:
        name = entry.get("benchmark") or entry.get("name") or "<unnamed>"
        normalised.append({
            "benchmark": name,
            "min_ns":    int(entry.get("min_ns", 0)),
            "mean_ns":   int(entry.get("mean_ns", 0)),
            "max_ns":    int(entry.get("max_ns", 0)),
            "iters":     int(entry.get("iters", 0)),
            "pass":      int(entry.get("max_ns", 0)) < THRESHOLD_NS,
        })
    return normalised, None


def html_escape(s):
    return (str(s)
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;"))


# ---------------------------------------------------------------------------
# HTML builders
# ---------------------------------------------------------------------------

_CSS = """
body { font-family: sans-serif; max-width: 900px; margin: 2em auto; color: #222; }
h1   { border-bottom: 2px solid #333; padding-bottom: .3em; }
h2   { margin-top: 2em; border-bottom: 1px solid #aaa; }
table { border-collapse: collapse; width: 100%; margin-top: 1em; }
th, td { border: 1px solid #ccc; padding: .4em .7em; text-align: right; }
th   { background: #f0f0f0; text-align: center; }
td.name { text-align: left; font-family: monospace; font-size: .9em; }
.pass { background: #e8f5e9; }
.fail { background: #ffebee; }
.badge-pass { color: #2e7d32; font-weight: bold; }
.badge-fail { color: #c62828; font-weight: bold; }
footer { margin-top: 3em; font-size: .8em; color: #777; border-top: 1px solid #ddd; padding-top: .5em; }
pre  { background: #f5f5f5; padding: 1em; overflow-x: auto; font-size: .85em; }
"""


def benchmark_section(benchmarks, error):
    if error:
        return f"<h2>Benchmark Results</h2><p class='fail'>Error loading benchmarks: {html_escape(error)}</p>"

    all_pass = all(r["pass"] for r in benchmarks)
    verdict_cls  = "badge-pass" if all_pass else "badge-fail"
    verdict_text = "PASS" if all_pass else "FAIL"

    rows = []
    for r in benchmarks:
        row_cls = "pass" if r["pass"] else "fail"
        p99 = 3 * r["mean_ns"]
        rows.append(
            f'<tr class="{row_cls}">'
            f'<td class="name">{html_escape(r["benchmark"])}</td>'
            f'<td>{r["min_ns"]}</td>'
            f'<td>{r["mean_ns"]}</td>'
            f'<td>{r["max_ns"]}</td>'
            f'<td>{p99}</td>'
            f'<td>{r["iters"]:,}</td>'
            f'<td><span class="{verdict_cls if not r["pass"] else "badge-pass"}">'
            f'{"PASS" if r["pass"] else "FAIL"}</span></td>'
            f'</tr>'
        )

    return f"""
<h2>Benchmark Results
  <span class="{verdict_cls}" style="font-size:.75em;margin-left:1em">Overall: {verdict_text}</span>
</h2>
<p>Threshold: {THRESHOLD_NS:,} ns ({THRESHOLD_NS/1000:.0f} µs).
   p99 estimate = 3 × mean (conservative, no raw samples).</p>
<table>
  <thead><tr>
    <th>Benchmark</th><th>Min (ns)</th><th>Mean (ns)</th><th>Max (ns)</th>
    <th>p99est (ns)</th><th>Iterations</th><th>Result</th>
  </tr></thead>
  <tbody>{''.join(rows)}</tbody>
</table>
"""


def test_results_section(evidence_dir):
    pattern = os.path.join(evidence_dir, "*.json")
    files   = sorted(glob.glob(pattern))

    if not files:
        return "<h2>Test Results</h2><p>No JSON evidence files found in evidence directory.</p>"

    sections = ["<h2>Test Results</h2>"]
    for fpath in files:
        basename = os.path.basename(fpath)
        data = load_json_safe(fpath)
        sections.append(f"<h3>{html_escape(basename)}</h3>")
        if "_error" in data:
            sections.append(f"<p class='fail'>Parse error: {html_escape(data['_error'])}</p>")
            continue

        # Render generically as a <pre> block for arbitrary JSON shapes
        sections.append(f"<pre>{html_escape(json.dumps(data, indent=2))}</pre>")

    return "\n".join(sections)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate Haven HTML evidence report.")
    parser.add_argument("--evidence-dir", default="build/evidence",
                        metavar="DIR",  help="Directory with test-result JSON files (default: %(default)s)")
    parser.add_argument("--benchmarks",  default="build/benchmarks/isolation-latency.json",
                        metavar="FILE", help="Benchmark JSON file (default: %(default)s)")
    parser.add_argument("--output",      default="build/evidence/report.html",
                        metavar="FILE", help="Output HTML file (default: %(default)s)")
    args = parser.parse_args()

    timestamp   = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
    commit_hash = git_commit()

    benchmarks, bench_err = load_benchmarks(args.benchmarks)

    bench_html  = benchmark_section(benchmarks, bench_err)
    tests_html  = test_results_section(args.evidence_dir)

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Haven Hypervisor - Evidence Report</title>
  <style>{_CSS}</style>
</head>
<body>
  <h1>Haven Hypervisor - Evidence Report</h1>
  <p><strong>Generated:</strong> {html_escape(timestamp)}</p>

  {bench_html}
  {tests_html}

  <footer>
    Git commit: <code>{html_escape(commit_hash)}</code> &mdash; Haven Project
  </footer>
</body>
</html>
"""

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.output, "w") as f:
        f.write(html)

    print(f"Evidence report written to {args.output}")


if __name__ == "__main__":
    main()
