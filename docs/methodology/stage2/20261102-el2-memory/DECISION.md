# EL2 memory contract before the board (FINDINGS A1)

**Status:** PROPOSAL, 2026-11-02, waiting for the owner's choice of option.
Analysis by Claude; no code changed yet.

## Correction first

Claude said earlier that the CPU 2 hand-off "only works because QEMU has no
caches". That was wrong for the hand-off itself. With EL2 stage-1
translation off, every data access Haven makes is Device-nGnRnE, which no
cache holds, so the mailbox and the B3 ready handshake are coherent between
CPUs by architecture. The real hazards are elsewhere, below.

## What Haven does today (source-derived)

| Item | Setting | Where |
| - | - | - |
| Haven's own data accesses | Stage-1 off (SCTLR_EL2.M = 0): Device-nGnRnE. SCTLR_EL2.C = 1 has no effect on data while M = 0 | `arch/arm64/cpu.c` |
| CPU stage-2 table walks | Normal, inner and outer write-back, inner shareable (VTCR_EL2 IRGN0 = ORGN0 = 1, SH0 = 3) | `asm/sysregs.h` `VTCR_HAVEN_DEFAULT` |
| SMMU table, queue and stream-table accesses | Write-back, inner shareable (SMMU_CR1) | `drivers/iommu/smmu_v3.c` |
| Cache maintenance after table writes | None anywhere | (no `dc` instruction in the tree) |
| Alignment | Haven is built without `-mstrict-align`; Device memory faults on any unaligned access | `cmake/flags.cmake` |

## Hazards on a real board (inference from the Arm architecture; QEMU models no caches, so none of this can be observed in QEMU)

1. **Mismatched attributes.** Haven writes stage-2 tables, stream table
   entries and SMMU queues as Device (non-cacheable), while the CPU table
   walker and the SMMU read them as cacheable write-back. A walker or the
   SMMU can keep a stale cached copy of a line Haven has just changed: a
   revoked mapping could stay usable. This is a direct threat to the
   isolation claim, and read-back in QEMU cannot see it.
2. **Alignment faults.** Any unaligned access the compiler generates at EL2
   faults on Device memory. This was observed in QEMU in Stage 0 (A1
   evidence, ESR 0x96000021 and 0x96000061); the current build avoids it
   by layout luck.
3. **Speed.** Every Haven data access bypasses the caches. That would make
   the interrupt-latency and fault-path numbers in the evaluation chapter
   unrepresentative of a real hypervisor.

## Options

| | A. EL2 stage-1 on, identity map (recommended) | B. Stay off, make everything non-cacheable | C. Stay off, add cache maintenance |
| - | - | - | - |
| Change | EL2 page tables: Haven image and its tables Normal write-back inner shareable, MMIO Device-nGnRE; MAIR, TCR, TTBR0 set on each CPU before any table is written | VTCR and SMMU_CR1 to non-cacheable; build Haven with `-mstrict-align` | `dc civac` after every table or queue write |
| Fixes 1, 2, 3 | 1 yes (all agents use the same cacheable, coherent view), 2 yes, 3 yes | 1 yes, 2 yes, 3 no (also makes every guest TLB miss walk uncached memory) | 1 partly (still mismatched attributes), 2 no, 3 no |
| Trusted code | About 150 to 250 lines more (Xen, Bao, Jailhouse and KVM all run EL2 with stage-1 on) | About 10 lines | Many call sites, easy to miss one |
| Risk | The enable sequence must be exact (invalidate, then enable, on every CPU); read-back must also check Haven's own EL2 map | Low code risk, but the timing chapter loses meaning | High: one missed maintenance site is a silent hole |

**Recommendation: A.** It is what every production static partitioner does,
it removes the attribute mismatch rather than patching around it, and the
evaluation chapter needs cached EL2 code. The FINDINGS A1 caution ("do not
add EL2 page tables merely because one workaround booted") is met: the
reason here is the attribute mismatch, not a boot workaround.

## If A is chosen: plan and checks

1. Static EL2 tables in Haven's image (no allocation): identity map of
   Haven's own image and page-table pool as Normal WB, UART, GIC, SMMU and
   PCI ECAM as Device-nGnRE, nothing else (partition memory is not mapped
   at EL2 unless Haven needs it).
2. Enable on the boot CPU in `boot.S` before C code runs, and on each
   secondary in `secondary_entry` before its first stack use, with the
   architected sequence (TLB invalidate, MAIR, TCR, TTBR0, ISB, SCTLR.M|C|I).
3. Checks: all current QEMU checks unchanged (both boot checks, partition
   B, read-back v4 baseline); read-back extended to walk Haven's EL2 table
   and report any partition memory mapped at EL2 (a new projection, owner
   approval needed for its grant); a control that maps one partition page
   at EL2 must be reported.
4. Not provable in QEMU: hazard 1. On the board, a revocation test (unmap a
   DMA window, then DMA into it) is the evidence; recorded as a board
   experiment.
