# Read-back of a partition with three vCPUs (design)

**Status:** design and expectations, 2026-11-28, before any code.
Owner-approved as the next step ("Approve", 2026-11-28).

## Question

Read-back reads HAVEN's vCPU table generically (v12 to v14) and checks
each CPU's PPIs (v6, v12) and stage-2 root (v15). It has only run against
partition A with one or two vCPUs. The 8-vCPU record lists "read-back of
an N-vCPU partition" as not shown (`../20261112-multi-vcpu-n/`). Does
read-back see a fault that touches only a third vCPU, and does it keep its
stated limit there (a vCPU HAVEN never started reads as "waiting for the
guest", `../20261119-evaluation-campaign/s2/`, E5)?

## Setup [P]

Scenario 2's setup with three vCPUs instead of two, in QEMU 8.2.2 (CI):
`-DHAVEN_PART_A_VCPUS=3`, vCPU 2 on CPU 3 (`PART_A_CPU(2)` on qemu-virt),
partition B on CPU 2 as before. The same SDK kernel, hotplug init (CPU 1
off and on), KDAL module and demand; the tree gets three cpu nodes and
three redistributor frames (`run_n.py`'s `tree`).

- `tools/campaign/linux2.py` takes the number of vCPUs; a new scenario
  `linux3` in `run_case.py`, `run_shard.py` and the campaign workflow.
  `linux2` stays as it is.
- Grant `grant-n3.json`: `grant-s2-qemu82.json` plus CPU 3 for partition
  1, its PPI 27, HAVEN's PPI 26, PPI 27 enabled by the guest, and CPU 3
  on request with CPU 1.
- Cases file `tools/campaign/cases/n3.json`: three controls and the three
  written cases below.

## Written cases [P]

Each changes only what vCPU 2 or CPU 3 gets. The patches are written with
the code; each row's prediction is fixed here first.

| Case | Class | Fault | Expected |
| - | - | - | - |
| N3-W1 | GIC-PPI | the loop that gives vCPUs 1 to N-1 their timer PPI stops one early, so CPU 3 never gets PPI 27 | M6 finds it (CPU 3 PPI 27: missing binding); M2 a finding or a pass [?] (Linux on CPU 2 without a tick) |
| N3-W2 | S2-OWNER | the CPU running vCPU 2 enables partition B's stage-2 tables instead of A's | M6 finds it (CPU 3's VTTBR_EL2 belongs to partition 2); M2 finds it (Linux cannot run on CPU 2) |
| N3-W3 | CPU-ASSIGN | vCPU 2's entry in HAVEN's vCPU table gets vCPU 1's MPIDR, so the guest's CPU_ON for MPIDR 2 finds no vCPU | M2 finds it (CPU 2 does not come up); **M6 passes**: CPU 3 is on request and HAVEN never started it, read-back's stated limit (E5) |

M1 (host tests) and M3 (the static checks) pass in all three, as they do
not boot. M4 is n/a (scenario 2 has no partition-B Linux mode). M5 (the
demand) passes where the boot reaches it, else is an error.

## Expectations

| Row | Check | Expected |
| - | - | - |
| N1 | host tests, Python tests | pass; scenario 2's three controls (`lr-controls-linux2.json`) still pass every method |
| N2 | the Mac, one control boot with read-back | Linux brings up 3 CPUs; read-back: matches the grant; CPU 3's PPIs 26 and 27 enabled |
| N3 | the three controls in CI (`linux3`) | every method passes |
| N4 | N3-W1, N3-W2, N3-W3 | as in the table |
| N5 | time | under 10 minutes per case, setup included |

A row that does not hold is recorded before anything changes.

## Results (2026-11-28) [F]

At `4952608` (pull request #229). Mac: QEMU 11.1.3, one control with a
Mac grant (the SMP grant with CPU 3 added, kept out of the repository,
since QEMU 11 has the SMMU stage 2 that CI's QEMU 8.2.2 lacks). CI:
campaign run 46125619906 (`linux3`, `n3.json`, six shards) and run
46125622042 (`linux2`, scenario 2's controls).

| Row | Result |
| - | - |
| N1 | host and Python tests pass (13 in `test_linux2`); scenario 2's three controls pass every method. **Holds** |
| N2 | Mac control: Linux brings up 3 CPUs; read-back matches the grant, reads a vCPU table of 3 entries, all on, and CPU 3 has PPIs 25, 26 and 27 enabled. **Holds** |
| N3 | N3-BASE-1 and -3 pass every method; **N3-BASE-2's M5 is an error**: `no demand printed`. The demand was printed; a kernel line from another CPU landed on the marker's own line (`KDAL-DEMAND-BEGIN[    4.098657] kdal: gpu0 probed`), and `linux2.demand()` expects a line break right after the marker. A harness defect (as R49's in scenario 2), not a verdict. **Does not hold as written** |
| N4 | N3-W2: M2 and M6 find it (CPU 3: CPU stage-2 drift), as predicted. N3-W3: M2 finds it, M6 passes (CPU 3 on request, never started), as predicted. **N3-W1 differs twice**: M6 passes (predicted: finding) and M1 finds it (predicted: pass) |
| N5 | the slowest shard took 5.2 minutes, setup included (N3-W1, whose read-back waits 90 s for a marker that never comes). **Holds** |

**Why read-back misses N3-W1 [F].** Read-back reports `guest-enabled
INTID 27 on CPU 3: waiting for the guest`. Its rule for interrupts the
guest enables (v8) takes "did the guest ask?" from HAVEN's GIC model. The
model ignores an enable write for an interrupt the partition does not own
(`src/core/irq/vgic.c`, `set_enable`: "not this partition's: WI"). With
the route missing, HAVEN believes the partition does not own PPI 27 on
CPU 3, so the guest's enable never reaches the model, and read-back reads
"not asked yet". **The fault hides behind HAVEN's own ownership table**,
the state it corrupts. This is a limit of read-back v8's rule that no
case had shown, distinct from E5 (a CPU never started).

**Why M1 finds N3-W1 [F].** The host test of the 8-vCPU cix-p1 layout
(`test_partition_layout_cix8`) counts partition A's interrupt routes; one
loop iteration fewer is one route fewer. My prediction assumed the host
tests do not cover the routing loop.

**Proposal [P], for the owner.** Read-back could compare the grant's PPIs
with HAVEN's ownership table (as it reads the GIC model and the vCPU
table) and report a granted PPI that HAVEN does not hold for that
partition as "missing interrupt binding". It is a new rule (v19) with its
own expectations; not done here.

**Next [P].** The demand parse is fixed with a test built from
N3-BASE-2's log, and the six cases run again; the rerun is recorded below.
