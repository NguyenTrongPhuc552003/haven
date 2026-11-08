#!/usr/bin/env python3
"""SMP2 of README.md: read-back v12 on the two-vCPU Linux setup, and the
seeded faults F-CPU3, F-SGI and F-REFUSE (patches/).

Usage: [ONLY=V1,V2,...] run_smp2.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT

HAVEN is built from the repository's HEAD (git archive) plus the run's
patch, with qemu-psci and HAVEN_PART_A_VCPUS=2. Writes OUT/<run>/ and
OUT/results.md. FB also builds the host tests with its patch and runs
test_sysreg_trap.
"""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


smp1 = load("smp1", os.path.join(HERE, "run_smp1.py"))
rc = smp1.rc
V12 = os.path.join(HERE, "grant-smp-v12.json")
SMP = os.path.join(HERE, "grant-smp.json")
RUNS = [  # name, patch, grant, read point
    ("V1", None, V12, "break"),
    ("V2", None, V12, "KDAL-DEMAND-END"),
    ("V3", None, SMP, "break"),
    ("FA", "F-CPU3", V12, "KDAL-DEMAND-END"),
    ("FB", "F-SGI", V12, "KDAL-DEMAND-END"),
    ("FC", "F-REFUSE", V12, "KDAL-DEMAND-END"),
]
ONLY = os.environ.get("ONLY", "").split(",") if os.environ.get("ONLY") else None


def tree(repo, patch, log):
    src = tempfile.mkdtemp(prefix="smp2-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    if patch:
        subprocess.run(["git", "apply", os.path.join(HERE, "patches", patch + ".patch")], cwd=src, check=True)
    return src


def build(src, out, log):
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-psci "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2 && cmake --build {out}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    return out


def host_test(src, log):
    with open(log, "w") as fh:
        subprocess.run("cmake --preset host-tests -B hb && cmake --build hb --target test_sysreg_trap",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    exe = subprocess.run(["find", os.path.join(src, "hb"), "-name", "test_sysreg_trap", "-type", "f"],
                         capture_output=True, text=True).stdout.split()[0]
    r = subprocess.run([exe], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="smp2-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    ipa = 0x48000000
    dtb = os.path.join(work, "smp.dtb")
    dts = os.path.join(work, "smp.dts")
    with open(dts, "w") as fh:
        fh.write(smp1.smp_dts(rc.dts(rc.CASES["B0"][2], ipa, ipa + os.path.getsize(cpio))))
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
    builds, lines = {}, []
    for name, patch, grant, point in RUNS:
        if ONLY and name not in ONLY:
            continue
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        lines += [f"## {name} ({patch or 'no patch'}, {os.path.basename(grant)}, read at {point})", ""]
        if patch not in builds:
            src = tree(repo, patch, None)
            builds[patch] = build(src, os.path.join(work, f"build-{patch or 'base'}"), os.path.join(d, "build.log"))
            if patch == "F-SGI":
                code, text = host_test(src, os.path.join(d, "host-build.log"))
                with open(os.path.join(d, "test_sysreg_trap.txt"), "w") as fh:
                    fh.write(text)
                lines.append(f"- host test `test_sysreg_trap`: exit {code}; "
                             + " / ".join(f"`{x}`" for x in smp1.pick(text, r"FAIL|PASS|passed|failed", 6)))
            shutil.rmtree(src, ignore_errors=True)
        cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), builds[patch], grant,
               os.path.join(d, "rb"), "--cpu", "cortex-a72", "--wait", "90", "--device", rc.DEV,
               "--guest-a", image, "--load", f"{dtb}@{rc.DTB_PA:#x}", "--load", f"{cpio}@{ipa + rc.PA_OFFSET:#x}"]
        cmd += ["--break", "0x40000000"] if point == "break" else ["--marker", point]
        r = subprocess.run(cmd, capture_output=True, text=True)
        text = (r.stdout + r.stderr).replace(work, "<tmp>").replace(repo, "<workspace>/haven")
        with open(os.path.join(d, "readback.txt"), "w") as fh:
            fh.write(text)
        for f in ("readback.json", "uart.log", "gdb.json"):
            if os.path.exists(os.path.join(d, "rb", f)):
                shutil.copy(os.path.join(d, "rb", f), os.path.join(d, f))
        shutil.rmtree(os.path.join(d, "rb"))
        uart = open(os.path.join(d, "uart.log"), errors="replace").read() if os.path.exists(os.path.join(d, "uart.log")) else ""
        lines += [f"- `{x}`" for x in smp1.pick(text, r"RESULT|FINDING|CPU [0-9]: granted|waiting|trigger|time limit", 16)]
        lines.append("- UART: " + (" / ".join(f"`{x}`" for x in smp1.pick(
            uart, r"smp: Brought up|failed to|CPU1: Booted|KDAL-DEMAND-END|vCPU 1|CPU_ON|not delivered|EL2 FATAL", 8)) or "none"))
        lines.append("")
        print("\n".join(lines[-20:]), flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# SMP2 runs (generated by run_smp2.py)\n\n" + "\n".join(lines))
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
