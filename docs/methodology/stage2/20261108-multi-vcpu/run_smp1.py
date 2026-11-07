#!/usr/bin/env python3
"""SMP1 of README.md: Linux on two vCPUs of partition A (CPUs 0 and 1),
partition B on CPU 2, HAVEN built for qemu-psci with HAVEN_PART_A_VCPUS=2.

Usage: [ONLY=F1,R1,R2] run_smp1.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT

Runs:
  F1  free boot until partition A powers off (UART log only)
  R1  read-back with grant-smp.json at the KDAL-DEMAND-END marker
  R2  read-back with grant-smp.json at the entry breakpoint (0x40000000)
Writes OUT/<run>/... and OUT/results.md. HAVEN is built from the
repository's HEAD (git archive), so the code is committed before a run.
"""
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    "cases", os.path.join(HERE, "..", "20261106-linux-chain-cases", "run_cases.py"))
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)
GRANT = os.path.join(HERE, "grant-smp.json")
ONLY = os.environ.get("ONLY", "").split(",") if os.environ.get("ONLY") else None

CPU0 = ('\t\t\tenable-method = "psci";\n\t\t};\n')
CPU1 = ('\t\t\tenable-method = "psci";\n\t\t};\n'
        '\t\tcpu@1 {\n\t\t\tdevice_type = "cpu";\n\t\t\tcompatible = "arm,cortex-a72";\n'
        '\t\t\treg = <0x1>;\n\t\t\tenable-method = "psci";\n\t\t};\n')
GICR1 = "<0x0 0x080a0000 0x0 0x20000>;\t/* GICR, one CPU */"
GICR2 = "<0x0 0x080a0000 0x0 0x40000>;\t/* GICR, two CPUs */"


def smp_dts(s):
    """The case set's B0 tree with a second CPU and its redistributor frame."""
    assert s.count(CPU0) == 1 and s.count(GICR1) == 1
    return s.replace(CPU0, CPU1, 1).replace(GICR1, GICR2, 1)


def build(repo, out, log):
    src = tempfile.mkdtemp(prefix="smp1-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-psci "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2 && cmake --build {out}",
                       shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return out


def free_boot(b, image, dtb, cpio, initrd_pa, uart, limit=120):
    """As run_cases.boot, but waits for CPU 0's own stop line."""
    cmd = ["qemu-system-aarch64", "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
           "-cpu", "cortex-a72", "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none",
           "-nic", "none", "-device", rc.DEV, "-serial", f"file:{uart}",
           "-device", f"loader,file={b}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
           "-device", f"loader,file={image},addr=0x80800000,force-raw=on",
           "-device", f"loader,file={dtb},addr={rc.DTB_PA:#x},force-raw=on",
           "-device", f"loader,file={cpio},addr={initrd_pa:#x},force-raw=on",
           "-device", f"loader,file={b}/guest_b.bin,addr=0xA0800000,force-raw=on"]
    q = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < limit and q.poll() is None:
        text = open(uart, errors="replace").read() if os.path.exists(uart) else ""
        if "partition stopped by PSCI" in text or "EL2 FATAL" in text:
            time.sleep(1.0)
            break
        time.sleep(0.25)
    q.kill()
    q.wait()
    return round(time.time() - t0, 1)


def pick(text, pat, n=4):
    return [l.strip() for l in text.splitlines() if re.search(pat, l)][:n]


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="smp1-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(ksrc, "scripts/ci/kunit-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    ipa = 0x48000000
    dts = os.path.join(out, "partition-a-smp.dts")
    with open(dts, "w") as fh:
        fh.write(smp_dts(rc.dts(rc.CASES["B0"][2], ipa, ipa + os.path.getsize(cpio))))
    dtb = os.path.join(work, "smp.dtb")
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
    b = build(repo, os.path.join(work, "build"), os.path.join(out, "build.log"))
    lines = []
    if not ONLY or "F1" in ONLY:
        d = os.path.join(out, "F1")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        uart = os.path.join(d, "uart.log")
        secs = free_boot(b, image, dtb, cpio, ipa + rc.PA_OFFSET, uart)
        t = open(uart, errors="replace").read()
        lines += [f"## F1 free boot ({secs} s)", ""]
        for label, pat in [("Linux CPUs", r"smp: Brought up|CPU1: Booted|failed to come online"),
                           ("KDAL", r"KDAL-DEMAND-END"),
                           ("HAVEN vCPU start", r"partition A has|CPU_ON|CPU 1 ready|vCPU 1"),
                           ("Delivered", r"virtual interrupts delivered"),
                           ("Stops", r"partition stopped|parked|did not stop|EL2 FATAL"),
                           ("Partition B", r"partition B started|B result|not started"),
                           ("Not delivered", r"not delivered")]:
            lines.append(f"- {label}: " + (" / ".join(f"`{x}`" for x in pick(t, pat, 6)) or "none"))
        lines.append("")
    for name, point in (("R1", "KDAL-DEMAND-END"), ("R2", "break")):
        if ONLY and name not in ONLY:
            continue
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), b, GRANT,
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
        lines += [f"## {name} read-back at {point}", ""]
        lines += [f"- `{x}`" for x in pick(text, r"RESULT|CPU [0-9]: |finding|drift|missing", 12)]
        lines.append("")
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# SMP1 runs (generated by run_smp1.py)\n\n" + "\n".join(lines))
    print("\n".join(lines))
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
