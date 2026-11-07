# Multi-vCPU partitions, steps SMP0 to SMP2: expectations

**Status:** owner-approved 2026-11-07 (G2: multi-vCPU partitions are
committed work; "Approve" to start with a design note and expectations).
Design: `../../../architecture/multi-vcpu.md`. These expectations are
written by Claude **before any of the code exists**; results are added
below as each step runs. Configuration for SMP1 and SMP2: QEMU virt,
Linux profile with `qemu-psci`, partition A (Linux, KDAL) on CPUs 0 and 1,
partition B on CPU 2, the case set's B0 device tree extended with a
second CPU node and redistributor frame.

## SMP0: unit tests (host)

| Test | Expected |
| - | - |
| PSCI CPU_ON for a CPU of the calling partition that is off | SUCCESS, the vCPU becomes ON_PENDING then ON |
| CPU_ON for a CPU of another partition, an unlisted CPU, or one already on | DENIED, ALREADY_ON or INVALID_PARAMETERS as PSCI 1.0 defines; nothing started |
| AFFINITY_INFO for each vCPU | follows the per-vCPU state |
| ICC_SGI1R decoding: target list, affinity levels, IRM | the set of target vCPUs of the same partition only; targets outside the partition dropped |
| Shared distributor: an SPI enabled from vCPU 1 | visible from vCPU 0's model; the physical enable happens once |
| All existing host tests | still pass |

## SMP1: Linux on two vCPUs

