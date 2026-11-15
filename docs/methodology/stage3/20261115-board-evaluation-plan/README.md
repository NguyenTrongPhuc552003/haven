# Board evaluation plan (Orange Pi 6 Plus, CIX P1)

**Status:** DRAFT, 2026-11-15, by Claude (owner-approved item: "a frozen
board evaluation plan before the G3 gate"). It becomes **frozen** when the
owner approves it at G3 (early December 2026): from then on, metrics,
configurations, sample sizes and pass rules change only through a dated
amendment section added before the first board measurement; after the
first measurement they do not change, and any deviation is recorded as a
deviation, never edited in. Labels: **[F]** fact from our records, **[S]**
source-derived, **[P]** proposal, **[?]** for the owner.

This plan supersedes `../../EVALUATION_PLAN.md` and
`../../PHYSICAL_BOARD_VALIDATION.md` (written for the i.MX95 Dev Kit,
before the board choice). The interference experiment keeps its own plan,
`../../stage2/20261102-interference/PLAN.md`, which part D uses unchanged.

## What the board evaluation must answer

| Part | Question | Charter |
| - | - | - |
| A | Does HAVEN enter at EL2 and run both partitions on the board? | evidence ladder L4 |
| B | Do the isolation properties seen in QEMU hold on the board? | L5 |
| C | Does read-back on the board match the grant, and catch the seeded faults that apply there? | RQ, contribution 1; L6 |
| D | What does isolation cost, and how much does a busy partition disturb the real-time one? | section 8, "HOW WELL"; not a novelty claim |
| E | Can HAVEN set MPAM partitioning on P1 and read its settings back? | contribution 3: characterisation only |

[F] QEMU never stands in for the board (charter section 8): no board
question is answered by a QEMU result, and QEMU timing is never compared
with board timing.

## Preconditions (before any number counts)

1. The probes P1 to P8 of the loading study (`docs/porting/cix-p1-loading.md`),
   each with its log kept: free memory ranges, `haven-probe.efi`
   output at EL2, ACPI and Device Tree modes, vendor Linux's EL2 and KVM
   lines, UART3 pins, watchdog behaviour, display DMA, UART2 after the MMU
   is off.
2. The board's firmware version, SCMI frequency settings and thermal state
   are recorded for every session; core frequencies are pinned through SCMI
   if the firmware allows it, otherwise recorded per run (PLAN.md threats).
3. Every image is built by CI from a tagged commit with `ecosystem.lock.json`
   at that commit; nothing measured on the board is built on the Mac.

## A. Bring-up (L4)

| Step | Observable | Pass |
| - | - | - |
| A1 | `haven-boot.efi` loads HAVEN at 0xF000_0000 and enters it at EL2 (UEFI Shell, no flashing) | HAVEN's banner on UART3 |
| A2 | CPU map: the cores of the platform's map come online in HAVEN's order | HAVEN's CPU ready lines |
| A3 | Partition B (real-time) on its cores | its status record, as `partition-b-check.py` reads it in QEMU |
| A4 | Partition A: Linux 6.18 with KDAL on 2 and 3 vCPUs, then up to 8 | the `linux-partition-check.py` markers |

## B. Isolation on the board (L5)

The QEMU negative tests repeated on the board where the hardware applies:
partition B's read of partition A's memory denied (stage-2 abort, logged),
an interrupt routed to the wrong partition refused, a DMA initiator
outside its window denied (only with a real initiator and SMMU stage 2;
charter section 7 keeps DMA out of scope until both exist). Pass: every
attempt denied and logged, the victim partition's status unchanged.

## C. Read-back on the board (L6)

- **Method order** (G2 decision, 2026-11-07): an external debugger on J21
  first, then an EL3 firmware service, behaviour probes always, HAVEN's
  self-report last; no read-back partition.
- **Runs:** the correct build (expected: enforced state matches the grant,
  0 findings), then each seeded fault class of the QEMU campaign that
  exists on CIX P1 hardware (expected: each reported as its class), then
  the owner's held-out cases, sealed by hash before any scoring (charter
  section 8, independence). The board grant is the platform's (D3 and D5
  records), not QEMU's.
- **Platform cross-check:** the vendor IORT and device tree are inputs to
  compare, never ground truth (charter section 7); read-back's SMMU view is
  compared with both.
- **Pass:** 0 findings on the correct build; every seeded fault detected;
  every miss recorded with its reason. A miss is a result, not a failure of
  the plan.

## D. Cost and interference

- **Interference:** `../../stage2/20261102-interference/PLAN.md` unchanged
  (victim on an A720 and an A520 core; aggressor none, CPU loop, memory
  stream; countermeasure none or HAVEN budgets; at least 10 runs per cell,
  10^5 samples each; p50, p99, p99.9, max).
- **Comparison metrics** [S], with the method of Martins and Pinto,
  "Shedding Light on Static Partitioning Hypervisors for Arm-based
  Mixed-Criticality Systems" (arXiv 2303.11186; the charter cites RTAS 2023,
  venue to confirm from the proceedings), so the numbers can be read next to
  their figures for Bao, Jailhouse and Xen:
  - interrupt latency: a periodic Generic Timer interrupt every 10 ms, cold
    caches per sample (instruction cache flushed, data cache primed with
    unrelated data), with and without interference;
  - IPI latency: from the source vCPU's write to the (virtual) distributor
    to the handler;
  - boot time: per stage (firmware, UEFI, `haven-boot.efi`, HAVEN, first
    output of each partition) for a small real-time image and the Linux
    image;
  - code size: SLoC by `cloc` and `.text` of `haven.elf`, by feature group
    (charter section 14: the Linux guest support as its own item).
- **Baseline** [P]: the same victim image on bare metal (no HAVEN) on the
  same core, for interrupt latency and the MiBench automotive programs
  (qsort, susan, bitcount, basicmath) that the paper uses; the overhead
  is HAVEN versus bare metal on this board, not versus the paper's board.
- [F] No QEMU number is reported as a cost of HAVEN.

## E. MPAM (characterisation)

Only if the firmware exposes MPAM to EL2 (to be checked by `haven-probe`
and the vendor tables in A): HAVEN sets cache-portion and bandwidth
partitions per partition; read-back reads the MPAM registers back and
compares them with the grant; part D's interference cells are repeated with
MPAM on. Not claimed: guarantees. If MPAM is not usable, part E is recorded
as not done and why; contribution 3 is narrowed, as charter section 10
says.

## Data and records

Each board session is a record under `docs/methodology/stage3/` with the
commit, the lock file, the firmware state of precondition 2, the raw UART
and probe logs (sanitised as `../../LOG-SANITIZING.md` says) and the
analysis script's output; the analysis scripts are committed before the
session they analyse.

## Threats to validity

Firmware frequency scaling and thermal state; EL3 activity on the measured
cores (ledger rows 2 and 14); measurement overhead of the probes; one board
of one SoC (charter section 6: a portability test needs the other boards);
J21 unusable (then C runs through the EL3 service only, and says so).

## For the owner before freezing [?]

1. Is the bare-metal baseline in D wanted, or are HAVEN's own numbers next
   to the paper's enough?
2. Are 10 runs of 10^5 samples per cell (PLAN.md) affordable in board
   time, or should the matrix shrink (for example A520 placement later)?
3. Which held-out cases does the owner write for C, and when are they
   sealed?
4. Is UART-only bring-up acceptable if J21 stays unusable (G3 question 3)?
