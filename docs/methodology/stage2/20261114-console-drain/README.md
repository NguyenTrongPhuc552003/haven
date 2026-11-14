# The flaky Linux-partition check: draining the guest console

Owner-approved 2026-11-14. This record covers one CI fault: the
`linux-partition` job (Linux in partition A on QEMU, `.github/workflows/ci.yml`)
sometimes fails although nothing it tests has changed.

## Problem and evidence (before this record)

- **Observed rate.** In the latest 100 runs of `ci.yml` (latest attempt of
  each run, read with `gh api .../actions/runs/ID/jobs` on 2026-11-14) the
  job failed once and passed 99 times. One failure in 100 is a rate
  between about 0.03 % and 5.4 % (exact 95 % interval), so the rate itself
  is poorly known.
- **The failing run** (recorded in
  [20261113-quality-gate](../20261113-quality-gate/README.md), fix list part 5):
  at N = 2 the guest printed `KDAL-DEMAND-BEGIN`, and the next console line
  is the kernel's `reboot: Power down`. The demand JSON and
  `KDAL-DEMAND-END` never arrived; the same commit passed in other runs.
- **The code.** The guest's `/init` (`kdal/scripts/ci/kunit-init.c`)
  writes the markers to the console with `write(1, ...)` and calls
  `reboot(LINUX_REBOOT_CMD_POWER_OFF)` straight after.

## Hypothesis

A `write()` to the console tty returns once the bytes are in the tty's
transmit buffer; the serial driver sends them later, from its transmit
interrupt. The kernel's own messages (`printk`) are written to the UART
synchronously. So `reboot()` can power the machine off while user output
is still queued, and the kernel's power-off line overtakes it. The window
is short, which fits a rare fault that depends on timing (here 2 vCPUs, on
a shared CI host).

`ioctl(fd, TCSBRK, 1)` is how `tcdrain()` is implemented on Linux: the
kernel waits until the tty's buffer and the driver's transmitter are empty
(`tty_wait_until_sent`), and a non-zero argument sends no break.

## Design

- **D1, the fix (KDAL).** `kunit-init.c` calls `ioctl(1, TCSBRK, 1)`
  before `reboot()`: one raw system call (`ioctl` is 29 on arm64). If the
  console never drains, `/init` waits forever; the boot then stops at
  QEMU's time limit with the stop line missing, which is a visible
  failure, not a silent pass.
- **D2, one setup.** The steps that pin, fetch and build what the job
  boots (the KDAL checkout, the ELMOS SDK pulled by digest, the tools,
  `kdal.ko`) move into a composite action,
  `.github/actions/linux-partition`, used by the `linux-partition` job and
  by the stress workflow, so the two cannot drift.
  `ecosystem.py check-workflow` then checks every workflow and action
  file: no pinned literal in any of them, and every job that uses the
  action runs on the pinned runner.
- **D3, a stress workflow.** `.github/workflows/linux-stress.yml`, on
  demand only: 10 shards in parallel, each building HAVEN once for N = 2
  and once for N = 3 and booting both ROUNDS times (default 100), so
  2,000 boots per campaign. Each boot is checked against the same markers
  as the CI check (`scripts/qemu/linux-partition-check.py`, whose check
  becomes a function both use). A failing boot's UART log is kept; a final
  job adds up the shards and fails if any boot failed.
- **D4, before and after on the same workflow.** "Before" is a campaign on
  `main` (KDAL at the current pin, `f11e87e`). "After" is a campaign
  dispatched on the branch that moves `ecosystem.lock.json` to the KDAL
  commit with D1, before that branch merges, so the "after" campaign
  tests exactly what CI will then use.
- **Not changed:** HAVEN, the kernel, QEMU and the markers checked.
  `run_n.py` is refactored only so the stress script can reuse its
  initramfs, tree, build and boot steps; its output stays the same.

## Expectations

