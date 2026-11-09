#!/usr/bin/env python3
"""Run D4 part 4: D3's trees through the new dt_check.py (README.md, Expectations).

Usage: run_d4.py

Reads D3's committed trees (../20261109-board-partition-tree/partition-a.dtb
and seeds/*.dtb, not regenerated) and runs dt_check.py in set A (D3's
command, grant v0), set B (grant v1 with MPIDRs, --vgic-intids 352) and row
V (clean tree, grant v1, --vgic-intids 64). Writes the new seed S7 (D3's
partition-a.dts with the GIC's #interrupt-cells set to 2) to seeds/ with
dtc (not -q, stderr kept), checks it, and keeps one log per row in runs/.
For set A it also says whether dt_check's output equals D3's log. Needs
dtc on PATH for S7 only. Run from anywhere.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
D3 = os.path.join(HERE, "..", "20261109-board-partition-tree")
GRANT_V0 = os.path.join(D3, "grant-cixp1-v0.json")
GRANT_V1 = os.path.join(HERE, "grant-cixp1-v1.json")
EMULATED = ["--emulated", "0x0E010000+0x10000", "--emulated", "0x0E090000+0x100000"]
D3_ROWS = ["C", "S1", "S2a", "S2b", "S3", "S3b", "S4", "S5", "S6"]


def rel(p):
    return os.path.relpath(os.path.normpath(p), REPO)


def tree(row):
    return os.path.join(D3, "partition-a.dtb" if row == "C" else f"seeds/partition-a-{row}.dtb")


def run(name, grant, dtb, extra=()):
    cmd = ["python3", "tools/readback/dt_check.py", rel(grant), rel(dtb), "--partition", "1"] + EMULATED
    cmd += list(extra)
    c = subprocess.run([sys.executable] + cmd[1:], cwd=REPO, capture_output=True, text=True)
    with open(os.path.join(HERE, "runs", f"{name}.txt"), "w") as f:
        f.write("$ " + " ".join(cmd) + "\n" + c.stdout + c.stderr + f"exit {c.returncode}\n")
    lines = c.stdout.splitlines()
    nf = sum(1 for ln in lines if ln.strip().startswith("FINDING"))
    ni = sum(1 for ln in lines if ln.strip().startswith("info"))
    return c, f"{name}: exit {c.returncode}, {nf} finding(s), {ni} info"


def same_as_d3(row, out):
    with open(os.path.join(D3, "runs", f"{row}.txt")) as f:
        old = f.read().splitlines()[1:]  # without the command line
    return old == (out.stdout + out.stderr + f"exit {out.returncode}\n").splitlines()


def make_s7():
    src = open(os.path.join(D3, "partition-a.dts")).read()
    gic = "\t\t#interrupt-cells = <3>;"
    if src.count(gic) != 1:
        sys.exit("S7: the GIC's #interrupt-cells line was not found exactly once")
    head = ("// SEEDED FAULT S7 (D4 README.md, Expectations), written by D4's run_d4.py from D3's\n"
            "// partition-a.dts: the GIC's #interrupt-cells set to 2. Not a tree to boot\n")
    src = src.replace("\n\n/ {", "\n" + head + "\n/ {", 1).replace(gic, "\t\t#interrupt-cells = <2>;")
    dts = os.path.join(HERE, "seeds", "partition-a-S7.dts")
    with open(dts, "w") as f:
        f.write(src)
    dtb = dts[:-1] + "b"
    cmd = ["dtc", "-I", "dts", "-O", "dtb", "-o", rel(dtb), rel(dts)]
    c = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    with open(os.path.join(HERE, "runs", "S7-dtc.txt"), "w") as f:
        f.write("$ " + " ".join(cmd) + "\n" + c.stdout + c.stderr + f"exit {c.returncode}\n")
    warnings = [ln for ln in c.stderr.splitlines() if "Warning" in ln]
    print(f"S7-dtc: exit {c.returncode}, {len(warnings)} warning(s)")
    return dtb


def main():
    os.makedirs(os.path.join(HERE, "runs"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "seeds"), exist_ok=True)
    for row in D3_ROWS:  # set A: D3's command, unchanged
        out, line = run(f"A-{row}", GRANT_V0, tree(row))
        print(line, "(output as D3)" if same_as_d3(row, out) else "(output differs from D3)")
    for row in D3_ROWS:  # set B: MPIDRs in the grant, emulated GIC of 352 IDs
        print(run(f"B-{row}", GRANT_V1, tree(row), ["--vgic-intids", "352"])[1])
    print(run("V", GRANT_V1, tree("C"), ["--vgic-intids", "64"])[1])
    print(run("S7", GRANT_V1, make_s7(), ["--vgic-intids", "352"])[1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
