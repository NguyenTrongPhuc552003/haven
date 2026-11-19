#!/usr/bin/env python3
"""Campaign step C2: the written cases, as edits, into patches and a cases file.

Each case is one or more exact edits (each must match exactly once in the
tree at HEAD); v3's ten other written cases and M5 come from their patches
in tools/campaign/cases/. Expectations are per method (M1 to M4 and M6;
M5 and M7 are n/a), proposed by Claude and approved by the owner before
the run (CASES-C2.md). Run from the repository root:

  make_cases.py --check   every edit matches exactly once; nothing written
  make_cases.py           write tools/campaign/cases/c2/*.patch and
                          tools/campaign/cases/c2-written.json
"""

import json
import os
import subprocess
import sys

PART = "src/core/partition.c"
MM = "arch/arm64/mm.c"
S2A = "arch/arm64/include/asm/s2_attrs.h"
SMMU = "src/core/dma/smmu.c"
SMMUV3 = "drivers/iommu/smmu_v3.c"
GIC = "drivers/irqchip/gic_v3.c"
OWN = "src/core/irq/ownership.c"
BUD = "arch/arm64/budget_hw.c"
PLAT = "src/platform/qemu-virt/platform.c"
GRANT = "docs/methodology/stage2/20261108-el3-readback/grant-demo-cpus.json"

P, F = "pass", "finding"


def exp(m1=P, m2=P, m4=P, m6=F):
    return {"M1": m1, "M2": m2, "M3": P, "M4": m4, "M6": m6}


