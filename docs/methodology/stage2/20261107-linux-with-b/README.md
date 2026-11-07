# Linux in partition A with partition B running

**Status:** owner-approved 2026-11-07 ("Approve", item 1 of 21:36).
Design and expectations by Claude, committed before the runs below.

## Problem (`../20261107-cpu-assignment/RESULTS.md`)

The Linux runs since L1 build HAVEN with the `qemu-direct` boot profile,
which has no PSCI provider: HAVEN cannot switch CPU 2 on, so partition B
never starts, although the grant gives it CPU 2, its RAM and PPI 27
there. The evaluation needs both partitions running at once (a Linux
partition and a real-time partition), so the QEMU Linux configuration
should run both.

## Change

Build the Linux profile with `-DHAVEN_BOOT_PROFILE=qemu-psci` instead of
`qemu-direct`. In that profile QEMU's built-in PSCI answers HAVEN's
CPU_ON over SMC (`CMakeLists.txt`, owner approval 2026-11-02), as in the
demo runs where B already starts (`../20261102-partition-b/`). No HAVEN
code changes. The QEMU command line is the same. Partition A's own PSCI
calls still go to HAVEN (HVC, emulated since L1).

From here on, new Linux runs use `qemu-psci`; the frozen records keep
`qemu-direct` as they were run.

## Runs and expectations (Claude's, before the runs)

Grant: `../20261107-cpu-assignment/grant-l4-cpus.json` (partition A on
CPU 0, B on CPU 2). Tree: the case set's B0 (L4).

| Run | Profile | Read at | Expected |
| - | - | - | - |
| Y1 | qemu-psci | `KDAL-DEMAND-END` | read-back matches; CPU table 1/1, 0/0, 2/2, 0/0; HAVEN logs "CPU 2 ready, partition B started"; Linux reaches `KDAL-DEMAND-END` as in L4 |
| Y2 | qemu-psci | entry breakpoint | matches, same CPU table (HAVEN starts B before it enters A); no Linux output yet |
| Y3 | qemu-direct | `KDAL-DEMAND-END` | control, as X2: 1 finding, CPU 2 granted to B but running nothing; B inactive |

Not predicted, recorded if seen: whether B's demo guest prints on the
UART Linux owns (HAVEN's console policy is an open item), and Linux's
boot time with B running beside it.
