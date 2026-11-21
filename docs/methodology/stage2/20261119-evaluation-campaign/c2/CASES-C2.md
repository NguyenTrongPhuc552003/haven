# Campaign step C2: the written cases (for the owner's approval)

**Status:** owner-approved 2026-11-21 ("Approve to all of your suggested
actions order": every row as proposed; P1 stays under S2-EXTENT; the
balance of one table-level and one lower-layer case per class kept);
frozen by `freeze/SHA256SUMS-C2` (tools, read-back, workflow, grant,
taxonomy, cases file, patches, this file) before the run. Proposed
2026-11-19. Nothing ran before the freeze. The owner approves (or
corrects) each row's expected verdicts before the run; then the taxonomy,
tools, grant, this list and the patches are frozen by SHA-256 (C2's last
step), and only then does `campaign.yml` run them.

**What a row says.** One fault, written as exact edits by
`make_cases.py` (each edit must match exactly once at HEAD), or one of
case set v3's patches. The columns are the expected verdict of each method
on both CPU models: **F** a finding, pass none. M3 (the grant's
structural check) is expected to pass everywhere, since no case changes
the grant; M5 and M7 do not apply (no Linux partition; EL3 read-back not
wired). The last column is the read-back finding expected, or why a miss
is expected.

**How the expectations were made** [P]: from reading the code, not by
running anything. M1 is a finding where a unit test covers the edited
table (`test_partition_layout` for `partition.c`'s tables,
`test_stage2_attrs` for `s2_attrs.h`) or where the C1 runs showed today's
tests catch a v3 case. M2 is a finding where partition A's probe of
`0x60000000` (B's IPA) stops faulting. M4 is a finding where partition B
loses its memory, its CPU or its timer. Two misses of read-back are
predicted (W21, W34) besides v3's S9 and P1: read-back compares no DMA
VMID and no budget value. A wrong prediction is a disagreement in the
summary, kept and explained (E5), not edited away.

**Coverage.** 40 cases: 28 new, v3's eleven, and the
unchanged tree; every class of `tools/campaign/taxonomy.json` has at
least one case.
Controls: W35, W36 (equivalent rewrites), v3's C6 and C7, and the
baseline.

| Case | Class | Files | M1 | M2 | M4 | M6 | Expected read-back finding, or why not |
| - | - | - | - | - | - | - | - |
| C2-W01 | S2-EXTENT | `arch/arm64/mm.c` | pass | **F** | pass | **F** | extent drift on every region (one page more); A's probe of 0x60000000 no longer faults |
| C2-W02 | S2-EXTENT | `src/core/partition.c` | **F** | pass | pass | **F** | A UART: extent drift |
| C2-W03 | S2-PERM | `src/core/partition.c` | **F** | pass | pass | **F** | A RO: access drift |
| C2-W05 | S2-PERM | `arch/arm64/mm.c` | pass | pass | pass | **F** | A RO: execute drift (2 MiB blocks only) |
| C2-W06 | S2-TYPE | `arch/arm64/mm.c` | pass | pass | pass | **F** | A RAM, B RAM, A RO: shareability drift (2 MiB blocks only) |
| C2-W07 | S2-TYPE | `src/core/partition.c` | **F** | pass | pass | **F** | A UART: memory-type and shareability drift |
| C2-W08 | S2-OWNER | `src/core/partition.c` | **F** | **F** | **F** | **F** | B RAM: association drift (mapped for A); B cannot run |
| C2-W11 | S2-MISSING | `src/core/partition.c` | **F** | pass | pass | **F** | A RO: missing binding |
| C2-W12 | S2-EXTRA | `src/core/partition.c` | **F** | pass | pass | **F** | extra binding: the shared memory in A at 0x71000000 |
| C2-W14 | SMMU-UNBOUND | `src/core/dma/smmu.c` | pass | pass | pass | **F** | SID 8: DMA enforcement drift (the stream stays aborting while HAVEN reports the grant) |
| C2-W16 | SMMU-EXTENT | `src/core/partition.c` | **F** | pass | pass | **F** | SID 8: DMA extent drift (2 MiB window) |
| C2-W17 | SMMU-EXTENT | `arch/arm64/mm.c` | pass | pass | pass | **F** | SID 8: DMA extent drift (one page more) |
| C2-W19 | SMMU-PERM | `arch/arm64/mm.c` | pass | pass | pass | **F** | SID 8: DMA access drift (read-only) |
| C2-W21 | SMMU-TAG | `drivers/iommu/smmu_v3.c` | pass | pass | pass | pass | predicted miss: read-back does not compare a DMA VMID with the grant (v3's S9) |
| C2-W22 | GIC-ROUTE | `src/core/irq/ownership.c` | pass | pass | pass | **F** | SPI 33: interrupt routing drift (CPU 1) |
| C2-W23 | GIC-ROUTE | `drivers/irqchip/gic_v3.c` | pass | pass | pass | **F** | SPI 33: interrupt routing drift (1-of-N) |
| C2-W24 | GIC-ENABLE | `src/core/irq/ownership.c` | pass | pass | pass | **F** | SPI 33: interrupt enable drift |
| C2-W27 | GIC-GROUP | `drivers/irqchip/gic_v3.c` | pass | pass | pass | **F** | SPI 33: interrupt group drift |
| C2-W29 | GIC-PRIO | `arch/arm64/budget_hw.c` | pass | pass | pass | **F** | CPU 0 and CPU 2 PPI 26: interrupt priority drift |
| C2-W30 | GIC-PPI | `src/core/partition.c` | **F** | pass | **F** | **F** | CPU 2 PPI 27: missing interrupt binding (enabled on CPU 1) |
| C2-W31 | GIC-PPI | `src/core/irq/ownership.c` | pass | pass | **F** | **F** | CPU 2 PPI 27: missing interrupt binding (enabled on CPU 0) |
| C2-W09 | CPU-ASSIGN | `src/core/partition.c` | pass | pass | **F** | **F** | CPU 2: CPU assignment drift (A's tables); B faults at its entry |
| C2-W32 | CPU-ASSIGN | `src/core/partition.c` | pass | pass | **F** | **F** | CPU 2: CPU assignment drift; B's timer is no longer injected |
| C2-W33 | EL2-MAP | `src/platform/qemu-virt/platform.c` | pass | pass | pass | **F** | partition memory mapped at EL2 (A RAM) |
| C2-W25 | EL2-TIMER | `arch/arm64/budget_hw.c` | pass | pass | pass | **F** | CPU 0 and CPU 2 PPI 26: missing interrupt binding |
| C2-W34 | EL2-TIMER | `arch/arm64/budget_hw.c` | pass | pass | pass | pass | predicted miss: read-back reads no budget value (A's 8 ms becomes 32 ms, B's 2 ms 8 ms) |
| C2-W35 | CONTROL | `drivers/irqchip/gic_v3.c` | pass | pass | pass | pass | control: the same bit for an unsigned irq |
| C2-W36 | CONTROL | `arch/arm64/include/asm/s2_attrs.h` | pass | pass | pass | pass | control: the same bits through an if |
| C2-V3-M3 | S2-PERM | `arch/arm64/include/asm/s2_attrs.h` | **F** | **F** | **F** | **F** | A RAM, B RAM: execute drift |
| C2-V3-M4 | S2-TYPE | `arch/arm64/include/asm/s2_attrs.h` | **F** | pass | pass | **F** | A UART, PCI config, PCI BAR: memory-type and shareability drift |
| C2-V3-M5 | S2-EXTENT | `src/platform/qemu-virt/memory.h` | **F** | pass | pass | **F** | B RAM: extent drift |
| C2-V3-S8 | SMMU-PERM | `src/core/partition.c` | **F** | pass | pass | **F** | SID 8: DMA access drift |
| C2-V3-S9 | SMMU-TAG | `src/core/dma/smmu.c` | **F** | pass | pass | pass | v3 blind spot: the shared VMID is not seen |
| C2-V3-I7 | GIC-PRIO | `drivers/irqchip/gic_v3.c` | pass | pass | pass | **F** | SPI 33: interrupt priority drift (E3) |
| C2-V3-I8 | GIC-GROUP | `drivers/irqchip/gic_v3.c` | pass | pass | **F** | **F** | GIC distributor drift |
| C2-V3-P1 | S2-EXTENT | `src/core/partition.c` | pass | pass | pass | pass | v3 blind spot (E3): the BAR moves under its mapping |
| C2-V3-P2 | S2-MISSING | `src/core/partition.c` | **F** | pass | pass | **F** | A PCI BAR: missing binding |
| C2-V3-C6 | CONTROL | `drivers/iommu/smmu_v3.c` | pass | pass | pass | pass | control |
| C2-V3-C7 | CONTROL | `arch/arm64/include/asm/s2_attrs.h` | pass | pass | pass | pass | control |
| C2-BASELINE | CONTROL | `` | pass | pass | pass | pass | the unchanged tree |

## For the owner [?]

1. Is each row's expectation right? Mark any row you disagree with; it is
   corrected before the freeze, with your reason.
2. Taxonomy gap: v3's P1 (a BAR placed 16 KiB away from its mapping) fits
   no class well; it is filed under S2-EXTENT. Should device setup (BAR
   placement, PCI enable) become a class of its own before the freeze?
3. Classes with only W cases at the lower layers (`mm.c`, `gic_v3.c`,
   `budget_hw.c`) have no unit test; those rows are where the hypothesis
   (E2) is tested. Is that the balance you want, or should some classes
   get a second, table-level case?
