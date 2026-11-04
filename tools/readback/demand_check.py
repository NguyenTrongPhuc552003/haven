#!/usr/bin/env python3
"""Check a driver's resource demand against a HAVEN grant.

Usage: demand_check.py GRANT DEMAND --partition N

GRANT is a HAVEN grant (format 1, or format 2 in the FF-A vocabulary, see
grant.py). DEMAND is what KDAL reports in /sys/kernel/debug/kdal/demand
(format "kdal-demand/1": FF-A device-regions with base-address,
pages-count, attributes, interrupts, smmu-id, stream-ids), with addresses
as the partition sees them (IPA).

Findings:
- unmet MMIO demand: a demanded page range is not inside a device region
  granted to the partition;
- insufficient access: granted, but without the demanded read or write;
- unmet interrupt demand: an SPI not granted to the partition, or a PPI
  not granted on any of the partition's CPUs;
- unmet DMA demand: a StreamID not granted to the partition.
Granted device regions nothing demands are listed as "not demanded"
(information: candidates for a smaller grant), not as findings.

Exit status: 0 every demand is met, 1 findings, 2 bad input.
"""
import argparse
import json
import sys

import grant as grant_file

PAGE = 0x1000
DEV_ACCESS = {"DEVICE_R": 0x1, "DEVICE_RW": 0x3}


def check(g, demand, pid):
    """Return (findings, info); g is a format 1 grant."""
    pid = int(pid)
    regions = [r for r in g["partitions"].get(str(pid), []) if r["attrs"] in DEV_ACCESS]
    granted = [(int(r["ipa"], 16), int(r["size"], 16), DEV_ACCESS[r["attrs"]], r["name"]) for r in regions]
    gic = g.get("gic", {})
    spis = {int(i) for i, s in gic.get("spis", {}).items() if int(s["partition"]) == pid}
    cpus = {int(c) for c, o in gic.get("cpus", {}).items() if int(o) == pid}
    ppis = {i for c, v in gic.get("ppis", {}).items() if int(c) in cpus for i in v}
    streams = {int(s) for s, v in g.get("smmu", {}).get("streams", {}).items() if int(v["partition"]) == pid}

    findings, used = [], set()
    for d in demand.get("device-regions", []):
        name = d.get("description", "?")
        pages = int(d.get("pages-count", 0))
        if pages:
            base = int(d["base-address"], 16)
            end = base + pages * PAGE
            want = int(str(d.get("attributes", "0x3")), 16) & 0x3
            inside = [x for x in granted if x[0] <= base and end <= x[0] + x[1]]
            if not inside:
                findings.append((name, "unmet MMIO demand", f"{base:#x}-{end:#x} not granted to partition {pid}"))
            else:
                used.update(x[3] for x in inside)
                if not any(want & x[2] == want for x in inside):
                    findings.append((name, "insufficient access",
                                     f"wants {want:#x}, granted {max(x[2] for x in inside):#x}"))
        for irq in d.get("interrupts", []):
            i = int(irq["id"])
            ok = i in spis if i >= 32 else i in ppis if i >= 16 else False
            if not ok:
                findings.append((name, "unmet interrupt demand", f"INTID {i} not granted to partition {pid}"))
        for sid in d.get("stream-ids", []):
            if int(sid) not in streams:
                findings.append((name, "unmet DMA demand", f"StreamID {sid} not granted to partition {pid}"))
    info = [f"not demanded: {x[3]} {x[0]:#x}+{x[1]:#x}" for x in granted if x[3] not in used]
    return findings, info


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("grant")
    ap.add_argument("demand")
    ap.add_argument("--partition", required=True, type=int)
    a = ap.parse_args()
    try:
        g = grant_file.load(a.grant)
        demand = json.load(open(a.demand))
    except (OSError, ValueError, KeyError) as e:
        print(f"bad input: {e}")
        return 2
    if not str(demand.get("format", "")).startswith("kdal-demand/"):
        print("bad input: demand format is not kdal-demand/*")
        return 2
    findings, info = check(g, demand, a.partition)
    for name, cls, detail in findings:
        print(f"  FINDING {name}: {cls} ({detail})")
    for line in info:
        print(f"  info {line}")
    print("RESULT every demand is met by the grant" if not findings else f"RESULT {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
