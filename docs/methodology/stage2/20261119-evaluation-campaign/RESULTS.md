# Evaluation campaign: results and threats to validity (step C5)

**Status:** results of step C4 (2026-11-22) read against the campaign's
fixed expectations E1 to E5 (`README.md`). Every number below comes
from the two C4 runs at `49fe767` under `c4/freeze/SHA256SUMS-C4`
unless a row says otherwise. Labels: **[F]** fact from a run or the code,
**[I]** inference, **[P]** proposal.

## What ran [F]

| Set | Who chose the faults | Cases | Expectations | Scenario |
| - | - | - | - | - |
| Written (C2) | Claude, one or more per class, approved by the owner | 40 (35 faults, 5 controls) | per case and method, before the run | 1 |
| Random (C2) | `mutate.py`, seed fixed by a merge commit | 60 | none (blind); classes named before the run | 1 and 2 (the 40 that scenario 1 never reached) |
| Historical (C3) | HAVEN's own fixes, reverted, by a rule | 6 (one does not build, one control) | none | 1 |

Scenario 1: one boot of the bare test guests, both CPU models
(cortex-a72, max), QEMU 10.0.13. Scenario 2: Linux in partition A with
two vCPUs and CPU hotplug, cortex-a72, the pinned QEMU 8.2.2 (no SMMU
stage 2, so the DMA grant is withheld and SMMU classes are not compared
there). Methods: M1 host unit tests, M2 boot checks, M3 the grant's
structure, M4 partition B's own check (scenario 1 only), M5 KDAL's
demand (scenario 2 only), M6 read-back (v15). M7 (EL3 read-back under
TF-A) was wired after C4 and ran on the 106 scenario-1 cases
(`m7/README.md`); its results are in E4 below.

## Headline results [F]

- **No false finding on any control from any method**: 0 of 9 control
  runs (5 written, 1 historical, 3 in scenario 2).
- **Every verdict is reproducible**: the 149 case runs of C4 give the
  same 1,043 method verdicts as their first runs.
- **Read-back finds what nothing else does**: 21 faults in scenario 1
  are found by M6 alone (13 written, 8 random), and every one of them is
  below HAVEN's own tables or in state no test inspects (`mm.c`,
  `gic_v3.c`, `budget_hw.c`, the SMMU driver, the EL2 map, DMA windows,
  BAR placement, one interrupt group, priorities) [I, from the diffs].
- **Read-back is a snapshot**: it misses faults whose effect is not in
  the enforced state when it reads (a CPU that is off or never started,
  HAVEN halting before stage-2 setup, code a scenario never runs).

| Set (scenario) | Faults | Found by any method | by M6 | by M1 to M5 | M6 only | M1 to M5 only | M6 error |
| - | - | - | - | - | - | - | - |
| Written (1) | 35 | 32 | 30 | 19 | 13 | 2 (W08, S9) | 1 (W08) |
| Random (1) | 60 | 20 | 14 | 12 | 8 | 6 | 4 |
| Historical (1) | 5 (H1 does not build) | 0 that build | 0 | 0 | 0 | 0 | 1 (H1) |
| Random (2) | 40 | 5 | 2 | 5 | 0 | 3 | 0 |

Missed by every method in scenario 1: W21 (DMA VMID), W34 (budget) and
v3's P1 (BAR placement), all three predicted; 40 random cases; 4
historical. "Found" means a method reported a finding on the case, not
that the finding names the fault correctly (see "Construct validity").

## Per class, scenario 1, all 106 cases (`score.py`) [F]

Detection rate with its exact 95 % interval (Clopper-Pearson). A
method's `error` verdicts are left out of its denominator; M4's column
counts partition B's check, M5 does not apply in scenario 1.

