#!/usr/bin/env python3
"""E0 of haven docs/methodology/stage2/20261124-lr-overflow: run_n.py's
free boots with the probe build, N in (2, 3), LRS in (0 = all, 2), REPS each.
Usage: e0.py HAVEN KDAL IMAGE KO OUT REPS"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

haven, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
reps = int(sys.argv[6])
spec = importlib.util.spec_from_file_location(
    "run_n", os.path.join(haven, "docs/methodology/stage2/20261112-multi-vcpu-n/run_n.py"))
rn = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rn)
LRS = {"v": "0"}


def build(repo, bout, log, n):
    shutil.rmtree(bout, ignore_errors=True)
    src = tempfile.mkdtemp(prefix="e0-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {bout} -DHAVEN_BOOT_PROFILE=qemu-psci "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS={n} "
                       f"-DHAVEN_TEST_LRS={LRS['v']} && cmake --build {bout}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return bout


rn.build = build
os.makedirs(out, exist_ok=True)
work = tempfile.mkdtemp(prefix="e0-")
cpio = rn.initramfs(ksrc, ko, work)
rows = []
for lrs in os.environ.get("E0_LRS", "0 2").split():
    LRS["v"] = lrs
    for n in [int(x) for x in os.environ.get("E0_N", "2 3").split()]:
        d = os.path.join(out, f"L{lrs}-N{n}")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        b, dtb = rn.prepare(haven, cpio, n, d, work)
        for r in range(reps):
            uart = os.path.join(d, f"uart{r}.log")
            secs = rn.boot(b, image, dtb, cpio, uart)
            t = open(uart, errors="replace").read()
            cpus = re.findall(r"smp: Brought up 1 node, (\d+) CPU", t)
            e0 = re.findall(r"E0 CPU (\d+) LRs (\d+) dup-pending (\d+) dup-active (\d+) "
                            r"hw-no-lr (\d+) sgi-left (\d+) last-dup (0x[0-9a-f]+)", t)
            nd = re.findall(r"(?:IRQ|PPI) \d+ (?:on CPU \d+ )?not delivered", t)
            rows.append(f"| {'all' if lrs == '0' else lrs} | {n} | {r} | {secs} | "
                        f"{cpus[0] if cpus else 'none'} | {'yes' if 'KDAL-DEMAND-END' in t else 'no'} | "
                        + "; ".join(f"CPU {c}: P {p} A {a} hw {h} sgi {s} last {x}" for c, _, p, a, h, s, x in e0)
                        + f" | {'; '.join(sorted(set(nd))) or 'none'} |")
            print(rows[-1], flush=True)
with open(os.path.join(out, "results.md"), "w") as fh:
    fh.write("| LRs | vCPUs | run | s | Linux CPUs | demand | E0 per CPU (dup Pending, dup Active, "
             "hw no LR, SGIs left) | not delivered |\n| - | - | - | - | - | - | - | - |\n"
             + "\n".join(rows) + "\n")
shutil.rmtree(work, ignore_errors=True)
