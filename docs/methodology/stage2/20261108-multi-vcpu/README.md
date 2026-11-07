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
| SPI 33 (virtio, partition A) enabled by Linux from whichever vCPU | one `GICD_ISENABLER` model state shared by both vCPUs; read-back v12a shows it enabled and routed to CPU 0 at `KDAL-DEMAND-END` |