# id: (class, [(file, old, new)], expectations, expected M6 finding text or why)
CASES = {
    "W01": ("S2-EXTENT", [(MM, "\tuint64_t cur_pa = pa;\n\tuint64_t end_ipa = ipa + size;\n",
                           "\tuint64_t cur_pa = pa;\n\tuint64_t end_ipa = ipa + size + HV_PAGE_SIZE;\n")],
            exp(m2=F), "extent drift on every region (one page more); A's probe of 0x60000000 no longer faults"),
    "W02": ("S2-EXTENT", [(PART, ".size = 0x1000UL, /* 4 KB MMIO page */", ".size = 0x2000UL, /* 4 KB MMIO page */")],
            exp(m1=F), "A UART: extent drift"),
    "W03": ("S2-PERM", [(PART, "\t\t.attrs = HV_MEM_R,\n", "\t\t.attrs = HV_MEM_RW,\n")],
            exp(m1=F), "A RO: access drift"),
    "W05": ("S2-PERM", [(MM, "S2_DESC_BLOCK | hw_attrs | S2_AF;",
                         "S2_DESC_BLOCK | (hw_attrs & ~(uint64_t)S2_XN_NEVER) | S2_AF;")],
            exp(), "A RO: execute drift (2 MiB blocks only)"),
    "W06": ("S2-TYPE", [(MM, "S2_DESC_BLOCK | hw_attrs | S2_AF;",
                         "S2_DESC_BLOCK | (hw_attrs & ~(uint64_t)S2_SH_IS) | S2_AF;")],
            exp(), "A RAM, B RAM, A RO: shareability drift (2 MiB blocks only)"),
    "W07": ("S2-TYPE", [(PART, ".attrs = HV_MEM_DEVICE_RW, /* Device nGnRE, never executable */",
                         ".attrs = HV_MEM_RW, /* Device nGnRE, never executable */")],
            exp(m1=F), "A UART: memory-type and shareability drift"),
    "W08": ("S2-OWNER", [(PART, "\t.partition_id = PARTITION_B_ID,\n\t.regions = part_b_regions,",
                          "\t.partition_id = PARTITION_A_ID,\n\t.regions = part_b_regions,")],
            exp(m1=F, m2=F, m4=F), "B RAM: association drift (mapped for A); B cannot run"),
    "W11": ("S2-MISSING", [(PART, ".region_count = sizeof(part_a_regions) / sizeof(part_a_regions[0]),",
                            ".region_count = sizeof(part_a_regions) / sizeof(part_a_regions[0]) - 1U,")],
            exp(m1=F), "A RO: missing binding"),
    "W12": ("S2-EXTRA", [(PART, "\t\t.attrs = HV_MEM_R,\n\t},\n#endif\n};",
                          "\t\t.attrs = HV_MEM_R,\n\t},\n#endif\n\t{\n\t\t.ipa_base = 0x71000000UL,\n"
                          "\t\t.pa_base = SHARED_PA_BASE,\n\t\t.size = SHARED_SIZE,\n\t\t.attrs = HV_MEM_RW,\n\t},\n};")],
            exp(m1=F), "extra binding: the shared memory in A at 0x71000000"),
    "W14": ("SMMU-UNBOUND", [(SMMU, "rc = smmu_v3_set_ste_partition(streamid, vttbr, vmid);", "rc = HV_OK;")],
            exp(), "SID 8: DMA enforcement drift (the stream stays aborting while HAVEN reports the grant)"),
    "W16": ("SMMU-EXTENT", [(PART, "\t\t.size = PART_A_DMA_SIZE,\n", "\t\t.size = PART_A_DMA_SIZE * 2U,\n")],
            exp(m1=F), "SID 8: DMA extent drift (2 MiB window)"),
    "W17": ("SMMU-EXTENT", [(MM, "rc = s2_map_table(dma_roots[sid], iova, pa, size,",
                             "rc = s2_map_table(dma_roots[sid], iova, pa, size + HV_PAGE_SIZE,")],
            exp(), "SID 8: DMA extent drift (one page more)"),
    "W19": ("SMMU-PERM", [(MM, "((uint64_t)access << S2AP_SHIFT));", "((uint64_t)1U << S2AP_SHIFT));")],
            exp(), "SID 8: DMA access drift (read-only)"),
    "W21": ("SMMU-TAG", [(SMMUV3, "ste[2] = STE2_S2_ENCODE(vmid);", "ste[2] = STE2_S2_ENCODE(vmid + 1U);")],
            exp(m6=P), "predicted miss: read-back does not compare a DMA VMID with the grant (v3's S9)"),
    "W22": ("GIC-ROUTE", [(OWN, "uint64_t mpidr = cpu_to_mpidr(route->target_cpu);",
                           "uint64_t mpidr = cpu_to_mpidr(route->target_cpu + 1U);")],
            exp(), "SPI 33: interrupt routing drift (CPU 1)"),
    "W23": ("GIC-ROUTE", [(GIC, "\t\t    gicd_irouter_from_mpidr(mpidr));", "\t\t    GICD_IROUTER_IRM);")],
            exp(), "SPI 33: interrupt routing drift (1-of-N)"),
    "W24": ("GIC-ENABLE", [(OWN, "\t\tgic_v3_enable_irq(route->irq_id);\n", "")],
            exp(), "SPI 33: interrupt enable drift"),
    "W27": ("GIC-GROUP", [(GIC, "gic_write32(gic.gicd_base, GICD_IGROUPR(i), 0xffffffff);",
                           "gic_write32(gic.gicd_base, GICD_IGROUPR(i), 0x0);")],
            exp(), "SPI 33: interrupt group drift"),
    "W29": ("GIC-PRIO", [(BUD, "GIC_PRIO_HYPERVISOR) !=", "GIC_PRIO_LINUX) !=")],
            exp(), "CPU 0 and CPU 2 PPI 26: interrupt priority drift"),
    "W30": ("GIC-PPI", [(PART, "\t .target_cpu = PART_B_CPU},", "\t .target_cpu = 1U},")],
            exp(m1=F, m4=F), "CPU 2 PPI 27: missing interrupt binding (enabled on CPU 1)"),
    "W31": ("GIC-PPI", [(OWN, "gic_v3_enable_ppi(route->target_cpu, route->irq_id);",
                         "gic_v3_enable_ppi(0U, route->irq_id);")],
            exp(m4=F), "CPU 2 PPI 27: missing interrupt binding (enabled on CPU 0)"),
    "W09": ("CPU-ASSIGN", [(PART, "hv_arch_stage2_enable(PARTITION_B_ID, PARTITION_B_ID);",
                            "hv_arch_stage2_enable(PARTITION_A_ID, PARTITION_B_ID);")],
            exp(m4=F), "CPU 2: CPU assignment drift (A's tables); B faults at its entry"),
    "W32": ("CPU-ASSIGN", [(PART, "hv_arch_set_cpu_partition(hv_arch_cpu_id(), PARTITION_B_ID);",
                            "hv_arch_set_cpu_partition(hv_arch_cpu_id(), PARTITION_A_ID);")],
            exp(m4=F), "CPU 2: CPU assignment drift; B's timer is no longer injected"),
    "W33": ("EL2-MAP", [(PLAT, "{HAVEN_LOAD_PA, HAVEN_SIZE, HV_EL2_NORMAL},",
                         "{HAVEN_LOAD_PA, HAVEN_SIZE + 0x20000000UL, HV_EL2_NORMAL},")],
            exp(), "partition memory mapped at EL2 (A RAM)"),
    "W25": ("EL2-TIMER", [(BUD, "\t\t    HV_OK ||\n\t    gic_v3_enable_ppi(cpu, HV_CNTHP_PPI) != HV_OK)",
                           "\t\t    HV_OK)")],
            exp(), "CPU 0 and CPU 2 PPI 26: missing interrupt binding"),
    "W34": ("EL2-TIMER", [(BUD, "b->budget = ns_to_ticks(budget_ns);", "b->budget = ns_to_ticks(budget_ns * 4U);")],
            exp(m6=P), "predicted miss: read-back reads no budget value (A's 8 ms becomes 32 ms, B's 2 ms 8 ms)"),
    "W35": ("CONTROL", [(GIC, "GICD_ISENABLER(irq / 32), 1U << (irq % 32));",
                         "GICD_ISENABLER(irq / 32), 1U << (irq & 31U));")],
            exp(m6=P), "control: the same bit for an unsigned irq"),
    "W36": ("CONTROL", [(S2A, "\thw |= ((attrs & HV_MEM_W) != 0U) ? (uint64_t)S2AP_RW :\n\t\t\t\t\t   (uint64_t)S2AP_RO;\n",
                         "\tif ((attrs & HV_MEM_W) != 0U) {\n\t\thw |= (uint64_t)S2AP_RW;\n\t} else {\n"
                         "\t\thw |= (uint64_t)S2AP_RO;\n\t}\n")],
            exp(m6=P), "control: the same bits through an if"),
}

