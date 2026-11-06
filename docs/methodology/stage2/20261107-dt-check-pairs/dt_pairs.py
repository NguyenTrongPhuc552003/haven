#!/usr/bin/env python3
"""Pairs of device-tree faults for the second blind test of dt_check.py.

Uses the frozen catalogue of the single-fault test
(../20261107-dt-check-blind/dt_faults.py, unchanged). Operators that edit
the same part of the tree would overwrite each other, so they are grouped
by what they edit, and a pair takes two different groups. A pair is
labelled outside the grant if either fault is.

Usage: dt_pairs.py draw SEED_TEXT BASE.dts OUTDIR [N]
"""
import hashlib
import json
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "20261107-dt-check-blind"))
import dt_faults  # noqa: E402

GROUPS = {  # what each operator edits
    "memory node": ["O1", "O2"],
    "initramfs range": ["O3"],
    "KDAL node": ["O4", "O5", "O6", "O10"],
    "timer node": ["O7"],
    "CPU nodes": ["O8"],
    "extra node": ["O9"],
}


def apply_pair(base, pair):
    t = base
    for f in pair:
        t = dt_faults.apply(t, f["op"], f["index"])
    return t


def draw(seed_text, n=12):
    seed = hashlib.sha256(seed_text.encode()).hexdigest()
    rnd = random.Random(int(seed, 16))
    groups = list(GROUPS)
    out = []
    for i in range(n):
        pair = []
        for g in rnd.sample(groups, 2):
            op = rnd.choice(GROUPS[g])
            idx = rnd.randrange(len(dt_faults.CATALOGUE[op][1]))
            p = dt_faults.CATALOGUE[op][1][idx]
            pair.append({"group": g, "op": op, "index": idx, "operator": dt_faults.CATALOGUE[op][0],
                         "parameter": p[0], "label": p[2], "reason": p[3]})
        out.append({"name": f"P{i + 1:02d}", "faults": pair, "label": any(f["label"] for f in pair)})
    return seed, out


def main():
    if len(sys.argv) < 5 or sys.argv[1] != "draw":
        sys.exit(__doc__)
    seed_text, base_path, out = sys.argv[2], sys.argv[3], sys.argv[4]
    n = int(sys.argv[5]) if len(sys.argv) > 5 else 12
    with open(base_path) as fh:
        base = fh.read()
    seed, pairs = draw(seed_text, n)
    os.makedirs(out, exist_ok=True)
    for p in pairs:
        with open(os.path.join(out, p["name"] + ".dts"), "w") as fh:
            fh.write(apply_pair(base, p["faults"]))
    with open(os.path.join(out, "draw.json"), "w") as fh:
        json.dump({"seed_text": seed_text, "seed_sha256": seed, "pairs": pairs}, fh, indent=1)
    for p in pairs:
        a, b = p["faults"]
        print(f"{p['name']} [{a['op']} {a['parameter']}: {a['label']}] + [{b['op']} {b['parameter']}: {b['label']}]"
              f" -> {p['label']}")


if __name__ == "__main__":
    main()
