"""Grant files for read-back: format 1 (HAVEN's own) and format 2 (FF-A vocabulary).

Format 2 writes the grant in the vocabulary of Arm's FF-A partition
manifest binding (Hafnium and TF-A): per partition, `memory-regions` and
`device-regions` with `base-address`, `pages-count` (4 KiB pages),
`attributes` (0x1 read, 0x2 write, 0x4 execute), `smmu-id`, `stream-ids`,
`stream-ids-access-permissions` and `interrupts`, where each interrupt's
attributes use the binding's layout (bits 7:0 priority, bit 9 level
triggered, bits 11:10 type: 0b10 SPI, 0b01 PPI). It is a JSON rendering
of those properties, not a device tree. HAVEN needs a few things FF-A does
not express; they carry a `haven,` prefix, as vendor properties do in a
device tree:

- `haven,pa`: the physical address of a region (static partitioning picks
  it; FF-A leaves it to the partition manager). Absent: identity mapped.
- `haven,cpus`: the CPUs a partition owns.
- `haven,target-cpu`: the CPU an SPI is routed to.
- `haven,ppis`: per-CPU private interrupts the partition owns
  ({"cpu": [intid, ...]}).
- top-level `haven`: what belongs to HAVEN itself (its interrupts, its
  per-CPU timer PPIs, priorities, SMMU state, devices allowed to stay
  unenforced).

One deviation from FF-A, deliberate: a memory region with `stream-ids` is
a DMA window only. It must lie inside a CPU-mapped region of the same
partition, and read-back checks it on the SMMU side, not as a second CPU
mapping. In FF-A the same region would also be mapped for the partition.

Read-back compares against format 1 internally; `load()` accepts either and
returns format 1, so the frozen case sets (format 1) are read unchanged.
"""
import json

PAGE = 0x1000
MEM_ATTRS = {"R": 0x1, "RW": 0x3, "RX": 0x5, "RWX": 0x7}
DEV_ATTRS = {"DEVICE_R": 0x1, "DEVICE_RW": 0x3}
DMA_PERMS = {"R": 0x1, "W": 0x2, "RW": 0x3}
SPI_TYPE, PPI_TYPE, LEVEL = 0b10 << 10, 0b01 << 10, 1 << 9


def _hex(v):
    return "0x%X" % v


def _num(v):
    return int(v, 16) if isinstance(v, str) else int(v)


def _inv(d):
    return {v: k for k, v in d.items()}


