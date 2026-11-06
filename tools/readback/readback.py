#!/usr/bin/env python3
"""External read-back of HAVEN's enforced state in QEMU (probe v7).

Boots a qemu-direct HAVEN build, stops the machine and reads, from outside
HAVEN:
  - stage-2: every partition's tables (found through pt_roots), decoded with
    the Arm VMSAv8-64 stage-2 format (4 KiB granule, 39-bit IPA, start level 1);
  - SMMU (when the grant file has an "smmu" section): SMMU_CR0ACK, GBPA and the
    stream table found through SMMU_STRTAB_BASE, plus every DMA-capable device
    QEMU emulates, classified as denied, translated, bypassed or unenforced
    (not behind any SMMU). Every stream entry that lets DMA through must be
    granted. A translated stream's table is walked wherever S2TTB points:
    a partition's root (the device then reaches that whole partition) or a
    table of its own, which is attributed to the partition that owns the
    physical memory it maps. Its reach, addresses and access rights are
    compared with the window the grant gives the device. Which buses sit
    behind the SMMU comes from QEMU's own device tree, not from HAVEN.
It compares what it reads with the grant and reports drift.

Changes in v10 (2026-11-07, dt_check blind test, M01): HAVEN's per-CPU
partition table (symbol cpu_partition in arch/arm64/irq.c: the partition
each CPU runs, 0 for none, used to decide which partition an interrupt is
injected into) is read and compared with the grant's gic.cpus. A CPU
granted to one partition but run for another, or for none, is CPU
assignment drift. Without the symbol the check is skipped.

Changes in v9 (2026-11-07, read-back v8 result V6): --break ADDR reads
at a fixed instruction instead of after a UART line. QEMU starts paused
with its GDB stub on a local socket; read-back sets a breakpoint at ADDR
(for example partition A's entry, IPA 0x40000000) over the GDB remote
protocol, resumes, and reads when a CPU stops there. A UART marker is
seen some time after the guest printed it (V6: about 90 ms of guest time);
a breakpoint has no such delay. The emulation in QEMU only; on hardware
the same read point needs a debugger.

Changes in v8 (2026-11-07, Linux chain case set v1, finding 3): the
grant's "gic" section may list "guest_enabled" per CPU: interrupts HAVEN
enables only when the guest enables them in its emulated GIC (Linux
profile). For those, read-back reads the emulated GIC's own enable bits
from HAVEN's memory (symbol by_cpu, hv_vgic.enabled) and compares them
with the hardware: both off is "waiting for the guest" (information);
enabled by the guest but off in the GIC is a missing binding; on in the
GIC but never enabled by the guest is enable drift. Without the model
(no symbol or no model on that CPU) the old rule applies.

Changes in v7 (2026-11-03, RB3): the priority of a granted interrupt that
is not enabled is no longer checked; its "missing interrupt binding" already
says it is not set up (v6 reported both).

Changes in v6 (2026-11-02, budgets enforced by the EL2 timer): the grant's
"gic" section may list "haven_ppis" per CPU (HAVEN's own private interrupts,
such as the EL2 timer, PPI 26). They may be enabled, must be enabled
(otherwise "missing interrupt binding": the enforcement mechanism is off),
and must have HAVEN's priority.

Changes in v5 (2026-11-02, after FINDINGS A1 option A): HAVEN's own EL2
stage-1 table (found through hv_el2_ttbr) is walked. Partition memory that
the stage-2 tables map as Normal must not appear in it ("partition memory
mapped at EL2"), and every EL2 mapping must be an identity mapping. EL2
translation off is reported as an observation. AttrIndx is read with HAVEN's
documented MAIR_EL2 (0xff04: 0 Device-nGnRE, 1 Normal WB), because QEMU's
monitor does not expose MAIR_EL2: an assumption, stated in the report.

Changes in v4 (2026-11-02, after the random held-out cases and partition B):
  - RB1: a partition whose stage-2 root is zero or outside HAVEN's image
    (HAVEN halted before stage-2 setup) is a probe failure (exit 2) with a
    report, not a crash;
  - RB2: a granted PPI that is not enabled on its CPU is a missing interrupt
    binding, as for SPIs;
  - priorities: when the grant's "gic" section has "priority" ({"haven": P,
    "partition": P}), every granted SPI and PPI is checked against it
    (GICD_IPRIORITYR, GICR_IPRIORITYR); a partition interrupt that does not
    match is interrupt priority drift. This covers the I7 and R2 blind spot.
v3 is reproducible from haven ce220bb (frozen in case set v3).

Changes in v3 (after the SM1 to SM3 fixes): stream tables that are not a
partition root are walked and attributed by memory ownership; window
association and access are checked; the command queue registers are read and
a queue error is a finding; --smmu-stage selects QEMU's SMMU model. QEMU 9.2
and later give the virt machine's SMMU both stages ("nested", the default
here; hw/arm/virt.c sets it and overrides -global); "1" and "2" use the
virt-9.1 machine, whose SMMU honours the stage property (default stage 1).
Also in v3: the SMMU event queue (what the SMMU stopped) is decoded and
reported as observations; a virtio PCI device without
VIRTIO_F_ACCESS_PLATFORM (QEMU iommu_platform=false) is an unenforced DMA
master, because QEMU lets it reach physical memory without the SMMU; and
--marker chooses the UART line after which the memory is read.
GIC (when the grant has a "gic" section): GICD_CTLR, and per SPI the
enable, group and IROUTER, plus each redistributor's PPI enables, compared
with the granted interrupt owners and the CPUs each partition runs on.

Changes from v1 (frozen in docs/methodology/stage1/20261101-a1-readback):
the memory is read when partition A reaches its post-fault line or after a
time limit, whichever comes first, so a stalled guest still gets a verdict;
SMMU read-back; extra QEMU devices; command-line options.

Usage:
  readback.py BUILD GRANT.json OUT [--cpu CPU] [--nm NM] [--wait SECONDS]
              [--device QEMU-DEVICE ...] [--smmu-stage nested|1|2] [--marker TEXT]
              [--guest-a FILE] [--load FILE@ADDR ...] [--break ADDR]
Exit status: 0 = enforced state matches the grant, 1 = findings, 2 = probe failure.
Needs qemu-system-aarch64, an aarch64 nm and dtc on PATH (or --nm).
"""
import argparse
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time

import grant as grant_file  # tools/readback/grant.py

IMG_PA, IMG_SIZE = 0x80000000, 0x800000
SMMU_BASE = 0x09050000
GICD_BASE, GICR_BASE, GICR_STRIDE, NCPUS = 0x08000000, 0x080A0000, 0x20000, 4  # QEMU virt, -smp 4
MARKER = "Post-fault check OK"  # default; --marker overrides
EVT_NAMES = {0x02: "bad StreamID", 0x03: "STE fetch fault", 0x04: "bad STE", 0x06: "stream disabled",
             0x10: "translation fault", 0x11: "address size fault", 0x12: "access fault",
             0x13: "permission fault"}
