# CPU 1 of partition A, and read-back v10 (CPU assignment)

**Status:** owner-approved 2026-11-07 ("Approve", item 1), with the choice
between changing the grant and changing HAVEN left to Claude. Decision,
design and expectations by Claude, committed before the runs below.

## Problem (`../20261107-dt-check-blind/RESULTS.md`, M01)

Every grant since the GIC read-back (2026-11-02) gives partition A CPUs 0
and 1 (`gic.cpus`), following `configs/arm64/qemu-virt.yaml`
(`linux: [0, 1]`). HAVEN runs one vCPU per partition and answers PSCI
CPU_ON with DENIED (`src/core/exc/psci.c`), so partition A only ever runs
on CPU 0. Read-back never compared CPU assignment, so the gap was silent.

## Decision: change the grant, not HAVEN (for now)

| | Grant drops CPU 1 (chosen) | HAVEN starts CPU 1 for partition A |
| - | - | - |
| Trusted code | none | per-vCPU state, CPU_ON routing, cross-CPU SGIs, a second budget and timer: a multi-vCPU partition |
| Matches what HAVEN does | yes | yes, after the work |
| Thesis question | unchanged: read-back of enforced state | adds SMP partitions, not needed to answer it |

A multi-CPU Linux partition will matter on the 12-core CIX P1, so it goes
to the parking lot for a later gate, not into this step. The new grant is
`grant-l4-cpus.json` (v8's `grant-l4-guest.json` with CPU 1 unassigned).
`configs/arm64/qemu-virt.yaml` is left as is: it is a design file that
HAVEN's build does not read.

## Read-back v10

Reads HAVEN's per-CPU partition table (`cpu_partition` in
`arch/arm64/irq.c`: the partition each CPU runs, 0 for none; HAVEN uses
it to choose which partition an interrupt is injected into) and compares
it with `gic.cpus`. Any difference is **CPU assignment drift**. `irq.c`
asserts the 32-bit entry size at compile time; unit tests:
`tools/readback/test_cpu_assignment.py` (4).

Seeded fault S1 (`S1.patch`): HAVEN records CPU 0 as partition B's just
before entering partition A.

Observed before this record, with read-back v9: X3's configuration
(qemu-psci demo, `grant-budget.json`) matches, and partition B starts on
CPU 2.

## Runs and expectations (Claude's, before the runs)

| Run | Build | Grant | Read at | Expected |
| - | - | - | - | - |
| X1 | Linux profile | `grant-l4-guest.json` (old) | `KDAL-DEMAND-END` | 2 findings: CPU 1 (granted to A, runs none) and CPU 2 (granted to B, runs none: the Linux profile boots `qemu-direct`, where partition B is not started) |
| X2 | Linux profile | `grant-l4-cpus.json` (new) | `KDAL-DEMAND-END` | 1 finding: CPU 2 |
| X3 | qemu-psci demo | `grant-budget.json` | `IRQ test done` | 1 finding: CPU 1; CPU 0 runs A and CPU 2 runs B as granted |
| X4 | Linux profile + S1 | `grant-l4-cpus.json` | entry breakpoint | 2 findings: CPU 0 (granted to A, runs B) and CPU 2 |
| X5 | Linux profile | `grant-l4-cpus.json` | entry breakpoint | 1 finding: CPU 2 (CPU 0 is already set to A at the entry) |

No other finding is expected in any run.

**Consequence already visible:** with the Linux profile, partition B is
granted CPU 2 (and its RAM and PPI 27 there) but never runs. That is the
next mismatch v10 will report; whether the Linux profile should start B
or its grant should say B is inactive is a separate decision, not taken
here.
