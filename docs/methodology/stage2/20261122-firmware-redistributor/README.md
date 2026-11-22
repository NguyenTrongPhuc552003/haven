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
