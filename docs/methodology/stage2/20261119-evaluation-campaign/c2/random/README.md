# Campaign step C2, the random cases: design and expectations

**Status:** design and expectations, 2026-11-21, written before the
draw. The random cases are part of step C2 as the owner approved it on
2026-11-19 (about 60; `../../README.md`, design item 2); C2 ran only the
written cases, a gap found on 2026-11-21 (`../../c3/README.md`).

## What is drawn [P]

- **Generator.** `tools/campaign/mutate.py` as merged (row K1 holds): all
  single-point mutants of its default files (the 11 enforcement files of
  `taxonomy.json`), five operators, shuffled by the seed, each kept only
  if the host tests and the qemu-direct image build; skipped ones are
  counted in `draw.json`.
- **Count.** 60 kept mutants.
- **Seed text, fixed by a rule Claude cannot tune:**
  `haven <H> | campaign C2 random, 60 cases`, where `<H>` is the full
  hash of the merge commit of the pull request that adds this record.
  Nobody knows `<H>` before the merge, so the draw cannot be chosen by
  trying seeds. The draw runs at `<H>`.
- **Where.** A new on-demand workflow, `campaign-draw.yml` (Debian
  trixie container, as `campaign.yml`), runs `mutate.py REPO draw SEED
  60` and uploads `draw/` (patches and `draw.json`). Nothing on the Mac.
- **After the draw, before any run:** Claude names each mutant's class
  from its diff (`taxonomy.json`; `CONTROL` is never used for a random
  case; a mutant outside every class is named `OUTSIDE` and counted
  apart), writes `tools/campaign/cases/c2-random.json` (source `random`,
  no expectations: the cases are blind), and freezes the patches, the
  cases file and the tools as `c2/freeze/SHA256SUMS-C2-random`.
- **The run.** `campaign.yml` on `c2-random.json`, eight shards.

## Expectations

