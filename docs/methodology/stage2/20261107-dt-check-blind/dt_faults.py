#!/usr/bin/env python3
"""Device-tree faults for the blind test of tools/readback/dt_check.py.

Each operator changes one thing in the case set's T1 tree (KDAL's node over
the granted UART; matches grant-l4.json). Each parameter carries a label,
written by hand from grant-l4.json and dt_check's stated specification
(README.md), not from its code: True = the tree uses something the grant
does not give partition 1 (dt_check should report it), False = it does not.

Usage: dt_faults.py draw SEED_TEXT BASE.dts OUTDIR [N]
Writes OUTDIR/draw.json and OUTDIR/Mxx.dts (the faults applied to BASE).
The runner re-applies the drawn faults to the tree it boots.
"""
import hashlib
import json
import os
import random
import re
import sys

KDAL_RE = re.compile(r"\tkdal-uart@9000000 \{.*?\n\t\};\n", re.S)
MEM = "reg = <0x0 0x40000000 0x0 0x20000000>;"
TIMER = "interrupts = <1 13 4>, <1 14 4>, <1 11 4>, <1 10 4>;"
CPU0_END = '\t\t\tenable-method = "psci";\n\t\t};\n'


def kdal(base="0x09000000", size="0x1000", irq="0 1 4", name="kdal-uart@9000000"):
    return (f"\t{name} {{\n\t\tcompatible = \"kdal,device\";\n"
            f"\t\treg = <0x0 {base} 0x0 {size}>;\n\t\tinterrupts = <{irq}>;\n\t}};\n")


def sub_kdal(t, block):
    assert KDAL_RE.search(t)
    return KDAL_RE.sub(lambda m: block, t, count=1)


def mem(base, size):
    return lambda t: t.replace(MEM, f"reg = <0x0 {base:#x} 0x0 {size:#x}>;")


def initrd(delta):
    def f(t):
        def shift(m):
            return f"{m.group(1)}<0x0 {int(m.group(2), 16) + delta:#x}>;"
        return re.sub(r"(linux,initrd-(?:start|end) = )<0x0 (0x[0-9a-f]+)>;", shift, t)
    return f


def timer(entries):
    return lambda t: t.replace(TIMER, "interrupts = " + ", ".join(f"<1 {e} 4>" for e in entries) + ";")


def cpus(n):
    extra = "".join(f'\t\tcpu@{i} {{\n\t\t\tdevice_type = "cpu";\n\t\t\tcompatible = "arm,cortex-a72";\n'
                    f'\t\t\treg = <{i:#x}>;\n\t\t\tenable-method = "psci";\n\t\t}};\n' for i in range(1, n + 1))
    return lambda t: t.replace(CPU0_END, CPU0_END + extra, 1)


def add_node(base, size, status=None):
    blk = (f"\textra@{int(base, 16):x} {{\n\t\tcompatible = \"test,extra\";\n"
           f"\t\treg = <0x0 {base} 0x0 {size}>;\n" + (f'\t\tstatus = "{status}";\n' if status else "") + "\t};\n")

    def f(t):
        i = t.rstrip().rindex("};")
        return t[:i] + "\n" + blk + "};\n"
    return f


def reorder(t):
    m = KDAL_RE.search(t)
    t = t[:m.start()] + t[m.end():]
    i = t.index("\tpl011@9000000 {")
    return t[:i] + m.group(0) + "\n" + t[i:]


