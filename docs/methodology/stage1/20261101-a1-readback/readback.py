#!/usr/bin/env python3
"""A1 feasibility probe: external read-back of HAVEN's stage-2 tables (H-ext).

Boots a HAVEN qemu-direct build in QEMU, waits for partition A to run, dumps
HAVEN's image region with QMP (stop, then pmemsave), locates pt_roots by
symbol, walks each partition's stage-2 tables, coalesces leaf descriptors
into ranges, and compares them with the expected binding derived from the
grant G (expected-G.json). Nothing runs inside HAVEN; the decoder follows the
Arm VMSAv8-64 stage-2 descriptor format for a 4 KiB granule, 39-bit IPA,
walk starting at level 1 (VTCR T0SZ=25, SL0=1).

Usage: readback.py <build-dir> <nm> <expected-G.json> <out-dir> [cpu]
Exit status: 0 = H matches G, 1 = drift found, 2 = probe failure.
"""
import json
import os
import socket
import struct
import subprocess
import sys
import time

IMG_PA, IMG_SIZE = 0x80000000, 0x800000
LEVEL_SHIFT = {1: 30, 2: 21, 3: 12}
EXPECT = {  # MM1 proposal section 4 translation table
    "RWX": {"s2ap": 3, "memattr": 0xF, "sh": 3, "xn": 0},
    "RW": {"s2ap": 3, "memattr": 0xF, "sh": 3, "xn": 2},
    "R": {"s2ap": 1, "memattr": 0xF, "sh": 3, "xn": 2},
    "RX": {"s2ap": 1, "memattr": 0xF, "sh": 3, "xn": 0},
    "DEVICE_RW": {"s2ap": 3, "memattr": 0x1, "sh": 0, "xn": 2},
    "DEVICE_R": {"s2ap": 1, "memattr": 0x1, "sh": 0, "xn": 2},
}