| Row | Check | Expected |
| - | - | - |
| D1 | the draw | `draw.json` names the seed text with `<H>`, 60 kept mutants and the skipped ones; the mutant total is near 1,062 (v3's count, K1) if the 11 files changed little since `e47b722` |
| D2 | the draw again with the same seed (same workflow) | the same 60 mutants, the same order (K2's claim, at the scale of the campaign) |
| D3 | classes, before the run | every mutant has a class or `OUTSIDE`; the table of classes is in this record before the run |
| D4 | the run | every case gives a verdict per method; a method's `error` is counted apart from pass and finding |
| D5 | candidates for equivalence: random cases where every method passes | listed; each checked by reading its diff (all if 10 or fewer, else 10 drawn with the same seed), recorded as equivalent (counted apart, design item 2) or a miss (with a named cause, E5) |

No per-case verdict is predicted (blind cases). What the run measures is
the per-class detection rate of each method over mutants Claude did not
write. v3's four random cases (`../../../20261102-a1-cases-v3/RESULTS.md`)
[F]: R4 found by read-back and the boot check, R1 by a unit test alone,
R2 by no method (a priority: the I7 blind spot, closed since by v7), and
R3 crashed read-back. Four cases are too few to predict a rate [I].

## Threats to validity [I]

- Claude names the classes and judges equivalence; Claude also wrote
  read-back. The diffs and the verdicts are committed, so the owner or a
  reviewer can re-check the classes and the equivalence calls; D5's
  sample is drawn, not chosen.
- Single-point mutants are not the faults a developer makes (they are
  small and syntactic); the written and held-out sets cover the other
  kind. The three sets are reported apart (design item 5).

## The draw (2026-11-21)

`campaign-draw.yml` at `f1da2b1fcfafcd7689757f28ae6348cb51e7b5e9` (the
merge of #177), twice, in parallel [F]:

| Row | Result |
| - | - |
| D1 | seed text `haven f1da2b1fcfafcd7689757f28ae6348cb51e7b5e9 \| campaign C2 random, 60 cases`; 1,375 mutants, 60 kept, 9 skipped (did not build); `draw.json` here. Not near 1,062: the 11 files grew by 912 lines since `e47b722` (`git diff --stat`), so the row's condition did not hold; the count is explained, not a defect |
| D2 | the second draw's `draw.json` and all 60 patches are byte-identical to the first; holds |

The patches are `tools/campaign/cases/c2r/R1.patch` to `R60.patch`.

## Classes, named before the run (row D3)

Each class names the enforcement the changed line serves, read from the
diff and its function at `f1da2b1` (Claude's reading; see the threats
above). "Error path", "teardown path" and "park path" mark code a clean
boot may never run, so those cases are candidates for equivalence. A
BAR placement fault is S2-EXTENT, as P1 is (owner, 2026-11-21).

| Case | Where | Mutation | Class | What it touches |
| - | - | - | - | - |
| R1 | `src/core/mm/stage2.c:226` | `comparison != -> ==` | S2-MISSING | map loop: success taken as failure |
| R2 | `src/core/partition.c:159` | `bitwise \| -> &` | S2-PERM | A's DMA config page attributes, Device and read-only |
| R3 | `drivers/irqchip/gic_v3.c:62` | `delete call: gicd_wait_rwp(gic.gicd_base);` | GIC-GROUP | no wait after the distributor is disabled for setup |
| R4 | `src/core/partition.c:544` | `comparison == -> !=` | CPU-ASSIGN | which CPUs are partition A's |
| R5 | `drivers/iommu/smmu_v3.c:196` | `comparison < -> <=` | SMMU-UNBOUND | command-queue sync loop one past |
| R6 | `arch/arm64/irq.c:558` | `comparison < -> <=` | GIC-ENABLE | pending virtual interrupts loop one past |
| R7 | `src/core/partition.c:551` | `constant 0 -> 1` | CPU-ASSIGN | vCPU lookup for A's first CPU |
| R8 | `src/core/mm/stage2.c:336` | `delete call: hv_arch_stage2_flush_tlb();` | S2-EXTRA | teardown: no TLB flush after unmap (teardown path) |
| R9 | `src/core/mm/stage2.c:197` | `comparison != -> ==` | S2-MISSING | map: return code check inverted |
| R10 | `drivers/iommu/smmu_v3.c:302` | `delete call: ste_write_abort(i);` | SMMU-TAG | streams not set to abort at init |
| R11 | `src/core/irq/ownership.c:81` | `constant 0U -> 1U` | GIC-ROUTE | interrupt ownership: unowned test |
| R12 | `drivers/irqchip/gic_v3.c:204` | `comparison < -> <=` | GIC-ENABLE | enable: SPI 32 refused |
| R13 | `src/core/irq/ownership.c:101` | `comparison != -> ==` | GIC-ROUTE | interrupt ownership: conflict test inverted |
| R14 | `src/core/partition.c:423` | `delete call: hv_arch_set_cpu_partition(hv_arch_cpu_id(), 0U);` | CPU-ASSIGN | a parked CPU keeps its partition (park path) |
| R15 | `src/core/dma/smmu.c:110` | `constant 0 -> 1` | SMMU-UNBOUND | stream ID allocation skips slot 0 |
| R16 | `src/core/mm/stage2.c:14` | `constant 0x1000ULL -> 0x2000ULL` | S2-EXTENT | stage-2 page size 8 KiB |
| R17 | `drivers/iommu/smmu_v3.c:436` | `delete call: hv_printk("HAVEN: SMMU events so far: %u\n", (unsigned)n);` | OUTSIDE | an SMMU event log line deleted |
| R18 | `arch/arm64/irq.c:406` | `comparison >= -> >` | CPU-ASSIGN | stop-CPU bound check (error path) |
| R19 | `drivers/irqchip/gic_v3.c:72` | `constant 1 -> 2` | GIC-GROUP | SPIs 32 to 63 left out of Group 1 |
| R20 | `arch/arm64/irq.c:414` | `constant 0 -> 1` | CPU-ASSIGN | stop-CPU return value |
| R21 | `drivers/iommu/smmu_v3.c:53` | `constant 1U -> 2U` | SMMU-UNBOUND | command queue size doubled |
| R22 | `src/core/dma/smmu.c:272` | `constant 1 -> 2` | SMMU-UNBOUND | DMA window's configured flag value |
| R23 | `drivers/iommu/smmu_v3.c:95` | `constant 1 -> 2` | SMMU-TAG | abort STE: words after the first not cleared |
| R24 | `src/core/mm/stage2.c:228` | `constant 0U -> 1U` | S2-EXTRA | rollback skips region 0 (error path) |
| R25 | `src/platform/qemu-virt/memory.h:35` | `constant 0x20000000UL -> 0x20001000UL` | S2-EXTENT | partition A's RAM 4 KiB larger |
| R26 | `src/core/partition.c:855` | `constant 1000000U -> 1000001U` | OUTSIDE | a budget print's divisor |
| R27 | `src/platform/qemu-virt/memory.h:28` | `constant 0x80000000UL -> 0x80001000UL` | EL2-MAP | HAVEN's load address 4 KiB higher |
| R28 | `arch/arm64/mm.c:346` | `delete call: dsb_ish();` | S2-EXTRA | no barrier before the stage-2 TLB flush |
| R29 | `src/core/mm/stage2.c:329` | `comparison < -> <=` | S2-EXTRA | teardown: unmap loop one past (teardown path) |
| R30 | `src/core/dma/smmu.c:320` | `comparison >= -> >` | SMMU-UNBOUND | device lookup bound check |
| R31 | `drivers/pci/ecam.c:70` | `constant 4U -> 5U` | S2-EXTENT | BAR placement: upper half read 1 byte off |
| R32 | `arch/arm64/mm.c:284` | `comparison == -> !=` | SMMU-UNBOUND | DMA root allocation test inverted |
| R33 | `arch/arm64/irq.c:374` | `constant 44 -> 45` | GIC-ROUTE | IPI target affinity field shifted |
| R34 | `drivers/irqchip/gic_v3.c:216` | `constant 32 -> 33` | GIC-ENABLE | disable: bit index modulo 33 |
| R35 | `src/core/dma/smmu.c:257` | `delete call: dma_vmid_put(vmid);` | SMMU-TAG | VMID not released on error (error path) |
| R36 | `src/core/partition.c:875` | `constant 2 -> 3` | OUTSIDE | a message names CPU 3 |
| R37 | `src/core/irq/ownership.c:117` | `comparison == -> !=` | GIC-ROUTE | interrupt target: invalid-CPU test inverted |
| R38 | `arch/arm64/irq.c:497` | `constant 16U -> 17U` | GIC-PPI | physical interrupt: INTID 16 not taken as a PPI |
| R39 | `src/core/partition.c:716` | `comparison != -> ==` | GIC-ENABLE | virtual GIC setup: result test inverted |
| R40 | `arch/arm64/irq.c:431` | `constant 0U -> 1U` | GIC-ROUTE | cross-CPU requests: count starts at 1 |
| R41 | `src/core/irq/ownership.c:41` | `constant 0U -> 1U` | GIC-ROUTE | interrupt owner map starts owned |
| R42 | `src/core/partition.c:862` | `delete call: hv_arch_stage2_enable(PARTITION_A_ID, PARTITION_A_ID);` | CPU-ASSIGN | partition A's stage 2 never enabled |
| R43 | `drivers/pci/ecam.c:60` | `logic && -> \|\|` | S2-EXTENT | BAR placement: 64-bit test |
| R44 | `drivers/iommu/smmu_v3.c:385` | `bitwise & -> \|` | OUTSIDE | SMMU event interrupt: acknowledgement test |
| R45 | `drivers/iommu/smmu_v3.c:490` | `comparison >= -> >` | SMMU-TAG | set-abort bound check |
| R46 | `arch/arm64/mm.c:275` | `comparison >= -> >` | SMMU-UNBOUND | DMA map: stream bound check |
| R47 | `drivers/iommu/smmu_v3.c:348` | `constant 0 -> 1` | OUTSIDE | SMMU event queue producer starts at 1 |
| R48 | `arch/arm64/irq.c:375` | `constant 8 -> 9` | GIC-ROUTE | IPI target affinity field |
| R49 | `drivers/irqchip/gic_v3.c:125` | `constant 0 -> 1` | GIC-PRIO | redistributor priority register 0 not written |
| R50 | `drivers/pci/ecam.c:73` | `comparison > -> >=` | S2-EXTENT | BAR placement: size bound |
| R51 | `drivers/irqchip/gic_v3.c:76` | `comparison < -> <=` | GIC-ENABLE | distributor: disable loop one past |
| R52 | `src/platform/qemu-virt/memory.h:15` | `constant 0x08000000UL -> 0x8001000UL` | EL2-MAP | GIC distributor base address |
| R53 | `src/core/partition.c:650` | `constant 0 -> 1` | CPU-ASSIGN | vCPU off: index test |
| R54 | `arch/arm64/mm.c:275` | `comparison == -> !=` | SMMU-UNBOUND | DMA map: size test inverted |
| R55 | `arch/arm64/irq.c:413` | `constant 1 -> 2` | CPU-ASSIGN | stop-CPU return value |
| R56 | `src/core/partition.c:727` | `constant 1U -> 2U` | GIC-ENABLE | virtual GIC setup skips vCPU 1 |
| R57 | `src/core/mm/stage2.c:58` | `comparison < -> <=` | S2-EXTENT | range containment test |
| R58 | `src/core/mm/stage2.c:94` | `constant 0U -> 1U` | S2-MISSING | stage-2 init skips partition 0 |
| R59 | `src/core/mm/stage2.c:228` | `comparison < -> <=` | S2-EXTRA | rollback one past (error path) |
| R60 | `src/core/partition.c:237` | `comparison != -> ==` | SMMU-UNBOUND | DMA grants: return code check inverted |

Per class: SMMU-UNBOUND 9, CPU-ASSIGN 8, GIC-ROUTE 7, GIC-ENABLE 6, S2-EXTENT 6, OUTSIDE 5, S2-EXTRA 5, SMMU-TAG 4, S2-MISSING 3, EL2-MAP 2, GIC-GROUP 2, GIC-PPI 1, GIC-PRIO 1, S2-PERM 1.

No random mutant fell in S2-TYPE, S2-OWNER, SMMU-EXTENT, SMMU-PERM or
EL2-TIMER; those classes rest on the written and held-out cases. D3
holds: every mutant has a class or OUTSIDE.

**Freeze.** `../freeze/SHA256SUMS-C2-random` covers the patches, the cases
file `tools/campaign/cases/c2-random.json`, `draw.json`, this record and
the tools (the files of `SHA256SUMS-V15` as they are now), before the run.
The design named it `SHA256SUMS-C2R`; renamed, since on a case-insensitive
file system (the Mac's) that name is the same file as `SHA256SUMS-C2r`.

## The run (2026-11-21, row D4)

`campaign.yml` on `c2-random.json`, eight shards, at `a32b66e` (the
merge of #179); `SHA256SUMS-C2-random` holds there (130 files) [F].
Every case gave a verdict per method; D4 holds. F is a finding, E an
error (the method could not judge), M5 and M7 n/a throughout:

| Case | Class | M1 | M2 | M3 | M4 | M6 |
| - | - | - | - | - | - | - |
| R1 | S2-MISSING | . | **F** | . | **F** | **F** |
| R2 | S2-PERM | . | . | . | . | **F** |
| R3 | GIC-GROUP | . | . | . | . | . |
| R4 | CPU-ASSIGN | . | . | . | . | . |
| R5 | SMMU-UNBOUND | . | **F** | . | **F** | E |
| R6 | GIC-ENABLE | . | . | . | . | . |
| R7 | CPU-ASSIGN | . | . | . | . | . |
| R8 | S2-EXTRA | . | . | . | . | . |
| R9 | S2-MISSING | **F** | **F** | . | **F** | E |
| R10 | SMMU-TAG | . | . | . | . | . |
| R11 | GIC-ROUTE | **F** | **F** | . | **F** | **F** |
| R12 | GIC-ENABLE | . | . | . | . | . |
| R13 | GIC-ROUTE | **F** | **F** | . | **F** | **F** |
| R14 | CPU-ASSIGN | . | . | . | . | . |
| R15 | SMMU-UNBOUND | **F** | . | . | . | . |
| R16 | S2-EXTENT | **F** | **F** | . | **F** | E |
| R17 | OUTSIDE | . | . | . | . | . |
| R18 | CPU-ASSIGN | . | . | . | . | . |
| R19 | GIC-GROUP | . | . | . | . | **F** |
| R20 | CPU-ASSIGN | . | . | . | . | . |
| R21 | SMMU-UNBOUND | . | . | . | . | . |
| R22 | SMMU-UNBOUND | . | . | . | . | . |
| R23 | SMMU-TAG | . | . | . | . | . |
| R24 | S2-EXTRA | . | . | . | . | . |
| R25 | S2-EXTENT | **F** | **F** | . | **F** | **F** |
| R26 | OUTSIDE | . | . | . | . | . |
| R27 | EL2-MAP | **F** | **F** | . | **F** | E |
| R28 | S2-EXTRA | . | . | . | . | . |
| R29 | S2-EXTRA | . | . | . | . | . |
| R30 | SMMU-UNBOUND | . | . | . | . | . |
| R31 | S2-EXTENT | . | . | . | . | **F** |
| R32 | SMMU-UNBOUND | . | . | . | . | **F** |
| R33 | GIC-ROUTE | . | . | . | . | . |
| R34 | GIC-ENABLE | . | . | . | . | . |
| R35 | SMMU-TAG | . | . | . | . | . |
| R36 | OUTSIDE | . | . | . | . | . |
| R37 | GIC-ROUTE | . | **F** | . | **F** | **F** |
| R38 | GIC-PPI | . | . | . | . | . |
| R39 | GIC-ENABLE | . | . | . | . | . |
| R40 | GIC-ROUTE | . | . | . | . | . |
| R41 | GIC-ROUTE | **F** | . | . | . | . |
| R42 | CPU-ASSIGN | . | **F** | . | **F** | **F** |
| R43 | S2-EXTENT | . | . | . | . | **F** |
| R44 | OUTSIDE | . | . | . | . | . |
| R45 | SMMU-TAG | . | . | . | . | . |
| R46 | SMMU-UNBOUND | . | . | . | . | . |
| R47 | OUTSIDE | . | . | . | . | . |
| R48 | GIC-ROUTE | . | . | . | . | . |
| R49 | GIC-PRIO | . | . | . | . | . |
| R50 | S2-EXTENT | . | . | . | . | **F** |
| R51 | GIC-ENABLE | . | . | . | . | . |
| R52 | EL2-MAP | . | . | . | . | . |
| R53 | CPU-ASSIGN | . | . | . | . | . |
| R54 | SMMU-UNBOUND | . | . | . | . | **F** |
| R55 | CPU-ASSIGN | . | . | . | . | . |
| R56 | GIC-ENABLE | . | . | . | . | . |
| R57 | S2-EXTENT | . | . | . | . | . |
| R58 | S2-MISSING | . | . | . | . | . |
| R59 | S2-EXTRA | . | . | . | . | . |
| R60 | SMMU-UNBOUND | . | . | . | . | **F** |

Per class (`score.py`; a method's errors are left out of its
denominator, so M6's column can be smaller):


| Class | M1 | M2 | M3 | M4 | M5 | M6 | M7 |
| - | - | - | - | - | - | - | - |
| CPU-ASSIGN | 0/8 [0.000, 0.369] | 1/8 [0.003, 0.527] | 0/8 [0.000, 0.369] | 1/8 [0.003, 0.527] | - | 1/8 [0.003, 0.527] | - |
| EL2-MAP | 1/2 [0.013, 0.987] | 1/2 [0.013, 0.987] | 0/2 [0.000, 0.842] | 1/2 [0.013, 0.987] | - | 0/1 [0.000, 0.975] | - |
| GIC-ENABLE | 0/6 [0.000, 0.459] | 0/6 [0.000, 0.459] | 0/6 [0.000, 0.459] | 0/6 [0.000, 0.459] | - | 0/6 [0.000, 0.459] | - |
| GIC-GROUP | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | - | 1/2 [0.013, 0.987] | - |
| GIC-PPI | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | - | 0/1 [0.000, 0.975] | - |
| GIC-PRIO | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | - | 0/1 [0.000, 0.975] | - |
| GIC-ROUTE | 3/7 [0.099, 0.816] | 3/7 [0.099, 0.816] | 0/7 [0.000, 0.410] | 3/7 [0.099, 0.816] | - | 3/7 [0.099, 0.816] | - |
| OUTSIDE | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | - | 0/5 [0.000, 0.522] | - |
| S2-EXTENT | 2/6 [0.043, 0.777] | 2/6 [0.043, 0.777] | 0/6 [0.000, 0.459] | 2/6 [0.043, 0.777] | - | 4/5 [0.284, 0.995] | - |
| S2-EXTRA | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | - | 0/5 [0.000, 0.522] | - |
| S2-MISSING | 1/3 [0.008, 0.906] | 2/3 [0.094, 0.992] | 0/3 [0.000, 0.708] | 2/3 [0.094, 0.992] | - | 1/2 [0.013, 0.987] | - |
| S2-PERM | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | 0/1 [0.000, 0.975] | - | 1/1 [0.025, 1.000] | - |
| SMMU-TAG | 0/4 [0.000, 0.602] | 0/4 [0.000, 0.602] | 0/4 [0.000, 0.602] | 0/4 [0.000, 0.602] | - | 0/4 [0.000, 0.602] | - |
| SMMU-UNBOUND | 1/9 [0.003, 0.482] | 1/9 [0.003, 0.482] | 0/9 [0.000, 0.336] | 1/9 [0.003, 0.482] | - | 3/8 [0.085, 0.755] | - |
| false findings on controls | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 | 0/0 |

Only read-back detects: C2-R19, C2-R2, C2-R31, C2-R32, C2-R43, C2-R50, C2-R54, C2-R60

What the run shows [F]:

- **40 of 60 cases pass every method.** 20 are found by at least one
  method: 14 by read-back (M6), 12 by M1 to M4 together.
- **Read-back alone finds 8**: R2, R19, R31, R32, R43, R50, R54, R60
  (the DMA window and BAR cases, and one interrupt group).
- **M1 alone finds 2**: R15 (stream ID allocation) and R41 (owner map);
  read-back passes both [F]; why is not examined yet [?].
- **M6 errors on 4** (R5, R9, R16, R27): HAVEN halts before stage-2
  setup (`partition 1 root 0x0`), as W08 did in the written set; M2 and
  M4 find all four. Not read-back misses, but cases where there is no
  enforced state to read.
- No method finds anything in GIC-ENABLE (0 of 6), GIC-PPI, GIC-PRIO,
  SMMU-TAG (0 of 4), S2-EXTRA (0 of 5) or OUTSIDE (0 of 5).
- M6's median run time 0.9 s (max 40.9 s, the halting cases' wait).

## Candidates for equivalence (row D5)

The 40 cases where every method passes are more than 10, so 10 were
drawn with the draw's seed text (`random.Random(sha256(seed)).sample`
over the 40 in case order): R14, R21, R28, R40, R44, R45, R46, R51, R53,
R59. Each judged by reading its diff and the code at `f1da2b1` [I, no
run checks these]:

| Case | Judgement | Reason |
| - | - | - |
| R14 | not exercised | the park path runs only when CPU 2 is not B's or reports late; in the run CPU 2 enters B |
| R21 | not exercised | HAVEN's index mask assumes 512 queue entries while the SMMU is told 256; it matters only after 256 commands, more than a boot issues |
| R28 | equivalent under QEMU | a missing barrier before the TLB flush; QEMU's TCG does not reorder those accesses; on the board it may not be equivalent |
| R40 | not exercised | the changed counter is used only when a vCPU posts a request to another CPU; the grant gives each partition one CPU |
| R44 | equivalent | the test can only stop the enable when the event queue is off; QEMU's SMMU has one, so the outcome is the same |
| R45 | not exercised | a bound check at stream ID = the stream count; no call passes that ID |
| R46 | not exercised | the same, for the DMA context map |
| R51 | equivalent | one more distributor disable register is written, covering interrupts already disabled at reset or above the count (ignored) |
| R53 | not exercised | the vCPU-off path (PSCI CPU_OFF) is not used in the run |
| R59 | not exercised | the rollback after a failed mapping; no mapping fails |

D5 holds with three kinds instead of two: **equivalent** (2: R44, R51;
counted apart), **equivalent under QEMU** (1: R28; kept apart, a board
question), and **not exercised** (7). The last are misses with a named
cause (E5): the campaign's single scenario (one boot, one CPU per
partition, no fault in mapping, no CPU_OFF) never runs their code. No
method could have seen them in this scenario, read-back included, so
they say something about the scenario, not about the methods.

From the sample, about 2 to 3 of the 40 all-pass cases would be
equivalent [I]; the 30 not judged stay in the denominators, so the rates
above are conservative for every method alike.

**Next [P].** (1) Why read-back passes R15 and R41 (M1 finds them):
read the two diffs against what read-back reads, then decide whether it
is a blind spot (E5). (2) A second scenario for the campaign (two vCPUs
in A, a CPU_OFF, a failed mapping) would exercise the "not exercised"
code; it changes the campaign's design, so it is a proposal for the
owner, not done here.

## R15 and R41: why read-back passes them (2026-11-22)

Both are "not exercised" in the sense of D5, not read-back blind spots:

- **R15** (`hv_smmu_allocate_streamid` skips slot 0): the function has
  no caller in `src/`, `drivers/` or `arch/`; only the unit tests
  `test_smmu_dma` and `test_smmu_hardware` call it [F, `git grep`]. The
  boot binds streams by their hardware IDs (`hv_smmu_assign_streamid`
  in `apply_dma_grants`), so no enforced state can differ.
- **R41** (`irq_owner_map` starts as "owned by 1" instead of "free"):
  every SPI the grant assigns is partition 1's on CPU 0
  (`part_a_irqs`; partition B's only interrupt is a PPI, kept apart), so
  the boot's assignments pass the ownership checks either way and the GIC
  ends up as granted [F, `partition.c`]. The fault would show only when a
  second partition claims an SPI, which this grant never does [I]. The
  unit tests do, and fail.

So M1 finds both because the unit tests run code or inputs the boot
scenario lacks. They join the "not exercised" kind.

**Revised 2026-11-28 for R41** (`../../20261128-readback-v19/`): what
the GIC holds is as granted, as said above, but HAVEN's own ownership
table is not: it gives every SPI to partition 1. Read-back v17 did not
read that table, so for R41 "not a read-back blind spot" does not hold;
read-back v19 reads it and finds R41 (M6 and M7). R15 stands as written.