| Check | Expected |
| - | - |
| Linux log | "smp: Brought up 1 node, 2 CPUs"; reaches `KDAL-DEMAND-END`; SYSTEM_OFF stops both vCPUs (HAVEN logs it for CPU 0 and CPU 1) |
| HAVEN log | CPU 1 started for partition A on the guest's CPU_ON; virtual SGIs delivered on CPU 1 (count > 0: Linux sends rescheduling and function-call IPIs) |
| Partition B | still starts on CPU 2; its probe and isolation checks as in `../20261107-pipeline-linux/` |
| Read-back v10 with an SMP grant (CPU 1 to partition 1, its PPIs and HAVEN's PPI 26) at `KDAL-DEMAND-END` | matches; CPU table 1/1, 1/1, 2/2, 0/0 |
| Read-back v10 at the entry breakpoint | **one finding: CPU 1 granted to partition 1, runs nothing** (the guest has not called CPU_ON yet), which v12 will turn into "waiting for the guest" |
| Trusted code | grows by roughly 250 code lines (design estimate; measured, not judged) |

## SMP2: read-back v12 and seeded faults

| Run | Expected |
| - | - |
| Entry breakpoint, SMP grant with `cpus_on_request` CPU 1 | matches; CPU 1 "waiting for the guest" |
| `KDAL-DEMAND-END`, same grant | matches; CPU 1 runs partition 1 |
| Fault: vCPU 1 started on physical CPU 3 | two findings: CPU 1 granted to partition 1 runs nothing; CPU 3 granted to no partition runs partition 1 |
| Fault: a cross-CPU SGI injected into the sending vCPU instead of the target | read-back: no finding (no state is left behind); caught by the SMP0 unit test for SGI targeting; Linux may stall (not predicted) |

Results follow below as each step runs.

## SMP0 results (2026-11-08, host, macOS arm64)

Commands: `cmake --build build-host`, `ctest --test-dir build-host -E
bench_temporal_isolation`, then the three changed test binaries; cross
builds of the `arm64-qemu` (qemu-direct, qemu-psci), `arm64-imx95` and
`arm64-imx8qm` presets; the CI QEMU checks `qemu-direct-check.sh` and
`partition-b-check.py` on cortex-a72 and max.

| Test | Expected | Observed | Verdict |
| - | - | - | - |
| CPU_ON for an off vCPU of the caller | SUCCESS, ON_PENDING then ON | SUCCESS, action START_VCPU, index 1, state ON_PENDING; the move to ON is the caller's (integration) step, checked here only as table state | as expected |
| CPU_ON for another partition's CPU, unlisted, already on | DENIED, ALREADY_ON or INVALID_PARAMETERS | another partition's CPU is not in the caller's table, so INVALID_PARAMETERS; already on: ALREADY_ON; nothing started | as expected |
| (not predicted) CPU_ON again while the target is pending | none | ON_PENDING (PSCI 1.0 code -5) | extra check |
| AFFINITY_INFO per vCPU | follows the state | 1 off, 2 on-pending, 0 on; level not 0 or unknown CPU: INVALID_PARAMETERS | as expected |
| ICC_SGI1R decoding | targets of the same partition only | target list, Aff1 mismatch, RS, IRM (all peers but the sender) as expected; peers are the partition's own vCPUs, so a CPU outside it is never a target; a cross-CPU SGI is posted to the target's physical CPU, not injected locally | as expected |
| Shared distributor: SPI enabled from vCPU 1 visible from vCPU 0 | visible; one physical enable | **not tested in SMP0.** By design both CPUs' GICD regions use the same `vgic[0]` model, so this is an integration property; moved to SMP1 (added row below, written before the SMP1 code) | not run |
| All existing host tests | pass | 25 of 25 ctest tests; read-back suite unchanged (29); all four cross builds link; the four CI QEMU checks PASS | as expected |

Deviation found while scoring (observation): an SGI sent to a vCPU that
has that SGI disabled is **dropped**, while GICv3 keeps it pending until
it is enabled. The self-only path already behaved this way; SMP0 extends
it to cross-CPU SGIs. Inference: Linux enables SGIs on a secondary in
`gic_cpu_init` before marking it online, so normal IPIs should not hit
this, but an early IPI to CPU 1 would be lost. Recorded as a known
limitation and an SMP1 risk; fixing it needs a pending bit in the model.

Expectation added for SMP1 before its code exists:

| Check | Expected |
| - | - |
| SPI 33 (the PL011 UART, partition A; first written as "virtio", a label error corrected before any SMP1 run) enabled by Linux from whichever vCPU | one `GICD_ISENABLER` model state shared by both vCPUs; read-back v12a shows it enabled and routed to CPU 0 at `KDAL-DEMAND-END` |

## SMP1 results (2026-11-08, QEMU 4 CPUs cortex-a72, TCG)

HAVEN `a27dc96` (code, runner `run_smp1.py`, grant `grant-smp.json` and
the two-CPU tree all committed before the first run), built for
`qemu-psci` with `-DHAVEN_PART_A_VCPUS=2`. Command:
`run_smp1.py <workspace>/haven <workspace>/kdal <Linux v6.18 Image> kdal.ko runs`,
first with `ONLY=F1`, then `ONLY=R1,R2` (the second call rewrote
`runs/results.md`; the F1 section was merged back by hand). Raw output in
`runs/`. Every run below is the first of its kind: no code changed
between runs.

| Check | Expected | Observed | Verdict |
| - | - | - | - |
| Linux log | "Brought up 1 node, 2 CPUs"; `KDAL-DEMAND-END`; SYSTEM_OFF stops both vCPUs, logged for CPU 0 and CPU 1 | F1: `CPU1: Booted secondary processor 0x0000000001`, `smp: Brought up 1 node, 2 CPUs` at 0.060 s; `KDAL-DEMAND-END` at 0.64 s; after `reboot: Power down`, HAVEN logs CPU 1's report and "CPU 1 parked", then CPU 0's report and "partition stopped by PSCI OFF" | as expected |
| HAVEN log | CPU 1 started on the guest's CPU_ON; virtual SGIs on CPU 1, count > 0 | `CPU 1 ready, partition A vCPU 1 started at IPA 0x41e40570`, then `PSCI CPU_ON ... x1=0x1 -> 0x0`; CPU 1: SGI 0 x24, 1 x229, 2 x1, 5 x30, PPI 27 x104; CPU 0: SGI 0 x9, 1 x220, 5 x36, PPI 27 x106 | as expected |
| Partition B | starts on CPU 2; probe and isolation as before | `CPU 2 ready, partition B started`; B's probe of IPA 0x40000000 denied and read as zero, the same two lines as run Y4 of `../20261107-linux-with-b/`; no undelivered interrupt | as expected |
| Read-back at `KDAL-DEMAND-END` (R1) | matches; CPU table 1/1, 1/1, 2/2, 0/0 | `RESULT enforced state matches the grant`; CPUs 1/1, 1/1, 2/2, 0/0; guest-enabled 33 and 27 on CPU 0, 27 on CPU 1, each also enabled in the GIC | as expected |
| Read-back at the entry breakpoint (R2) | one finding: CPU 1 granted to partition 1, runs nothing | **two findings**: that one, and `CPU 1 PPI 26: missing interrupt binding (granted but disabled)` | **prediction error** |
| SPI 33 shared distributor state (row added after SMP0) | one model state; enabled and routed to CPU 0 at `KDAL-DEMAND-END` | R1: enabled in the model and in the GIC, routing checked by v12a without a finding. Which vCPU wrote the enable was not recorded | as expected; the "from vCPU 1" case is not shown |
| Trusted code | about +250 code lines (design estimate) | `tcb-image-loc.py`: 6,762 code lines at `32da55c` (before SMP0), 7,187 at `a27dc96`: **+425**. SMP0's pure layers +150 (psci.c +56, vgic.c +47, sysreg_trap.c +23, headers +24), SMP1's integration +275 (partition.c +157, irq.c +76, el2_exceptions.c +42). The count reads whole files, so the one-vCPU Linux build counts the same | measured, 1.7 times the estimate |

R2 prediction error (Claude's): the SMP grant gives HAVEN's EL2 timer PPI
26 on CPU 1, which HAVEN enables only when vCPU 1 starts (partition A's CPU
time budget there). Before the guest's CPU_ON nothing runs on CPU 1, so
the PPI is granted but off. The expectation counted the CPU only, not its
HAVEN interrupt, which is the same omission as run Y1 of
`../20261107-linux-with-b/` in the other direction. For SMP2, read-back
v12's "waiting for the guest" state for a CPU in `cpus_on_request` should
cover HAVEN's own interrupts of that CPU as well as the CPU itself; the
SMP2 expectations are amended below, before any SMP2 code exists.

Observations not predicted: Linux sent IPI_CPU_STOP (SGI 2) to CPU 1 at
power-down and HAVEN delivered it (CPU 1, SGI 2 x1) before SYSTEM_OFF.
Boot to `KDAL-DEMAND-END` took 0.64 s with two vCPUs, against 0.71 s in Y4
with one (one run each, TCG; not a measurement of anything).

Limitations kept from SMP0 and found here (inference unless marked):
a disabled SGI is dropped, not kept pending; a vCPU stopped by CPU_OFF
cannot be started again (HAVEN parks the CPU at EL2 instead of switching
it off, and refuses the second CPU_ON with INTERNAL_FAILURE); a virtual
SGI that finds no free list register waits for the next physical
interrupt on its CPU; one multi-vCPU partition at most (one peer set).
`tcb-image-loc.py --by-file` prints the rows, then fails on the missing
SUM row (observed; a tool bug, parked).

SMP2 expectation amended (Claude, before any SMP2 code):

| Run | Expected |
| - | - |
| Entry breakpoint, SMP grant with `cpus_on_request` CPU 1 | matches; CPU 1 "waiting for the guest", **and its HAVEN PPI 26 "waiting for the guest" too** (was: CPU 1 only) |
