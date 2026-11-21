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

## M5's baseline (correction before any run, 2026-11-22)

Writing S1's test showed that the unchanged setup's demand check already
reports two findings [F, on `runs-smp3/H1/uart.log`]: KDAL's GPIO driver
demands `0x9030000` and INTID 39, which `grant-smp-v12.json` does not
give partition A. So "M5 passes on the control" (row S2) holds only
against a baseline: M5's verdict in scenario 2 is a finding when the
demand check's findings differ from those two (`linux2.KNOWN_DEMAND`),
either way. The two findings are a property of the setup (the test
device tree offers a GPIO the grant leaves out), recorded here.

Implementation: `tools/campaign/linux2.py`, `run_case.py --scenario
linux2`, `run_shard.py` (both case jobs), a `run-linux2` job in
`campaign.yml` (input `scenario`), the cases file `s2-linux2.json`; S1:
`test_linux2.py`, 9 tests.

## First run (2026-11-22): S2 fails on M6, a design error

`campaign.yml`, `scenario=linux2`, `s2-linux2.json`, eight shards, at
`0e44f90` (the merge of #185), on the pinned QEMU 8.2.2 [F]. Every case
ran; scenario 1 on `k3-k4.json` at the same commit still passes (the
case jobs now go through `run_shard.py`).

- **S2 fails**: on all three controls M1, M2, M3 and M5 pass, but **M6
  has three findings**: `A PCI config: missing binding`, `A PCI BAR:
  missing binding`, `pci-1af4:1044: DMA enforcement drift` (SID 8 denied,
  expected translated). The UART says why: `DMA grant SID 8 -> partition
  1 withheld (rc=-95)`. On QEMU 8.2.2 HAVEN withholds the DMA grant and
  the PCI device's pages, as amendment 3 recorded; this design said the
  DMA classes were "not comparable" but kept the DMA parts in the grant
  read-back compares with. **Design error, mine**; every case's M6
  carries the same three findings, so M6 says nothing in this run.
- **M5 error on R49** (`demand_check exit 2`): the demand block caught a
  kernel log line from the other vCPU between `KDAL-DEMAND-BEGIN` and
  `-END` (`bad input: Extra data`), so the JSON did not parse. A harness
  defect (two CPUs print at once), not a verdict about R49.
- **S3 holds**: of the 9 "not exercised" cases, **R40** (M2: no demand,
  no power-off; the cross-CPU request counter) and **R53** (M2:
  `HOTPLUG-ON ok` missing; the vCPU-off path) are now found; R14 is not.
  R15, R21, R41, R45, R46 and R59 pass every method as predicted (R15 and
  R41 were not in this set: only the 40 all-pass cases of scenario 1 are).
- **S4** (preliminary, M6 aside): besides R40 and R53, M2 now finds
  **R4** and **R7** (vCPU 1 never started: which CPUs are A's, the vCPU
  lookup) and **R39** (the virtual GIC setup: Linux brings up one CPU, no
  demand). 5 of the 40.

**Fix, then the run again [P]:**

1. `s2/grant-s2-qemu82.json`: `grant-smp-v12.json` without what HAVEN
   withholds when QEMU has no SMMU stage 2 (the PCI config page, the BAR
   and stream 8). `linux2.py` uses it for M5 and M6.
2. `linux2.demand()` drops kernel log lines (`[ seconds ]` prefix) inside
   the demand block, with a test built from R49's case.

| Row | Check | Expected |
| - | - | - |
| S2b | the run again, same cases | the controls: every method passes, M6 included; then S3 and S4 are read with M6 |

## The run again (row S2b, 2026-11-22)

At `9e19bb2` (the merge of #187), same cases, eight shards [F]:

- **S2b holds**: all three controls pass every method, M6 included;
  no disagreement with the expectations. R49's M5 now passes (the demand
  parses).
- **Found in scenario 2, of the 40 that scenario 1 never reached** (M4
  and M7 n/a throughout):

| Case | Class | M1 | M2 | M3 | M5 | M6 |
| - | - | - | - | - | - | - |
| R4 | CPU-ASSIGN | pass | **F** vCPU 1 never started | pass | pass | pass |
| R7 | CPU-ASSIGN | pass | **F** vCPU 1 never started | pass | pass | pass |
| R39 | GIC-ENABLE | pass | **F** one CPU, no demand | pass | error (no demand) | **F** PPI 26 missing bindings |
| R40 | GIC-ROUTE | pass | **F** no demand, no power-off | pass | error (no demand) | **F** PPI 27 missing bindings on CPUs 0 and 1 |
| R53 | CPU-ASSIGN | pass | **F** `HOTPLUG-ON ok` missing | pass | pass | pass |

  The other 35 pass every method again.
- **S3 holds** (as in the first run): R40 and R53 found; R14 not; R21,
  R45, R46 and R59 still pass every method.
- **S4: 5 of 40** come from the scenario. M2 (the Linux boot check)
  finds all five, read-back two (R39, R40), none read-back alone.

**Why read-back passes R4, R7 and R53 [I].** In all three, vCPU 1 is
not running when read-back reads: never started (R4, R7) or not back
after CPU_OFF (R53). The SMP grant marks CPU 1 "on request"
(`cpus_on_request`, read-back v12): a CPU the guest has not started is
"waiting for the guest", information, not a finding. Read-back cannot
tell a guest that did not ask from a HAVEN that did not start the CPU;
only the guest's own view (M2) can. Recorded as a limit of read-back's
snapshot for on-request CPUs (E5), not changed here.

**S5** holds: the slowest shard took about 7 minutes for five or six
cases, under 10 minutes a case.