LEVEL_SHIFT = {1: 30, 2: 21, 3: 12}
EXPECT = {  # portable attributes -> stage-2 fields (MM1 proposal, section 4)
    "RWX": {"s2ap": 3, "memattr": 0xF, "sh": 3, "xn": 0},
    "RW": {"s2ap": 3, "memattr": 0xF, "sh": 3, "xn": 2},
    "R": {"s2ap": 1, "memattr": 0xF, "sh": 3, "xn": 2},
    "RX": {"s2ap": 1, "memattr": 0xF, "sh": 3, "xn": 0},
    "DEVICE_RW": {"s2ap": 3, "memattr": 0x1, "sh": 0, "xn": 2},
    "DEVICE_R": {"s2ap": 1, "memattr": 0x1, "sh": 0, "xn": 2},
}
STE_CONFIG = {0: "abort", 4: "bypass", 5: "stage-1", 6: "stage-2", 7: "stage-1+2"}
DMA_ACCESS = {"R": 1, "W": 2, "RW": 3}  # grant access -> S2AP of a DMA table
MACHINE = "virt,virtualization=on,gic-version=3,iommu=smmuv3"


# ---------------------------------------------------------------- QEMU access
class GDB:
    """Minimal GDB remote protocol client for QEMU's stub (v9): enough to
    set a breakpoint, resume and wait for the stop."""

    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.buf = b""
        self.log = []

    def _recv(self, timeout):
        self.s.settimeout(timeout)
        while True:
            m = re.search(rb"\$([^#]*)#[0-9a-fA-F]{2}", self.buf)
            if m:
                self.buf = self.buf[m.end():]
                self.s.sendall(b"+")
                return m.group(1).decode()
            chunk = self.s.recv(4096)
            if not chunk:
                raise ConnectionError("GDB stub closed the connection")
            self.buf += chunk

    def send(self, data, timeout=10.0):
        pkt = f"${data}#{sum(data.encode()) % 256:02x}".encode()
        self.s.sendall(pkt)
        reply = self._recv(timeout)
        self.log.append({"send": data, "reply": reply})
        return reply

    def break_and_run(self, addr, timeout):
        """Breakpoint at addr, continue; returns (stop reply, reached)."""
        if self.send(f"Z1,{addr:x},4") != "OK" and self.send(f"Z0,{addr:x},4") != "OK":
            raise RuntimeError("the GDB stub refused the breakpoint")
        self.s.sendall(f"$c#{ord('c'):02x}".encode())
        try:
            reply = self._recv(timeout)
            reached = True
        except socket.timeout:
            self.s.sendall(b"\x03")  # interrupt: stop where it is
            reply = self._recv(10.0)
            reached = False
        self.log.append({"send": "c", "reply": reply, "reached": reached})
        return reply, reached


class QMP:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.s.settimeout(20)
        self.f = self.s.makefile("rw")
        self.log = [json.loads(self.f.readline())]
        self.cmd("qmp_capabilities")

    def cmd(self, name, **args):
        msg = {"execute": name}
        if args:
            msg["arguments"] = args
        self.f.write(json.dumps(msg) + "\n")
        self.f.flush()
        while True:
            line = self.f.readline()
            if not line:
                return None
            r = json.loads(line)
            if "event" in r:
                continue
            self.log.append({"cmd": msg, "reply": r})
            return r.get("return")

    def hmp(self, line):
        return self.cmd("human-monitor-command", **{"command-line": line}) or ""

    def read32(self, pa):
        out = self.hmp(f"xp /1wx {pa:#x}")
        return int(out.split()[-1], 16)

    def read64(self, pa):
        out = self.hmp(f"xp /1gx {pa:#x}")
        return int(out.split()[-1], 16)

    def read_words(self, pa, n, size="w"):
        """n consecutive 32-bit (size w) or 64-bit (size g) MMIO registers,
        one access each: a multi-value xp reads device memory in wider
        chunks, which the GIC model answers with zeros (observed, QEMU 11.1)."""
        step = 4 if size == "w" else 8
        return [(self.read32 if size == "w" else self.read64)(pa + i * step) for i in range(n)]


def qemu_args(build, cpu, devices, extra, smmu_stage="nested", guest_a=None, loads=()):
    machine = MACHINE if smmu_stage == "nested" else MACHINE.replace("virt,", "virt-9.1,", 1)
    args = ["qemu-system-aarch64", "-machine", machine + extra, "-cpu", cpu, "-smp", "4",
            "-m", "2G", "-display", "none", "-nic", "none"]
    if smmu_stage == "2":
        args += ["-global", "arm-smmuv3.stage=2"]
    for d in devices:
        args += ["-device", d]
    return args + [
        "-device", f"loader,file={build}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
        "-device", f"loader,file={guest_a or build + '/guest_a.bin'},addr=0x80800000,force-raw=on",
        "-device", f"loader,file={build}/guest_b.bin,addr=0xA0800000,force-raw=on"] + [
        a for f, addr in loads for a in ("-device", f"loader,file={f},addr={addr},force-raw=on")]


def parse_load(spec):
    """--load FILE@ADDR: an extra file at a physical address (for example a
    Linux guest's device tree and initramfs inside partition A's RAM)."""
    f, sep, addr = spec.rpartition("@")
    if not sep or not f:
        raise argparse.ArgumentTypeError(f"--load wants FILE@ADDR, got {spec!r}")
    int(addr, 0)
    return f, addr


def device_tree(build, cpu, devices, out, smmu_stage):
    """QEMU's own hardware description for this machine (dumpdtb)."""
    dtb = os.path.join(out, "machine.dtb")
    subprocess.run(qemu_args(build, cpu, devices, f",dumpdtb={dtb}", smmu_stage), capture_output=True, timeout=60)
    dts = subprocess.run(["dtc", "-q", "-I", "dtb", "-O", "dts", dtb], capture_output=True, text=True).stdout
    open(os.path.join(out, "machine.dts"), "w").write(dts)
    return dts


