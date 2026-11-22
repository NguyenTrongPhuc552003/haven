# A secondary CPU's redistributor after firmware's CPU_ON (design)

**Status:** design and expectations, 2026-11-22, before any code.
Owner-approved ("Approve", 2026-11-22: fix the firmware-boot bug, with
expectations written before the code).

## Problem [F]

From the EL3 dump of the unchanged tree under TF-A v2.12.0 and TF-A's
source (`../20261119-evaluation-campaign/m7/README.md`, "E0's baseline
finding explained"):

- HAVEN's boot CPU sets up every CPU's redistributor at `gic_v3_init`
  (`gicr_cpu_init`: SGIs and PPIs Group 1 Non-secure, SGIs 0 to 15
  enabled, PPIs disabled, priority `GIC_PRIO_LINUX`), then enables the
  PPIs partitions own on their CPUs (`hv_irq_assign` -> `ppi_assign` ->
  `gic_v3_enable_ppi`), and only then starts each secondary CPU with
  PSCI `CPU_ON`.
- TF-A's `CPU_ON` path runs `gicv3_rdistif_init` on the CPU it starts:
  every SGI and PPI disabled, all Group 1 Non-secure, default priorities,
  then its own secure SGIs 8 to 15 and PPI 29.
- Result on CPU 2: `GICR_ISENABLER0 = 0x2400ff00`. B's timer PPI 27 is
  disabled, and so are SGIs 0 to 7, which HAVEN uses (its physical SGI 1,
  `arch/arm64/irq.c`). Under `qemu-psci` (QEMU's own PSCI, no firmware)
  nothing re-initialises the redistributor, so M2, M4 and M6 never see
  it.

Partition A's vCPUs 1 and 2 (Linux with more than one vCPU) are started
the same way, so the same holds for their CPUs. Every board boots
through TF-A, so this bears on G3.

What it does to B [I, to test first]: its virtual timer interrupt never
reaches EL2, so B counts no timer interrupts.

## Design [P]

1. `gic_v3_cpu_online(cpu)`: the calling CPU sets up its own
   redistributor again, as `gicr_cpu_init` does at boot.
2. `hv_irq_cpu_online(cpu)`: re-enables, on that CPU, every PPI the
   ownership table holds for it (`ppi_owner_map[cpu]`).
3. Both are called in `partition_b_secondary_cpu_init` and
   `partition_a_secondary_cpu_init`, after `hv_arch_phys_irq_cpu_init`
   and before the CPU reports ready. The boot-time writes stay: under
   `qemu-psci` they already take effect, and the second write is the same
   value.
4. `scripts/qemu/partition-b-check.py --tfa DIR`: the same check booted
   through TF-A (as `readback.py --tfa` does), so B's timer can be
   checked under firmware before and after the fix; and a CI job that
   runs it (TF-A built as in `campaign.yml`).

Not in scope: CPUs no partition starts (CPU 1 and 3 in scenario 1) stay
as firmware leaves them, so W30's drift still cannot happen under TF-A.

## Expectations

| Row | Check | Expected |
| - | - | - |
| F0 | `partition-b-check.py --tfa` on `main`, before the fix | B's result "too few timer interrupts" (`timer_irqs` 0) on both CPU models; without `--tfa` it passes (the inference above, tested) |
| F1 | host unit tests | pass, with a new test: after `hv_irq_cpu_online(cpu)` each PPI owned on that CPU has been enabled once on that CPU and no other |
| F2 | `partition-b-check.py --tfa` with the fix | passes on both CPU models |
| F3 | M7 on the unchanged tree with the fix | no finding: CPU 2's `GICR_ISENABLER0` has PPI 27 and SGIs 0 to 7 |
| F4 | `qemu-direct` and `qemu-psci`, unchanged tree | M2, M4 and M6 pass as before; the Linux two-vCPU CI job passes |
| F5 | E0's baseline rule retired (`M7_BASELINE` empty), M7 on the 106 scenario-1 cases | the controls pass; **W30 now agrees** (M7 finds `CPU 2 PPI 27: missing interrupt binding`, which under TF-A is W30's real symptom); the six other differences stay (W09, R42 per-CPU VTTBR_EL2; W27, R19 groups; W14, R60 the EL3 path's missing DMA-master list): 100 of 106 agree |

F0 runs before any change to HAVEN (the `--tfa` option of the check
script comes first, in its own pull request). If F0 does not hold, the
inference is wrong and the design is revisited before code.

## F0 (2026-11-22, on the Mac) [F]

`partition-b-check.py --tfa` (the pull request that adds the option) on
`main` at `cc90a88`, qemu-tfa build, TF-A v2.12.0 with
`tools/el3-readback/`'s patch, QEMU 11.1.3:

| CPU model | under TF-A | under `qemu-psci` |
| - | - | - |
| cortex-a72 | FAIL: stage 4, `timer_irqs=0`, result "too few timer interrupts" | PASS, `timer_irqs=8` |
| max | FAIL: the same, `timer_irqs=0` | PASS |

**F0 holds**: under firmware partition B receives no timer interrupt at
all. The inference of the problem section is now an observation. The
same pull request moves the check's QMP socket from `/tmp` to the
temporary directory (`TMPDIR`).

## The fix and F1 to F4 (2026-11-22, on the Mac) [F]

`gic_v3_cpu_online` and `hv_irq_cpu_online` as designed; the call sits
after each secondary CPU's ready report and before its budget start (the
budget start enables HAVEN's own timer PPI, which the re-initialisation
would otherwise disable again), where it changes no context line of the
campaign's patches: all 106 apply as on `main` (`v3-P2.patch` with fuzz,
as since #198). F1's test checks the selection, not the GIC writes: a
host build has no GIC, so `hv_irq_ppis_on` is what it tests, and F2 and
F3 check the writes in QEMU. `M7_BASELINE` is now empty (F5).

| Row | Result |
| - | - |
| F1 | host tests: 30 of 30 pass, with the new assertions in `test_irq_ppi_contract`. **Holds** |
| F2 | `partition-b-check.py --tfa` with the fix: PASS on cortex-a72 and max, `timer_irqs=8`. **Holds**; the new CI job `Partition B under TF-A (QEMU)` runs it on every pull request |
| F3 | `readback.py --el3` on the unchanged tree with the fix: `enforced state matches the grant`; CPU 2's `GICR_ISENABLER0 = 0x2c00ffff` (SGIs 0 to 15, PPIs 26, 27 and TF-A's 29). **Holds** |
| F4 | qemu-direct M2 and qemu-psci M4 pass on both CPU models, M6 (max) matches the grant; the Linux job runs in CI on this pull request. **Holds** on the Mac |
| F5 | after merge: the campaign's scenario 1 with M7 and no baseline |

