#!/usr/bin/env python3
"""Check a guest's device tree (T) against its partition's grant (G).

Usage: dt_check.py GRANT DT --partition N [--emulated BASE+SIZE ...]

DT is a .dts or .dtb (a .dts is compiled with dtc). GRANT is a HAVEN grant
(format 1 or 2, see grant.py). --emulated names register ranges HAVEN
emulates for the partition (for the Linux profile: the GIC distributor
0x08000000+0x10000 and redistributor 0x080a0000+0x20000); a device there
needs no grant.

What the guest is told must be what it is granted:
- memory: every memory node range lies in RAM granted to the partition
  (a Normal-memory region);
- initramfs: /chosen linux,initrd-start..end lies in granted RAM;
- device registers: every reg range of an enabled device node lies in a
  device region granted to the partition, or in an emulated range;
- interrupts (GIC, three cells): every SPI is granted to the partition,
  every PPI to the partition on one of its CPUs. For the architected
  timer (arm,armv8-timer) only the virtual timer (third entry) must be
  granted: a Linux guest at EL1 uses it (drivers/clocksource/
  arm_arch_timer.c); the other three are listed as information;
- CPUs: no more cpu nodes than CPUs assigned to the partition.

Exit status: 0 the device tree matches the grant, 1 findings, 2 bad input.
Written after Linux chain case set v1 (finding 5): T4 and T5 would have
been caught before boot.
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

import grant as grant_file

NORMAL = {"R", "RW", "RX", "RWX"}
DEVICE = {"DEVICE_R", "DEVICE_RW"}


# ------------------------------------------------------------ FDT reader
def parse_fdt(blob):
    """Device tree blob -> nested dict {"name", "props": {name: bytes}, "children": [...]}."""
    magic, _, off_struct, off_strings = struct.unpack_from(">IIII", blob, 0)
    if magic != 0xD00DFEED:
        raise ValueError("not a device tree blob")
    strings = blob[off_strings:]
    pos, stack, root = off_struct, [], None
    while True:
        (tok,) = struct.unpack_from(">I", blob, pos)
        pos += 4
        if tok == 1:  # BEGIN_NODE
            end = blob.index(b"\0", pos)
            node = {"name": blob[pos:end].decode(), "props": {}, "children": []}
            pos = (end + 4) & ~3
            if stack:
                stack[-1]["children"].append(node)
            else:
                root = node
            stack.append(node)
        elif tok == 2:  # END_NODE
            stack.pop()
        elif tok == 3:  # PROP
            ln, nameoff = struct.unpack_from(">II", blob, pos)
            pos += 8
            name = strings[nameoff:strings.index(b"\0", nameoff)].decode()
            stack[-1]["props"][name] = blob[pos:pos + ln]
            pos = (pos + ln + 3) & ~3
        elif tok == 4:  # NOP
            continue
        elif tok == 9:  # END
            return root
        else:
            raise ValueError(f"bad token {tok}")


def cells(v):
    return list(struct.unpack(f">{len(v) // 4}I", v))


def num(cs):
    n = 0
    for c in cs:
        n = (n << 32) | c
    return n


def strs(v):
    return [s.decode() for s in v.split(b"\0") if s]


def load_dt(path):
    if path.endswith(".dtb"):
        with open(path, "rb") as f:
            return parse_fdt(f.read())
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "t.dtb")
        r = subprocess.run(["dtc", "-q", "-I", "dts", "-O", "dtb", "-o", out, path], capture_output=True, text=True)
        if r.returncode:
            raise ValueError(f"dtc: {r.stderr.strip()}")
        with open(out, "rb") as f:
            return parse_fdt(f.read())


# ------------------------------------------------------------ the check
def walk(node, path, addr_cells, size_cells, phandles, out):
    p = node["props"]
    here = path + "/" + node["name"] if node["name"] else "/"
    if "phandle" in p:
        phandles[cells(p["phandle"])[0]] = node
    out.append((here, node, addr_cells, size_cells))
    ac = cells(p["#address-cells"])[0] if "#address-cells" in p else 2
    sc = cells(p["#size-cells"])[0] if "#size-cells" in p else 1
    for c in node["children"]:
        walk(c, here.rstrip("/"), ac, sc, phandles, out)


def ranges_of(reg, ac, sc):
    cs, step, out = cells(reg), ac + sc, []
    for i in range(0, len(cs) - step + 1, step):
        out.append((num(cs[i:i + ac]), num(cs[i + ac:i + step])))
    return out


def inside(base, size, spans):
    """Is [base, base+size) covered by the union of spans?"""
    pos, end = base, base + size
    for b, s in sorted(spans):
        if b <= pos < b + s:
            pos = b + s
        if pos >= end:
            return True
    return pos >= end


def check(g, root, pid, emulated):
    pid = int(pid)
    regions = g["partitions"].get(str(pid), [])
    ram = [(int(r["ipa"], 16), int(r["size"], 16)) for r in regions if r["attrs"] in NORMAL]
    dev = [(int(r["ipa"], 16), int(r["size"], 16)) for r in regions if r["attrs"] in DEVICE] + list(emulated)
    gic = g.get("gic", {})
    spis = {int(i) for i, s in gic.get("spis", {}).items() if int(s["partition"]) == pid}
    cpus = {int(c) for c, o in gic.get("cpus", {}).items() if int(o) == pid}
    ppis = {i for c, v in gic.get("ppis", {}).items() if int(c) in cpus for i in v}

    nodes, phandles = [], {}
    walk(root, "", 2, 1, phandles, nodes)
    root_parent = cells(root["props"]["interrupt-parent"])[0] if "interrupt-parent" in root["props"] else None
    findings, info = [], []

    ncpu = sum(1 for path, n, _, _ in nodes if path.startswith("/cpus/") and n["props"].get("device_type", b"").startswith(b"cpu"))
    if gic and ncpu > len(cpus):
        findings.append(f"/cpus: {ncpu} CPU nodes, partition {pid} has {len(cpus)} CPU(s)")

    for path, n, ac, sc in nodes:
        p = n["props"]
        if strs(p.get("status", b"okay")) not in ([], ["okay"], ["ok"]):
            continue
        compat = strs(p.get("compatible", b""))
        if path == "/chosen" and "linux,initrd-start" in p and "linux,initrd-end" in p:
            s, e = num(cells(p["linux,initrd-start"])), num(cells(p["linux,initrd-end"]))
            if e > s and not inside(s, e - s, ram):
                findings.append(f"/chosen: initramfs {s:#x}-{e:#x} not in RAM granted to partition {pid}")
        if "reg" in p and not path.startswith("/cpus"):
            is_mem = strs(p.get("device_type", b"")) == ["memory"]
            for base, size in ranges_of(p["reg"], ac, sc):
                if is_mem and not inside(base, size, ram):
                    findings.append(f"{path}: memory {base:#x}+{size:#x} not granted to partition {pid}")
                elif not is_mem and size and not inside(base, size, dev):
                    findings.append(f"{path}: registers {base:#x}+{size:#x} not granted to partition {pid}")
        if "interrupts" in p and "interrupt-controller" not in p:
            parent = phandles.get(cells(p["interrupt-parent"])[0] if "interrupt-parent" in p else root_parent)
            if not parent or "arm,gic-v3" not in strs(parent["props"].get("compatible", b"")):
                info.append(f"{path}: interrupts of a non-GICv3 parent not checked")
                continue
            cs = cells(p["interrupts"])
            entries = [cs[i:i + 3] for i in range(0, len(cs) - 2, 3)]
            for k, (typ, nr, _flags) in enumerate(entries):
                intid = nr + (32 if typ == 0 else 16)
                timer_other = "arm,armv8-timer" in compat and k != 2
                if timer_other:
                    info.append(f"{path}: timer interrupt {intid} (entry {k}) not used by an EL1 guest")
                    continue
                ok = intid in spis if typ == 0 else intid in ppis
                if not ok:
                    kind = "SPI" if typ == 0 else "PPI"
                    findings.append(f"{path}: {kind} {intid} not granted to partition {pid}")
    return findings, info


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("grant")
    ap.add_argument("dt")
    ap.add_argument("--partition", required=True)
    ap.add_argument("--emulated", action="append", default=[], metavar="BASE+SIZE")
    a = ap.parse_args()
    try:
        g = grant_file.load(a.grant)
        root = load_dt(a.dt)
        emulated = [tuple(int(x, 0) for x in e.split("+")) for e in a.emulated]
    except (OSError, ValueError, KeyError) as e:
        print(f"bad input: {e}")
        return 2
    findings, info = check(g, root, a.partition, emulated)
    for f in findings:
        print(f"  FINDING {f}")
    for i in info:
        print(f"  info {i}")
    print(f"RESULT {len(findings)} finding(s)" if findings else "RESULT the device tree matches the grant")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