def capture(args_ns, out):
    sock = f"/tmp/readback-{os.getpid()}.sock"  # macOS limits socket paths to 104 bytes
    uart = os.path.join(out, "uart.log")
    for p in (sock, uart):
        if os.path.exists(p):
            os.remove(p)
    gsock = f"/tmp/readback-{os.getpid()}.gdb"
    cmd = qemu_args(args_ns.build, args_ns.cpu, args_ns.device, "", args_ns.smmu_stage,
                    args_ns.guest_a, args_ns.load) + [
        "-serial", f"file:{uart}", "-qmp", f"unix:{sock},server,nowait"]
    if args_ns.break_at is not None:  # v9: start paused, GDB stub on a socket
        if os.path.exists(gsock):
            os.remove(gsock)
        cmd += ["-S", "-chardev", f"socket,id=gdb0,path={gsock},server=on,wait=off", "-gdb", "chardev:gdb0"]
    open(os.path.join(out, "qemu-command.txt"), "w").write(" ".join(cmd) + "\n")
    q = subprocess.Popen(cmd)
    snap = {"trigger": None}
    try:
        t0 = time.time()
        while not os.path.exists(sock) and time.time() - t0 < 10:
            time.sleep(0.05)
        if args_ns.break_at is not None:
            while not os.path.exists(gsock) and time.time() - t0 < 10:
                time.sleep(0.05)
            g = GDB(gsock)
            reply, reached = g.break_and_run(args_ns.break_at, args_ns.wait)
            snap["gdb"] = g.log
            snap["trigger"] = (f"breakpoint at {args_ns.break_at:#x} (stop reply {reply})" if reached else
                               f"time limit ({args_ns.wait:g} s, breakpoint at {args_ns.break_at:#x} not reached)")
        while args_ns.break_at is None and time.time() - t0 < args_ns.wait:
            if os.path.exists(uart) and args_ns.marker in open(uart, errors="replace").read():
                snap["trigger"] = "marker"
                break
            if q.poll() is not None:
                break
            time.sleep(0.2)
        else:
            if args_ns.break_at is None:
                snap["trigger"] = f"time limit ({args_ns.wait:g} s, partition A did not print {args_ns.marker!r})"
        if q.poll() is not None:
            return None
        m = QMP(sock)
        m.cmd("stop")
        img = os.path.join(out, "haven-image.bin")
        m.cmd("pmemsave", val=IMG_PA, size=IMG_SIZE, filename=os.path.abspath(img))
        snap["image"] = open(img, "rb").read()
        snap["smmu"] = {
            "cr0": m.read32(SMMU_BASE + 0x20), "cr0ack": m.read32(SMMU_BASE + 0x24),
            "gbpa": m.read32(SMMU_BASE + 0x44), "strtab_base": m.read64(SMMU_BASE + 0x80),
            "strtab_cfg": m.read32(SMMU_BASE + 0x88), "idr0": m.read32(SMMU_BASE + 0x0),
            "gerror": m.read32(SMMU_BASE + 0x60), "cmdq_base": m.read64(SMMU_BASE + 0x90),
            "cmdq_prod": m.read32(SMMU_BASE + 0x98), "cmdq_cons": m.read32(SMMU_BASE + 0x9C),
            "evtq_base": m.read64(SMMU_BASE + 0xA0), "evtq_prod": m.read32(SMMU_BASE + 0x100A8),
            "evtq_cons": m.read32(SMMU_BASE + 0x100AC)}
        sm = snap["smmu"]
        if sm["cr0ack"] & 1:
            size = (1 << (sm["strtab_cfg"] & 0x3F)) * 64
            base = sm["strtab_base"] & 0x000FFFFFFFFFFFC0
            stab = os.path.join(out, "stream-table.bin")
            m.cmd("pmemsave", val=base, size=size, filename=os.path.abspath(stab))
            snap["stream_table"] = open(stab, "rb").read()
        snap["gic"] = read_gic(m)
        snap["pci"] = m.cmd("query-pci") or []
        snap["qtree"] = m.hmp("info qtree")
        json.dump(m.log, open(os.path.join(out, "qmp.json"), "w"), indent=1)
        if "gdb" in snap:
            json.dump(snap["gdb"], open(os.path.join(out, "gdb.json"), "w"), indent=1)
        m.cmd("quit")
        q.wait(timeout=10)
    finally:
        if q.poll() is None:
            q.kill()
        if os.path.exists(sock):
            os.remove(sock)
    return snap


