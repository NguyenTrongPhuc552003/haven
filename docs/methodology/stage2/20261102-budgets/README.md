# CPU time budgets enforced by the EL2 timer

**Status:** EXPERIMENT, observed 2026-11-02. Work approved by the owner.
Checks and duty bands by Claude, committed in `5d7d908` before the run;
not blind. `grant-budget.json` was approved by the owner on 2026-11-03.

## Changes

| Commit | Change |
| - | - |
| `d6d87a6` | `arch/arm64/budget_hw.c`: per CPU, Haven arms the EL2 physical timer (CNTHP, PPI 26, Haven priority 0x40) at period start + budget; on expiry it holds the CPU at EL2 until the next period boundary of a fixed grid, then re-arms. Partition A 8 ms per 10 ms on CPU 0, partition B 2 ms per 10 ms on CPU 2. Statistics in `hv_cpu_budget`. Both demo guests measure their duty cycle on the virtual counter (`tests/demos/guest_duty.h`). PPI 26 on CIX P1: vendor GTDT defaults (`data/common/cix-bios`) |
| `5d7d908` | Read-back v6: HAVEN's own PPIs in the grant (`haven_ppis`) must be enabled at HAVEN's priority |

## Results (`run_budget.py`, `runs/results.md`): 2 of 8 as expected by the bands fixed beforehand

| Check | Change | Partition A duty (both CPUs) | Partition B duty | Read-back | Matched |
| - | - | - | - | - | - |
| P1 | none | 527 / 564 per mille, longest gap about 9.6 ms | 319 per mille, longest gap 7.8 to 7.9 ms, 20 gaps in 200 ms | matches the grant | yes, both |
| P2 | enforcement off | 994 to 997, 2 to 4 short gaps | 987 to 1000, up to 3 short gaps | missing binding on PPI 26, CPUs 0 and 2, **plus** priority drift | no |
| P3 | B's budget = its period | 619 to 627 (throttled) | 1000, no gap | missing binding on CPU 2 PPI 26 **plus** priority drift | no |
| P4 | EL2 timer priority 0xf0 (masked) | 990 to 992, 2 short gaps | 995 to 1000, at most 1 gap | priority drift on PPI 26, CPUs 0 and 2 | no |

Why the six misses are prediction errors, not enforcement failures
(experimental observation):

1. **"Unthrottled" was defined as no gap at all.** Without enforcement the
   guests still saw 1 to 4 gaps of 0.3 to 1.7 ms per 200 ms (QEMU host
   scheduling). Every unthrottled duty value is at least 987 per mille,
   against 319 to 627 when throttled, so the throttled and unthrottled
   cases separate clearly; but by the band fixed beforehand they fail. A
   band allowing a few short gaps would pass them; that revision is post
   hoc and is not used to rescore.
2. **Read-back reports a never-configured PPI twice.** In P2 and P3 the
   PPI 26 that Haven never set up is both disabled (missing binding, as
   expected) and left at the default priority 0xa0 (priority drift, not
   expected). Both are true; the second is redundant. Candidate tool
   change RB3: skip the priority check for a disabled granted interrupt.

## Observations

- **Budgets work, but QEMU's timer is late.** Haven's statistics show the
  expiry seen 0.6 ms late on CPU 2 and about 2.1 ms late on CPU 0 (up to
  60 ms in rare outliers on the `max` model). On CPU 2 that lifts B from
  the nominal 200 to about 250 to 320 per mille. On CPU 0 the delay
  exceeds the 2 ms between budget end and period end, so most expiries
  arrive after the period ended (`missed` 35 to 47 of 45 to 57), and the
  conservative policy holds A until the next boundary: A runs about 10 ms
  in every 20 (527 to 627 per mille instead of 800). Haven never let a
  partition run more than its budget plus the delay; it under-served A.
- **Why CPU 0 is later than CPU 2 in QEMU is not known.** It is a QEMU
  host-timing effect (no other code arms CNTHP); it must be measured
  again on the board, where it becomes the "timer delivery latency" metric.
- **Read-back sees disabled or mis-prioritised enforcement** (P2 to P4),
  which no functional check catches: the partitions still run.

## Limits

- While a CPU is held at EL2 its interrupts wait, including Haven's SMMU
  event interrupt on CPU 0 (up to 2 ms per period).
- One partition per CPU: a budget only bounds consumption; there is no
  second partition on the same CPU to receive the time.
- QEMU numbers are not board numbers (see `../20261102-interference/PLAN.md`).
