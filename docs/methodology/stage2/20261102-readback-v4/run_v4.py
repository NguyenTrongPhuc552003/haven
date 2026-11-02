#!/usr/bin/env python3
"""Regression checks for read-back v4 (RB1, RB2, interrupt priorities).

Usage: run_v4.py <haven-repo> <out-dir> [check ...]
Each check exports a haven commit into a scratch copy, applies its change
(edits that must match exactly once, or a held-out patch), builds it, and
runs read-back v4 (tools/readback/readback.py from the repository's working
tree) on cortex-a72 and max. A check passes when the exit status and the set
of finding kinds equal the expectation. Findings are taken only from
readback.json; an exit status without a report is "no report", never
findings (the v3 runner's RB1 mistake).

These are not held-out cases. Claude wrote the tool change and knew the v3
outcomes of R2, R3, I7 and K3 before writing the expectations; the checks
show that the change does what it was written for and changes nothing else.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = "docs/methodology/stage2/20261102-readback-v4"
V3 = "docs/methodology/stage2/20261102-a1-cases-v3"
GIC = "drivers/irqchip/gic_v3.c"
PART = "src/core/partition.c"
FROZEN = "ce220bb"  # case set v3 freeze (qemu-direct, no partition B timer)
I7 = [(GIC, "\t\t\t    GIC_PRIO_LINUX * 0x01010101U);\n\n\t/* Route all SPIs",
       "\t\t\t    GIC_PRIO_LOWEST * 0x01010101U);\n\n\t/* Route all SPIs")]
B_TIMER = "\t .owner_partition_id = PARTITION_B_ID,\n\t .target_cpu = PART_B_CPU},"
K3 = [(PART, B_TIMER, B_TIMER.replace("PARTITION_B_ID", "PARTITION_A_ID"))]
R4_FINDINGS = [("A RAM", "missing binding"), ("A UART", "missing binding"), ("A RO", "missing binding"),
               ("A PCI config", "missing binding"), ("A PCI BAR", "missing binding"),
               ("pci-1af4:1044", "DMA enforcement drift"), ("SPI 33", "missing interrupt binding")]
# name: (commit, profile, grant, edits, held-out patch, expected exit, expected findings)
CHECKS = {
    "V0": (FROZEN, "qemu-direct", "grant-v4.json", [], None, 0, []),
    "R1": (FROZEN, "qemu-direct", "grant-v4.json", [], "R1", 0, []),
    "R2": (FROZEN, "qemu-direct", "grant-v4.json", [], "R2", 1, [("SPI 33", "interrupt priority drift")]),
    "R3": (FROZEN, "qemu-direct", "grant-v4.json", [], "R3", 2, []),
    "R4": (FROZEN, "qemu-direct", "grant-v4.json", [], "R4", 1, R4_FINDINGS),
    "I7": (FROZEN, "qemu-direct", "grant-v4.json", I7, None, 1, [("SPI 33", "interrupt priority drift")]),
    "W0": ("HEAD", "qemu-psci", "grant-partb-v4.json", [], None, 0, []),
    "W3": ("HEAD", "qemu-psci", "grant-partb-v4.json", K3, None, 1, [("CPU 2 PPI 27", "missing interrupt binding")]),
}
DEVICE = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"
CPUS = ["cortex-a72", "max"]
TOOLCHAIN = "/opt/homebrew/Cellar/aarch64-unknown-linux-gnu/15.2.0/toolchain/bin"
ENV = dict(os.environ, PATH=f"/opt/homebrew/bin:/usr/local/bin:{TOOLCHAIN}:" + os.environ.get("PATH", ""))


def sh(cmd, cwd, log, timeout=900):
    with open(log, "a") as f:
        f.write(f"$ {cmd}\n")
        f.flush()
        try:
            return subprocess.run(cmd, cwd=cwd, shell=True, stdout=f, stderr=subprocess.STDOUT,
                                  env=ENV, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            f.write("TIMEOUT\n")
            return 124


def mutate(src, edits):
    for path, old, new in edits:
        p = os.path.join(src, path)
        s = open(p).read()
        if s.count(old) != 1:
            raise SystemExit(f"edit does not match exactly once in {path} (found {s.count(old)})")
        open(p, "w").write(s.replace(old, new))


def main():
    repo, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    only = sys.argv[3:] or list(CHECKS)
    head = subprocess.check_output(["git", "-C", repo, "rev-parse", "--short", "HEAD"], text=True).strip()
    tool = os.path.join(repo, "tools/readback/readback.py")
    os.makedirs(out, exist_ok=True)
    results = {}
    for name in only:
        commit, profile, grant, edits, patch, want_rc, want = CHECKS[name]
        cdir = os.path.join(out, name)
        os.makedirs(cdir, exist_ok=True)
        log = os.path.join(cdir, "build.log")
        open(log, "w").close()
        src = tempfile.mkdtemp(prefix=f"rbv4-{name}-")
        subprocess.run(f"git -C {repo} archive {commit} | tar -x -C {src}", shell=True, check=True)
        if patch:
            subprocess.run(["git", "apply", os.path.join(repo, V3, "heldout-random", f"{patch}.patch")],
                           cwd=src, check=True)
        mutate(src, edits)
        r = {"check": name, "commit": commit, "profile": profile, "grant": grant, "patch": patch,
             "edited": bool(edits), "expect_rc": want_rc, "expect": want}
        r["build_rc"] = sh(f"cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE={profile} && cmake --build build", src, log)
        for cpu in CPUS:
            c = {"rc": None, "findings": None}
            if r["build_rc"] == 0:
                rb = os.path.join(cdir, f"readback-{cpu}")
                p = subprocess.run([sys.executable, tool, os.path.join(src, "build"), os.path.join(repo, HERE, grant),
                                    rb, "--cpu", cpu, "--wait", "20", "--device", DEVICE, "--marker", "IRQ test done"],
                                   capture_output=True, text=True, env=ENV, timeout=300)
                open(rb + ".txt", "w").write(p.stdout + p.stderr)
                c["rc"] = p.returncode
                try:
                    rep = json.load(open(os.path.join(rb, "readback.json")))
                    c["findings"] = [f[:2] for f in rep["findings"]]
                    c["probe_failure"] = rep.get("probe_failure")
                except (OSError, ValueError, KeyError):
                    pass
            r[cpu] = c
        results[name] = r
        print(name, json.dumps(r), flush=True)
        subprocess.run(["rm", "-rf", src])
    json.dump({"haven_commit": head, "results": results}, open(os.path.join(out, "results.json"), "w"), indent=1)
    rows = [f"# Read-back v4 regression checks (tool at haven {head})", "",
            "| Check | Code | CPU | Exit (expected) | Findings | As expected |", "| - | - | - | - | - | - |"]
    for name, r in results.items():
        for cpu in CPUS:
            c = r[cpu]
            got = c["findings"]
            ok = c["rc"] == r["expect_rc"] and got is not None and \
                sorted({tuple(x) for x in got}) == sorted({tuple(x) for x in r["expect"]})
            shown = "no report" if got is None else ("; ".join(f"{a}: {b}" for a, b in got) or
                                                     (c.get("probe_failure") and "probe failure") or "none")
            code = r["commit"] + (f" + {r['patch']}" if r["patch"] else "") + (" + edit" if r["edited"] else "")
            rows.append(f"| {name} | {code} | {cpu} | {c['rc']} ({r['expect_rc']}) | {shown} | {'yes' if ok else 'no'} |")
    open(os.path.join(out, "results.md"), "w").write("\n".join(rows) + "\n")
    print("\n".join(rows))


if __name__ == "__main__":
    main()
