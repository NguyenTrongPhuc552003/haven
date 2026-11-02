# EL2 stage-1 identity map: results (FINDINGS A1, option A)

**Status:** EXPERIMENT, observed 2026-11-02. Option A chosen by the owner.
Controls and expectations by Claude, committed with the runner in
`d0d5002` before the run; not blind. Raw output in `runs/`, large dumps
listed in `runs/DUMP_SHA256SUMS`.

## Changes

| Commit | Change |
| - | - |
| `443a7de` | `arch/arm64/mmu_el2.c`, `asm/el2_mmu.h`: static level-1 and level-2 tables built from the platform's EL2 region list; MAIR_EL2 0xff04, TCR_EL2 39-bit VA, 4 KiB granule, WB inner shareable walks. Boot CPU: build, invalidate stale lines over the image (`dc ivac`), enable. Secondary: enable first thing in `secondary_entry`, no invalidation. QEMU virt list: 1 GiB Device at 0, 1 GiB Device at 0x40_0000_0000 (ECAM), Haven's 8 MiB image Normal. Legacy i.MX: empty list, translation stays off |
| `d0d5002` | Read-back v5: walks the EL2 table through `hv_el2_ttbr`; partition memory mapped Normal by stage-2 must not appear in it; mappings must be identity |

## Results (`run_a1.py`, both CPUs identical, 10 of 10 as expected)

| Check | Change | Boot and partitions | Read-back v5 |
| - | - | - | - |
| N1 | none | "EL2 stage-1 on"; partition B ready and done; partition A done | matches; EL2 map: 3 ranges as listed |
| N2 | partition B's first 2 MiB also mapped at EL2 | everything still works | **partition memory mapped at EL2** (partition 2) |
| N3 | CPU 2 loads a wrong TTBR0_EL2 | "CPU 2 not ready within 100 ms"; B never runs; A unaffected | matches |
| N4 | Normal region covers only half of Haven's image | "EL2 map rejected (rc=-4)", Haven panics | not run |
| N5 | no EL2 map | "EL2 stage-1 off"; both partitions work as before | matches; EL2 translation off reported as an observation |

Also with the baseline image: both `qemu-direct` boot checks pass, read-back
of DMA unchanged (inside window OK, outside blocked), host tests 21 of 21,
legacy i.MX95 and i.MX8QM images link.

## Observations

- **N2 is a fault only read-back sees.** Mapping partition RAM into
  Haven's own address space breaks nothing functionally, so the boot check
  and the partition checks pass; read-back reports it. Unit tests do not
  cover the EL2 map at all.
- **N3 shows the secondary path really runs translated:** a wrong table
  base on CPU 2 alone stops CPU 2 before it reports ready.
- **Device sharing at EL2 (observation, not a finding):** Haven's 1 GiB
  device mapping also covers partition A's UART, its PCI BAR and its
  config page. Haven prints on the UART; it never touches the BAR. Finer
  device mappings (only the GIC, SMMU, UART and ECAM pages Haven needs)
  would narrow this, at the cost of a level-3 table.

## Limits

- Not observable in QEMU: the attribute-mismatch hazard this change
  removes, cache invalidation correctness, firmware dirty lines over the
  image (`dc ivac` may be performed as clean and invalidate). Board
  experiment: unmap a DMA window, then DMA into it; repeat after stage-2
  revocation.
- Haven's image is mapped read-write and executable as one 8 MiB range
  (no W^X yet, as before).
- MAIR_EL2 is assumed by read-back, not read (QEMU's monitor does not show
  it).