| Class | M1 | M2 | M3 | M4 | M5 | M6 | M7 |
| - | - | - | - | - | - | - | - |
| CPU-ASSIGN | 0/11 [0.000, 0.285] | 1/11 [0.002, 0.413] | 0/11 [0.000, 0.285] | 3/11 [0.060, 0.610] | - | 3/11 [0.060, 0.610] | - |
| EL2-MAP | 1/3 [0.008, 0.906] | 1/3 [0.008, 0.906] | 0/3 [0.000, 0.708] | 1/3 [0.008, 0.906] | - | 1/2 [0.013, 0.987] | - |
| EL2-TIMER | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | - | 1/2 [0.013, 0.987] | - |
| GIC-ENABLE | 0/8 [0.000, 0.369] | 0/8 [0.000, 0.369] | 0/8 [0.000, 0.369] | 0/8 [0.000, 0.369] | - | 1/8 [0.003, 0.527] | - |
| GIC-GROUP | 0/5 [0.000, 0.522] | 1/5 [0.005, 0.716] | 0/5 [0.000, 0.522] | 2/5 [0.053, 0.853] | - | 3/4 [0.194, 0.994] | - |
| GIC-PPI | 1/4 [0.006, 0.806] | 0/4 [0.000, 0.602] | 0/4 [0.000, 0.602] | 2/4 [0.068, 0.932] | - | 2/4 [0.068, 0.932] | - |
| GIC-PRIO | 0/3 [0.000, 0.708] | 0/3 [0.000, 0.708] | 0/3 [0.000, 0.708] | 0/3 [0.000, 0.708] | - | 2/3 [0.094, 0.992] | - |
| GIC-ROUTE | 3/9 [0.075, 0.701] | 3/9 [0.075, 0.701] | 0/9 [0.000, 0.336] | 3/9 [0.075, 0.701] | - | 5/9 [0.212, 0.863] | - |
| OUTSIDE | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | 0/5 [0.000, 0.522] | - | 0/5 [0.000, 0.522] | - |
| S2-EXTENT | 4/10 [0.122, 0.738] | 3/10 [0.067, 0.652] | 0/10 [0.000, 0.308] | 2/10 [0.025, 0.556] | - | 7/9 [0.400, 0.972] | - |
| S2-EXTRA | 1/6 [0.004, 0.641] | 0/6 [0.000, 0.459] | 0/6 [0.000, 0.459] | 0/6 [0.000, 0.459] | - | 1/6 [0.004, 0.641] | - |
| S2-MISSING | 3/5 [0.147, 0.947] | 2/5 [0.053, 0.853] | 0/5 [0.000, 0.522] | 2/5 [0.053, 0.853] | - | 3/4 [0.194, 0.994] | - |
| S2-OWNER | 1/1 [0.025, 1.000] | 1/1 [0.025, 1.000] | 0/1 [0.000, 0.975] | 1/1 [0.025, 1.000] | - | - | - |
| S2-PERM | 2/4 [0.068, 0.932] | 1/4 [0.006, 0.806] | 0/4 [0.000, 0.602] | 1/4 [0.006, 0.806] | - | 4/4 [0.398, 1.000] | - |
| S2-TYPE | 2/3 [0.094, 0.992] | 0/3 [0.000, 0.708] | 0/3 [0.000, 0.708] | 0/3 [0.000, 0.708] | - | 3/3 [0.292, 1.000] | - |
| SMMU-EXTENT | 1/2 [0.013, 0.987] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | - | 2/2 [0.158, 1.000] | - |
| SMMU-PERM | 1/2 [0.013, 0.987] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | 0/2 [0.000, 0.842] | - | 2/2 [0.158, 1.000] | - |
| SMMU-TAG | 1/7 [0.004, 0.579] | 0/7 [0.000, 0.410] | 0/7 [0.000, 0.410] | 0/7 [0.000, 0.410] | - | 0/7 [0.000, 0.410] | - |
| SMMU-UNBOUND | 1/10 [0.003, 0.445] | 1/10 [0.003, 0.445] | 0/10 [0.000, 0.308] | 1/10 [0.003, 0.445] | - | 4/9 [0.137, 0.788] | - |
| false findings on controls | 0/6 | 0/6 | 0/6 | 0/6 | 0/0 | 0/6 | 0/0 |

`score.py` counts a failed build as a finding: H1, which does not build,
is in GIC-GROUP's M2 and M4 cells; without it GIC-GROUP's M2 is 0 of 4
and M4 1 of 4. OUTSIDE holds the 5 random mutants outside every class
(mostly log lines); CONTROL is the last row.

Scenario 2's 43 cases (`score.py` on run 44989942726): the 3 controls
pass every method; of the 40 random cases, M2 finds 5 (R4, R7, R39,
R40, R53), M6 finds 2 (R39, R40), none is found by M6 alone
(`s2/README.md`).

R40 was later repeated (`../20261124-r40-repeat/`): the same code
found and passed it, so it is a timing-dependent case and its verdict is
counted neither way. Of the other 39 random cases, M2 finds 4 (R4, R7,
R39, R53) and M6 finds 1 (R39).

## The expectations E1 to E5

| Row | Expected (fixed in C0) | Result |
| - | - | - |
| E1 | every control: no finding from any method | **holds** [F]: 0 of 9 control runs, every method |
| E2 | at least one class that M1 to M5 miss in every case and M6 detects in most | **holds, on few cases** [F]: GIC-PRIO, M1 to M4 0 of 3, M6 2 of 3 (interval 0.09 to 0.99); EL2-TIMER (M6 1 of 2) and GIC-ENABLE (M6 1 of 8) are not "most". With 3 cases the class-level claim is weak; the case-level claim (21 faults only M6 finds) is the stronger result [I] |
| E3 | I7 detected by M6 (v7 reads priorities); P1 still missed by every method | **holds** [F]: I7 is found by M6 alone; P1 by none |
| E4 | M6 and M7 agree wherever M7 applies | **holds with named exceptions** (2026-11-22, `m7/`) [F]: M7 now runs on all 106 scenario-1 cases; with the unchanged tree's TF-A finding as baseline (the prototype's E0), the two channels agree on 99; four differences come from what each channel can see (per-CPU VTTBR_EL2: W09, R42; groups under TF-A: W27, R19); three were open and are explained in `m7/` (W14, R60: read-back's EL3 path has no DMA-master list; W30: under TF-A the drift does not happen). The baseline finding itself was a HAVEN defect under firmware (TF-A resets CPU 2's redistributor at `CPU_ON`), fixed in #202; rerun without a baseline: 100 of 106 agree, W30 included (`20261122-firmware-redistributor/`, F5); with read-back v16, which checks granted streams through their own entries, **102 of 106**: only W09, R42 (a CPU's EL2 registers, not in the dump) and W27, R19 (cannot happen under TF-A) differ (`20261123-readback-v16/`) |
| E5 | every M6 miss has a named cause, recorded before anything changes | **holds for the cases read, not for all** [F]: every written, historical and scenario-2 miss is named (`c2/RESULTS-C2.md`, `c3/README.md`, `s2/README.md`), and 12 random ones (`c2/random/README.md`); about 30 random cases that pass every method in scenario 1 and 35 in scenario 2 are not read one by one |

