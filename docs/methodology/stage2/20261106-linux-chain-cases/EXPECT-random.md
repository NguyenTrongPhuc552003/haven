# Expectations for the randomly drawn held-out cases (R1 to R3)

Written by Claude after the draw (`heldout/draw.json`: haven `9e2024e`,
928 mutants, 3 kept, none skipped) and before any run, then frozen with
the rest of the set. The faults were drawn, not chosen; these
expectations are Claude's and are scored like the 12 approved predictions.

| Case | Drawn mutation | What it breaks | Demand check | Read-back | Boot | HAVEN log |
| - | - | - | - | - | - | - |
| R1 | `vgic.c:104` `n < first \|\| n > last` -> `&&` | The emulated GIC's group-register range check never rejects: a word outside the view's IDs is read or written in the model's arrays (out of bounds of `group[]`/`grpmod[]`, into the rest of the model). | as L4 | matches | completes | nothing new |
| R2 | `vgic.c:61` affinity mask `0xFFFFFF` -> `0x1000000` | GICR_TYPER reports the vCPU's affinity with Aff2..Aff0 dropped (only bit 24 of MPIDR kept). | as L4 | matches | completes | nothing new |
| R3 | `el2_exceptions.c:385` `logged < 4 && hv_printk` -> `\|\|` | The cap of four log lines for trapped system registers no longer applies: every such trap is logged. | as L4 | matches | completes | nothing new |

Reasons (inference from source, not from a run):

- **R1:** Linux v6.18 writes the group registers only for word 1 of the
  distributor (`for (i = 32; i < GIC_LINE_NR; i += 32)`, 64 IDs here) and
  word 0 of the redistributor (`ppi_nr + SGI_NR` = 32), which are inside
  the views' ranges (`irq-gic-v3.c` lines 989 and 1342); it does not touch
  IGRPMODR, and there are no extended SPIs. The out-of-range path is never
  reached: a real defect that this guest does not exercise.
- **R2:** partition A's vCPU has MPIDR 0x80000000 (Aff2..Aff0 = 0, MT = 0,
  as Linux printed `Booting Linux on physical CPU 0x0000000000`), so the
  wrong mask still gives affinity 0 and the redistributor matches. On a
  vCPU with a non-zero affinity, Linux would find no redistributor.
- **R3:** the baseline (B0, L4) traps no system register other than
  ICC_SGI1R_EL1, which takes the other branch, so nothing is logged
  either way.

Taken together: if these hold, none of the three random faults in the
Linux-guest code is visible to any of the four checks under this
workload. That would itself be a result for the thesis (end-state checks
and a single workload leave the emulation's own defects untested), not
a reason to change the expectations.