def qmp(sock_path, cmds, log):
    """Run QMP commands in order; return the replies. QMP avoids the human
    monitor's expression parser, which misread the dump file path."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock_path)
    s.settimeout(15)
    f = s.makefile("rw")
    replies = [json.loads(f.readline())]  # greeting
    for c in [{"execute": "qmp_capabilities"}] + cmds:
        f.write(json.dumps(c) + "\n")
        f.flush()
        while True:
            line = f.readline()
            if not line:
                break
            r = json.loads(line)
            if "event" in r:
                continue
            replies.append(r)
            break
    s.close()
    with open(log, "w") as fh:
        json.dump(replies, fh, indent=1)
    return replies


def boot_and_dump(build, out, cpu):
    sock = "/tmp/a1-%d.sock" % os.getpid()  # macOS limits socket paths to 104 bytes
    uart = os.path.join(out, "uart.log")
    mem = os.path.join(out, "haven-image.bin")
    for p in (sock, uart, mem):
        if os.path.exists(p):
            os.remove(p)
    cmd = ["qemu-system-aarch64",
           "-machine", "virt,virtualization=on,gic-version=3,iommu=smmuv3",
           "-cpu", cpu, "-smp", "4", "-m", "2G", "-display", "none",
           "-serial", "file:" + uart,
           "-qmp", "unix:%s,server,nowait" % sock,
           "-device", "loader,file=%s/haven.bin,addr=0x80000000,force-raw=on,cpu-num=0" % build,
           "-device", "loader,file=%s/guest_a.bin,addr=0x80800000,force-raw=on" % build,
           "-device", "loader,file=%s/guest_b.bin,addr=0xA0800000,force-raw=on" % build]
    with open(os.path.join(out, "qemu-command.txt"), "w") as f:
        f.write(" ".join(cmd) + "\n")
    q = subprocess.Popen(cmd)
    try:
        deadline = time.time() + 20
        while time.time() < deadline:
            if os.path.exists(uart) and "Post-fault check OK" in open(uart, errors="replace").read():
                break
            time.sleep(0.2)
        else:
            print("probe failure: partition A did not reach its post-fault line")
            return None
        qmp(sock, [{"execute": "stop"},
                   {"execute": "pmemsave", "arguments": {"val": IMG_PA, "size": IMG_SIZE,
                                                          "filename": os.path.abspath(mem)}},
                   {"execute": "quit"}], os.path.join(out, "qmp.json"))
        q.wait(timeout=10)
    finally:
        if q.poll() is None:
            q.kill()
        if os.path.exists(sock):
            os.remove(sock)
    return open(mem, "rb").read() if os.path.exists(mem) else None


def symbol(nm, elf, name):
    for line in subprocess.check_output([nm, elf], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit("symbol %s not found" % name)


def rd64(mem, pa):
    off = pa - IMG_PA
    if off < 0 or off + 8 > len(mem):
        raise ValueError("PA 0x%x outside the dumped image" % pa)
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
            leaves.append({"ipa": ipa, "error": "reserved L3 encoding", "raw": d})
            continue
        size = 1 << LEVEL_SHIFT[level]
        oa = d & 0x0000FFFFFFFFF000 & ~(size - 1)
        leaves.append({"ipa": ipa, "size": size, "pa": oa,
                       "memattr": (d >> 2) & 0xF, "s2ap": (d >> 6) & 3,
                       "sh": (d >> 8) & 3, "af": (d >> 10) & 1,
                       "xn": (d >> 53) & 3, "level": level})


def coalesce(leaves):
    out = []
    for l in sorted((x for x in leaves if "error" not in x), key=lambda x: x["ipa"]):
        key = (l["memattr"], l["s2ap"], l["sh"], l["af"], l["xn"])
        if out and out[-1]["key"] == key and out[-1]["ipa"] + out[-1]["size"] == l["ipa"] \
                and out[-1]["pa"] + out[-1]["size"] == l["pa"]:
            out[-1]["size"] += l["size"]
        else:
            out.append(dict(l, key=key))
    for r in out:
        r.pop("key")
    return out


def compare(h, g):
    findings = []
    for want in g:
        ipa, pa, size = (int(want[k], 16) for k in ("ipa", "pa", "size"))
        exp = EXPECT[want["attrs"]]
        cover = [r for r in h if r["ipa"] < ipa + size and r["ipa"] + r["size"] > ipa]
        if not cover:
            findings.append((want["name"], "missing binding", "no descriptor"))
            continue
        got = sum(min(r["ipa"] + r["size"], ipa + size) - max(r["ipa"], ipa) for r in cover)
        if got < size:
            findings.append((want["name"], "missing binding", "0x%x of 0x%x bytes mapped" % (got, size)))
        for r in cover:
            if r["ipa"] < ipa or r["ipa"] + r["size"] > ipa + size:
                findings.append((want["name"], "extent drift", "range 0x%x+0x%x crosses grant" % (r["ipa"], r["size"])))
            if r["pa"] - r["ipa"] != pa - ipa:
                findings.append((want["name"], "association drift", "OA 0x%x for IPA 0x%x" % (r["pa"], r["ipa"])))
            for field, cls in (("s2ap", "access drift"), ("xn", "execute drift"),
                               ("memattr", "memory-type drift"), ("sh", "shareability drift")):
                if r[field] != exp[field]:
                    findings.append((want["name"], cls, "%s=%#x expected %#x" % (field, r[field], exp[field])))
            if r["af"] != 1:
                findings.append((want["name"], "access-flag drift", "AF=0"))
    for r in h:
        if not any(int(w["ipa"], 16) <= r["ipa"] < int(w["ipa"], 16) + int(w["size"], 16) for w in g):
            findings.append(("(none)", "extra binding", "IPA 0x%x+0x%x not granted" % (r["ipa"], r["size"])))
    return findings


def main():
    build, nm, gfile, out = sys.argv[1:5]
    cpu = sys.argv[5] if len(sys.argv) > 5 else "cortex-a72"
    os.makedirs(out, exist_ok=True)
    mem = boot_and_dump(build, out, cpu)
    if mem is None or len(mem) != IMG_SIZE:
        print("probe failure: no memory dump")
        return 2
    roots = symbol(nm, os.path.join(build, "haven.elf"), "pt_roots")
    g = json.load(open(gfile))["partitions"]
    report, status = {"cpu": cpu, "pt_roots": hex(roots), "partitions": {}}, 0
    all_pa = []
    for pid, grants in g.items():
        root = rd64(mem, roots + 8 * int(pid))
        leaves = []
        walk(mem, root, 1, 0, leaves)
        h = coalesce(leaves)
        f = compare(h, grants)
        all_pa += [(pid, r["pa"], r["size"]) for r in h]
        report["partitions"][pid] = {
            "root_pa": hex(root),
            "H": [{k: (hex(v) if k in ("ipa", "pa", "size") else v) for k, v in r.items()} for r in h],
            "findings": f}
        print("partition %s root=%#x" % (pid, root))
        for r in h:
            print("  H ipa=%#011x pa=%#011x size=%#010x s2ap=%d memattr=%#x sh=%d xn=0b%s af=%d"
                  % (r["ipa"], r["pa"], r["size"], r["s2ap"], r["memattr"], r["sh"],
                     format(r["xn"], "02b"), r["af"]))
        for name, cls, detail in f:
            print("  DRIFT %s: %s (%s)" % (name, cls, detail))
            status = 1
    for i, (p1, a1, s1) in enumerate(all_pa):
        for p2, a2, s2 in all_pa[i + 1:]:
            if p1 != p2 and a1 < a2 + s2 and a2 < a1 + s1:
                print("  DRIFT cross-partition alias: PA %#x reachable from %s and %s" % (max(a1, a2), p1, p2))
                status = 1
    json.dump(report, open(os.path.join(out, "H-ext.json"), "w"), indent=1)
    print("RESULT", "H matches G" if status == 0 else "drift found")
    return status


if __name__ == "__main__":
    sys.exit(main())
