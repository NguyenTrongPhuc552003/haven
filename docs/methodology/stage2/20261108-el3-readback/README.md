# EL3 read-back prototype in QEMU (board fallback B)

**Status:** owner-approved 2026-11-07 ("Approve to prototype the EL3
read-back service in QEMU now"). Design and expectations by Claude,
committed before the comparison runs below.

## Question

If a board has no usable external debugger, can firmware at EL3 read
HAVEN's enforced state from outside HAVEN well enough that read-back
reaches the same verdicts as with the QEMU monitor?

## What was built

- **HAVEN under TF-A in QEMU** (first time in a record): TF-A v2.12.0,
  QEMU platform with GICv3, BL33 preloaded at HAVEN's address; HAVEN's
  `qemu-tfa` profile. HAVEN boots at EL2, partition B starts on CPU 2
  through TF-A's PSCI, and the demo partitions run as before (seen
  2026-11-07 while building this record).
- **The EL3 service** (`tools/el3-readback/`): BL31 arms the secure
  physical timer; when it fires (8 s), it dumps HAVEN's region, the GIC and
  SMMU registers and the interrupted CPU's EL2 registers on the secure UART.
- **Read-back v11**: `--tfa DIR` boots under TF-A; `--el3` reads only the
  dump. The GIC dump uses Secure accesses and is converted to the
  Non-secure view the comparison expects.

## Calibration (before this record's expectations)

On the unmodified demo build, without a PCI device, both channels were
run once (`--tfa`, monitor at `caches on`; `--tfa --el3`):

- the converted EL3 view matched the monitor's in GICD_CTLR (0x12),
  enables, routing and every priority (0xa0, 0x40);
- one difference: the monitor reported "interrupt group drift" on SPIs 33
  and 106, the EL3 view did not. With two security states (TF-A sets
  `GICD_CTLR.DS = 0`), Non-secure reads of `GICD_IGROUPR` return zero, so
  the monitor's Non-secure view cannot see groups. Read-back v11 therefore
  skips the group check in monitor mode under TF-A and says so; EL3 mode
  checks groups from the Secure view.
- both channels reported "CPU 2 PPI 27: missing interrupt binding" at
  their late read moments; it is recorded here, not explained.

`grant-demo-cpus.json`: the owner-approved `grant-budget.json` with CPU 1
unassigned (as `grant-l4-cpus.json` did for Linux).

## Runs and expectations (Claude's, before the runs)

`run_el3.py`: per case, the demo build with the `qemu-tfa` profile plus a
seeded fault from earlier records (applied to HEAD), read once through the
monitor and once from the EL3 dump, with the virtio PCI device behind the
SMMU.

| Case | Fault (record) | Expected findings, both channels |
| - | - | - |
| E0 | none | the calibration's CPU 2 PPI 27 finding only (the PCI grants now bind) |
| M3 | stage-2 execute bit dropped (A1 v3) | E0 plus execute drift on A RAM and B RAM |
| M4 | UART mapped Normal (A1 v3) | E0 plus memory-type and shareability drift on A UART, A PCI config, A PCI BAR |
| M5 | partition B 2 MiB larger (A1 v3) | E0 plus extent drift on B RAM |
| S8 | DMA grant read-only (A1 v3) | E0 plus DMA access drift on SID 8 (the stream table lies inside HAVEN's region, so the dump has it) |
| I1 | SPI 33 routed to another CPU (GIC read-back) | E0 plus routing drift on SPI 33 |
| I4 | all SPIs written Group 0 (GIC read-back) | E0 only: with two security states HAVEN's Non-secure writes to `GICD_IGROUPR` are ignored, so the fault does not reach the hardware; the EL3 view shows Group 1 Non-secure |
| I5 | all CPU 0 PPIs enabled (GIC read-back) | E0 plus interrupt enable drift on CPU 0 PPIs |
| S1 | CPU 0 recorded as partition B (CPU assignment) | E0 plus CPU assignment drift on CPU 0 (the monitor may read at the time limit: partition A gets no interrupts) |

Main expectation: **the two channels report the same findings in all 9
cases.** The read moments differ (monitor at `caches on`, EL3 at 8 s),
which could make a time-dependent finding differ; if one does, it is
reported as such.