# v3's written cases (CASES-v3.md), from their patches; M1 as today's tests
# showed in the C1 runs (the baseline grew since v3), M6 as v3's approved list
V3 = {
    "M3": ("S2-PERM", exp(m1=F, m2=F, m4=F), "A RAM, B RAM: execute drift"),
    "M4": ("S2-TYPE", exp(m1=F), "A UART, PCI config, PCI BAR: memory-type and shareability drift"),
    "M5": ("S2-EXTENT", exp(m1=F), "B RAM: extent drift"),
    "S8": ("SMMU-PERM", exp(m1=F), "SID 8: DMA access drift"),
    "S9": ("SMMU-TAG", exp(m1=F, m6=P), "v3 blind spot: the shared VMID is not seen"),
    "I7": ("GIC-PRIO", exp(), "SPI 33: interrupt priority drift (E3)"),
    "I8": ("GIC-GROUP", exp(m4=F), "GIC distributor drift"),
    "P1": ("S2-EXTENT", exp(m6=P), "v3 blind spot (E3): the BAR moves under its mapping"),
    "P2": ("S2-MISSING", exp(m1=F), "A PCI BAR: missing binding"),
    "C6": ("CONTROL", exp(m6=P), "control"),
    "C7": ("CONTROL", exp(m6=P), "control"),
}


def check():
    bad = 0
    for cid, (_, edits, _, _) in CASES.items():
        for path, old, _ in edits:
            n = open(path).read().count(old)
            if n != 1:
                print(f"{cid}: {path}: matches {n} times")
                bad += 1
    for name in V3:
        if not os.path.exists(f"tools/campaign/cases/v3-{name}.patch"):
            print(f"v3 {name}: no patch")
            bad += 1
    return bad


def main():
    if check():
        return 1
    if sys.argv[1:] == ["--check"]:
        print(f"{len(CASES)} written edits match once; {len(V3)} v3 patches present")
        return 0
    os.makedirs("tools/campaign/cases/c2", exist_ok=True)
    cases, expect = [], {}
    for cid, (cls, edits, e, why) in CASES.items():
        files = sorted({p for p, _, _ in edits})
        for path, old, new in edits:
            s = open(path).read()
            open(path, "w").write(s.replace(old, new))
        diff = subprocess.run(["git", "diff", "--", *files], capture_output=True, text=True, check=True).stdout
        subprocess.run(["git", "checkout", "-q", "--", *files], check=True)
        patch = f"tools/campaign/cases/c2/{cid}.patch"
        open(patch, "w").write(diff)
        cases.append({"case": f"C2-{cid}", "class": cls, "source": "written", "patch": patch, "m6": why})
        expect[f"C2-{cid}"] = e
    for name, (cls, e, why) in V3.items():
        cases.append({"case": f"C2-V3-{name}", "class": cls, "source": "written",
                      "patch": f"tools/campaign/cases/v3-{name}.patch", "m6": why})
        expect[f"C2-V3-{name}"] = e
    cases.append({"case": "C2-BASELINE", "class": "CONTROL", "source": "written", "patch": None,
                  "m6": "the unchanged tree"})
    expect["C2-BASELINE"] = exp(m6=P)
    out = {"schema": "haven-campaign-cases/1",
           "source": "campaign step C2, written cases (docs/methodology/stage2/20261119-evaluation-campaign/c2/)",
           "grant": GRANT, "cases": cases, "expect": expect}
    with open("tools/campaign/cases/c2-written.json", "w") as f:
        json.dump(out, f, indent=1)
        f.write("\n")
    print(f"{len(cases)} cases written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
