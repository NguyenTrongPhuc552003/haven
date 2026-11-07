#!/usr/bin/env python3
"""Two faults in the same node, for the third blind test of dt_check.py.

The pair test kept its two faults in different nodes. Here both change the
same node, merged field by field so neither overwrites the other: the KDAL
node's register base (catalogue O5), size (O4) and interrupt (O6); the
memory node's base (O2) with a size (O1); the timer node's secure entry
(O7, PPI 9) with its virtual entry (O7, PPI 10, 12 or 15). The values are
those of the frozen single-fault catalogue (../20261107-dt-check-blind/).

No label is attached here: a merged node can be inside the grant when one
of its faults alone is outside (or the reverse), so each drawn
combination is labelled by hand after the draw and before dt_check runs.

Usage: dt_same.py draw SEED_TEXT BASE.dts OUTDIR [N]
"""
import hashlib
import json
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "20261107-dt-check-blind"))
import dt_faults  # noqa: E402

BASES = ["0x09000800", "0x09010000", "0x10000000", "0x10003000", "0x10003800"]  # O5
SIZES = ["0x800", "0x1001", "0x2000", "0x10000"]  # O4
IRQS = [("SPI 0", "0 0 4"), ("SPI 2", "0 2 4"), ("SPI 74", "0 74 4"), ("SPI 1 edge", "0 1 1")]  # O6
MEM_BASES = [0x40100000, 0x3FF00000]  # O2's changed bases (0x40000000 is T1's own)
MEM_SIZES = [0x10000000, 0x1F000000, 0x20001000, 0x30000000, 0x40000000]  # O1
VTIMER = [10, 12, 15]  # O7, with the secure entry changed to PPI 9


def combos():
    out = []
    for b in BASES:
        for z in SIZES:
            out.append(("KDAL node", f"base {b}, size {z}", ("kdal", b, z, "0 1 4")))
    for b in BASES:
        for name, irq in IRQS:
            out.append(("KDAL node", f"base {b}, {name}", ("kdal", b, "0x1000", irq)))
    for z in SIZES:
        for name, irq in IRQS:
            out.append(("KDAL node", f"size {z}, {name}", ("kdal", "0x09000000", z, irq)))
    for b in MEM_BASES:
        for z in MEM_SIZES:
            out.append(("memory node", f"base {b:#x}, size {z:#x}", ("mem", b, z)))
    for v in VTIMER:
        out.append(("timer node", f"secure PPI 9, virtual PPI {v}", ("timer", [9, 14, v, 10])))
    return out


def apply(base, spec):
    if spec[0] == "kdal":
        _, b, z, irq = spec
        return dt_faults.sub_kdal(base, dt_faults.kdal(base=b, size=z, irq=irq))
    if spec[0] == "mem":
        return dt_faults.mem(spec[1], spec[2])(base)
    return dt_faults.timer(spec[1])(base)


def draw(seed_text, n=12):
    seed = hashlib.sha256(seed_text.encode()).hexdigest()
    rnd = random.Random(int(seed, 16))
    all_c = combos()
    picks = rnd.sample(range(len(all_c)), n)  # without replacement
    return seed, [{"name": f"S{i + 1:02d}", "index": k, "node": all_c[k][0], "change": all_c[k][1],
                   "spec": all_c[k][2]} for i, k in enumerate(picks)]


def main():
    if len(sys.argv) < 5 or sys.argv[1] != "draw":
        sys.exit(__doc__)
    seed_text, base_path, out = sys.argv[2], sys.argv[3], sys.argv[4]
    n = int(sys.argv[5]) if len(sys.argv) > 5 else 12
    with open(base_path) as fh:
        base = fh.read()
    seed, picks = draw(seed_text, n)
    os.makedirs(out, exist_ok=True)
    for p in picks:
        with open(os.path.join(out, p["name"] + ".dts"), "w") as fh:
            fh.write(apply(base, p["spec"]))
    with open(os.path.join(out, "draw.json"), "w") as fh:
        json.dump({"seed_text": seed_text, "seed_sha256": seed, "combinations": len(combos()), "picks": picks},
                  fh, indent=1)
    for p in picks:
        print(f"{p['name']} #{p['index']} {p['node']}: {p['change']}")


if __name__ == "__main__":
    main()
