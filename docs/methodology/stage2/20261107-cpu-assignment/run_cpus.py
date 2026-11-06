#!/usr/bin/env python3
"""Runs X1 to X5 of README.md with read-back v10.

Usage: run_cpus.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT

X1, X2, X4, X5: the Linux profile (case set builder, B0 tree, optional
S1.patch); X3: the qemu-psci demo build with grant-budget.json.
Writes OUT/<run>/{readback.txt,readback.json,uart.log} and OUT/results.md.
"""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "cases", os.path.join(HERE, "..", "20261106-linux-chain-cases", "run_cases.py"))
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)
OLD = os.path.join(HERE, "..", "20261107-readback-v8", "grant-l4-guest.json")
NEW = os.path.join(HERE, "grant-l4-cpus.json")
BUDGET = os.path.join(HERE, "..", "20261102-budgets", "grant-budget.json")
S1 = os.path.relpath(os.path.join(HERE, "S1.patch"), rc.HERE)
DEMO_DEV = "virtio-rng-pci,disable-legacy=on,iommu_platform=on"

RUNS = [  # name, build ("linux", "linux-S1" or "psci"), grant, read point
    ("X1", "linux", OLD, "KDAL-DEMAND-END"),
    ("X2", "linux", NEW, "KDAL-DEMAND-END"),
    ("X3", "psci", BUDGET, "IRQ test done"),
    ("X4", "linux-S1", NEW, "break"),
    ("X5", "linux", NEW, "break"),
]


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cpus-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    ipa = 0x48000000
    dtb = os.path.join(work, "b0.dtb")
    with open(os.path.join(work, "b0.dts"), "w") as fh:
        fh.write(rc.dts(rc.CASES["B0"][2], ipa, ipa + os.path.getsize(cpio)))
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, os.path.join(work, "b0.dts")], check=True)
    builds = {"linux": rc.build(repo, "", os.path.join(work, "linux"), os.path.join(out, "build-linux.log")),
              "linux-S1": rc.build(repo, S1, os.path.join(work, "linux-s1"), os.path.join(out, "build-s1.log"))}
    src = tempfile.mkdtemp(prefix="cpus-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    with open(os.path.join(out, "build-psci.log"), "w") as log:
        subprocess.run(f"cmake --preset arm64-qemu -B {work}/psci -DHAVEN_BOOT_PROFILE=qemu-psci && "
                       f"cmake --build {work}/psci", shell=True, cwd=src, stdout=log, stderr=log, check=True)
    builds["psci"] = os.path.join(work, "psci")
    rows = ["| Run | Build | Grant | Read at | Read-back | CPU table (granted / HAVEN runs) |",
            "| - | - | - | - | - | - |"]
    for name, build, grant, point in RUNS:
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), builds[build], grant,
               os.path.join(d, "rb"), "--cpu", "cortex-a72", "--wait", "45"]
        cmd += ["--break", "0x40000000"] if point == "break" else ["--marker", point]
        if build == "psci":
            cmd += ["--device", DEMO_DEV]
        else:
            cmd += ["--device", rc.DEV, "--guest-a", image, "--load", f"{dtb}@{rc.DTB_PA:#x}",
                    "--load", f"{cpio}@{ipa + rc.PA_OFFSET:#x}"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        with open(os.path.join(d, "readback.txt"), "w") as fh:
            fh.write(r.stdout + r.stderr)
        for f in ("readback.json", "uart.log", "gdb.json"):
            if os.path.exists(os.path.join(d, "rb", f)):
                shutil.copy(os.path.join(d, "rb", f), os.path.join(d, f))
        shutil.rmtree(os.path.join(d, "rb"))
        lines = r.stdout.splitlines()
        res = next((l for l in lines if l.startswith("RESULT")), "no RESULT")
        finds = "; ".join(l.strip().replace("FINDING ", "") for l in lines if "FINDING" in l)
        cpus = ", ".join(l.strip().replace("granted to partition ", "").replace(", HAVEN runs partition ", "/")
                         for l in lines if l.strip().startswith("CPU ") and "HAVEN runs" in l)
        at = next((l for l in lines if l.startswith("read at")), "").replace("read at: ", "")
        rows.append(f"| {name} | {build} | {os.path.basename(grant)} | {at[:40]} | "
                    f"{res}{': ' + finds if finds else ''} | {cpus} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# CPU assignment runs\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)
    shutil.rmtree(src, ignore_errors=True)


if __name__ == "__main__":
    main()