## Why read-back misses what it misses [I, from the named causes]

1. **No enforced state to read**: HAVEN halts before stage-2 setup (W08,
   R5, R9, R16, R27). M6 reports an error, not a pass, and the boot
   checks find the fault; read-back is not designed for a HAVEN that
   never ran.
2. **Not in the state at read time**: a fault that lives only between
   two writes, or on a CPU that is off or never started while the grant
   allows it to wait (R4, R7, R53 in scenario 2: an on-request CPU that
   is not running is "waiting", by design).
3. **Not exercised**: code or inputs the scenario never reaches (a second
   DMA stream, boundary stream IDs, more than 256 SMMU commands, a failed
   mapping, a function only unit tests call). No boot-level method can
   see these; scenario 2 reached 5 of the 40. Scenario 3 (a second grant,
   `s3/`, 2026-11-22) reached two more: H2 (found by M6 alone) and R41
   (found by every boot method); R45 and R46 need a stream ID beyond the
   table, which no valid grant holds, so only unit tests reach them.
4. **Not modelled by QEMU**: barriers, reset values of list registers,
   redistributor frame layout (R28, H3, H5). Equivalent on QEMU, maybe
   not on the board: questions for the board gate (G3).
5. **Outside what read-back reads**: the DMA VMID (W21), budgets (W34),
   BAR placement (P1), list-register state. Known blind spots, each
   predicted before its run.

## Threats to validity [I]

**Internal.** Claude wrote read-back, the written cases, their
expectations, the class of every random and historical case, and every
equivalence judgement. The held-out property of the C0 design is not
met: the historical faults were chosen by a rule, but Claude knew them.
Mitigations: a seed nobody could choose, the historical rule fixed
before extraction, classes and expectations committed before each run,
every diff and verdict in the repository for a reviewer. The sealed
channel (C3, K7) remains ready for cases Claude has not seen.

**Construct.** "Found" means any finding on the case. Read-back
sometimes names a consequence rather than the fault (R2, a permission
change, is reported as missing bindings; W07r's type fault also as an
EL2 overlap). Detection rates therefore measure "something is wrong
here", not "the report names the fault". Classes come from the diff,
not from the effect.

**External.** Everything ran on QEMU (TCG), two QEMU versions, two CPU
models, one machine type, one grant per scenario. TCG does not model
caches, reordering or the board's GIC and SMMU implementations; faults
QEMU makes equivalent may be real on the board (four named above).
Nothing here is a statement about the Orange Pi 6 Plus or the Radxa
Orion O6 until the board gate.

**Conclusion.** Per-class samples are small (1 to 11 faults); most
intervals span more than half of the range. The campaign supports
case-level statements (which faults which method finds, and why) far
better than class-level rates. Random cases are single-point mutants,
which are smaller and more syntactic than developers' faults; the
written and historical sets cover the other kind, and the three are
reported apart.

## What this answers, and what it does not [I]

Read-back's claim in this thesis is that reading the enforced hardware
state back and comparing it with the grant finds isolation faults that
the existing checks (unit tests, boot checks, the grant's structure, the
partitions' own checks, KDAL's demand) miss. The campaign supports it
for faults that leave wrong state in stage-2 tables, SMMU stream tables
and DMA windows, the GIC's distributor and redistributors, each CPU's
VTTBR_EL2 (v15) and HAVEN's EL2 map: 21 such faults are found by
read-back alone, with no false finding. It does not support it for
faults outside the state at the read point, and it says nothing yet
about EL3 read-back (M7) or real hardware.

## Next [P]

1. **M7 into the campaign** (E4): the TF-A boot profile in CI, then the
   written cases through it; or E4 stated as a limit.
2. **Board gate (G3)**: R28, H3, H5 and the QEMU-only equivalences as
   questions for the first board runs.
3. **A second grant** (two DMA streams, a second partition with SPIs):
   the "not exercised" causes H2, R41, R45 and R46. Done 2026-11-22
   (`s3/`): H2 and R41 found, R45 and R46 reachable only by unit tests.
4. The thesis chapter (Evaluation) from this record, in haven-thesis.
