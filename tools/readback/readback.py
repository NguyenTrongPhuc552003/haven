#!/usr/bin/env python3
"""External read-back of HAVEN's enforced state in QEMU (probe v2).

Boots a qemu-direct HAVEN build, stops the machine and reads, from outside
HAVEN:
  - stage-2: every partition's tables (found through pt_roots), decoded with
    the Arm VMSAv8-64 stage-2 format (4 KiB granule, 39-bit IPA, start level 1);
  - SMMU (when the grant file has an "smmu" section): SMMU_CR0ACK, GBPA and the
    stream table found through SMMU_STRTAB_BASE, plus every DMA-capable device
    QEMU emulates, classified as denied, translated, bypassed or unenforced
    (not behind any SMMU). Every stream entry that lets DMA through must be
    granted, and a translated stream's reach (the partition's whole stage-2
    map) is compared with the window the grant gives the device. Which buses sit behind the SMMU comes from QEMU's
    own device tree, not from HAVEN.
It compares what it reads with the grant and reports drift.

Changes from v1 (frozen in docs/methodology/stage1/20261101-a1-readback):
the memory is read when partition A reaches its post-fault line or after a
time limit, whichever comes first, so a stalled guest still gets a verdict;
SMMU read-back; extra QEMU devices; command-line options.

Usage:
  readback.py BUILD GRANT.json OUT [--cpu CPU] [--nm NM] [--wait SECONDS]
              [--device QEMU-DEVICE ...]
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

IMG_PA, IMG_SIZE = 0x80000000, 0x800000
SMMU_BASE = 0x09050000
MARKER = "Post-fault check OK"
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
MACHINE = "virt,virtualization=on,gic-version=3,iommu=smmuv3"


# ---------------------------------------------------------------- QEMU access
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


def qemu_args(build, cpu, devices, extra):
    args = ["qemu-system-aarch64", "-machine", MACHINE + extra, "-cpu", cpu, "-smp", "4",
            "-m", "2G", "-display", "none", "-nic", "none"]
    for d in devices:
        args += ["-device", d]
    return args + [
        "-device", f"loader,file={build}/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0",
        "-device", f"loader,file={build}/guest_a.bin,addr=0x80800000,force-raw=on",
        "-device", f"loader,file={build}/guest_b.bin,addr=0xA0800000,force-raw=on"]


def device_tree(build, cpu, devices, out):
    """QEMU's own hardware description for this machine (dumpdtb)."""
    dtb = os.path.join(out, "machine.dtb")
    subprocess.run(qemu_args(build, cpu, devices, f",dumpdtb={dtb}"), capture_output=True, timeout=60)
    dts = subprocess.run(["dtc", "-q", "-I", "dtb", "-O", "dts", dtb], capture_output=True, text=True).stdout
    open(os.path.join(out, "machine.dts"), "w").write(dts)
    return dts


def capture(args_ns, out):
    sock = f"/tmp/readback-{os.getpid()}.sock"  # macOS limits socket paths to 104 bytes
    uart = os.path.join(out, "uart.log")
    for p in (sock, uart):
        if os.path.exists(p):
            os.remove(p)
    cmd = qemu_args(args_ns.build, args_ns.cpu, args_ns.device, "") + [
        "-serial", f"file:{uart}", "-qmp", f"unix:{sock},server,nowait"]
    open(os.path.join(out, "qemu-command.txt"), "w").write(" ".join(cmd) + "\n")
    q = subprocess.Popen(cmd)
    snap = {"trigger": None}
    try:
        t0 = time.time()
        while not os.path.exists(sock) and time.time() - t0 < 10:
            time.sleep(0.05)
        while time.time() - t0 < args_ns.wait:
            if os.path.exists(uart) and MARKER in open(uart, errors="replace").read():
                snap["trigger"] = "marker"
                break
            if q.poll() is not None:
                break
            time.sleep(0.2)
        else:
            snap["trigger"] = f"time limit ({args_ns.wait:g} s, partition A did not reach its post-fault line)"
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
            "strtab_cfg": m.read32(SMMU_BASE + 0x88)}
        sm = snap["smmu"]
        if sm["cr0ack"] & 1:
            size = (1 << (sm["strtab_cfg"] & 0x3F)) * 64
            base = sm["strtab_base"] & 0x3FFFFFFFFFFC0
            stab = os.path.join(out, "stream-table.bin")
            m.cmd("pmemsave", val=base, size=size, filename=os.path.abspath(stab))
            snap["stream_table"] = open(stab, "rb").read()
        snap["pci"] = m.cmd("query-pci") or []
        snap["qtree"] = m.hmp("info qtree")
        json.dump(m.log, open(os.path.join(out, "qmp.json"), "w"), indent=1)
        m.cmd("quit")
        q.wait(timeout=10)
    finally:
        if q.poll() is None:
            q.kill()
        if os.path.exists(sock):
            os.remove(sock)
    return snap


# ---------------------------------------------------------------- stage-2
def symbol(nm, elf, name):
    for line in subprocess.check_output([nm, elf], text=True).splitlines():
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


