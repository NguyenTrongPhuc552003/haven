# Campaign scenario 3: a second grant that reaches the "not exercised" code (design)

**Status:** design and expectations, 2026-11-22, before any code.
Owner-approved ("Approve", 2026-11-22: a second grant for the
not-exercised causes).

## Problem [F]

Four cases pass every method because the one grant of scenarios 1 and 2
never gives their code an input (`../RESULTS.md`, cause 3):

| Case | What the fault needs to show |
| - | - |
| H2 (historical) | a DMA stream whose VMID has non-zero low four bits: the first DMA VMID is 0xC0 |
| R41 (random) | a second partition claiming an SPI: every SPI the grant assigns is partition 1's |
| R45, R46 (random) | a stream ID at the edge of the table: none is used |

## Design [P]

1. **Grant 2**, a build option of the qemu-virt platform
   (`HAVEN_CAMPAIGN_GRANT2=ON`, off by default so every existing image is
   byte-identical):
   - a **second DMA device** for partition A (a second
     `virtio-rng-pci` at the next PCI slot, its own stream ID and DMA
     window), so the DMA VMIDs are 0xC0 and **0xC1** (H2);
   - partition B owns **one SPI** (a QEMU device interrupt no partition
     uses today, routed to CPU 2), so two partitions claim SPIs (R41);
   - the second stream ID is the **last** one HAVEN's DMA table holds
     (`HV_ARCH_MAX_DMA_STREAMS - 1`) (R45, R46).
2. **Scenario 3** = scenario 1 (bare test guests, both CPU models, QEMU
   10.0, M1 to M4, M6, M7) on the grant-2 build, with a grant file
   `s3/grant-2.json` that lists the second stream and B's SPI.
3. **Cases**: the unchanged tree three times (controls), the four cases
   above, and every other written and historical case (their verdicts
   under grant 2 against scenario 1's, as a check that the second grant
   changes nothing else).

## Expectations

| Row | Check | Expected |
| - | - | - |
| G1 | the default build | byte-identical to before (`haven.bin` hash unchanged with the option off) |
| G2 | the grant-2 build, unchanged tree, three times | every method passes; read-back sees two translated streams and B's SPI routed to CPU 2 |
| G3 | H2 | M6 finds it (S2TTB of the 0xC1 stream points outside any partition's tables) |
| G4 | R41 | M2 or M4 finds it (B's SPI assignment refused: HAVEN panics at boot), and M6 errors or finds a missing binding |
| G5 | R45, R46 | the edge stream ID is accepted by both the unchanged tree and the mutants, so no difference is expected: they stay "not exercised" unless a stream ID beyond the table is offered, which a grant never does [I] |
| G6 | every other written and historical case | the same verdicts as in scenario 1, except where the case's own region or interrupt moved because grant 2 added one |

G5 is a prediction that the design does not reach R45 and R46: their
fault is in a bound that only an out-of-range ID tests, and a valid
grant cannot hold one. If so they are recorded as reachable only by
unit tests.

## Implementation (2026-11-22)

[F] What the code does, and what changed from the design before any
campaign run:

- `HAVEN_CAMPAIGN_GRANT2` (CMake option, off) defines the macro of the
  same name for the `haven` target; `src/platform/qemu-virt/memory.h`
  then defines the second device and B's SPI, and `partition.c` adds the
  second entry of `dma_grants[]` and assigns `part_b_spi`, a separate
  route (inside `part_b_irqs[]` it changed the context of W30's patch,
  which then did not apply).
- The second device: `virtio-rng-pci` at `addr=1f.7` (requester ID 255,
  stream 255, the last of HAVEN's 256), config page `0x40_100F_F000`,
  BAR 4 placed at **`0x1001_0000`**, not `0x1000_4000`: next to the
  first BAR, the two stage-2 mappings coalesce and read-back reports
  both BAR regions as extent drift (seen once on the Mac). DMA window
  IPA `[0x5F100000, 0x5F200000)`. B's SPI is INTID 34 (the PL031 RTC).
- `run_case.py --scenario grant2` builds all three profiles with the
  option and gives M6 and M7 both devices; `campaign.yml` takes
  `scenario: grant2`; the cases file is `tools/campaign/cases/s3-grant2.json`
  (three controls, then the 106 cases of `c4-boot.json`) with
  `s3/grant-2.json`.
- Every patch of the 106 cases applies to the grant-2 tree as it does to
  `main` (103 exactly, W30 exactly after the change above), except
  `v3-P2.patch`, which applies with fuzz (its trailing context is the end
  of `dma_grants[]`); the fuzzed result changes the first grant's
  `mmio_count`, as on `main`.

**G1 holds** [F, on the Mac]: with the option off, `haven.bin` of
qemu-direct and qemu-psci is byte-identical to `main`'s, given a fixed
`SOURCE_DATE_EPOCH` (the image carries its build time in the banner, so
two builds a second apart already differ).

A preview on the Mac (QEMU 11.1.3, not the campaign's QEMU; one CPU model
each) [O, not the result]: the unchanged tree passes M2, M4 and M6 (both
streams translated, SPI 34 on CPU 2); H2 is found by M6 (`SID 255 ...
S2TTB 0x100008001a000`: the VMID's low bit in the table address); R41 by
M4 and M6 (B's SPI refused, HAVEN stops); R45 and R46 pass M4 and M6.
The rows are judged on the campaign run.
