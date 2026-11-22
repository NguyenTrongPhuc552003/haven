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