def dma_masters(snap, pcie_maps):
    masters = []
    for bus in snap["pci"]:
        for d in bus.get("devices", []):
            if d.get("class_info", {}).get("class") == 0x0600:  # host bridge
                continue
            rid = (bus["bus"] << 8) | (d["slot"] << 3) | d["function"]
            m = next((x for x in pcie_maps if x["rid_base"] <= rid < x["rid_base"] + x["length"]), None)
            ident = d.get("id", {})
            masters.append({"device": d.get("qdev_id") or "pci-%04x:%04x" % (ident.get("vendor", 0), ident.get("device", 0)),
                            "bus": f"pci {bus['bus']:02x}:{d['slot']:02x}.{d['function']}",
                            "sid": None if m is None else rid - m["rid_base"] + m["sid_base"],
                            "behind_smmu": m is not None})
    # virtio-mmio transports that have a device plugged in
    for m in re.finditer(r"(?m)^( +)bus: virtio-mmio-bus\.(\d+)\r?\n\1  type virtio-mmio-bus\r?\n\1  dev: ([\w-]+)",
                         snap["qtree"]):
        masters.append({"device": m.group(3), "bus": f"virtio-mmio.{m.group(2)}", "sid": None,
                        "behind_smmu": False})
    return masters


def classify(master, snap, roots):
    sm = snap["smmu"]
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
        ttb = w3 & 0x0000FFFFFFFFFFF0
        part = next((p for p, r in roots.items() if r == ttb), None)
        return f"translated:{part}" if part else "translated:unknown", f"STE {cfg}, S2TTB {ttb:#x}"
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


def compare_smmu(snap, masters, expected, roots, h_by_part):
    f, report = [], []
    if bool(snap["smmu"]["cr0ack"] & 1) != expected.get("enabled", True):
        f.append(("SMMU", "SMMU enable drift", f"CR0ACK={snap['smmu']['cr0ack']:#x}"))
    streams = {int(k): v for k, v in expected.get("streams", {}).items()}
    allowed = set(expected.get("allowed_unenforced", []))
    for m in masters:
        state, why = classify(m, snap, roots)
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
            # Granted and translated: the device reaches the whole stage-2 map
            # of the partition the entry points to; compare with the window.
            win = streams[sid].get("window")
            ttb = w3 & 0x0000FFFFFFFFFFF0
            part = next((p for p, r in roots.items() if r == ttb), None)
            if win and part is not None:
                lo, size = int(win["ipa"], 16), int(win["size"], 16)
                reach = sum(r["size"] for r in h_by_part.get(part, []))
                if any(r["ipa"] < lo or r["ipa"] + r["size"] > lo + size for r in h_by_part.get(part, [])):
                    f.append((f"SID {sid}", "DMA extent drift",
                              f"granted window {lo:#x}+{size:#x}, enforced reach is partition {part}: {reach:#x} bytes"))
        if extra:
            f.append(("SMMU", "extra stream binding", f"{len(extra)} stream(s) not denied without a grant: SID {ranges(extra)}"))
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
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    grant = json.load(open(a.grant))
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
        walk(snap["image"], root, 1, 0, leaves)
        h = coalesce(leaves)
        f = compare_stage2(h, grants)
        findings += f
        report["partitions"][pid] = {"root": hex(root), "H": [{k: hex(v) if k in ("ipa", "pa", "size") else v
                                                               for k, v in r.items()} for r in h]}
        print(f"partition {pid} root={root:#x}")
        for r in h:
            print(f"  H ipa={r['ipa']:#011x} pa={r['pa']:#011x} size={r['size']:#010x} s2ap={r['s2ap']} "
                  f"memattr={r['memattr']:#x} sh={r['sh']} xn=0b{r['xn']:02b} af={r['af']}")
    all_pa = [(p, r["pa"], r["size"]) for p, d in report["partitions"].items()
              for r in ({"pa": int(x["pa"], 16), "size": int(x["size"], 16)} for x in d["H"])]
    for i, (p1, a1, s1) in enumerate(all_pa):
        for p2, a2, s2 in all_pa[i + 1:]:
            if p1 != p2 and a1 < a2 + s2 and a2 < a1 + s1:
                findings.append(("*", "cross-partition alias", f"PA {max(a1, a2):#x} reachable from {p1} and {p2}"))
    if "smmu" in grant:
        pcie_maps, mmio_with_iommu = dt_smmu_coverage(device_tree(a.build, a.cpu, a.device, a.out))
        masters = dma_masters(snap, pcie_maps)
        h_by_part = {int(p): [{"ipa": int(x["ipa"], 16), "size": int(x["size"], 16)} for x in d["H"]]
                     for p, d in report["partitions"].items()}
        f, rep = compare_smmu(snap, masters, grant["smmu"], roots, h_by_part)
        findings += f
        sm = snap["smmu"]
        report["smmu"] = {k: hex(v) for k, v in sm.items()}
        report["dma_masters"] = rep
        print(f"SMMU enabled={bool(sm['cr0ack'] & 1)} GBPA={sm['gbpa']:#x} "
              f"stream table {sm['strtab_base'] & 0x3FFFFFFFFFFC0:#x}, {1 << (sm['strtab_cfg'] & 0x3F)} entries; "
              f"virtio-mmio nodes behind an SMMU in the device tree: {mmio_with_iommu}")
        for m in rep:
            print(f"  DMA {m['device']:<22} {m['bus']:<16} SID={m['sid']} -> {m['state']} ({m['reason']})")
    report["findings"] = findings
    json.dump(report, open(os.path.join(a.out, "readback.json"), "w"), indent=1)
    for name, cls, detail in findings:
        print(f"  FINDING {name}: {cls} ({detail})")
    print("RESULT", "enforced state matches the grant" if not findings else f"{len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
