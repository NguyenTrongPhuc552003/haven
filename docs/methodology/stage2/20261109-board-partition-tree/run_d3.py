#!/usr/bin/env python3
"""Run D3: generate partition A's tree and the seeded trees, check each with dt_check.py.

Usage: run_d3.py LINUX_DTS_DIR

Writes partition-a.dts/.dtb and seeds/ next to this script, and one log per
row of README.md's Expectations table under runs/ (the command, dt_check's
output and its exit status). Run from anywhere; paths are resolved from this
file's location in the haven repository.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
GRANT = os.path.join(HERE, "grant-cixp1-v0.json")
EMULATED = ["--emulated", "0x0E010000+0x10000", "--emulated", "0x0E090000+0x100000"]
ROWS = [("C", "partition-a.dtb")] + [(s, f"seeds/partition-a-{s}.dtb")
                                      for s in ["S1", "S2a", "S2b", "S3", "S3b", "S4", "S5", "S6"]]


def rel(p):
    return os.path.relpath(p, REPO)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    runs = os.path.join(HERE, "runs")
    os.makedirs(runs, exist_ok=True)
    gen = [sys.executable, rel(os.path.join(HERE, "gen_partition_a.py")), sys.argv[1], rel(GRANT), rel(HERE),
           "--seeds"]
    r = subprocess.run(gen, cwd=REPO, capture_output=True, text=True)
    with open(os.path.join(runs, "G.txt"), "w") as f:
        f.write("$ python3 " + " ".join(gen[1:]).replace(sys.argv[1], "<linux-dts-cix/v7.3>") + "\n")
        f.write(r.stdout + r.stderr + f"exit {r.returncode}\n")
    print(f"G: exit {r.returncode}")
    if r.returncode:
        print(r.stdout + r.stderr)
        return 1
    for row, tree in ROWS:
        cmd = ["python3", "tools/readback/dt_check.py", rel(GRANT), rel(os.path.join(HERE, tree)),
               "--partition", "1"] + EMULATED
        c = subprocess.run([sys.executable] + cmd[1:], cwd=REPO, capture_output=True, text=True)
        with open(os.path.join(runs, f"{row}.txt"), "w") as f:
            f.write("$ " + " ".join(cmd) + "\n" + c.stdout + c.stderr + f"exit {c.returncode}\n")
        findings = [ln for ln in c.stdout.splitlines() if ln.strip().startswith("FINDING")]
        infos = [ln for ln in c.stdout.splitlines() if ln.strip().startswith("info")]
        print(f"{row}: exit {c.returncode}, {len(findings)} finding(s), {len(infos)} info")
    return 0


if __name__ == "__main__":
    sys.exit(main())
