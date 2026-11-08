#!/usr/bin/env python3
"""Check HAVEN's lines in a UART log (README.md, rows P1 to P3).

Usage: check_lines.py UART.log [...]
Prints, per log: HAVEN lines, broken HAVEN lines (with the reasons), lines
where a Linux line ran into a HAVEN line (counted only), and whether the
run reached KDAL-DEMAND-END and how many stop lines it has. Exit 1 if any
HAVEN line is broken.
"""
import re
import sys

REPORT = re.compile(r"^HAVEN: virtual interrupts delivered on CPU \d+:( \d+ x\d+)*$")
PSCI = re.compile(r"^HAVEN: PSCI [A-Z_?]+ fid=0x[0-9a-f]+ x1=0x[0-9a-f]+ -> 0x[0-9a-f]+ cpu=\d+$")


def check(path):
    text = open(path, errors="replace").read()
    haven, broken, mixed = 0, [], 0
    for n, line in enumerate(text.splitlines(), 1):
        if "HAVEN" not in line:
            continue
        haven += 1
        why = []
        if line.count("HAVEN:") != 1:
            why.append("not exactly one HAVEN: prefix")
        start = line.find("HAVEN:")
        if start > 0:
            mixed += 1  # a Linux line ran into a HAVEN line: counted only
        body = line[max(start, 0):]
        if "virtual interrupts delivered" in body and not REPORT.match(body):
            why.append("report line not whole")
        if "PSCI" in body and "fid=" in body and not PSCI.match(body):
            why.append("PSCI line not whole")
        if why:
            broken.append((n, ", ".join(why), line[:120]))
    return {"haven": haven, "broken": broken, "mixed_with_linux": mixed,
            "marker": "KDAL-DEMAND-END" in text, "stop_lines": len(re.findall(r"partition stopped", text))}


def main():
    bad = 0
    for p in sys.argv[1:]:
        r = check(p)
        print(f"{p}: {r['haven']} HAVEN lines, {len(r['broken'])} broken, "
              f"{r['mixed_with_linux']} meeting a Linux line, marker {r['marker']}, stop lines {r['stop_lines']}")
        for n, why, line in r["broken"]:
            print(f"  line {n}: {why}: {line!r}")
        bad += len(r["broken"])
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
