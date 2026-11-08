#!/usr/bin/env python3
"""Runs C1 to C4 of README.md: HAVEN built with HAVEN_CONSOLE_UART=1 (its
console on QEMU virt's second PL011), the two-vCPU Linux setup with the
CPU hotplug init of ../20261108-multi-vcpu/.

Usage: [ONLY=C1,...] run_console.py HAVEN_REPO KDAL_SRC IMAGE KDAL_KO OUT
Writes OUT/<run>/{linux-uart.log,haven-uart.log} (C1 to C3) and
OUT/C4/ (read-back), then OUT/results.md with check_lines.py's verdicts.
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
MV = os.path.join(HERE, "..", "20261108-multi-vcpu")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


smp1 = load("smp1", os.path.join(MV, "run_smp1.py"))
lines_mod = load("check_lines", os.path.join(HERE, "..", "20261108-printk-lock", "check_lines.py"))
rc = smp1.rc
GRANT = os.path.join(MV, "grant-smp-v12.json")
ONLY = os.environ.get("ONLY", "").split(",") if os.environ.get("ONLY") else None


def build(repo, out, log):
    src = tempfile.mkdtemp(prefix="con-src-")
    subprocess.run(f"git -C {repo} archive HEAD | tar -x -C {src}", shell=True, check=True)
    with open(log, "w") as fh:
        subprocess.run(f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-psci "
                       f"-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2 -DHAVEN_CONSOLE_UART=1 "
                       f"&& cmake --build {out}", shell=True, cwd=src, stdout=fh, stderr=fh, check=True)
    shutil.rmtree(src)
    return out


def boot(b, image, dtb, cpio, initrd_pa, linux_log, haven_log, limit=120):
    cmd = ["qemu-system-aarch64", "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
           "-cpu", "cortex-a72", "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none",
           "-nic", "none", "-device", rc.DEV, "-serial", f"file:{linux_log}", "-serial", f"file:{haven_log}",
           "-device", f"loader,file={b}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
           "-device", f"loader,file={image},addr=0x80800000,force-raw=on",
           "-device", f"loader,file={dtb},addr={rc.DTB_PA:#x},force-raw=on",
           "-device", f"loader,file={cpio},addr={initrd_pa:#x},force-raw=on",
           "-device", f"loader,file={b}/guest_b.bin,addr=0xA0800000,force-raw=on"]
    q = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    while time.time() - t0 < limit and q.poll() is None:
        text = open(haven_log, errors="replace").read() if os.path.exists(haven_log) else ""
        if "partition stopped by PSCI" in text or "EL2 FATAL" in text:
            time.sleep(1.0)
            break
        time.sleep(0.25)
    q.kill()
    q.wait()
    return round(time.time() - t0, 1)


def judge(linux_log, haven_log):
    h = open(haven_log, errors="replace").read()
    li = open(linux_log, errors="replace").read()
    foreign = [l for l in h.splitlines() if l.strip() and not l.startswith("HAVEN")
               and l.strip() != "Haven hypervisor starting"]
    r = lines_mod.check(haven_log)
    return {"haven_lines": r["haven"], "broken": len(r["broken"]), "foreign_in_haven_log": len(foreign),
            "foreign_examples": foreign[:3], "haven_in_linux_log": li.count("HAVEN"),
            "marker": "KDAL-DEMAND-END" in li, "hotplug": "HOTPLUG-ON ok, online: 0-1" in li,
            "stop_lines": len(re.findall(r"partition stopped", h))}


def main():
    repo, ksrc, image, ko, out = (os.path.abspath(x) for x in sys.argv[1:6])
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="con-")
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(MV, "hotplug-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(ksrc, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    ipa = 0x48000000
    dts, dtb = os.path.join(work, "smp.dts"), os.path.join(work, "smp.dtb")
    with open(dts, "w") as fh:
        fh.write(smp1.smp_dts(rc.dts(rc.CASES["B0"][2], ipa, ipa + os.path.getsize(cpio))))
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
    b = build(repo, os.path.join(work, "build"), os.path.join(out, "build.log"))
    rows = ["| Run | Result |", "| - | - |"]
    for name in ("C1", "C2", "C3"):
        if ONLY and name not in ONLY:
            continue
        d = os.path.join(out, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        secs = boot(b, image, dtb, cpio, ipa + rc.PA_OFFSET, os.path.join(d, "linux-uart.log"),
                    os.path.join(d, "haven-uart.log"))
        j = judge(os.path.join(d, "linux-uart.log"), os.path.join(d, "haven-uart.log"))
        rows.append(f"| {name} ({secs} s) | {j} |")
        print(rows[-1], flush=True)
    if not ONLY or "C4" in ONLY:
        d = os.path.join(out, "C4")
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        cmd = [sys.executable, os.path.join(repo, "tools/readback/readback.py"), b, GRANT, os.path.join(d, "rb"),
               "--cpu", "cortex-a72", "--wait", "90", "--device", rc.DEV, "--haven-uart",
               "--guest-a", image, "--load", f"{dtb}@{rc.DTB_PA:#x}", "--load", f"{cpio}@{ipa + rc.PA_OFFSET:#x}",
               "--marker", "KDAL-DEMAND-END"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        text = (r.stdout + r.stderr).replace(work, "<tmp>").replace(repo, "<workspace>/haven")
        open(os.path.join(d, "readback.txt"), "w").write(text)
        for f in ("readback.json", "uart.log", "haven-uart.log"):
            if os.path.exists(os.path.join(d, "rb", f)):
                shutil.copy(os.path.join(d, "rb", f), os.path.join(d, f))
        shutil.rmtree(os.path.join(d, "rb"))
        res = [l.strip() for l in text.splitlines() if re.search(r"RESULT|FINDING|CPU [0-9]: granted", l)]
        rows.append(f"| C4 | {' / '.join(res)} |")
        print(rows[-1], flush=True)
    with open(os.path.join(out, "results.md"), "w") as fh:
        fh.write("# Console runs (generated by run_console.py)\n\n" + "\n".join(rows) + "\n")
    shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
