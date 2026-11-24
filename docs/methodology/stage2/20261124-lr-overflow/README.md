# List registers: duplicates and overflow (design)

**Status:** design and expectations, 2026-11-24, before any code. Waits
for the owner's approval. This is the G3 item "remaining multi-vCPU
limits" (`stages/G3-BOARD-READINESS.md`), needed before 8 vCPUs run on
the board.

## Problem

HAVEN delivers a virtual interrupt by writing a free GICv3 list register
(LR) on the CPU that runs the vCPU (`arch/arm64/irq.c`). Four defects or
limits sit on that path [F, from the code]:

1. **The same vINTID in two LRs.** `hv_arch_inject_virtual_irq` takes the
   first Invalid LR without looking whether another LR already holds the
   same vINTID. Arm's description of `ICH_LR<n>_EL2` says behaviour is
   UNPREDICTABLE if two or more LRs hold the same vINTID while their
   state is Pending, Active, or Pending and active [F, source:
   ICH_LR<n>_EL2, vINTID field]. The path exists: a virtual SGI posted
   again while the earlier copy is still pending or being handled (Linux
   sends a second rescheduling IPI to a vCPU that has interrupts masked,
   or while it handles the first). Whether today's runs ever take it is
   not known [?]; E0 measures it. This defect was not on the G3 list; it
   was found while reading the code for this record.
2. **A hardware interrupt that finds no free LR is disabled for good.**
   An SPI is disabled on every CPU, a PPI on that CPU, with `IRQ n not
   delivered, disabled`. For the guest's timer (PPI 27) the vCPU loses its
   tick. S2-R40 ended this way under its flood of SGIs
   (`../20261123-readback-board-path/`, B4) [F].
3. **A virtual SGI that finds no free LR waits** in the CPU's request
   word until the next physical interrupt on that CPU. A tickless guest
   may not get one for a long time [I].
4. **The one-vCPU path drops a self-SGI the vCPU has disabled**; the
   multi-vCPU path keeps it pending since SMP3
   (`../20261108-multi-vcpu/`).

Not in this step: the 72 linearly searched MMIO regions (a performance
question, measured on the board as the 8-vCPU record says) and "one
multi-vCPU partition at most" (it stays a stated limit).

## Design [P]

**One decision for every injection.** A new pure function in
`src/core/irq/` (host-testable) takes the LR states and the vINTID and
returns what to do:

| Found | Action |
| - | - |
| an LR with this vINTID, Pending or Pending and active | nothing: it is already pending (an SGI and the timer have one pending state) |
| an LR with this vINTID, Active only, software interrupt (HW = 0) | set that LR to Pending and active (Arm allows this state for software interrupts) |
| otherwise an Invalid LR | write it, as today |
| no Invalid LR | overflow (below) |

A hardware interrupt (HW = 1) cannot meet its own active copy: its
physical interrupt stays active until the guest deactivates it, so it
does not fire again.

**Overflow instead of disabling.** A hardware interrupt that finds no LR
gets its priority dropped (`ICC_EOIR1_EL1`) but is not deactivated (no
`ICC_DIR_EL1`), so it stays active and cannot fire again. Its INTID goes
into a per-CPU waiting set: a bit mask for the PPIs and eight slots for
SPIs. Only when the set is full does today's behaviour remain (disable,
logged). A virtual SGI that finds no LR stays in the request word as
today. In both cases HAVEN sets `ICH_HCR_EL2.UIE`.

**The maintenance interrupt.** With UIE set, the GIC raises the
maintenance interrupt when at most one LR is valid, so at least one LR
is free when it fires (with two or more LRs). It is INTID 25 (PPI 9) on
QEMU's virt machine and on the CIX P1 (`sky1.dtsi`: `interrupts =
<GIC_PPI 9 IRQ_TYPE_LEVEL_LOW 0>`) [F, source]. HAVEN enables it at EL2
on each CPU it runs, at HAVEN's priority, and the redistributor replay
after `CPU_ON` covers it like PPI 26. Its handler moves waiting hardware
interrupts and then waiting SGIs into free LRs and clears UIE when
nothing waits. With fewer than two LRs (`ICH_VTR_EL2.ListRegs` = 0)
HAVEN keeps today's behaviour.

**The one-vCPU self-SGI** uses SMP3's pending bit (`hv_vgic_sgi_hold`)
as the multi-vCPU path does.

**Test aid.** `-DHAVEN_TEST_LRS=2` caps the number of LRs HAVEN uses at
two, so that Linux on three vCPUs in QEMU overflows. Test builds only;
without it the build uses every LR the CPU implements.

**Read-back.** PPI 25 joins `haven_ppis` in the grants ("may be
enabled"), like the EL2 timer.

**Trusted code:** estimate +60 to +120 lines.

## E0: measure first (probe, not merged)

A probe build counts, per CPU: injections that found the same vINTID
Pending, or Active; hardware interrupts that found no LR; SGIs left in
the request word. Runs on the Mac: Linux on 2 and 3 vCPUs (`run_n.py`),
with all LRs and with two. The counts are recorded before any code. They
do not decide the design (defect 1 stands from the architecture) but
show whether today's tests can see the defects at all.

## Expectations

| Row | Check | Expected |
| - | - | - |
| E0 | the probe, before the fix | counts recorded; hypothesis H1: with two LRs and 3 vCPUs, an overflow happens and Linux shows `not delivered` or stalls |
| U1 | host test of the decision function | each row of the table above; a duplicate Pending gives "nothing", an Active software copy "pending and active", a full set "overflow" |
| U2 | host test of the waiting set | PPIs and SPIs are taken back in order; the ninth SPI is refused |
| U3 | `test_sysreg_trap` | a one-vCPU self-SGI the vCPU has disabled is held, then delivered once when it enables it |
| Q1 | CI's Linux job (2 vCPUs, all LRs) | unchanged: `KDAL-DEMAND-END`, no `not delivered` |
| Q2 | `HAVEN_TEST_LRS=2`, Linux on 3 vCPUs, QEMU | all three CPUs up, `KDAL-DEMAND-END`, power-off; no `not delivered, disabled`; HAVEN's report shows maintenance interrupts taken (more than 0) |
| Q3 | partition B under TF-A (CI) | passes: PPI 25 is replayed after `CPU_ON` |
| Q4 | read-back on the scenario-1 controls with the new grants | every method passes, M6 included |
| Q5 | the default images | change (the one-vCPU path and the maintenance handler); size difference recorded |
| Q6 | trusted code | +60 to +120 lines |
| Q7 | campaign scenario 2 after the fix | the controls pass every method; R40 reported on its own [?], since the fix changes the path R40 floods; the frozen campaign's figures stay as recorded |

Board checks to add to the plan: `ICH_VTR_EL2.ListRegs` on the Cortex-A720
and A520 cores, and the maintenance INTID in the board's firmware tables.

Verdict rule as before: a row that does not hold is recorded before any
change, then fixed or explained.
