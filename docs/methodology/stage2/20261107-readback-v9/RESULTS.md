# Read-back v9: results

**Date:** 2026-11-07, macOS, QEMU virt. Expectations in `README.md` were
committed in `50c6fbb` before the runs. Raw output: `runs/W1..W6/`
(UART log, device tree, read-back text and JSON, `gdb.json`) and
`runs/results.md`.

| Run | Case | Read at | Observed | Expectation |
| - | - | - | - | - |
| W1 | B0 | entry breakpoint, CPU 0 | matches; 27 and 33 waiting for the guest | held |
| W2 | H3 | entry breakpoint, CPU 0 | 2 findings: enable drift on SPI 33 and on CPU 0 PPI 27 (guest 0, GIC 1 for both) | held |
| W3 | H3 | entry breakpoint, CPU 0 | identical to W2 (`readback.txt` and UART log byte for byte) | held |
| W4 | H3 | entry breakpoint, CPU 0 | identical to W2 | held |
| W5 | H1 | entry breakpoint, CPU 0 | matches; both waiting | held |
| W6 | B0 | `KDAL-DEMAND-END` | matches; 27 and 33 enabled by guest and GIC (as V1) | held |

Every breakpoint run stopped on thread 1 (CPU 0) with stop reply
`T05thread:01;`, and its UART log has 20 lines, all from HAVEN, none from
Linux. 6 of 6 expectations held.

**Observation.** The H3 drift on PPI 27 that V6 missed is found at the
entry breakpoint, and three repeated runs give the same result byte for
byte. The read point no longer depends on how quickly read-back notices
a UART line.

**Inference.** Read-back now has two read points with different jobs:
the entry breakpoint sees what HAVEN set before the guest ran (H3), and a
late read sees what changes on the guest's requests (missing bindings,
H1 if its effect lasts). Neither alone covers both; a run should use both.

**Limits.**
- QEMU only. On the board the same read point needs an external debugger
  (or a HAVEN-side record, the fallback in `README.md`); not yet checked
  for the Orange Pi 6 Plus.
- One address (partition A's entry). Partition B's entry, or later fixed
  points such as the guest's first GIC access, are not covered.
- W5 shows the breakpoint read reports nothing false for H1; it does not
  catch H1, which acts only after the guest starts.