def read_gic(m):
    """GICv3 distributor and redistributor state (QEMU virt addresses)."""
    ctlr, typer = m.read32(GICD_BASE), m.read32(GICD_BASE + 0x4)
    nirq = min(((typer & 0x1F) + 1) * 32, 1020)
    g = {"ctlr": ctlr, "typer": typer, "nirq": nirq,
         "igroupr": m.read_words(GICD_BASE + 0x80, nirq // 32),
         "isenabler": m.read_words(GICD_BASE + 0x100, nirq // 32),
         "irouter": m.read_words(GICD_BASE + 0x6000 + 8 * 32, nirq - 32, "g"),
         "ipriorityr": m.read_words(GICD_BASE + 0x400, nirq // 4), "cpus": []}
    for c in range(NCPUS):
        base = GICR_BASE + c * GICR_STRIDE
        g["cpus"].append({"typer": m.read64(base + 0x8), "igroupr0": m.read32(base + 0x10080),
                          "isenabler0": m.read32(base + 0x10100),
                          "ipriorityr": m.read_words(base + 0x10400, 8)})
    return g


def prio(words, irq):
    """Priority byte of irq from IPRIORITYR words (4 interrupts per word)."""
    return (words[irq // 4] >> (8 * (irq % 4))) & 0xFF


# ---------------------------------------------------------------- stage-2
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
    raise SystemExit(f"symbol {name} not found in {elf}")


def rd64(mem, pa):
    off = pa - IMG_PA
    if off < 0 or off + 8 > len(mem):
        raise ValueError(f"PA {pa:#x} outside the dumped image")
    return struct.unpack_from("<Q", mem, off)[0]


def walk(mem, table_pa, level, ipa_base, leaves):
    for i in range(512):
        d = rd64(mem, table_pa + 8 * i)
        if not d & 1:
            continue
        ipa = ipa_base | (i << LEVEL_SHIFT[level])
        is_table = (d >> 1) & 1
        if level < 3 and is_table:
            walk(mem, d & 0x0000FFFFFFFFF000, level + 1, ipa, leaves)
            continue
        if level == 3 and not is_table:
            continue  # reserved encoding: not a valid mapping
        size = 1 << LEVEL_SHIFT[level]
        leaves.append({"ipa": ipa, "size": size, "pa": d & 0x0000FFFFFFFFF000 & ~(size - 1),
                       "memattr": (d >> 2) & 0xF, "s2ap": (d >> 6) & 3, "sh": (d >> 8) & 3,
                       "af": (d >> 10) & 1, "xn": (d >> 53) & 3})


def coalesce(leaves):
    out = []
    for leaf in sorted(leaves, key=lambda x: x["ipa"]):
        key = (leaf["memattr"], leaf["s2ap"], leaf["sh"], leaf["af"], leaf["xn"])
        if out and out[-1]["key"] == key and out[-1]["ipa"] + out[-1]["size"] == leaf["ipa"] \
                and out[-1]["pa"] + out[-1]["size"] == leaf["pa"]:
            out[-1]["size"] += leaf["size"]
        else:
            out.append(dict(leaf, key=key))
    for r in out:
        r.pop("key")
    return out


EL2_MAIR = {0: "Device-nGnRE", 1: "Normal WB"}  # HAVEN's MAIR_EL2 0xff04 (assumed)


def compare_el2(a, snap, parts):
    """v5: HAVEN's own EL2 stage-1 map against partition memory."""
    f, rep = [], {"ttbr": None, "ranges": [], "mair_assumed": "0xff04"}
    try:
        ttbr = rd64(snap["image"], symbol(a.nm, os.path.join(a.build, "haven.elf"), "hv_el2_ttbr"))
    except (SystemExit, ValueError):
        rep["note"] = "no hv_el2_ttbr in this build (EL2 translation not implemented)"
        print(f"EL2 map: {rep['note']}")
        return f, rep
    rep["ttbr"] = hex(ttbr)
    if ttbr == 0:
        rep["note"] = "EL2 translation off: HAVEN accesses all memory as Device"
        print(f"EL2 map: {rep['note']}")
        return f, rep
    leaves = []
    try:
        walk(snap["image"], ttbr & 0x0000FFFFFFFFF000, 1, 0, leaves)
    except ValueError as e:
        f.append(("EL2", "EL2 map drift", f"table outside HAVEN's image ({e})"))
        return f, rep
    for r in coalesce(leaves):
        attr = EL2_MAIR.get(r["memattr"] & 7, f"AttrIndx {r['memattr'] & 7}")
        x = {"va": hex(r["ipa"]), "pa": hex(r["pa"]), "size": hex(r["size"]), "type": attr,
             "xn": (r["xn"] >> 1) & 1, "readonly": (r["s2ap"] >> 1) & 1}
        rep["ranges"].append(x)
        print(f"  EL2 va={r['ipa']:#012x} pa={r['pa']:#012x} size={r['size']:#012x} {attr} xn={x['xn']}")
        if r["ipa"] != r["pa"]:
            f.append(("EL2", "EL2 map drift", f"VA {r['ipa']:#x} maps PA {r['pa']:#x} (identity expected)"))
        for pid, d in parts.items():
            for h in d["H"]:
                hpa, hsz, hattr = int(h["pa"], 16), int(h["size"], 16), h["memattr"]
                if r["pa"] < hpa + hsz and hpa < r["pa"] + r["size"]:
                    lo, hi = max(r["pa"], hpa), min(r["pa"] + r["size"], hpa + hsz)
                    if hattr & 0xC:  # stage-2 Normal memory
                        f.append((f"EL2 / partition {pid}", "partition memory mapped at EL2",
                                  f"PA {lo:#x}-{hi:#x} ({attr} at EL2)"))
                    else:
                        rep.setdefault("shared_device", []).append(
                            f"partition {pid} device PA {lo:#x}-{hi:#x} also mapped at EL2")
    print(f"EL2 map: {len(rep['ranges'])} range(s), TTBR0_EL2 {ttbr:#x}")
    for n in rep.get("shared_device", []):
        print(f"  EL2 observation: {n}")
    return f, rep


def compare_stage2(h, grants):
    f = []
    for want in grants:
        ipa, pa, size = (int(want[k], 16) for k in ("ipa", "pa", "size"))
        exp = EXPECT[want["attrs"]]
        cover = [r for r in h if r["ipa"] < ipa + size and r["ipa"] + r["size"] > ipa]
        got = sum(min(r["ipa"] + r["size"], ipa + size) - max(r["ipa"], ipa) for r in cover)
        if got < size:
            f.append((want["name"], "missing binding", f"{got:#x} of {size:#x} bytes mapped"))
        for r in cover:
            if r["ipa"] < ipa or r["ipa"] + r["size"] > ipa + size:
                f.append((want["name"], "extent drift", f"range {r['ipa']:#x}+{r['size']:#x} crosses grant"))
            if r["pa"] - r["ipa"] != pa - ipa:
                f.append((want["name"], "association drift", f"OA {r['pa']:#x} for IPA {r['ipa']:#x}"))
            for field, cls in (("s2ap", "access drift"), ("xn", "execute drift"),
                               ("memattr", "memory-type drift"), ("sh", "shareability drift")):
                if r[field] != exp[field]:
                    f.append((want["name"], cls, f"{field}={r[field]:#x} expected {exp[field]:#x}"))
            if r["af"] != 1:
                f.append((want["name"], "access-flag drift", "AF=0"))
    for r in h:
        if not any(int(w["ipa"], 16) <= r["ipa"] < int(w["ipa"], 16) + int(w["size"], 16) for w in grants):
            f.append(("(none)", "extra binding", f"IPA {r['ipa']:#x}+{r['size']:#x} not granted"))
    return f


# ---------------------------------------------------------------- SMMU
def dt_smmu_coverage(dts):
    """From QEMU's device tree: SMMU phandles and the PCIe RID->SID map."""
    smmus = {}
    for m in re.finditer(r"smmuv3@([0-9a-f]+) \{(.*?)\n\t\};", dts, re.S):
        ph = re.search(r"phandle = <(0x[0-9a-f]+)>", m.group(2))
        if ph:
            smmus[int(ph.group(1), 16)] = int(m.group(1), 16)
    pcie_maps = []
    for m in re.finditer(r"pcie@[0-9a-f]+ \{(.*?)\n\t\};", dts, re.S):
        im = re.search(r"iommu-map = <(0x[0-9a-f]+) (0x[0-9a-f]+) (0x[0-9a-f]+) (0x[0-9a-f]+)>", m.group(1))
        if im:
            rid, ph, sid, length = (int(x, 16) for x in im.groups())
            pcie_maps.append({"rid_base": rid, "smmu": smmus.get(ph), "sid_base": sid, "length": length})
    mmio_with_iommu = len(re.findall(r"virtio_mmio@[0-9a-f]+ \{[^}]*iommus", dts))
    return pcie_maps, mmio_with_iommu


def virtio_platform(qtree):
    """(slot, function) -> iommu_platform for virtio PCI devices on bus 0. A
    virtio device without VIRTIO_F_ACCESS_PLATFORM does its DMA to physical
    addresses directly, outside any IOMMU (QEMU: iommu_platform=false)."""
    out = {}
    for m in re.finditer(r"dev: [\w-]+-pci, id \"[^\"]*\"\r?\n((?:(?!dev: [\w-]+-pci).)*)", qtree, re.S):
        a = re.search(r"addr = ([0-9a-f]{2})\.([0-7])", m.group(1))
        p = re.search(r"iommu_platform = (true|false)", m.group(1))
        if a and p:
            out[(int(a.group(1), 16), int(a.group(2)))] = p.group(1) == "true"
    return out


def dma_masters(snap, pcie_maps):
    masters = []
    platform = virtio_platform(snap["qtree"])
    for bus in snap["pci"]:
        for d in bus.get("devices", []):
            if d.get("class_info", {}).get("class") == 0x0600:  # host bridge
                continue
            rid = (bus["bus"] << 8) | (d["slot"] << 3) | d["function"]
            m = next((x for x in pcie_maps if x["rid_base"] <= rid < x["rid_base"] + x["length"]), None)
            ident = d.get("id", {})
            bypass = bus["bus"] == 0 and platform.get((d["slot"], d["function"])) is False
            masters.append({"device": d.get("qdev_id") or "pci-%04x:%04x" % (ident.get("vendor", 0), ident.get("device", 0)),
                            "bus": f"pci {bus['bus']:02x}:{d['slot']:02x}.{d['function']}",
                            "sid": None if m is None else rid - m["rid_base"] + m["sid_base"],
                            "behind_smmu": m is not None and not bypass, "virtio_bypass": bypass})
    # virtio-mmio transports that have a device plugged in
    for m in re.finditer(r"(?m)^( +)bus: virtio-mmio-bus\.(\d+)\r?\n\1  type virtio-mmio-bus\r?\n\1  dev: ([\w-]+)",
                         snap["qtree"]):
        masters.append({"device": m.group(3), "bus": f"virtio-mmio.{m.group(2)}", "sid": None,
                        "behind_smmu": False})
    return masters


def stream_table_reach(mem, ttb, parts):
    """Walk a stream's own stage-2 table; attribute it to the partition whose
    stage-2 map contains every physical range it maps (None if no single one)."""
    leaves = []
    walk(mem, ttb, 1, 0, leaves)
    reach = coalesce(leaves)
    owners = None
    for r in reach:
        own = {p for p, h in parts.items()
               if any(x["pa"] <= r["pa"] and r["pa"] + r["size"] <= x["pa"] + x["size"] for x in h)}
        owners = own if owners is None else owners & own
    owner = next(iter(owners)) if owners and len(owners) == 1 else None
    return reach, owner


def resolve_ttb(ttb, roots, mem, parts):
    """(partition or None, reach regions, kind) for a stream table base."""
    part = next((p for p, r in roots.items() if r == ttb), None)
    if part is not None:
        return part, parts.get(part, []), "partition table"
    try:
        reach, owner = stream_table_reach(mem, ttb, parts)
    except ValueError:
        return None, [], "table outside the dumped image"
    return owner, reach, "stream table"


def classify(master, snap, roots, parts):
    sm = snap["smmu"]
    if master.get("virtio_bypass"):
        return "unenforced", "virtio device without VIRTIO_F_ACCESS_PLATFORM: its DMA bypasses the SMMU"
    if not master["behind_smmu"]:
        return "unenforced", "no SMMU in the device's path"
    if not sm["cr0ack"] & 1:
        return ("denied", "SMMU disabled, GBPA.ABORT=1") if sm["gbpa"] & (1 << 20) else \
               ("bypassed", "SMMU disabled, GBPA bypass")
    sid, nstreams = master["sid"], 1 << (sm["strtab_cfg"] & 0x3F)
    if sid >= nstreams:
        return "denied", f"SID {sid} beyond the {nstreams}-entry stream table"
    w0, _, _, w3 = struct.unpack_from("<4Q", snap["stream_table"], sid * 64)
    if not w0 & 1:
        return "denied", "STE invalid"
    cfg = STE_CONFIG.get((w0 >> 1) & 7, "reserved")
    if cfg == "abort":
        return "denied", "STE abort"
    if cfg == "bypass":
        return "bypassed", "STE bypass"
    if cfg in ("stage-2", "stage-1+2"):
        ttb = w3 & 0x000FFFFFFFFFFFF0
        part, reach, kind = resolve_ttb(ttb, roots, snap["image"], parts)
        size = sum(r["size"] for r in reach)
        return (f"translated:{part}" if part is not None else "translated:unknown",
                f"STE {cfg}, S2TTB {ttb:#x} ({kind}, {size:#x} bytes)")
    return cfg, f"STE {cfg}"


def ranges(sids):
    """Compress a sorted list of integers into "a-b" ranges."""
    out, start, prev = [], None, None
    for x in sids:
        if start is None:
            start = prev = x
        elif x == prev + 1:
            prev = x
        else:
            out.append(f"{start}-{prev}" if start != prev else str(start))
            start = prev = x
    if start is not None:
        out.append(f"{start}-{prev}" if start != prev else str(start))
    return ", ".join(out)


def compare_smmu(snap, masters, expected, roots, parts):
    f, report = [], []
    sm = snap["smmu"]
    if bool(sm["cr0ack"] & 1) != expected.get("enabled", True):
        f.append(("SMMU", "SMMU enable drift", f"CR0ACK={sm['cr0ack']:#x}"))
    if (sm["cmdq_cons"] >> 24) & 0x7F or sm["gerror"] & 1:
        f.append(("SMMU", "SMMU command queue fault",
                  f"CMDQ_CONS={sm['cmdq_cons']:#x} GERROR={sm['gerror']:#x}"))
    streams = {int(k): v for k, v in expected.get("streams", {}).items()}
    allowed = set(expected.get("allowed_unenforced", []))
    for m in masters:
        state, why = classify(m, snap, roots, parts)
        report.append(dict(m, state=state, reason=why))
        if state == "unenforced":
            if m["device"] not in allowed:
                f.append((m["device"], "unenforced DMA master", f"{m['bus']}: {why}"))
            continue
        want = f"translated:{streams[m['sid']]['partition']}" if m["sid"] in streams else "denied"
        if state != want:
            f.append((m["device"], "DMA enforcement drift", f"{m['bus']} SID {m['sid']}: {state} ({why}), expected {want}"))
    if snap["smmu"]["cr0ack"] & 1:
        # Every stream table entry that lets DMA through must be granted.
        extra = []
        for sid in range(len(snap["stream_table"]) // 64):
            w0, _, _, w3 = struct.unpack_from("<4Q", snap["stream_table"], sid * 64)
            cfg = STE_CONFIG.get((w0 >> 1) & 7, "reserved") if w0 & 1 else "invalid"
            if cfg in ("abort", "invalid"):
                continue
            if sid not in streams:
                extra.append(sid)
                continue
            # Granted and translated: compare what the device reaches through
            # the table its entry points to with the granted window.
            win = streams[sid].get("window")
            ttb = w3 & 0x000FFFFFFFFFFFF0
            part, reach, kind = resolve_ttb(ttb, roots, snap["image"], parts)
            if win and part is not None:
                f += compare_window(sid, streams[sid], part, reach, kind, parts)
        if extra:
            f.append(("SMMU", "extra stream binding", f"{len(extra)} stream(s) not denied without a grant: SID {ranges(extra)}"))
    return f, report


def smmu_events(snap):
    """Events the SMMU wrote to its event queue (in HAVEN's image): what it
    stopped. Observations, not findings: a fault on a granted stream outside
    its window is enforcement working. HAVEN may already have consumed them
    (CONS); consumed entries stay in memory, so every entry written since
    the queue was set up is decoded while the ring has not wrapped, and the
    last full ring otherwise."""
    sm, out = snap["smmu"], []
    base = sm["evtq_base"] & 0x000FFFFFFFFFFFE0
    log2 = sm["evtq_base"] & 0x1F
    if not sm["cr0ack"] & (1 << 2) or not IMG_PA <= base < IMG_PA + IMG_SIZE:
        return out
    n, mask = 1 << log2, (2 << log2) - 1
    prod, cons = sm["evtq_prod"] & mask, sm["evtq_cons"] & mask
    count = prod if prod < n and not sm["evtq_prod"] & (1 << 31) else n
    for k in range(count):
        ptr = (prod - count + k) & mask
        w0, w1, w2, _ = struct.unpack_from("<4Q", snap["image"], base - IMG_PA + (ptr & (n - 1)) * 32)
        out.append({"id": w0 & 0xFF, "event": EVT_NAMES.get(w0 & 0xFF, f"event {w0 & 0xFF:#x}"),
                    "sid": w0 >> 32, "addr": w2, "write": not (w1 >> 35) & 1, "stage2": bool((w1 >> 39) & 1),
                    "consumed": ((prod - ptr) & mask) > ((prod - cons) & mask)})
    return out


def compare_window(sid, grant, part, reach, kind, parts):
    """A translated stream against its granted window. The window is in the
    owning partition's address space (IOVA = IPA), so each DMA mapping must
    lead to the physical address the partition's own stage-2 gives that IPA."""
    f, name = [], f"SID {sid}"
    lo, size = int(grant["window"]["ipa"], 16), int(grant["window"]["size"], 16)
    total = sum(r["size"] for r in reach)
    if any(r["ipa"] < lo or r["ipa"] + r["size"] > lo + size for r in reach):
        f.append((name, "DMA extent drift",
                  f"granted window {lo:#x}+{size:#x}, enforced reach ({kind}, partition {part}): {total:#x} bytes"))
    cover = sum(max(0, min(r["ipa"] + r["size"], lo + size) - max(r["ipa"], lo)) for r in reach)
    if cover < size:
        f.append((name, "DMA missing binding", f"{cover:#x} of {size:#x} window bytes mapped"))
    if kind != "stream table":
        return f
    want_ap = DMA_ACCESS[grant.get("access", "RW")]
    for r in reach:
        h = next((x for x in parts.get(part, [])
                  if x["ipa"] <= r["ipa"] and r["ipa"] + r["size"] <= x["ipa"] + x["size"]), None)
        if h is None or h["pa"] - h["ipa"] != r["pa"] - r["ipa"]:
            f.append((name, "DMA association drift",
                      f"IOVA {r['ipa']:#x} -> PA {r['pa']:#x}, partition {part} maps that IPA elsewhere"))
        if r["s2ap"] != want_ap:
            f.append((name, "DMA access drift", f"s2ap={r['s2ap']} expected {want_ap}"))
        if r["af"] != 1:
            f.append((name, "DMA access drift", "AF=0"))
    return f


# ---------------------------------------------------------------- GIC
VGIC_ENABLED_OFFSET = 40  # offsetof(struct hv_vgic, enabled), asserted in vgic.c


def read_vgic_enables(mem, nm, elf):
    """Per CPU, the emulated GIC's enable bits for INTIDs 0-63 (v8), or {}."""
    try:
        table = symbol(nm, elf, "by_cpu")
    except SystemExit:
        return {}
    out = {}
    for cpu in range(16):
        ptr = rd64(mem, table + 8 * cpu)
        if ptr:
            lo = struct.unpack_from("<I", mem, ptr + VGIC_ENABLED_OFFSET - IMG_PA)[0]
            hi = struct.unpack_from("<I", mem, ptr + VGIC_ENABLED_OFFSET + 4 - IMG_PA)[0]
            out[cpu] = lo | (hi << 32)
    return out


def read_cpu_partitions(mem, nm, elf):
    """v10: HAVEN's partition per CPU (0 = none) for the NCPUS CPUs, or None."""
    try:
        table = symbol(nm, elf, "cpu_partition")
    except SystemExit:
        return None
    return [struct.unpack_from("<I", mem, table + 4 * c - IMG_PA)[0] for c in range(NCPUS)]


def compare_cpus(served, expected):
    """v10: CPU assignment drift between gic.cpus and HAVEN's table."""
    granted = {int(c): int(p) for c, p in expected.get("cpus", {}).items()}
    f, rep = [], []

    def who(p):
        return f"partition {p}" if p else "no partition"
    for c, got in enumerate(served):
        want = granted.get(c, 0)
        rep.append({"cpu": c, "granted": want, "served": got})
        if got != want:
            f.append((f"CPU {c}", "CPU assignment drift", f"granted to {who(want)}, HAVEN runs {who(got)}"))
    return f, rep


def compare_gic(g, expected, model=None):
    """Interrupt ownership as enforced by the GIC against the grant: every
    enabled SPI must be granted, routed (affinity, not 1-of-N) to the granted
    CPU of the granted partition, in Group 1 Non-secure; per CPU, only
    granted PPIs may be enabled. SGIs (0-15) are not restricted by HAVEN."""
    f, report = [], {"spis": [], "cpus": []}
    # GICD_CTLR as seen by a Non-secure access: with DS=1 (one security
    # state) ARE is bit 4 and EnableGrp1 bit 1; with two security states the
    # Non-secure view has ARE_NS at bit 4 and EnableGrp1A at bit 1.
    if not g["ctlr"] & (1 << 4) or not g["ctlr"] & (1 << 1):
        f.append(("GIC", "GIC distributor drift", f"GICD_CTLR={g['ctlr']:#x} (affinity routing and Group 1 expected)"))
    cpu_part = {int(c): p for c, p in expected.get("cpus", {}).items()}
    aff_cpu = {cpu["typer"] >> 32: i for i, cpu in enumerate(g["cpus"])}
    spis = {int(k): v for k, v in expected.get("spis", {}).items()}
    lazy = {int(c): set(v) for c, v in expected.get("guest_enabled", {}).items()}  # v8
    model = model or {}
    report["guest_enabled"] = []

    def lazy_check(cpu, irq, hw_on):
        """v8: an interrupt enabled on the guest's request. Returns True if
        it was judged here (the caller skips its own enable rule)."""
        if irq not in lazy.get(cpu, set()) or cpu not in model:
            return False
        asked = (model[cpu] >> irq) & 1
        name = f"SPI {irq}" if irq >= 32 else f"CPU {cpu} PPI {irq}"
        report["guest_enabled"].append({"irq": irq, "cpu": cpu, "guest": asked, "gic": hw_on})
        if asked and not hw_on:
            f.append((name, "missing interrupt binding", "enabled by the guest, disabled in the GIC"))
        elif hw_on and not asked:
            f.append((name, "interrupt enable drift", "enabled in the GIC, never enabled by the guest"))
        return True
    extra = []
    for irq in range(32, g["nirq"]):
        en = (g["isenabler"][irq // 32] >> (irq % 32)) & 1
        grp1 = (g["igroupr"][irq // 32] >> (irq % 32)) & 1
        rt = g["irouter"][irq - 32]
        if not en and irq not in spis:
            continue
        irm = (rt >> 31) & 1
        aff = (rt & 0xFFFFFF) | ((rt >> 32) & 0xFF) << 24
        cpu = None if irm else aff_cpu.get(aff)
        state = {"irq": irq, "enabled": en, "group1": grp1, "irouter": hex(rt),
                 "cpu": "any (1-of-N)" if irm else cpu}
        report["spis"].append(state)
        if irq not in spis:
            extra.append(irq)
            continue
        want = spis[irq]
        name = f"SPI {irq}"
        if lazy_check(want["cpu"], irq, en) and not en:
            continue
        if not en:
            f.append((name, "missing interrupt binding", "granted but disabled"))
            continue
        if want["partition"] == 0:  # HAVEN's own interrupt: CPU only
            if irm or cpu != want["cpu"]:
                f.append((name, "interrupt routing drift", f"HAVEN interrupt routed to CPU {state['cpu']}, granted CPU {want['cpu']}"))
        elif irm:
            f.append((name, "interrupt routing drift", f"1-of-N routing: may reach any CPU, granted CPU {want['cpu']}"))
        elif cpu is None or cpu_part.get(cpu) != want["partition"]:
            f.append((name, "interrupt routing drift",
                      f"routed to CPU {cpu} (partition {cpu_part.get(cpu)}), granted partition {want['partition']}"))
        elif cpu != want["cpu"]:
            f.append((name, "interrupt routing drift", f"routed to CPU {cpu}, granted CPU {want['cpu']}"))
        if not grp1:
            f.append((name, "interrupt group drift", "Group 0 (expected Group 1 Non-secure)"))
    if extra:
        f.append(("GIC", "interrupt enable drift", f"{len(extra)} SPI(s) enabled without a grant: {ranges(extra)}"))
    ppis = {int(c): set(v) for c, v in expected.get("ppis", {}).items()}
    hppis = {int(c): set(v) for c, v in expected.get("haven_ppis", {}).items()}  # v6
    for i, cpu in enumerate(g["cpus"]):
        en = [b for b in range(16, 32) if (cpu["isenabler0"] >> b) & 1]
        report["cpus"].append({"cpu": i, "affinity": hex(cpu["typer"] >> 32), "ppis_enabled": en,
                               "sgis_enabled": [b for b in range(16) if (cpu["isenabler0"] >> b) & 1]})
        bad = [b for b in en if b not in ppis.get(i, set()) | hppis.get(i, set())]
        if bad:
            f.append((f"CPU {i} PPIs", "interrupt enable drift", f"enabled without a grant: {ranges(bad)}"))
        for b in sorted(ppis.get(i, set()) | hppis.get(i, set())):  # v4 RB2, v6
            if lazy_check(i, b, b in en):
                continue
            if b not in en:
                f.append((f"CPU {i} PPI {b}", "missing interrupt binding", "granted but disabled"))
    pr = expected.get("priority")
    if pr and "ipriorityr" in g:  # v4: priorities of granted interrupts
        want_h, want_p = int(str(pr["haven"]), 0), int(str(pr["partition"]), 0)
        report["priorities"] = []
        for irq, want in sorted(spis.items()):
            if not (g["isenabler"][irq // 32] >> (irq % 32)) & 1:
                continue  # v7, RB3: already a missing binding
            got = prio(g["ipriorityr"], irq)
            exp = want_h if want["partition"] == 0 else want_p
            report["priorities"].append({"irq": irq, "priority": hex(got), "expected": hex(exp)})
            if got != exp:
                why = " (outranks HAVEN)" if want["partition"] != 0 and got <= want_h else ""
                f.append((f"SPI {irq}", "interrupt priority drift", f"priority {got:#x}, granted {exp:#x}{why}"))
        for i, cpu in enumerate(g["cpus"]):
            for b in sorted(ppis.get(i, set()) | hppis.get(i, set())):
                if not (cpu["isenabler0"] >> b) & 1:
                    continue  # v7, RB3: already a missing binding
                got = prio(cpu["ipriorityr"], b)
                exp = want_h if b in hppis.get(i, set()) else want_p
                report["priorities"].append({"irq": b, "cpu": i, "priority": hex(got), "expected": hex(exp)})
                if got != exp:
                    why = " (outranks HAVEN)" if exp == want_p and got <= want_h else ""
                    f.append((f"CPU {i} PPI {b}", "interrupt priority drift",
                              f"priority {got:#x}, granted {exp:#x}{why}"))
    return f, report


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("build")
    ap.add_argument("grant")
    ap.add_argument("out")
    ap.add_argument("--cpu", default="cortex-a72")
    ap.add_argument("--nm", default=shutil.which("aarch64-unknown-linux-gnu-nm") or shutil.which("aarch64-linux-gnu-nm"))
    ap.add_argument("--wait", type=float, default=15)
    ap.add_argument("--device", action="append", default=[])
    ap.add_argument("--smmu-stage", default="nested", choices=["nested", "1", "2"])
    ap.add_argument("--marker", default=MARKER, help="UART line after which the memory is read")
    ap.add_argument("--guest-a", help="file loaded at partition A's base instead of BUILD/guest_a.bin")
    ap.add_argument("--load", action="append", default=[], type=parse_load, metavar="FILE@ADDR",
                    help="extra file at a physical address (repeatable)")
    ap.add_argument("--break", dest="break_at", type=lambda x: int(x, 0), metavar="ADDR",
                    help="read when a CPU reaches this instruction address (v9; QEMU GDB stub)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    grant = grant_file.load(a.grant)  # format 1 or 2 (FF-A vocabulary)
    snap = capture(a, a.out)
    if snap is None or len(snap.get("image", b"")) != IMG_SIZE:
        print("probe failure: QEMU exited before the memory could be read")
        return 2
    print(f"read at: {snap['trigger']}")
    roots_pa = symbol(a.nm, os.path.join(a.build, "haven.elf"), "pt_roots")
    findings, report = [], {"cpu": a.cpu, "trigger": snap["trigger"], "partitions": {}}
    roots = {}
    for pid, grants in grant["partitions"].items():
        root = rd64(snap["image"], roots_pa + 8 * int(pid))
        roots[int(pid)] = root
        leaves = []
        try:  # v4, RB1: no usable root means HAVEN stopped before stage-2 setup
            if root == 0:
                raise ValueError("root is zero")
            walk(snap["image"], root, 1, 0, leaves)
        except ValueError as e:
            msg = f"HAVEN halted before stage-2 setup: partition {pid} root {root:#x} ({e})"
            report.update({"findings": [], "probe_failure": msg})
            json.dump(report, open(os.path.join(a.out, "readback.json"), "w"), indent=1)
            print(f"probe failure: {msg}")
            return 2
        h = coalesce(leaves)
        f = compare_stage2(h, grants)
        findings += f
        report["partitions"][pid] = {"root": hex(root), "H": [{k: hex(v) if k in ("ipa", "pa", "size") else v
                                                               for k, v in r.items()} for r in h]}
        print(f"partition {pid} root={root:#x}")
        for r in h:
            print(f"  H ipa={r['ipa']:#011x} pa={r['pa']:#011x} size={r['size']:#010x} s2ap={r['s2ap']} "
                  f"memattr={r['memattr']:#x} sh={r['sh']} xn=0b{r['xn']:02b} af={r['af']}")
    el2_findings, report["el2"] = compare_el2(a, snap, report["partitions"])
    findings += el2_findings
    all_pa = [(p, r["pa"], r["size"]) for p, d in report["partitions"].items()
              for r in ({"pa": int(x["pa"], 16), "size": int(x["size"], 16)} for x in d["H"])]
    for i, (p1, a1, s1) in enumerate(all_pa):
        for p2, a2, s2 in all_pa[i + 1:]:
            if p1 != p2 and a1 < a2 + s2 and a2 < a1 + s1:
                findings.append(("*", "cross-partition alias", f"PA {max(a1, a2):#x} reachable from {p1} and {p2}"))
    if "smmu" in grant:
        pcie_maps, mmio_with_iommu = dt_smmu_coverage(device_tree(a.build, a.cpu, a.device, a.out, a.smmu_stage))
        masters = dma_masters(snap, pcie_maps)
        parts = {int(p): [{k: int(x[k], 16) for k in ("ipa", "pa", "size")} for x in d["H"]]
                 for p, d in report["partitions"].items()}
        f, rep = compare_smmu(snap, masters, grant["smmu"], roots, parts)
        findings += f
        report["smmu_events"] = smmu_events(snap)
        sm = snap["smmu"]
        report["smmu"] = {k: hex(v) for k, v in sm.items()}
        report["dma_masters"] = rep
        print(f"SMMU enabled={bool(sm['cr0ack'] & 1)} GBPA={sm['gbpa']:#x} "
              f"stream table {sm['strtab_base'] & 0x000FFFFFFFFFFFC0:#x}, {1 << (sm['strtab_cfg'] & 0x3F)} entries; "
              f"virtio-mmio nodes behind an SMMU in the device tree: {mmio_with_iommu}")
        print(f"SMMU IDR0={sm['idr0']:#x} (S2P={sm['idr0'] & 1}) CMDQ_BASE={sm['cmdq_base']:#x} "
              f"PROD={sm['cmdq_prod']:#x} CONS={sm['cmdq_cons']:#x} GERROR={sm['gerror']:#x}")
        for m in rep:
            print(f"  DMA {m['device']:<22} {m['bus']:<16} SID={m['sid']} -> {m['state']} ({m['reason']})")
        evs = report["smmu_events"]
        print(f"SMMU event queue: {len(evs)} event(s), {sum(e['consumed'] for e in evs)} consumed by HAVEN")
        for e in sorted({(e['event'], e['sid'], e['addr'] & ~0xFFF, e['write']) for e in evs}):
            n = sum(1 for x in evs if (x['event'], x['sid'], x['addr'] & ~0xFFF, x['write']) == e)
            print(f"  EVENT {e[0]} SID {e[1]} page {e[2]:#x} {'write' if e[3] else 'read'} x{n}")
    if "gic" in grant:
        model = read_vgic_enables(snap["image"], a.nm, os.path.join(a.build, "haven.elf"))
        f, rep = compare_gic(snap["gic"], grant["gic"], model)
        findings += f
        report["gic"] = rep
        g = snap["gic"]
        print(f"GIC GICD_CTLR={g['ctlr']:#x}, {g['nirq']} INTIDs; enabled SPIs:")
        for x in rep["spis"]:
            print(f"  SPI {x['irq']}: enabled={x['enabled']} group1={x['group1']} -> CPU {x['cpu']} (IROUTER {x['irouter']})")
        for c in rep["cpus"]:
            print(f"  CPU {c['cpu']} (affinity {c['affinity']}): PPIs enabled {c['ppis_enabled'] or 'none'}")
        for x in rep.get("guest_enabled", []):
            state = "waiting for the guest" if not x["guest"] and not x["gic"] else f"guest {x['guest']}, GIC {int(bool(x['gic']))}"
            print(f"  guest-enabled INTID {x['irq']} on CPU {x['cpu']}: {state}")
        for x in rep.get("priorities", []):
            where = f"CPU {x['cpu']} PPI" if "cpu" in x else "SPI"
            print(f"  {where} {x['irq']}: priority {x['priority']} (granted {x['expected']})")
        served = read_cpu_partitions(snap["image"], a.nm, os.path.join(a.build, "haven.elf"))  # v10
        if served is not None and "cpus" in grant["gic"]:
            f, rep = compare_cpus(served, grant["gic"])
            findings += f
            report["cpu_assignment"] = rep
            for x in rep:
                print(f"  CPU {x['cpu']}: granted to partition {x['granted']}, HAVEN runs partition {x['served']}")
    report["findings"] = findings
    json.dump(report, open(os.path.join(a.out, "readback.json"), "w"), indent=1)
    for name, cls, detail in findings:
        print(f"  FINDING {name}: {cls} ({detail})")
    print("RESULT", "enforced state matches the grant" if not findings else f"{len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
