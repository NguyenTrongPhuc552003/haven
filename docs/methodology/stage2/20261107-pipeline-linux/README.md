# Interference pipeline with Linux in partition A (QEMU check)

**Status:** owner-approved 2026-11-07 ("Approve", item 3 of 21:50).
Design and expectations by Claude, committed before the runs below.

**What this is:** a check that the measurement pipeline of
`../20261102-interference/PLAN.md` still works now that partition A runs
Linux and partition B runs beside it (`../20261107-linux-with-b/`). It is
**not** an interference measurement: QEMU's TCG models no caches, memory
bandwidth or interconnect, and its timing is the Mac's.

## Design

- **Aggressor (partition A):** Linux with a tiny `/init`
  (`load-init.c`, freestanding, raw system calls) that prints one line
  and then runs its load forever: `idle` (sleeps), `cpu` (busy loop) or
  `mem` (writes and reads a 64 MiB buffer, one access per 64 bytes).
  Linux is no longer powered off after KDAL, so the load is still running
  when the victim measures.
- **Victim (partition B):** the existing demo guest's latency probe: from
  4 s after power-on, 500 virtual-timer interrupts 1 ms apart, delay from
  the programmed deadline to the handler, read over QMP from B's status
  record (`scripts/qemu/partition-b-check.py`'s reader). No change to B.
- **B's budget:** off (10 ms per 10 ms) in three cells, as the 2026-11-03
  run recommended for interference questions; 2 ms per 10 ms in one
  control cell. HAVEN: Linux profile, `qemu-psci`. Tree: case set T1.
  CPU model: cortex-a72. One run per cell.

| Cell | Linux load | B budget |
| - | - | - |
| idle-Boff | idle | off |
| cpu-Boff | cpu | off |
| mem-Boff | mem | off |
| idle-B2ms | idle | 2 ms / 10 ms |

## Pipeline checks (per cell, 11)

B started on CPU 2; the load line printed; Linux reached `/init` before
4 s (kernel time), so the load runs during B's probe; partition A not
stopped; B's status magic, stage 5, 500 samples, result pass, caches on
(SCTLR M and C); B's deliberate read of A's memory denied on CPU 2; no
undelivered interrupt.

## Expectations (Claude's, before the runs)

1. All 11 checks pass in all 4 cells.
2. The latency numbers are reported, not interpreted, with one
   pipeline-level expectation carried from the 2026-11-03 run: the
   control cell (B budget 2 ms) averages several milliseconds (there:
   about 4 ms, the budget's own hold), and the three budget-off cells
   average under 1 ms (there: about 0.5 ms).

Any difference between the load cells is QEMU host contention, not
evidence about hardware.

Note on order (corrected): the expectations above were written before
the runner was first started, but the commit of this README failed (the
format hook rejected `load-init.c`) and the runner started anyway. When
Claude stopped it, two cells had already finished (idle-Boff and
cpu-Boff, all checks ok, averages 309 and 266 us); Claude saw that output
only after stopping it. An earlier version of this note, committed in
`5a8a0eb`, wrongly said no cell had run. The expectations text was not
changed after that output was seen; all four cells are run again from
this commit, and only that run is scored. The partial output is kept in
`partial-first-start.txt`.