## F5 (2026-11-22, campaign run 45042879186 at `1af691d`, the merge of #202) [F]

Scenario 1, all 106 cases, M7 with no baseline rule:

- The controls (`C2-BASELINE`, `C2-V3-C6`, `C2-V3-C7`) pass every
  method, M7 included: the unchanged tree under TF-A has no finding.
- **M7 and M6 agree on 100 of 106.** W30 now agrees (M7: `CPU 2 PPI 27:
  missing interrupt binding`). The six that differ are exactly the
  predicted ones: W09, R42 (per-CPU VTTBR_EL2), W27, R19 (groups under
  TF-A), W14, R60 (no DMA-master list in EL3 mode). **F5 holds.**

Against scenario 1 before the fix (run 44993759751, M7 counted against
E0's finding as #195 did), 104 cases have the same verdict from every
method. Two changed:

- W30, M7 pass to finding: predicted in F5.
- **W31, M4 finding to pass: not predicted.** W31 makes `ppi_assign`
  enable B's PPI on CPU 0 instead of its CPU. The ownership table still
  holds CPU 2, so `hv_irq_cpu_online` now enables the PPI on CPU 2 when
  the CPU comes up, and B gets its timer interrupts. The fault is still
  in the state (PPI 27 enabled on CPU 0 without a grant): M6 and M7 both
  report `CPU 0 PPIs: interrupt enable drift`. The fix made the boot
  CPU's enable redundant for a started CPU, so a fault in it no longer
  shows in B's behaviour, only in the enforced state. The case's M4
  expectation is not changed; the run's summary counts it as a
  disagreement with a named cause, beside C4's W08.

## Amendment 1 (2026-11-22): who decides what a started CPU enables (design)

Owner-approved ("Approve", 2026-11-22: decide whether the boot CPU should
still enable PPIs for CPUs it has not started).

**Problem [F].** #202 re-enables, on a started CPU, every PPI the
ownership table holds for it. That is not what HAVEN programmed in two
cases: W31 (the boot CPU enabled the PPI on the wrong CPU) is repaired
instead of shown (F5), and a PPI that HAVEN disabled on purpose for a
CPU not yet running (Linux partition A: the GIC models start with the
partition's interrupts disabled, `part_a_hw_enable`; an undeliverable PPI,
`arch/arm64/irq.c`) would be enabled again, against the guest's own
enable state [I, not observed: the Linux job passed on #202].

**Options considered.**

- (a) Keep #202: the ownership table decides. Simple; wrong for the two
  cases above.
- (b) Write a PPI only from its own CPU: `ppi_assign` defers to the
  CPU's start. One writer per redistributor, as Linux does with per-CPU
  interrupts; but every other place that writes another CPU's PPI
  (the GIC models' set-up, read-back's expectations of the boot state)
  would change too, and W31 becomes equivalent (its line no longer runs
  for a started CPU).
- (c) **Chosen**: the GIC driver keeps, per CPU, what HAVEN last wrote to
  that CPU's PPI enables and priorities (a shadow), and
  `gic_v3_cpu_online` replays it after setting the redistributor up
  again. The redistributor then ends as HAVEN programmed it, whoever
  reset it in between, so a started CPU behaves the same with and
  without firmware, and a fault in what HAVEN programmed shows again.
  `hv_irq_cpu_online` only calls `gic_v3_cpu_online`; the ownership walk
  (`hv_irq_ppis_on`) goes.

**Left for the board [P, G3].** HAVEN's boot CPU still writes the
redistributors of CPUs that are powered down (at `gic_v3_init` and for
PPIs it assigns). Under TF-A in QEMU those writes are lost, not harmful;
whether a powered-down core's redistributor accepts them on CIX P1's
GIC-700 is a board question. Option (b) is the change if it does not.

| Row | Check | Expected |
| - | - | - |
| A1 | host tests | pass (the driver is not in the host build; `hv_irq_ppis_on`'s assertions go with it) |
| A2 | `partition-b-check.py` with and without `--tfa`, both CPU models | pass (the CI job and the ARM64 job) |
| A3 | the Linux job (2 and 3 vCPUs) | passes |
| A4 | M7 on the unchanged tree | no finding (as F3) |
| A5 | campaign scenario 1, 106 cases | every verdict as in F5 (run 45042879186) except **W31's M4, back to finding** (as before #202); M7 and M6 agree on 100 |
