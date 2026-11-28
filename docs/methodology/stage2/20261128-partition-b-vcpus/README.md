# Partition B with more than one vCPU (design)

**Status:** design and expectations, 2026-11-28, before any code.
Owner-approved as the next step ("Approve", 2026-11-28).

## Question

The CIX P1 plan gives the real-time partition "2 to 4 cores of the other
A720 cluster" (`../../../architecture/cix-p1-partition-plan.md`). Partition B
has one vCPU today, started by HAVEN at boot on one CPU (`PART_B_CPU`), and
HAVEN's multi-vCPU code (the vCPU table, PSCI CPU_ON from the guest, the
peer set for SGIs) belongs to partition A only. What is the smallest
change that gives B several vCPUs whose isolation read-back can check?

## Options [P]

| Option | What B gets | Cost |
| - | - | - |
| B1 | HAVEN starts every vCPU of B at boot, one per CPU, each with its own stack slot and status record; the vCPUs share B's memory but send no interrupts to each other | a loop over B's CPUs where there is one CPU today; B's guest picks its slot from its MPIDR |
| B2 | B starts its vCPUs by PSCI CPU_ON, as A does | the vCPU table, request words and peer set per partition (lifts "one multi-vCPU partition at most"); more trusted code |
| B3 | an SMP RTOS in B (global scheduling, IPIs) | B2, plus SGIs between B's vCPUs |

**B1 now.** Partitioned scheduling (each core its own fixed task set, no
migration) is the usual way to run hard real-time work on several cores,
and it needs no IPIs [I]. B1 keeps B free of PSCI and SGI paths, so the
trusted code grows least. B2 and B3 go to the parking lot until a
workload needs global scheduling.

## Design B1 [P]

- **Platform:** `PART_B_VCPUS` (default 1) and `PART_B_CPU(i)`. qemu-virt:
  CPUs 2 and 3, so B with 2 vCPUs needs partition A on at most 2 (CPU 3 is
  A's vCPU 2 otherwise; a static check refuses the overlap). cix-p1:
  logical 8 and 9 (cores 0x800 and 0x900); that 9 is an A720 is to be read
  on the board, as for 8 (`../20261123-cix-p1-partition-b/`, P1).
- **HAVEN:** the CPU_ON and ready handshake for each B CPU in turn, as
  today for one; on each, B's stage-2 tables, `cpu_partition`, the timer
  PPI 27 routed to B, B's budget, and VMPIDR_EL2 = vCPU index, so the
  guest sees MPIDR 0, 1, ...
- **Guest B** (`tests/demos/guest_b_*`): the vCPU index from MPIDR_EL1.Aff0
  selects its stack and its status record (an array, one record per vCPU,
  same 19-word layout); vCPU 0 keeps the record's current place, so a
  one-vCPU build reads as before. `partition-b-check.py` reads N records.
- **Read-back:** no new rule; a grant with B on CPUs 2 and 3.

## Expectations

| Row | Check | Expected |
| - | - | - |
| P1 | default builds (`PART_B_VCPUS` 1) | `haven.bin` byte-identical to `main`'s; `guest_b.bin` may differ (the record array), and partition B's check passes as before |
| P2 | qemu-virt, A 1 vCPU, B 2 vCPUs | HAVEN logs both B CPUs ready; both records PASS (stage 5, at least 8 timer interrupts, no other interrupt); budget throttling on CPUs 2 and 3 |
| P3 | A 2 vCPUs (Linux) and B 2 vCPUs | Linux as in scenario 2; both B records PASS |
| P4 | A 3 vCPUs with B 2 vCPUs on qemu-virt | refused at compile time |
| P5 | read-back, grant with B on CPUs 2 and 3, P2's setup | matches the grant; two seeded faults on B's vCPU 1: A's stage-2 tables on CPU 3 (M6: CPU stage-2 drift), its timer route missing (M6, v19: missing interrupt binding) |
| P6 | cix-p1 builds with B 2 vCPUs | build, the static checks hold, the layout test passes |
| P7 | partition B under TF-A (CI) with B 2 vCPUs | passes (each started CPU replays its PPIs) |
| P8 | trusted code | +20 to +60 lines |

A row that does not hold is recorded before anything changes.
