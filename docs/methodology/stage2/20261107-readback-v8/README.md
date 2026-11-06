# Read-back v8: interrupts enabled on the guest's request

**Status:** owner-approved 2026-11-07 (follow-up 1 of the Linux chain case
set: "a grant rule for lazily enabled interrupts"). Design and
expectations by Claude, committed before the runs below.

## Problem (from `../20261106-linux-chain-cases/RESULTS.md`, finding 3)

Since L3 the emulated GIC keeps a partition's owned interrupts physically
disabled until the guest enables them. Read-back v7 reported such an
interrupt as "granted but disabled" whenever it read before the guest
had enabled it (cases T4 and T5), and could not tell "the guest has not
asked yet" from "HAVEN failed to enable it". It also could not see H3
(owned interrupts enabled before the guest asked), because by the time it
read, the guest had asked.

## Design

- **Grant:** `gic.guest_enabled` lists, per CPU, the interrupts HAVEN
  enables only on the guest's request (format 2: `haven.guest-enabled`).
  `grant-l4-guest.json` is the approved `grant-l4.json` plus
  `{"0": [27, 33]}`.
- **Read-back v8:** for those interrupts it reads the emulated GIC's own
  enable bits from HAVEN's memory (symbol `by_cpu`, `hv_vgic.enabled` at
  offset 40; `vgic.c` asserts the offset at compile time) and compares them
  with the hardware:

  | Guest enabled it | GIC has it enabled | Verdict |
  | - | - | - |
  | no | no | waiting for the guest (information) |
  | yes | yes | matches |
  | yes | no | missing interrupt binding |
  | no | yes | interrupt enable drift |

  Without the model (no symbol, no model on that CPU) the v7 rule applies.
- **HAVEN log rate limit** (follow-up 3): the first 16 denied accesses per
  CPU are logged, then every 4096th with its running number; the total is
  printed when a partition stops.

Unit tests: `tools/readback/test_guest_enabled.py` (6),
`tests/unit/test_el2_exceptions.c` (rate limit).

## Runs and expectations (Claude's, before the runs)

`rerun_v8.py` reuses the frozen case set's builder, device trees and
patches; only the grant and the read moment differ. "Early" reads when
Linux prints `Booting Linux`, before its GIC driver has run.

| Run | Image | Read at | Expected read-back verdict |
| - | - | - | - |
| V1 | L4 (B0) | `KDAL-DEMAND-END` | matches; 27 and 33 enabled by guest and GIC |
| V2 | T4 (memory 1 GiB) | time limit | matches; 27 and 33 waiting for the guest (v7: two findings) |
| V3 | T5 (timer as PPI 12) | time limit | matches; 27 and 33 waiting for the guest (v7: two findings) |
| V4 | H3 (owned interrupts enabled at start) | `KDAL-DEMAND-END` | matches (the guest has asked by then; still invisible) |
| V5 | L4 (B0) | early | matches; 27 and 33 waiting for the guest |
| V6 | H3 | early | two findings: enable drift on SPI 33 and on CPU 0 PPI 27 |

Expected HAVEN log in V2: at most 16 plus a few numbered denial lines,
and a total line when the run is cut (no total if the partition never
stops; the partition in T4 does not stop).