# operator: (description, [(parameter, transform, label, reason)])
CATALOGUE = {
    "O1": ("memory node size", [
        ("256 MiB", mem(0x40000000, 0x10000000), False, "inside the 512 MiB granted"),
        ("496 MiB", mem(0x40000000, 0x1F000000), False, "inside"),
        ("512 MiB + 4 KiB", mem(0x40000000, 0x20001000), True, "one page past granted RAM"),
        ("768 MiB", mem(0x40000000, 0x30000000), True, "past granted RAM"),
        ("1 GiB", mem(0x40000000, 0x40000000), True, "past granted RAM (as T4)")]),
    "O2": ("memory node base and size", [
        ("0x40000000 + 511 MiB", mem(0x40000000, 0x1FF00000), False, "inside"),
        ("0x40100000 + 511 MiB", mem(0x40100000, 0x1FF00000), False, "ends exactly at 0x60000000"),
        ("0x40100000 + 512 MiB", mem(0x40100000, 0x20000000), True, "1 MiB past 0x60000000"),
        ("0x3FF00000 + 512 MiB", mem(0x3FF00000, 0x20000000), True, "starts 1 MiB below granted RAM")]),
    "O3": ("initramfs range shifted", [
        ("-0x09000000", initrd(-0x09000000), True, "starts at 0x3F000000, below granted RAM"),
        ("+0x01000000", initrd(0x01000000), False, "inside"),
        ("+0x17000000", initrd(0x17000000), False, "ends below 0x60000000"),
        ("+0x17F00000", initrd(0x17F00000), True, "crosses 0x60000000")]),
    "O4": ("KDAL node register size", [
        ("0x800", lambda t: sub_kdal(t, kdal(size="0x800")), False, "inside the granted UART page"),
        ("0x1001", lambda t: sub_kdal(t, kdal(size="0x1001")), True, "one byte past the UART page"),
        ("0x2000", lambda t: sub_kdal(t, kdal(size="0x2000")), True, "second page not granted (as T2)"),
        ("0x10000", lambda t: sub_kdal(t, kdal(size="0x10000")), True, "not granted")]),
    "O5": ("KDAL node register base", [
        ("0x09000800", lambda t: sub_kdal(t, kdal(base="0x09000800")), True, "ends 0x800 past the UART page"),
        ("0x09010000", lambda t: sub_kdal(t, kdal(base="0x09010000")), True, "PL031 RTC, not granted"),
        ("0x10000000", lambda t: sub_kdal(t, kdal(base="0x10000000")), False, "inside the granted PCI BAR (DEVICE_RW)"),
        ("0x10003000", lambda t: sub_kdal(t, kdal(base="0x10003000")), False, "last page of the PCI BAR"),
        ("0x10003800", lambda t: sub_kdal(t, kdal(base="0x10003800")), True, "crosses the end of the PCI BAR")]),
    "O6": ("KDAL node interrupt", [
        ("SPI 0", lambda t: sub_kdal(t, kdal(irq="0 0 4")), True, "INTID 32 not granted"),
        ("SPI 2", lambda t: sub_kdal(t, kdal(irq="0 2 4")), True, "INTID 34 not granted"),
        ("SPI 74", lambda t: sub_kdal(t, kdal(irq="0 74 4")), True, "INTID 106 belongs to partition 0 (HAVEN)"),
        ("SPI 1 edge", lambda t: sub_kdal(t, kdal(irq="0 1 1")), False, "same INTID 33, only the trigger differs")]),
    "O7": ("timer node entries", [
        ("virtual timer PPI 10", timer([13, 14, 10, 10]), True, "INTID 26 is HAVEN's, not partition A's"),
        ("virtual timer PPI 12", timer([13, 14, 12, 10]), True, "INTID 28 not granted (as T5)"),
        ("virtual timer PPI 15", timer([13, 14, 15, 10]), True, "INTID 31 not granted"),
        ("secure timer PPI 9", timer([9, 14, 11, 10]), False, "only the virtual timer entry is used by an EL1 guest (spec)")]),
    "O8": ("extra CPU nodes", [
        ("1 more (2 CPUs)", cpus(1), False, "gic.cpus gives partition 1 CPUs 0 and 1"),
        ("2 more (3 CPUs)", cpus(2), True, "partition 1 has 2 CPUs"),
        ("3 more (4 CPUs)", cpus(3), True, "partition 1 has 2 CPUs")]),
    "O9": ("extra device node", [
        ("GPIO 0x09030000, okay", add_node("0x09030000", "0x1000"), True, "PL061 not granted"),
        ("GPIO 0x09030000, disabled", add_node("0x09030000", "0x1000", "disabled"), False, "disabled nodes are not used"),
        ("PCI BAR page 0x10001000", add_node("0x10001000", "0x1000"), False, "inside the PCI BAR"),
        ("0x70000000 (A RO)", add_node("0x70000000", "0x1000"), True, "A RO is Normal memory, not a device region")]),
    "O10": ("neutral edit", [
        ("KDAL node before the PL011", reorder, False, "same content"),
        ("KDAL node renamed kdal@9000000", lambda t: sub_kdal(t, kdal(name="kdal@9000000")), False, "same content"),
        ("bootargs quiet", lambda t: t.replace("rdinit=/init", "rdinit=/init quiet"), False, "no resource change"),
        ("KDAL node removed", lambda t: sub_kdal(t, ""), False, "fewer resources")]),
}


def apply(base, op, idx):
    t = CATALOGUE[op][1][idx][1](base)
    assert t != base, (op, idx)
    return t


def draw(seed_text, n=12):
    seed = hashlib.sha256(seed_text.encode()).hexdigest()
    rnd = random.Random(int(seed, 16))
    ops = sorted(CATALOGUE, key=lambda k: int(k[1:]))
    out = []
    for i in range(n):
        op = rnd.choice(ops)
        idx = rnd.randrange(len(CATALOGUE[op][1]))
        p = CATALOGUE[op][1][idx]
        out.append({"name": f"M{i + 1:02d}", "op": op, "index": idx, "operator": CATALOGUE[op][0],
                    "parameter": p[0], "label": p[2], "reason": p[3]})
    return seed, out


def main():
    if len(sys.argv) < 5 or sys.argv[1] != "draw":
        sys.exit(__doc__)
    seed_text, base_path, out = sys.argv[2], sys.argv[3], sys.argv[4]
    n = int(sys.argv[5]) if len(sys.argv) > 5 else 12
    with open(base_path) as fh:
        base = fh.read()
    seed, faults = draw(seed_text, n)
    os.makedirs(out, exist_ok=True)
    for f in faults:
        with open(os.path.join(out, f["name"] + ".dts"), "w") as fh:
            fh.write(apply(base, f["op"], f["index"]))
    with open(os.path.join(out, "draw.json"), "w") as fh:
        json.dump({"seed_text": seed_text, "seed_sha256": seed, "faults": faults}, fh, indent=1)
    for f in faults:
        print(f"{f['name']} {f['op']} {f['parameter']}: label {f['label']} ({f['reason']})")


if __name__ == "__main__":
    main()
