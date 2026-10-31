#!/usr/bin/env python3
"""P-A0 step 6: score baseline and candidate against the frozen oracle.

The only reader of the frozen "expected" field. The candidate method is the
frozen baseline plus the consistency relations (NEXT_EXPERIMENT.md: "cross-
boundary semantic checks ... than existing policy tests plus structural
validation"), so a case counts as caught by the candidate if either rejects.
Relation classes are fixed by checkpoint name, not by case:
  R2:*  field comparison against E    (protocol reject/reframe signal)
  R3:*  granularity relation          (non-structural)
  R4:*  effective-binding relation    (depends on known defect MM1)
Usage: compare.py <freeze/cases.json> <baseline run> <candidate run>
"""
import json
import sys

CLASS = {"R2": "field comparison against E",
         "R3": "granularity relation (non-structural)",
         "R4": "effective binding (known defect MM1)"}


def parse(path):
    out = {}
    for line in open(path):
        parts = line.split()
        if len(parts) >= 3:
            out[parts[0]] = (parts[1], parts[2].split("=", 1)[1])
    return out


def main():
    frozen = json.load(open(sys.argv[1]))
    base, cand = parse(sys.argv[2]), parse(sys.argv[3])
    rows, escapes = [], []
    for c in frozen["cases"]:
        cid, exp, kind = c["id"], c["expected"], c["scoring"]
        b, bchk = base[cid]
        r, rchk = cand[cid]
        combined = "REJECT" if "REJECT" in (b, r) else ("N/A" if b == "N/A" else "ACCEPT")
        cls = CLASS.get(rchk.split(":")[0], "") if b == "ACCEPT" and r == "REJECT" else ""
        rows.append((cid, kind, exp, b, bchk, combined, rchk if r == "REJECT" else "-", cls))
        if kind == "candidate" and exp == "REJECT" and b == "ACCEPT" and combined == "REJECT":
            escapes.append((cid, cls))
    print("| Case | Kind | Oracle | Baseline | Baseline checkpoint | Candidate | Candidate relation | Class |")
    print("| - | - | - | - | - | - | - | - |")
    for row in rows:
        print("| " + " | ".join(row) + " |")
    agree_b = sum(1 for r in rows if r[2] != "N/A" and r[2] == r[3])
    agree_c = sum(1 for r in rows if r[2] != "N/A" and r[2] == r[5])
    scored = sum(1 for r in rows if r[2] != "N/A")
    false_rej = [r[0] for r in rows if r[2] == "ACCEPT" and r[5] == "REJECT"]
    print()
    print(f"oracle agreement: baseline {agree_b}/{scored}, candidate {agree_c}/{scored}")
    print(f"false rejections of valid controls by candidate: {false_rej or 'none'}")
    print("candidate-only detections among candidate cases:")
    for cid, cls in escapes:
        print(f"  {cid}: {cls}")


if __name__ == "__main__":
    main()
