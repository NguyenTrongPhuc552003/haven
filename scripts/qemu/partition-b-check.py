#!/usr/bin/env python3
"""Boot a qemu-psci HAVEN build and check that partition B runs on CPU 2.

Usage: scripts/qemu/partition-b-check.py [build-dir] [cpu] [seconds]
  build-dir  directory with haven.bin, guest_a.bin, guest_b.bin, guest_b.elf
             (default: build; configure with -DHAVEN_BOOT_PROFILE=qemu-psci)
  cpu        QEMU CPU model (default: cortex-a72)
  seconds    time limit (default: 30)

Partition B has no UART.  It reports through guest_b_status in its own RAM
(tests/demos/guest_b_timer.c); this script reads the record over QMP while
the guests run.  The symbol's address in guest_b.elf is an IPA; partition
B's stage-2 maps IPA 0x60000000 to PA 0xA0800000 (PART_B_IPA_BASE and
PART_B_PA_BASE in src/platform/qemu-virt/memory.h, and the loader address
below).

Writes <build-dir>/evidence/partition-b-<cpu>-uart.log and
partition-b-<cpu>-status.json; exits 0 only when every check passes.
Needs qemu-system-aarch64 and an aarch64 nm on PATH.
"""

import json
import os
import shutil
import socket
import subprocess
import sys
import time

PART_B_IPA_BASE = 0x60000000
PART_B_PA_BASE = 0xA0800000
GUEST_B_MAGIC = 0x48564E42
FIELDS = ("magic", "stage", "timer_irqs", "other_irqs", "last_intid", "result",
          "duty_permille", "max_gap_us", "gaps",  # duty: reported, not judged here
          "lat_samples", "lat_max_ns", "lat_avg_ns",  # latency probe: reported only
          "lat_lt10us", "lat_lt100us", "lat_lt1ms", "lat_lt10ms", "lat_ge10ms", "sctlr_el1")
RESULTS = {0: "none", 1: "pass", 2: "too few timer interrupts", 3: "other interrupts"}
TIMER_IRQS = 8


class QMP:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.s.settimeout(10)
        self.f = self.s.makefile("rw")
        self.f.readline()
        self.cmd("qmp_capabilities")

    def cmd(self, name, **args):
        msg = {"execute": name}
        if args:
            msg["arguments"] = args
        self.f.write(json.dumps(msg) + "\n")
        self.f.flush()
        while True:
            r = json.loads(self.f.readline())
            if "event" not in r:
                return r.get("return")

    def read32(self, pa):
        out = self.cmd("human-monitor-command", **{"command-line": f"xp /1wx {pa:#x}"})
        return int(out.split()[-1], 16)


def symbol(nm, elf, name):
    """Address of name in elf. A prebuilt symbol list next to the ELF
    (<elf without .elf>.nm, the output of nm) is used when present, so the
    try-it bundle needs no cross binutils."""
    listing = os.path.splitext(elf)[0] + ".nm"
    if os.path.exists(listing):
        lines = open(listing).read().splitlines()
    else:
        if not nm:
            raise SystemExit(f"no aarch64 nm and no {listing} for {elf}")
        lines = subprocess.check_output([nm, elf], text=True).splitlines()
    for line in lines:
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit(f"partition-b-check: {name} not found in {elf}")


def read_status(m, status_pa):
    """Partition B's status record (tests/demos/guest_b_timer.c) at PA status_pa."""
    return dict(zip(FIELDS, (m.read32(status_pa + 4 * i) for i in range(len(FIELDS)))))


def b_checks(log, status):
    """The ten partition B checks, as (name, ok) pairs; also used by
    uefi-boot-check.py, which boots the same images through the UEFI loader."""
    denied_b = [l for l in log.splitlines() if "Denied stage-2 violation" in l and "IPA=0x40000000" in l]
    return [
        ("CPU 2 ready, partition B started", "CPU 2 ready, partition B started" in log),
        ("partition B IRQs assigned", "partition B IRQs assigned" in log),
        ("partition A: IRQ test done", "PARTITION_A: IRQ test done" in log),
        ("B's read of A's IPA denied on CPU 2", any("cpu=2" in l for l in denied_b)),
        ("B status magic", status["magic"] == GUEST_B_MAGIC),
        ("B reached stage 5 (done)", status["stage"] == 5),
        (f"B timer interrupts >= {TIMER_IRQS}", status["timer_irqs"] >= TIMER_IRQS),
        ("B saw no other interrupts", status["other_irqs"] == 0),
        ("B result: %s" % RESULTS.get(status["result"], "?"), status["result"] == 1),
        ("no undelivered interrupt", "not delivered" not in log),
    ]


