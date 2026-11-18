#!/usr/bin/env python3
"""Score the evaluation campaign: case files into the tables of design item 5.

docs/methodology/stage2/20261119-evaluation-campaign/README.md, design
item 5: per class and method, the detection rate with its exact 95 %
interval (Clopper-Pearson); the cases only read-back detects; false
findings on controls; oracle disagreement; read-back's run time. Written,
random and held-out cases are reported apart and together.

A case file (one per case, written by run_case.py):

    {"schema": "haven-campaign-case/1", "case": "W-S2-EXTENT-1",
     "source": "written" | "random" | "heldout", "class": "S2-EXTENT",
     "equivalent": false,
     "methods": {"M1": {"verdict": "pass" | "finding" | "n/a" | "error",
                        "seconds": 1.2, "evidence": ["..."]}, ...}}

A case of class CONTROL is a control. A case marked equivalent (a random
mutant no method can observe, read and confirmed) and a method's "n/a"
or "error" verdict are counted apart, never in a rate's denominator.

Expectations (written cases only): {"W-S2-EXTENT-1": {"M6": "finding"}};
a method whose verdict differs from its expectation is a disagreement.

Usage: score.py [--expect FILE] [--json OUT] CASE.json...
"""

import argparse
import json
import math
import sys

METHODS = ["M1", "M2", "M3", "M4", "M5", "M6", "M7"]
BASELINES = {"M1", "M2", "M3", "M4", "M5"}
READBACK = {"M6", "M7"}
SOURCES = ["written", "random", "heldout"]
CASE_SCHEMA = "haven-campaign-case/1"


def _tail_ge(k, n, p):
    """P(X >= k) for X ~ Binomial(n, p)."""
    return sum(math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(k, n + 1))


def _bisect(f, target, increasing):
    lo, hi = 0.0, 1.0
    for _ in range(200):
        mid = (lo + hi) / 2
        if (f(mid) < target) == increasing:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2


def clopper_pearson(k, n, alpha=0.05):
    """Exact two-sided (1 - alpha) interval for k successes in n trials."""
    if n <= 0 or not 0 <= k <= n:
        raise ValueError(f"bad count {k}/{n}")
    a = alpha / 2
    if k == 0:
        lower = 0.0
    elif k == n:
        lower = a ** (1 / n)
    else:
        lower = _bisect(lambda p: _tail_ge(k, n, p), a, True)
    if k == n:
        upper = 1.0
    elif k == 0:
        upper = 1 - a ** (1 / n)
    else:
        upper = _bisect(lambda p: 1 - _tail_ge(k + 1, n, p), a, False)
    return lower, upper


def load_case(path):
    with open(path) as f:
        case = json.load(f)
    if case.get("schema") != CASE_SCHEMA:
        raise ValueError(f"{path}: schema is not {CASE_SCHEMA}")
    for key in ("case", "source", "class", "methods"):
        if key not in case:
            raise ValueError(f"{path}: no {key}")
    if case["source"] not in SOURCES:
        raise ValueError(f"{path}: unknown source {case['source']}")
    return case


def verdict(case, method):
    return case["methods"].get(method, {}).get("verdict", "n/a")


def rate(k, n):
    if n == 0:
        return {"detected": 0, "cases": 0}
    lo, hi = clopper_pearson(k, n)
    return {"detected": k, "cases": n, "rate": k / n, "low": round(lo, 3), "high": round(hi, 3)}


