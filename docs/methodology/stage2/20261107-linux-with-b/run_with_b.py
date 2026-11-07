#!/usr/bin/env python3
"""Runs Y1 to Y3 of README.md: the Linux profile built for qemu-psci, so
HAVEN can start partition B on CPU 2 while Linux runs in partition A.

Usage: run_with_b.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT
Writes OUT/<run>/{readback.txt,readback.json,uart.log,gdb.json} and
OUT/results.md.
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
GRANT = os.path.join(HERE, "..", "20261107-cpu-assignment", "grant-l4-cpus.json")

RUNS = [  # name, profile, read point
    ("Y1", "qemu-psci", "KDAL-DEMAND-END"),
    ("Y2", "qemu-psci", "break"),
    ("Y3", "qemu-direct", "KDAL-DEMAND-END"),
]


def build(repo, profile, out, log):
    src = tempfile.mkdtemp(prefix="withb-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE={profile} "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 && cmake --build {out}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return out


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="withb-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    ipa = 0x48000000
    dts = os.path.join(work, "b0.dts")
    with open(dts, "w") as fh:
        fh.write(rc.dts(rc.CASES["B0"][2], ipa, ipa + os.path.getsize(cpio)))
    dtb = os.path.join(work, "b0.dtb")
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
    builds = {p: build(repo, p, os.path.join(work, p), os.path.join(out, f"build-{p}.log"))
              for p in ("qemu-psci", "qemu-direct")}
    rows = ["| Run | Profile | Read at | Read-back | CPU table (granted / runs) | Partition B | Linux reached |",
            "| - | - | - | - | - | - | - |"]
    for name, profile, point in RUNS:
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), builds[profile], GRANT,
               os.path.join(d, "rb"), "--cpu", "cortex-a72", "--wait", "45", "--device", rc.DEV,
               "--guest-a", image, "--load", f"{dtb}@{rc.DTB_PA:#x}", "--load", f"{cpio}@{ipa + rc.PA_OFFSET:#x}"]
        cmd += ["--break", "0x40000000"] if point == "break" else ["--marker", point]
        r = subprocess.run(cmd, capture_output=True, text=True)
        with open(os.path.join(d, "readback.txt"), "w") as fh:
            fh.write(r.stdout + r.stderr)
        for f in ("readback.json", "uart.log", "gdb.json"):
            if os.path.exists(os.path.join(d, "rb", f)):
                shutil.copy(os.path.join(d, "rb", f), os.path.join(d, f))
        shutil.rmtree(os.path.join(d, "rb"))
        with open(os.path.join(d, "uart.log"), errors="replace") as fh:
            log = fh.read()
        lines = r.stdout.splitlines()
        res = next((l for l in lines if l.startswith("RESULT")), "no RESULT")
        finds = "; ".join(l.strip().replace("FINDING ", "") for l in lines if "FINDING" in l)
        cpus = ", ".join(l.strip().replace("granted to partition ", "").replace(", HAVEN runs partition ", "/")
                         for l in lines if l.strip().startswith("CPU ") and "HAVEN runs" in l)
        at = next((l for l in lines if l.startswith("read at")), "").replace("read at: ", "")
        b = "started" if "partition B started" in log else ("inactive" if "partition B inactive" in log else "?")
        reached = "KDAL-DEMAND-END" if "KDAL-DEMAND-END" in log else ("Booting Linux" if "Booting Linux" in log
                                                                       else "no Linux output")
        rows.append(f"| {name} | {profile} | {at[:40]} | {res}{': ' + finds if finds else ''} | {cpus} | {b} | {reached} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# Linux with partition B runs\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