def main():
    build = sys.argv[1] if len(sys.argv) > 1 else "build"
    cpu = sys.argv[2] if len(sys.argv) > 2 else "cortex-a72"
    limit = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
    for f in ("haven.bin", "guest_a.bin", "guest_b.bin"):
        if not os.path.isfile(os.path.join(build, f)):
            print(f"partition-b-check: missing {build}/{f} (build with the qemu-psci profile first)")
            return 2
    nm = shutil.which("aarch64-unknown-linux-gnu-nm") or shutil.which("aarch64-linux-gnu-nm")
    have_lists = os.path.isfile(os.path.join(build, "guest_b.nm"))
    if not (nm or have_lists) or not shutil.which("qemu-system-aarch64"):
        print("partition-b-check: needs qemu-system-aarch64, and an aarch64 nm or prebuilt .nm lists")
        return 2

    status_pa = symbol(nm, os.path.join(build, "guest_b.elf"), "guest_b_status") - PART_B_IPA_BASE + PART_B_PA_BASE
    try:  # Haven's per-CPU budget statistics (arch/arm64/budget_hw.c), if built in
        budget_pa = symbol(nm, os.path.join(build, "haven.elf"), "hv_cpu_budget")
    except SystemExit:
        budget_pa = None
    out = os.path.join(build, "evidence")
    os.makedirs(out, exist_ok=True)
    uart = os.path.join(out, f"partition-b-{cpu}-uart.log")
    if os.path.exists(uart):
        os.remove(uart)
    sock = f"/tmp/partb-{os.getpid()}.sock"  # macOS limits socket paths to 104 bytes
    q = subprocess.Popen([
        "qemu-system-aarch64",
        "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
        "-cpu", cpu, "-smp", "4", "-m", "2G", "-display", "none", "-monitor", "none", "-nic", "none",
        "-serial", f"file:{uart}", "-qmp", f"unix:{sock},server,nowait",
        "-device", f"loader,file={build}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
        "-device", f"loader,file={build}/guest_a.bin,addr=0x80800000,force-raw=on",
        "-device", f"loader,file={build}/guest_b.bin,addr={PART_B_PA_BASE:#x},force-raw=on",
    ])
    status = dict.fromkeys(FIELDS, 0)
    budgets = {}
    log = ""
    try:
        t0 = time.time()
        while not os.path.exists(sock) and time.time() - t0 < 10:
            time.sleep(0.05)
        m = QMP(sock)
        while time.time() - t0 < limit and q.poll() is None:
            status = read_status(m, status_pa)
            log = open(uart, errors="replace").read() if os.path.exists(uart) else ""
            if status["stage"] == 5 and "PARTITION_A: IRQ test done" in log:
                break
            time.sleep(0.25)
        time.sleep(0.5)  # let the last HAVEN lines reach the log
        budgets = {}
        if budget_pa is not None and q.poll() is None:
            for c in (0, 2):  # struct cpu_budget: 5 x u64, then 4 x u32 (56 bytes)
                base = budget_pa + 56 * c
                lo = lambda off: m.read32(base + off) | (m.read32(base + off + 4) << 32)
                budgets[c] = {"budget_ticks": lo(8), "max_late_ticks": lo(32),
                              "active": m.read32(base + 40), "throttles": m.read32(base + 44),
                              "missed": m.read32(base + 48)}
    finally:
        q.kill()
        q.wait()
        if os.path.exists(sock):
            os.remove(sock)
    log = open(uart, errors="replace").read() if os.path.exists(uart) else ""
    status["status_pa"] = status_pa
    rec = {k: (hex(v) if k in ("magic", "status_pa") else v) for k, v in status.items()}
    rec["haven_budgets"] = budgets
    json.dump(rec, open(os.path.join(out, f"partition-b-{cpu}-status.json"), "w"), indent=1)

    print(f"---- UART ({uart}) ----")
    print(log, end="")
    print("---------------------")
    print("partition B status @PA %#x: %s" % (status_pa, ", ".join(
        f"{k}={status[k]:#x}" if k == "magic" else f"{k}={status[k]}" for k in FIELDS)))
    for c, b in budgets.items():  # reported, not judged here (QEMU timing)
        print(f"Haven budget CPU {c}: active={b['active']} throttles={b['throttles']} "
              f"missed={b['missed']} max_late_ticks={b['max_late_ticks']}")
    checks = b_checks(log, status)
    for name, ok in checks:
        print(("ok      " if ok else "MISSING ") + name)
    ok = all(c for _, c in checks)
    print(f"partition-b-check: {'PASS' if ok else 'FAIL'} ({cpu})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
