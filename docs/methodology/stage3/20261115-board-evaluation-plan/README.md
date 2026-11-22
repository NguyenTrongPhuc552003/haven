# Board evaluation plan (Orange Pi 6 Plus, CIX P1)

**Status:** **FROZEN 2026-11-23** (owner: "Approve", 2026-11-23, freeze
the plan with P9 and P10 before the board arrives; the owner's answers to
the open questions are in Amendment 3). Drafted 2026-11-15 by Claude
(owner-approved item: "a frozen board evaluation plan before the G3
gate"). It was to be frozen at G3; the owner froze it earlier. From the
freeze on, metrics,
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

## Amendment 1 (2026-11-19, before freezing; owner's question on a board without J21)

Source: `../../../porting/cix-p1-ledger.md`, "Without J21" (2026-11-19).

- [F] The fitted 10-pin header J135 carries UARTs only (UART2 console,
  UART4 power management, UART5 and UART6 security engine, 3.3 V); JTAG
  reaches only the unfitted J21. Bring-up (parts A and B) and part D need
  only UART; J135 and UART3 on the 40-pin header are their ports.
- [I] The EL3 fallback of part C is harder on this board than in QEMU: the
  EL3 firmware ships as signed binaries, no TF-A source for the SoC is
  known, and using it means a SPI NOR write (the owner's decision).
- **Part C, first step** [P]: before any read-back run on the board, the
  staged debugger test through a solder-free pogo-pin adapter on J21
  (debug port ID; access ports and ROM table; a core's authentication
  status and a halt). Only if the halt works is a header soldered.
- **Part C if neither JTAG nor EL3 is usable** [P]: the board runs the
  behaviour probes (always) and HAVEN's self-report, reported as such and
  never as independent read-back; independent read-back stays a QEMU
  result, and the thesis claim is narrowed accordingly (charter section
  10). Parts A, B, D and E are unchanged.
- Added to the owner's questions: 5. buy a pogo-pin 2x5 adapter and a
  1.8 V-capable probe with the board?

## Amendment 2 (2026-11-22, before freezing; from the QEMU campaign's results)

Source: `../../stage2/20261119-evaluation-campaign/RESULTS.md` and the
records it cites. The owner approved adding the campaign's open
questions to the board plan (2026-11-22).

**Faults QEMU makes invisible, to seed on the board in part C** [P].
Each is a fault the campaign could not judge because QEMU does not model
the effect; each runs on the board as a seeded case with the prediction
written before the run:

| Case | Fault (campaign record) | Why QEMU cannot show it [I] | Board prediction [P] |
| - | - | - | - |
| B-H3 | the list registers neither bounded by `ICH_VTR_EL2` nor cleared per CPU (historical H3, revert of `4ccee17`) | QEMU resets them to 0; the architecture leaves them UNKNOWN | stale virtual interrupts or an UNDEFINED access beyond the implemented count: a partition-visible fault (behaviour probes), not a read-back finding (read-back does not read list registers) |
| B-H5 | redistributor frames by fixed stride instead of `GICR_TYPER` affinity (historical H5, revert of `93cf892`) | QEMU's frames sit at the stride the revert assumes | on GIC-700 with 256 KiB frames or a boot CPU that is not core 0, PPIs programmed in the wrong CPU's frame: read-back finds PPI binding drift |
| B-R28 | no `dsb ish` before the stage-2 TLB flush (random R28) | TCG does not reorder those accesses | rarely visible; a stress probe after remapping, recorded as "not observed" unless it is |

**Limits from the campaign that the board plan now states** [F]:

- Read-back is a snapshot: an on-request CPU that is not running is
  "waiting", so a vCPU HAVEN failed to start is visible only to the
  guest (scenario 2: R4, R7, R53). Part C's behaviour probes are the
  check for that class on the board.
- The DMA VMID (W21) and budgets (W34) are outside what read-back reads.
- The held-out cases of part C: the owner left C3 to Claude, so the
  campaign used historical faults chosen by a rule instead
  (`.../c3/README.md`); the board run reuses that rule's cases that
  exist on CIX P1, and the sealed channel stays available for cases
  Claude has not seen.
- M7 (EL3 read-back) on the board needs the board's BL31 rebuilt and
  flashed (Amendment 1); in QEMU its agreement with M6 is measured in
  `.../m7/` (E4).

## Amendment 3 (2026-11-23, the freeze)

**The owner's answers** (2026-11-23) to "For the owner before freezing":

1. The bare-metal baseline of part D is **included** (the same victim
   image without HAVEN, on the same core).
2. The interference matrix is **kept as written**: 10 runs of 10^5
   samples per cell, the victim on an A720 and an A520 core.
3. Held-out cases: as Amendment 2 records (the campaign's historical-fault
   rule, the sealed channel kept for cases Claude has not seen).
4. Bring-up and evaluation over **UART only is acceptable** if J21 stays
   unusable; part C is then limited as Amendment 1 says.
5. A solder-free pogo-pin 2x5 adapter and a 1.8 V-capable probe are
   **bought with the board**, so part C's first step can run on day one.

**Added before the freeze** [F unless marked]:

- **Preconditions, P9 and P10** (`docs/porting/cix-p1-loading.md`'s
  probes extended; G3 day-one checks): P9, partition B's timer interrupts
  on its core under the vendor TF-A; P10, whether HAVEN's boot-core
  writes to a powered-down core's redistributor are accepted on the
  GIC-700. Source: `../../stage2/20261122-firmware-redistributor/`. In
  QEMU under TF-A partition B received no timer interrupt until HAVEN's
  started CPUs replayed their redistributor state (#202, #206); a CI job
  checks B under TF-A on every pull request.
- **A3, partition B on its cores**: B runs on one core, logical 8 (core
  0x800, A720), 64 MiB at PA 0xE000_0000 (to confirm with P1), as its
  own record says (`../../stage2/20261123-cix-p1-partition-b/`); the board
  grant is `grant-cixp1-v1.json`. A3 passes when B's status record shows
  stage 5 and at least 8 timer interrupts, read through the channel part
  C uses, or through HAVEN's console if none is usable yet.
- **Part C, input path**: read-back v17 reads a dump captured on the
  board with `--platform cix-p1 --dump FILE`, with the redistributor
  frames found by `GICR_TYPER` affinity
  (`../../stage2/20261123-readback-board-path/`). Producing that dump on
  the board is still the debugger or the EL3 service of Amendment 1.
  Case B-H5 (frames by stride) is reported by read-back on a dump that
  holds the frames.
- **What M7 showed in QEMU** (for the part C predictions): with read-back
  v16, the EL3 channel agrees with the monitor channel on 102 of 106
  cases; the four left are a CPU's own EL2 registers (W09, R42), which a
  dump made from another CPU does not hold, and group faults TF-A
  prevents (W27, R19) (`../../stage2/20261119-evaluation-campaign/`,
  `RESULTS.md`, E4).
- **Builds**: `ecosystem.lock.json` pins KDAL at `583b329` (#210); the
  kernel module's source is unchanged from the former pin.
