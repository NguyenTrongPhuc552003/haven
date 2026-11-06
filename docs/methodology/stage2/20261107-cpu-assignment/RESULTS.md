# CPU assignment: results

**Run:** 2026-11-07, macOS, QEMU virt, haven `7eb59a1` (expectations
committed there before the runs). Raw output: `runs/X1..X5/` and
`runs/results.md`.

| Run | Build | Grant | Read at | Observed (granted / HAVEN runs, CPUs 0 to 3) | Expectation |
| - | - | - | - | - | - |
| X1 | Linux profile | old | `KDAL-DEMAND-END` | 1/1, 1/0, 2/0, 0/0: 2 findings, CPU 1 and CPU 2 | held |
| X2 | Linux profile | new | `KDAL-DEMAND-END` | 1/1, 0/0, 2/0, 0/0: 1 finding, CPU 2 | held |
| X3 | qemu-psci demo | `grant-budget.json` | `IRQ test done` | 1/1, 1/0, 2/2, 0/0: 1 finding, CPU 1 | held |
| X4 | Linux profile + S1 | new | entry breakpoint | 1/2, 0/0, 2/0, 0/0: 2 findings, CPU 0 (runs B) and CPU 2 | held |
| X5 | Linux profile | new | entry breakpoint | 1/1, 0/0, 2/0, 0/0: 1 finding, CPU 2 | held |

No other finding in any run. 5 of 5 expectations held.

**Observations.**
- Read-back now sees which partition HAVEN runs on each CPU. The seeded
  fault S1 (CPU 0 recorded as partition B's) is reported at the entry
  breakpoint, before the guest runs; no earlier check looked at this
  table.
- With the new grant, partition A's CPUs match in every Linux run. The
  remaining finding, CPU 2 granted to partition B but running nothing, is
  real: the Linux profile boots `qemu-direct`, where HAVEN cannot start
  partition B. In the qemu-psci demo (X3) CPU 2 runs B as granted.
- The demo grants (`grant-budget.json` and its predecessors) also give
  partition A CPU 1; X3 shows the same gap there. They are kept as they
  were approved; a run that needs them clean uses a corrected copy.

**Open decision (not taken here).** For the Linux profile, either start
partition B (a PSCI path in `qemu-direct`, or boot Linux under
`qemu-psci`), or mark B inactive in the grant (no CPU, and HAVEN should
then not enable PPI 27 on CPU 2 for it).
