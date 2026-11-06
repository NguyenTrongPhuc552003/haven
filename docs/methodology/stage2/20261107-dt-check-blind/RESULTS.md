# Blind test of dt_check.py: results

**Run:** 2026-11-07, macOS, haven `52a2dca` (dt_check, catalogue and
runner match `freeze/SHA256SUMS`). Draw and predictions committed in
`52a2dca` before this run. Raw output: `runs/M00..M12/` (tree, dt_check
output, UART log) and `runs/results.md`.

## dt_check against the labels

| Fault | Fault drawn | Label | dt_check | Score |
| - | - | - | - | - |
| M01 | a second CPU node | inside | matches | TN |
| M02 | initramfs range +16 MiB | inside | matches | TN |
| M03 | memory 496 MiB | inside | matches | TN |
| M04 | KDAL node renamed | inside | matches | TN |
| M05 | KDAL registers at 0x09000800 | outside | registers 0x9000800+0x1000 not granted | TP |
| M06 | KDAL interrupt SPI 1 edge | inside | matches | TN |
| M07 | KDAL registers 0x2000 | outside | registers 0x9000000+0x2000 not granted | TP |
| M08 | virtual timer PPI 10 | outside | PPI 26 not granted | TP |
| M09 | virtual timer PPI 12 | outside | PPI 28 not granted | TP |
| M10 | three more CPU nodes | outside | 4 CPU nodes, partition 1 has 2 | TP |
| M11 | KDAL registers 0x800 | inside | matches | TN |
| M12 | KDAL node renamed (as M04) | inside | matches | TN |

**5 true positives, 7 true negatives, no false positive or negative.**
The control M00 (unchanged T1) matches.

## Boots

| Fault | Predicted | Observed | |
| - | - | - | - |
| M00 | completes | completes | held |
| M01 | completes; CPU 1 not online | completes; HAVEN answers Linux's CPU_ON for CPU 1 with -3; "Brought up 1 node, 1 CPU" | held |
| M02 | kernel panic | "Unpacking initramfs", then "Unable to mount root fs", kernel panic | held |
| M03, M04, M07, M11, M12 | completes | completes | held |
| M05 | completes, no denied access | completes, 0 denied | held |
| M06 | completes, possibly a trigger mismatch message | completes, no mismatch message | held (the optional part did not occur) |
| M08 | does not complete | stops after the console is enabled, no timer interrupt | held |
| M09 | does not complete | as M08 | held |
| M10 | completes; CPUs 1 to 3 not online | completes; CPU_ON for 1, 2, 3 each answered -3 | held |

## Against the predictions

All 13 dt_check predictions and all 13 boot predictions held. One error
in `EXPECT.md`: its summary says 6 faults outside and 6 inside, and
predicts 6 true positives and 6 true negatives. The table above it labels
5 outside and 7 inside, which is what the draw gives; the summary line
was miscounted. It is left as committed.

## Observations

- dt_check named every drawn fault that leaves the grant, including two
  it had not been tuned on (a register range crossing the end of a page,
  M05; the virtual timer moved to HAVEN's own PPI, M08), and reported
  nothing on the 7 faults inside the grant.
- Containment is not correctness. M02 is inside the grant and still
  stops Linux (the tree points at an initramfs that is not there). Of the
  3 faulty trees that broke the boot, dt_check reported 2.
- The grant gives partition 1 CPUs 0 and 1 (`gic.cpus`), but HAVEN starts
  no second CPU for partition A: Linux's CPU_ON for CPU 1 gets -3 (M01).
  The grant promises more than HAVEN provides here; read-back does not
  check that a granted CPU can be started.

## Limits

12 faults from one catalogue, one fault per tree; the labels and dt_check
come from the same specification and the same author (see `README.md`).
The result supports "dt_check implements its specification on unseen
faults of these kinds", not "dt_check finds all harmful trees".
