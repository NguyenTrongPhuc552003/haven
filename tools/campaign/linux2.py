#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Scenario 2 of the campaign: Linux in partition A with two vCPUs.

docs/methodology/stage2/20261119-evaluation-campaign/s2/README.md. The
setup is SMP3's (docs/methodology/stage2/20261108-multi-vcpu/run_smp3.py):
HAVEN built with qemu-psci, HAVEN_PART_A_DTB_IPA and two vCPUs; the
pinned ELMOS SDK kernel; hotplug-init.c as /init (vCPU 1 off and on by
PSCI, then KDAL's module and its demand, then power-off); the grant
grant-smp-v12.json. run_case.py calls methods() for the scenario's
M2, M4, M5 and M6 (M1 and M3 do not boot and are shared).

  M2  the free boot's UART: linux-partition-check.py's markers for N = 2
      and the two hotplug markers
  M4  n/a: partition B's check has no Linux mode (B's start is in M2)
  M5  KDAL's demand against the grant (tools/readback/demand_check.py);
      the setup's own unmet demand (KNOWN_DEMAND) is the baseline, so a
      finding is a demand-check result that differs from it
  M6  read-back at KDAL-DEMAND-END on cortex-a72
"""
import importlib.util
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SMP = os.path.join(REPO, "docs", "methodology", "stage2", "20261108-multi-vcpu")
GRANT = os.path.join(SMP, "grant-smp-v12.json")
HOTPLUG = ("HOTPLUG-OFF ok", "HOTPLUG-ON ok")
IPA = 0x48000000  # partition A's initramfs (IPA), as run_smp3.py
# The unchanged setup's demand check (s2/README.md, "M5 baseline"): KDAL's
# GPIO driver asks for a device the SMP grant does not give partition A.
KNOWN_DEMAND = (
    "kdal-gpio@9030000: unmet MMIO demand (0x9030000-0x9031000 not granted to partition 1)",
    "kdal-gpio@9030000: unmet interrupt demand (INTID 39 not granted to partition 1)",
)


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _smp1():
    return _load("smp1", os.path.join(SMP, "run_smp1.py"))


def _lpc():
    return _load("lpc", os.path.join(REPO, "scripts", "qemu", "linux-partition-check.py"))


def m2_verdict(text):
    """The free boot's UART text into M2's (verdict, evidence)."""
    bad = _lpc().problems(text, 2) + [f"missing: {m}" for m in HOTPLUG if m not in text]
    return ("finding", bad) if bad else ("pass", ["Linux, two vCPUs, hotplug, demand, power-off, B started"])


def demand(text):
    """KDAL's demand (JSON text) from the UART, or None."""
    m = re.search(r"KDAL-DEMAND-BEGIN\n(.*?)\nKDAL-DEMAND-END", text.replace("\r", ""), re.S)
    return m.group(1) if m else None


def m5_verdict(rc, output):
    """demand_check.py's exit code and output into M5's (verdict, evidence)."""
    found = [line.strip()[len("FINDING "):] for line in output.splitlines() if line.strip().startswith("FINDING ")]
    if rc not in (0, 1):
        return "error", [f"demand_check exit {rc}"]
    new = [f for f in found if f not in KNOWN_DEMAND]
    gone = [f for f in KNOWN_DEMAND if f not in found]
    if new or gone:
        return "finding", [f"new: {f}" for f in new] + [f"no longer reported: {f}" for f in gone]
    return "pass", ["demand as the unchanged setup's (KDAL's GPIO not granted)"]


def build(src, out, log):
    cmd = (f"cmake --preset arm64-qemu -B {out} -DHAVEN_BOOT_PROFILE=qemu-psci "
           f"-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2 && cmake --build {out}")
    with open(log, "w") as f:
        return subprocess.run(cmd, shell=True, cwd=src, stdout=f, stderr=subprocess.STDOUT).returncode == 0


