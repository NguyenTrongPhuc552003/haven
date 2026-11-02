# DMA hardening: HAVEN places the BAR, config page read-only, SMMU event interrupt

**Status:** EXPERIMENT, observed 2026-11-02 at haven `d0ed794`. Approved by
the owner (items 2 and 3 after G2). Checks and expected outcomes by Claude;
not blind.

## Change under test

- `drivers/pci/ecam.c` (`af55a6a`): HAVEN sizes and places the granted
  function's memory BAR (QEMU: BAR 4 at `0x1000_0000`), and enables memory
  decoding and bus mastering only after the SMMU enforces the DMA window.
  The partition gets the config page read-only (`DEVICE_R`), so a write to
  it faults at stage 2. A grant for an absent function is withheld.
- `1f84fe0`: the SMMU's wired event interrupt (QEMU: SPI 74, INTID 106) is
  routed to CPU 0; the CPU interface accepts Group 1 at EL2
  (`ICC_PMR_EL1 = 0xF0`, `ICC_IGRPEN1_EL1 = 1`); the handler drains the
  event queue and logs the first four events, then totals at powers of two.
  Other physical interrupts are completed without delivery (GI2).
- Read-back (`d0ed794`): also decodes events HAVEN already consumed, and
  accepts a HAVEN-owned interrupt (partition 0) in the GIC grant.

Grant: `grant-hardening.json`. Runner: `run_hardening.py`; results:
`runs/results.md`; dumps replaced by `runs/DUMP_SHA256SUMS`. QEMU 11.1.3,
virtio-rng-pci with `iommu_platform=on,disable-legacy=on`, cortex-a72 and max.

## Results (both CPUs identical, 12 of 12 as expected)

| Check | Change | Inside view (UART) | Read-back | SMMU events (total / consumed by HAVEN) |
| - | - | - | - | - |
| H1 | none | config write denied at stage 2 ("config page read-only"); DMA inside OK, outside blocked; HAVEN logs `SMMU event 0x10 SID 8 addr 0x5e000000 write` | matches the grant (incl. SPI 106 to CPU 0) | 17 / 17 |
| H2 | no device | HAVEN: grant withheld, device absent | missing binding for config page and BAR (the grant states the intent) | 0 / 0 |
| H3 | config page mapped read-write | "config write landed" (the partition moved its BAR) | **access drift** on the config page | 0 / 0 |
| H4 | bus mastering enabled before the SMMU window | DMA inside OK, outside blocked | matches the grant | 17 / 17 |
| H5 | SMMU interrupt routed to CPU 2 | no HAVEN event log | **routing drift** on SPI 106 | 17 / 0 |
| H6 | SMMU interrupt never enabled | no HAVEN event log | **missing interrupt binding** on SPI 106 | 17 / 0 |

Host unit tests 20 of 20; boot check passes on both CPUs (no device: grant
withheld).

## Observations

- The config page fault is visible in HAVEN's log
  (`IPA=0x4010008000 FAR=0x4010008020 ESR=0x9382004f`: data abort, write,
  permission fault level 3) and the BAR stays where HAVEN put it.
- HAVEN now sees what the SMMU stopped without external help; read-back
  shows the same 17 events and that HAVEN consumed them.
- **H4 is a blind spot by construction:** enabling bus mastering before the
  window is enforced leaves the same end state, so read-back of the final
  state cannot see it. Ordering faults need a trace or a probe during boot,
  not after it.

## Limits

One QEMU device; HAVEN's config handling is all-or-nothing (no filtered
writes, so the partition cannot use MSI or MSI-X); interrupts owned by
partitions are still not delivered (GI2).