def score(cases, expect=None):
    """Cases (dicts) and expectations into one report (a dict)."""
    expect = expect or {}
    names = [c["case"] for c in cases]
    if len(names) != len(set(names)):
        raise ValueError("a case appears twice")
    report = {"equivalent": sorted(c["case"] for c in cases if c.get("equivalent")),
              "not_scored": {}, "by_source": {}}
    scored = [c for c in cases if not c.get("equivalent")]
    for c in scored:
        for m in METHODS:
            if verdict(c, m) in ("n/a", "error"):
                report["not_scored"].setdefault(m, {}).setdefault(verdict(c, m), []).append(c["case"])
    groups = [(s, [c for c in scored if c["source"] == s]) for s in SOURCES] + [("all", scored)]
    for source, group in groups:
        faults = [c for c in group if c["class"] != "CONTROL"]
        controls = [c for c in group if c["class"] == "CONTROL"]
        table = {}
        for cls in sorted({c["class"] for c in faults}):
            row = {}
            for m in METHODS:
                judged = [c for c in faults if c["class"] == cls and verdict(c, m) in ("pass", "finding")]
                row[m] = rate(sum(verdict(c, m) == "finding" for c in judged), len(judged))
            table[cls] = row
        false = {}
        for m in METHODS:
            judged = [c for c in controls if verdict(c, m) in ("pass", "finding")]
            false[m] = {"false_findings": sum(verdict(c, m) == "finding" for c in judged), "controls": len(judged)}
        only = sorted(c["case"] for c in faults
                      if any(verdict(c, m) == "finding" for m in READBACK)
                      and not any(verdict(c, m) == "finding" for m in BASELINES))
        report["by_source"][source] = {"cases": len(group), "detection": table,
                                       "controls": false, "only_readback": only}
    disagree = []
    for c in cases:
        for m, want in sorted(expect.get(c["case"], {}).items()):
            got = verdict(c, m)
            if got != want:
                disagree.append({"case": c["case"], "method": m, "expected": want, "got": got})
    report["disagreements"] = disagree
    times = sorted(c["methods"]["M6"]["seconds"] for c in cases
                   if "seconds" in c["methods"].get("M6", {}))
    if times:
        mid = len(times) // 2
        median = times[mid] if len(times) % 2 else (times[mid - 1] + times[mid]) / 2
        report["readback_seconds"] = {"runs": len(times), "median": median, "max": times[-1]}
    return report


def markdown(report):
    out = []
    for source, part in report["by_source"].items():
        if not part["cases"]:
            continue
        out.append(f"### {source} ({part['cases']} cases)\n")
        out.append("| Class | " + " | ".join(METHODS) + " |")
        out.append("| - |" + " - |" * len(METHODS))
        for cls, row in part["detection"].items():
            cells = []
            for m in METHODS:
                r = row[m]
                cells.append(f"{r['detected']}/{r['cases']} [{r['low']:.3f}, {r['high']:.3f}]" if r["cases"] else "-")
            out.append(f"| {cls} | " + " | ".join(cells) + " |")
        ctl = part["controls"]
        out.append("| false findings on controls | "
                   + " | ".join(f"{ctl[m]['false_findings']}/{ctl[m]['controls']}" for m in METHODS) + " |")
        out.append("")
        out.append("Only read-back detects: " + (", ".join(part["only_readback"]) or "none") + "\n")
    out.append(f"Disagreements with the expectations: {len(report['disagreements'])}")
    for d in report["disagreements"]:
        out.append(f"- {d['case']} {d['method']}: expected {d['expected']}, got {d['got']}")
    out.append(f"\nEquivalent (not scored): {', '.join(report['equivalent']) or 'none'}")
    if "readback_seconds" in report:
        t = report["readback_seconds"]
        out.append(f"\nM6 run time: median {t['median']:.1f} s, max {t['max']:.1f} s over {t['runs']} runs")
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--expect", help="expectations JSON (written cases)")
    ap.add_argument("--json", help="write the report as JSON here")
    ap.add_argument("cases", nargs="+")
    args = ap.parse_args(argv)
    expect = {}
    if args.expect:
        with open(args.expect) as f:
            expect = json.load(f)
    report = score([load_case(p) for p in args.cases], expect)
    if args.json:
        with open(args.json, "w") as f:
            json.dump(report, f, indent=1)
            f.write("\n")
    sys.stdout.write(markdown(report))
    return 0


if __name__ == "__main__":
    sys.exit(main())