def guest_files(work, kdal, ko):
    """The hotplug init's initramfs and partition A's DTB, as run_smp3.py."""
    smp1 = _smp1()
    init, cpio = os.path.join(work, "init"), os.path.join(work, "initramfs.cpio")
    subprocess.run(["clang", "--target=aarch64-linux-gnu", "-ffreestanding", "-nostdlib", "-static", "-O2",
                    "-fno-stack-protector", "-fuse-ld=lld", "-Wl,-e,_start", "-o", init,
                    os.path.join(SMP, "hotplug-init.c")], check=True)
    subprocess.run([sys.executable, os.path.join(kdal, "scripts/ci/mkcpio.py"), cpio, init, ko], check=True)
    dts, dtb = os.path.join(work, "smp.dts"), os.path.join(work, "smp.dtb")
    with open(dts, "w") as f:
        f.write(smp1.smp_dts(smp1.rc.dts(smp1.rc.CASES["B0"][2], IPA, IPA + os.path.getsize(cpio))))
    subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", dtb, dts], check=True)
    return cpio, dtb


def methods(src, cdir, image, kdal, ko, m6_verdict):
    """M2, M4, M5 and M6 of one case; m6_verdict is run_case.py's."""
    smp1 = _smp1()
    b = os.path.join(src, "build-linux")
    if not build(src, b, os.path.join(cdir, "build-linux.log")):
        failed = {"verdict": "finding", "evidence": ["Linux-profile build failed"]}
        return {"M2": failed, "M4": {"verdict": "n/a", "evidence": ["no Linux mode"]},
                "M5": {"verdict": "error", "evidence": ["build failed: no demand"]},
                "M6": {"verdict": "error", "evidence": ["build failed: nothing to read"]}}
    cpio, dtb = guest_files(src, kdal, ko)
    initrd_pa = IPA + smp1.rc.PA_OFFSET
    uart = os.path.join(cdir, "uart-linux2.log")
    smp1.free_boot(b, image, dtb, cpio, initrd_pa, uart)
    text = open(uart, errors="replace").read()
    v, ev = m2_verdict(text)
    res = {"M2": {"verdict": v, "evidence": ev},
           "M4": {"verdict": "n/a", "evidence": ["partition B's check has no Linux mode; B's start is in M2"]}}
    d = demand(text)
    if d is None:
        res["M5"] = {"verdict": "error", "evidence": ["no demand printed (the boot did not reach it)"]}
    else:
        path = os.path.join(cdir, "demand.json")
        with open(path, "w") as f:
            f.write(d + "\n")
        r = subprocess.run([sys.executable, os.path.join(REPO, "tools/readback/demand_check.py"), GRANT, path,
                            "--partition", "1"], capture_output=True, text=True)
        v, ev = m5_verdict(r.returncode, r.stdout)
        res["M5"] = {"verdict": v, "evidence": ev}
    out = os.path.join(cdir, "m6-linux2")
    log = os.path.join(cdir, "m6-linux2.log")
    start = time.monotonic()
    with open(log, "w") as f:
        try:
            rc = subprocess.run([sys.executable, os.path.join(REPO, "tools/readback/readback.py"), b, GRANT, out,
                                 "--cpu", "cortex-a72", "--wait", "90", "--device", smp1.rc.DEV, "--guest-a", image,
                                 "--load", f"{dtb}@{smp1.rc.DTB_PA:#x}", "--load", f"{cpio}@{initrd_pa:#x}",
                                 "--marker", "KDAL-DEMAND-END"], stdout=f, stderr=subprocess.STDOUT,
                                timeout=400).returncode
        except subprocess.TimeoutExpired:
            rc = 124
    try:
        with open(os.path.join(out, "readback.json")) as f:
            report = json.load(f)
    except (OSError, ValueError):
        report = None
    v, e = m6_verdict(rc, report, log)
    res["M6"] = {"verdict": v, "evidence": [f"cortex-a72: {e}"], "seconds": round(time.monotonic() - start, 1)}
    return res