| Row | Check | Expected |
| - | - | - |
| C1 | the stress campaign before D1 (2,000 boots) | at least 1 failing boot, with the signature above (`KDAL-DEMAND-BEGIN` present, `KDAL-DEMAND-END` missing); 0 failures means the fault is not shown at this size |
| C2 | KDAL's CI after D1 | KUnit 40 of 40 and the module build pass, unchanged |
| C3 | the stress campaign after D1 (2,000 boots) | 0 failing boots |
| C4 | `run_n.py` and the CI check after D2 | the `linux-partition` job passes; `results.md` has the same lines as before |
| C5 | boot time | the mean time per boot changes by less than 1 s between C1 and C3 |
| C6 | `check-workflow` after D2 | passes on the real files; fails on a scratch copy of the stress workflow with a KDAL commit written into it, and on one whose job uses another runner |

**How strong the result can be.** With 0 failures in 2,000 boots (C3),
the failure rate after D1 is below about 0.15 % (exact 95 % bound). C1 is
what tells us whether 2,000 boots are enough to see the fault at all. If
C1 shows 0 failures, C3 says nothing about the fix: that outcome is
recorded as such, and the fix then rests on its mechanism alone.

## Results (haven #124, #127; KDAL #34)

Campaigns: `linux-stress.yml`, 10 shards of 100 rounds, each round one
boot at N = 2 and one at N = 3. "Before" on `main` at `842f989` (KDAL
`f11e87e`); "after" on the branch of #127 at `45aee69` (KDAL `5622c47`,
the commit with D1). The per-boot logs stay in CI.

| Row | Observed | Verdict |
| - | - | - |
| C1 | 21 of 2,000 boots failed (N = 2: 8 of 1,000; N = 3: 13 of 1,000), in 7 of 10 shards; every one misses `KDAL-DEMAND-END` and nothing else | holds |
| C2 | KDAL's CI on `5622c47`: green; KUnit 40 of 40 (9 + 6 + 10 + 8 + 5 + 2 in its six suites), module build and `insmod` ok | holds |
| C3 | 0 of 2,000 boots failed | holds |
| C4 | #124's `linux-partition` job passes through the action; the boot step prints the same 16 result lines as before, differing only in kernel timestamps and interrupt counts, which differ between any two runs | holds |
| C5 | mean seconds per boot, before and after: N = 2 2.48 and 2.59, N = 3 2.57 and 2.69 (0.11 s and 0.12 s more) | holds |
| C6 | `scripts/ci/test_ecosystem.py` (7 tests: literals in any file, short hashes, another runner, every user's runner, no user, the real files) passes on Linux, macOS and Windows; CI: "5 files ... 2 jobs use ./.github/actions/linux-partition on ubuntu-24.04" | holds |

**The two failure forms.** In 8 failing boots `KDAL-DEMAND-BEGIN` arrived
and the rest of the demand output did not; in 13 it is missing too (the
summary's classification). Two of shard 8's saved logs, read in full, show
each form: `KDAL-DEMAND-BEGIN` and then the kernel's `reboot: Power down`
(round 6, N = 2); `KDAL-CI: insmod ok` and then `reboot: Power down`
(round 17, N = 3). Both fit the hypothesis: what `/init` had written was still queued when the machine
powered off, and how much of it the UART had sent varied.

**Strength of the result [inference].** Before: 21 in 2,000 is 1.05 %
(exact 95 % interval 0.65 % to 1.60 %), in line with the 1 failure in 100
CI runs measured first. After: 0 in 2,000, so below about 0.15 % (one-sided
95 %). If D1 changed nothing, the chance of all 21 failures falling in the
"before" campaign is 4.5e-7 (Fisher's exact test, one-sided). The cause
is shown by the change that removes the fault, not observed directly in
the tty layer; this is a QEMU result, on CI hosts.

**Lock file.** #127 moved `kdal.commit` to `5622c47` and
`haven.tested_from` to `df625d3`, the commit it was made on.
