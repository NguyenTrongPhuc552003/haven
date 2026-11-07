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
