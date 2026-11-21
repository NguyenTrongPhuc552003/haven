# Campaign scenario 2: Linux in partition A with two vCPUs (design)

**Status:** design and expectations, 2026-11-22, before any code. The
owner left the second scenario to Claude ("please do those in
yourself", 2026-11-22). It adds a scenario to the C0 design; the
campaign's taxonomy, methods and scoring stay as they are.

## Problem [F]

C2's random cases (`../c2/random/`): 40 of 60 pass every method, and of
the 12 read so far (10 sampled, R15, R41) 9 sit in code or inputs the
single boot scenario never reaches: a second vCPU's requests (R40), the
vCPU-off path (R53), the park path (R14), a second partition claiming an
SPI (R41), boundary stream IDs (R45, R46), a failed mapping (R59), more
than 256 SMMU commands (R21), a function only the unit tests call (R15).
Scenario 1 is one boot of a bare test guest in A, one CPU per partition.

## What exists [F]

- HAVEN boots Linux in partition A with 2 or 3 vCPUs on QEMU, with the
  pinned private ELMOS SDK kernel and KDAL's module
  (`.github/actions/linux-partition`, `../../20261112-multi-vcpu-n/run_n.py`,
  `scripts/qemu/linux-partition-check.py`); CI runs it on every push.
  Linux brings up the vCPUs, prints KDAL's demand, powers off by PSCI,
  and partition B then starts.
- The Linux jobs run on Ubuntu 24.04 with QEMU 8.2.2, the version
  `ecosystem.lock.json` pins; the campaign's case jobs use Debian
  trixie's QEMU 10.0 because 8.2.2 has no SMMU stage 2 (amendment 3).

## Design [P]

1. **Scenario 2** = the Linux build with `HAVEN_PART_A_VCPUS=2`, as
   `run_n.py` builds and boots it, with the Linux grant of the E4 record.
2. **Methods in scenario 2.** M1 and M3 as in scenario 1 (they do not
   boot). M2: `linux-partition-check.py` for N = 2 (Linux brought both
   vCPUs up, KDAL's demand printed, PSCI off, B started, no forbidden
   line). M4: partition B's check after A powers off. **M5 applies**
   (KDAL's demand against the grant, `tools/readback/demand_check.py`).
   M6: read-back of the running Linux partition (supported since
   read-back v8: `--load` places the guest's files, and interrupts the
   guest enables itself are read from HAVEN's emulated GIC); the exact
   call is fixed in the implementation PR with its own unit test. M7 n/a, as in scenario 1.
3. **Where.** A new job in `campaign.yml`, selected by a `scenario`
   input: Ubuntu 24.04 with `.github/actions/linux-partition`, so the
   pins and the private SDK pull stay in one place. **On QEMU 8.2.2 the
   DMA grant is withheld** (no SMMU stage 2), so SMMU classes are
   reported as not comparable in scenario 2; the scenario targets the CPU,
   GIC and stage-2 paths that scenario 1 does not run.
4. **Cases.** The unchanged tree (control), then the 40 random cases
   that passed every method in scenario 1, with the same patches and
   classes (`c2-random.json`'s entries, a new cases file listing them
   with `scenario: linux2`).

## Expectations

| Row | Check | Expected |
| - | - | - |
| S1 | unit tests of the scenario-2 parts of `run_case.py` | each method's verdict from a recorded log of the passing Linux run and one with a missing marker |
| S2 | the unchanged tree, scenario 2, three times | every method passes (no false finding), M5 included |
| S3 | the 9 "not exercised" cases above, in scenario 2 | at least one of R14, R40, R53 found by some method (the scenario runs a second vCPU and a powered-off partition); R15, R21, R41, R45, R46, R59 still pass every method (their inputs are still absent) |
| S4 | the other 31 all-pass random cases | no prediction per case; the count found is the share of scenario 1's misses that came from the scenario, not the methods |
| S5 | run time | a scenario-2 case takes under 10 minutes (the Linux boot dominates) |

S3's first clause is a prediction I hold weakly: whether Linux's
power-off goes through the vCPU-off path that R53 changes is not
established by reading [?].

## Out of scope [P]

A failed mapping (R59) and stream IDs at the boundary (R45, R46) need
fault injection into a running boot; the unit tests cover those paths
(`test_fault_injection`), and they stay "not exercised" for the
boot-level methods. CPU_OFF from inside Linux (CPU hotplug) needs a
change to the SDK's init; a later step if S3 shows the vCPU paths matter.

## Refinements before code (2026-11-22, from reading the existing runners)

- **The setup is SMP3's** (`../../20261108-multi-vcpu/run_smp3.py`), not
  `run_n.py`'s: the same two-vCPU Linux build, but with
  `hotplug-init.c` as `/init`, which takes vCPU 1 off and on again (PSCI
  CPU_OFF and CPU_ON) before KDAL prints its demand. That exercises the
  vCPU-off path (R53) without changing the SDK, so the "Out of scope"
  note on CPU hotplug no longer applies. Grant: `grant-smp-v12.json`.
- **M2** = `linux-partition-check.py`'s markers for N = 2 plus
  `HOTPLUG-OFF ok` and `HOTPLUG-ON ok` on the free boot's UART. The
  committed SMP3 log (`runs-smp3/H1/uart.log`) has no problem by that
  check [F], so it is S1's passing fixture.
- **M4 is n/a in scenario 2**: `partition-b-check.py` boots its own
  bare-guest build and reads B's record over QMP; the Linux runs start B
  beside Linux, and the check has no Linux mode. B's start is in M2's
  markers (`partition B started`). Recorded as a limit: scenario 2 has no
  partition-B result check.
- **M6** reads at `KDAL-DEMAND-END` (SMP3's H3), cortex-a72 only, as
  every Linux run so far; scenario 1 keeps both CPU models.
- **S2's control** is the unchanged tree three times; the cases file is
  `tools/campaign/cases/s2-linux2.json`.
