#!/usr/bin/env python3
"""Runs V1 to V6 of README.md with read-back v8 and grant-l4-guest.json.

Usage: rerun_v8.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT

Reuses the frozen Linux chain case set's helpers (build from HEAD plus a
patch, device tree per case, boot); only the grant and the read moment
differ. Writes OUT/<run>/{uart.log,readback.txt,readback.json} and
OUT/results.md.
"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "cases", os.path.join(HERE, "..", "20261106-linux-chain-cases", "run_cases.py"))
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)
GRANT = os.path.join(HERE, "grant-l4-guest.json")

RUNS = [  # name, case of the frozen set, marker
    ("V1", "B0", "KDAL-DEMAND-END"),
    ("V2", "T4", "KDAL-DEMAND-END"),
    ("V3", "T5", "KDAL-DEMAND-END"),
    ("V4", "H3", "KDAL-DEMAND-END"),
    ("V5", "B0", "Booting Linux"),
    ("V6", "H3", "Booting Linux"),
]


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="v8-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    builds, rows = {}, ["| Run | Case | Read at | Read-back | Guest-enabled interrupts | Denied lines in log |",
                        "| - | - | - | - | - | - |"]
    for name, case, marker in RUNS:
        _, change, p = rc.CASES[case]
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        key = p.get("patch", "")
        if key not in builds:
            builds[key] = rc.build(repo, key, os.path.join(work, f"b{len(builds)}"), os.path.join(d, "build.log"))
        b = builds[key]
        ipa = p.get("initrd", 0x48000000)
        open(os.path.join(d, "partition-a.dts"), "w").write(rc.dts(p, ipa, ipa + os.path.getsize(cpio)))
        dtb = os.path.join(work, f"{name}.dtb")
        subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, os.path.join(d, "partition-a.dts")], check=True)
        r = subprocess.run([sys.executable, os.path.join(repo, "tools/readback/readback.py"), b, GRANT,
                            os.path.join(d, "rb"), "--cpu", "cortex-a72", "--device", rc.DEV, "--wait", "45",
                            "--marker", marker, "--guest-a", image, "--load", f"{dtb}@{rc.DTB_PA:#x}",
                            "--load", f"{cpio}@{ipa + rc.PA_OFFSET:#x}"], capture_output=True, text=True)
        open(os.path.join(d, "readback.txt"), "w").write(r.stdout + r.stderr)
        for f in ("readback.json", "uart.log"):
            if os.path.exists(os.path.join(d, "rb", f)):
                shutil.copy(os.path.join(d, "rb", f), os.path.join(d, f))
        shutil.rmtree(os.path.join(d, "rb"))
        log = open(os.path.join(d, "uart.log"), errors="replace").read() if os.path.exists(os.path.join(d, "uart.log")) else ""
        res = next((l for l in r.stdout.splitlines() if l.startswith("RESULT")), "no RESULT")
        finds = "; ".join(l.strip() for l in r.stdout.splitlines() if "FINDING" in l)
        ge = "; ".join(l.strip().replace("guest-enabled ", "") for l in r.stdout.splitlines() if "guest-enabled INTID" in l)
        at = next((l for l in r.stdout.splitlines() if l.startswith("read at")), "")
        rows.append(f"| {name} | {case} | {at.replace('read at: ', '')} | {res}{': ' + finds if finds else ''} | {ge} | "
                    f"{len(re.findall('Denied stage-2', log))} |")
        print(rows[-1], flush=True)
    open(os.path.join(out, "results.md"), "w").write("# Read-back v8 runs\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