def to_v2(v1, level_spis=()):
    """Format 1 -> format 2. `level_spis` lists SPIs that are level triggered."""
    gic = v1.get("gic", {})
    smmu = v1.get("smmu", {})
    prio = _num(gic.get("priority", {}).get("partition", "0xa0"))
    parts = []
    for pid, regions in v1["partitions"].items():
        p = {"id": int(pid), "memory-regions": [], "device-regions": []}
        p["haven,cpus"] = sorted(int(c) for c, o in gic.get("cpus", {}).items() if int(o) == int(pid))
        for r in regions:
            size = _num(r["size"])
            if size % PAGE:
                raise ValueError(f"{r['name']}: size {size:#x} is not a multiple of 4 KiB")
            out = {"description": r["name"], "base-address": r["ipa"], "pages-count": size // PAGE}
            if _num(r["pa"]) != _num(r["ipa"]):
                out["haven,pa"] = r["pa"]
            if r["attrs"] in DEV_ATTRS:
                out["attributes"] = _hex(DEV_ATTRS[r["attrs"]])
                p["device-regions"].append(out)
            else:
                out["attributes"] = _hex(MEM_ATTRS[r["attrs"]])
                p["memory-regions"].append(out)
        for sid, s in smmu.get("streams", {}).items():
            if int(s["partition"]) != int(pid):
                continue
            size = _num(s["window"]["size"])
            perm = _hex(DMA_PERMS[s.get("access", "RW")])
            p["memory-regions"].append({
                "description": f"DMA window, stream {sid}", "base-address": s["window"]["ipa"],
                "pages-count": size // PAGE, "attributes": perm, "smmu-id": 0,
                "stream-ids": [int(sid)], "stream-ids-access-permissions": [perm]})
        spis = []
        for irq, s in gic.get("spis", {}).items():
            if int(s["partition"]) == int(pid):
                attrs = SPI_TYPE | prio | (LEVEL if int(irq) in level_spis else 0)
                spis.append({"id": int(irq), "attributes": _hex(attrs), "haven,target-cpu": int(s["cpu"])})
        if spis:
            # FF-A attaches interrupts to a device region; format 1 does not
            # say which device an SPI belongs to, so they go on a named
            # pseudo device with no registers.
            p["device-regions"].append({"description": "interrupts", "pages-count": 0,
                                        "attributes": "0x0", "interrupts": spis})
        ppis = {c: v for c, v in gic.get("ppis", {}).items()
                if str(c) in {str(x) for x in p["haven,cpus"]}}
        if ppis:
            p["haven,ppis"] = {str(c): sorted(v) for c, v in ppis.items()}
        parts.append(p)
    haven = {
        "interrupts": [{"id": int(i), "haven,target-cpu": int(s["cpu"])}
                       for i, s in gic.get("spis", {}).items() if int(s["partition"]) == 0],
        "ppis": {str(c): sorted(v) for c, v in gic.get("haven_ppis", {}).items()},
        "unassigned-cpus": sorted(int(c) for c, o in gic.get("cpus", {}).items() if int(o) == 0),
        "priorities": gic.get("priority", {}),
        "smmu": {"enabled": smmu.get("enabled", True),
                 "allowed-unenforced": smmu.get("allowed_unenforced", [])},
    }
    if gic.get("guest_enabled"):  # interrupts HAVEN enables on the guest's request
        haven["guest-enabled"] = {str(c): sorted(v) for c, v in gic["guest_enabled"].items()}
    out = {"format": "haven-grant/2", "vocabulary": "FF-A partition manifest binding; haven,* are HAVEN extensions",
           "partitions": parts, "haven": haven}
    if "source" in v1:
        out["source"] = v1["source"]
    if "gic" not in v1:
        out["haven"]["no-gic"] = True
    if "smmu" not in v1:
        out["haven"]["no-smmu"] = True
    return out


def to_v1(v2):
    """Format 2 -> format 1 (what read-back compares against)."""
    if not str(v2.get("format", "")).startswith("haven-grant/2"):
        raise ValueError("not a format 2 grant")
    mem_inv, dev_inv, dma_inv = _inv(MEM_ATTRS), _inv(DEV_ATTRS), _inv(DMA_PERMS)
    haven = v2.get("haven", {})
    v1 = {"partitions": {}}
    if "source" in v2:
        v1["source"] = v2["source"]
    streams, spis, cpus, ppis = {}, {}, {}, {}
    for p in v2["partitions"]:
        pid = int(p["id"])
        regions, cpu_mapped = [], []
        for r in p.get("memory-regions", []):
            ipa, size = _num(r["base-address"]), r["pages-count"] * PAGE
            if "stream-ids" in r:
                continue  # DMA window, handled below
            regions.append({"name": r.get("description", ""), "ipa": _hex(ipa),
                            "pa": _hex(_num(r.get("haven,pa", ipa))), "size": _hex(size),
                            "attrs": mem_inv[_num(r["attributes"])]})
            cpu_mapped.append((ipa, size))
        for r in p.get("memory-regions", []):
            if "stream-ids" not in r:
                continue
            ipa, size = _num(r["base-address"]), r["pages-count"] * PAGE
            if not any(a <= ipa and ipa + size <= a + s for a, s in cpu_mapped):
                raise ValueError(f"partition {pid}: DMA window {ipa:#x} is not inside a CPU-mapped region")
            perms = r.get("stream-ids-access-permissions", [r["attributes"]])
            for i, sid in enumerate(r["stream-ids"]):
                perm = perms[i] if i < len(perms) else perms[-1]
                streams[str(sid)] = {"partition": pid, "window": {"ipa": _hex(ipa), "size": _hex(size)},
                                     "access": dma_inv[_num(perm)]}
        for r in p.get("device-regions", []):
            for irq in r.get("interrupts", []):
                if (_num(irq["attributes"]) >> 10) & 0b11 == 0b10:
                    spis[str(irq["id"])] = {"partition": pid, "cpu": irq["haven,target-cpu"]}
            if r.get("pages-count", 0) == 0:
                continue
            ipa = _num(r["base-address"])
            regions.append({"name": r.get("description", ""), "ipa": _hex(ipa),
                            "pa": _hex(_num(r.get("haven,pa", ipa))),
                            "size": _hex(r["pages-count"] * PAGE),
                            "attrs": dev_inv[_num(r["attributes"])]})
        for c in p.get("haven,cpus", []):
            cpus[str(c)] = pid
        for c, v in p.get("haven,ppis", {}).items():
            ppis[str(c)] = list(v)
        v1["partitions"][str(pid)] = regions
    for c in haven.get("unassigned-cpus", []):
        cpus[str(c)] = 0
    for irq in haven.get("interrupts", []):
        spis[str(irq["id"])] = {"partition": 0, "cpu": irq["haven,target-cpu"]}
    if not haven.get("no-smmu"):
        v1["smmu"] = {"enabled": haven.get("smmu", {}).get("enabled", True), "streams": streams,
                      "allowed_unenforced": haven.get("smmu", {}).get("allowed-unenforced", [])}
    if not haven.get("no-gic"):
        gic = {"cpus": cpus, "spis": spis, "ppis": ppis}
        if haven.get("priorities"):
            gic["priority"] = haven["priorities"]
        if haven.get("ppis"):
            gic["haven_ppis"] = haven["ppis"]
        if haven.get("guest-enabled"):
            gic["guest_enabled"] = haven["guest-enabled"]
        v1["gic"] = gic
    return v1


def load(path):
    """Read a grant file of either format; return format 1."""
    g = json.load(open(path))
    if str(g.get("format", "")).startswith("haven-grant/2"):
        return to_v1(g)
    return g


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="Convert a format 1 grant to format 2 (FF-A vocabulary)")
    ap.add_argument("grant")
    ap.add_argument("--level-spis", default="33", help="comma-separated level-triggered SPIs (default: 33, the PL011)")
    a = ap.parse_args()
    lv = [int(x) for x in a.level_spis.split(",") if x]
    print(json.dumps(to_v2(json.load(open(a.grant)), lv), indent=1))
