# Read-back v19: HAVEN's interrupt ownership table against the grant (design)

**Status:** design and expectations, 2026-11-28, before any code.
Owner-approved ("Approve", 2026-11-28).

## Why

N3-W1 (`../20261128-readback-n3/`) took vCPU 2's timer PPI away and
read-back passed it: read-back v8 takes "did the guest enable it?" from
HAVEN's GIC model, and the model drops an enable of an interrupt HAVEN
does not think the partition owns (`src/core/irq/vgic.c`, `set_enable`).
The fault hides behind HAVEN's ownership table, the state it corrupts
[F]. Read-back already reads HAVEN's own state where the hardware cannot
say enough (the GIC model, v8; the vCPU table, v12; `cpu_partition`,
v10). The ownership table is the same kind of state.

## The rule [P]

Read-back reads HAVEN's three ownership arrays from the image
(`src/core/irq/ownership.c`, static, found by symbol like
`cpu_partition`):

- `ppi_owner_map[16][16]`: per CPU, the owner of PPIs 16 to 31;
- `irq_owner_map[1024]` and `irq_target_cpu_map[1024]`: per SPI, the
  owner and target CPU.

Against the grant:

| Case | Finding |
| - | - |
| a PPI granted to the partition on CPU c (`ppis` or `guest_enabled`) whose table entry is not that partition | "missing interrupt binding" (`CPU c PPI n: HAVEN's table holds partition X`) |
| a table entry giving a partition a PPI the grant does not give it on that CPU | "interrupt ownership drift" |
| a granted SPI whose table owner or target CPU differs from the grant | "missing interrupt binding" or "interrupt routing drift" |
| a table entry giving a partition an SPI the grant does not | "interrupt ownership drift" |

HAVEN's own interrupts (`haven_ppis`, `maintenance_ppi`, SPIs granted to
partition 0) are not demanded from the table. (Clarified after the
review below: a table entry giving one of them to a partition is
"interrupt ownership drift".)
The EL3 path (M7) dumps HAVEN's image as well, so the rule runs there
too. An image without the symbols (an older build) skips it with an
observation.

## Expectations

| Row | Check | Expected |
| - | - | - |
| V1 | read-back unit tests | a table that matches: no finding; each row of the table above gives its finding; a missing symbol: an observation, no finding; all earlier tests pass |
| V2 | the controls: scenario 1 (six), scenario 2 (three), `linux3` (three) | every method passes, M6 and (scenario 1) M7 included (no false finding from the new rule) |
| V3 | N3-W1 again | M6 finds it: `CPU 3 PPI 27` missing interrupt binding, HAVEN's table holding no owner |
| V4 | the campaign's case sets again with v19 (scenario 1, 106 cases; scenario 2 without S2-R38, 42) | every M6 verdict that changes from v18 is listed and explained; the frozen campaign's figures stay as recorded, v19's are reported beside them [?: no prediction of the count] |
| V5 | read-back's time | the M6 median rises by less than 1 s |

## Results (2026-11-28) [F]

Code on pull request #233 (branch `feat/readback-v19`); the runs are
at `d091e36` (the code, `f46ec81`, and a cases file). Campaign runs: 46130034849
(scenario 1's six controls), 46130037381 (scenario 2's three),
46130040417 (`linux3`, `n3.json`), 46130043081 (scenario 1, 103 cases)
and 46130046780 (scenario 2, 42 cases).

| Row | Result |
| - | - |
| V1 | 8 new tests (`test_ownership.py`): a matching table, a missing PPI route (N3-W1's), a PPI held for the wrong partition, a PPI the grant does not give, an SPI with the wrong owner, the wrong target, an extra SPI, a missing SPI, HAVEN's own interrupts left out, reading the arrays from an image, an image without them. All 91 read-back tests pass. **Holds** |
| V2 | all twelve controls pass every method that applies (M4 and M7 are n/a in scenario 2 and `linux3`, M5 in scenario 1): scenario 1's six with M6 and M7 (EL3), scenario 2's three, `linux3`'s three. Read-back prints HAVEN's table on every run that reaches it (scenario 1: `PPIs CPU 2: [27]; SPIs 33 -> partition 1, CPU 0`); not on the five scenario-1 cases where HAVEN halts before stage 2 (C2-R5, R9, R16, R27, W08). **Holds** |
| V3 | N3-W1: M6 now finds it, `CPU 3 PPI 27: missing interrupt binding (HAVEN's ownership table holds no owner, granted partition 1)`. **Holds** |
| V4 | Scenario 1 against the v17 run 45052417129 (at `25be49b`): 102 of 103 cases give the same verdict from every method. **C2-R41 changes: M6 and M7 from pass to finding.** R41 starts HAVEN's owner map with every interrupt owned by partition 1 (`irq_owner_map[i] = 1U`); v19 reports every SPI the grant does not give as "interrupt ownership drift". The campaign had R41 as found by M1 alone in scenario 1, and explained read-back's pass as "not exercised, not a read-back blind spot" (`../20261119-evaluation-campaign/c2/random/`, "R15 and R41"). For R41 that explanation does not hold: the GIC was as granted, but HAVEN's table was not, and read-back did not read it. The campaign record is revised with a dated note. Six other cases (R11, R13, R31, R37, W30; R32 through M7 only) also get v19 findings, but were findings already; most of these are a second item under a label the case already had (for example a second `CPU 2 PPI 27: missing interrupt binding`). Scenario 2 against the run before v19 (46122075748): 42 of 42 same verdicts; only S2-R39, a finding already, gains one evidence line. **Holds**: every change listed and explained |
| V5 | M6's median 0.8 s over 102 scenario-1 runs (v17's run: 1.0 s over 105). **Holds** |

The V4 comparison with the v17 run also spans the LR change (#227); none
of the 103 verdicts changed by it in scenario 1. Three scenario-1 cases
(C2-V3-P2, C2-R38, C3-H3) were left out because their patches no longer
apply after that change; their frozen verdicts stand. The frozen
campaign's figures stay as recorded; v19's are these.

## Independent review (2026-11-28) [F]

A read-only sub-agent that had not seen the work checked this record
against the case files, logs and code (the first such review; rule in
the workspace `CLAUDE.md`). It confirmed the counts and verdicts above
and found the following, now handled:

1. **SPIs granted to partition 0 and special INTIDs.** The design said
   HAVEN's own interrupts are "not checked"; the code reports a table
   entry giving one to a partition as drift (in R41: `SPI 106`), which is
   the intended behaviour, so the text now says so and a test pins it.
   The code read INTIDs 32 to 1023; 1020 to 1023 are special INTIDs, not
   SPIs, and are no longer read (a test pins that too). R41's drift items
   drop from 991 to 987 per log; no verdict depends on it, and the runs
   above are not repeated for it.
2. **The V2 example** came from the `linux3` grant, not from a log; it is
   replaced by scenario 1's table line, with the five cases that never
   print it.
3. **R41's history.** The campaign had answered "why read-back passes
   R41" (not exercised); this record called it open. Corrected here, and
   the campaign record has a dated revision.
4. **The V4 baseline.** V4 compares with v18, but the run compared is
   v17's (45052417129 at `25be49b`). The span `25be49b..f46ec81` holds
   v18, the LR change (#227), the cix-p1 loader (#220), the KDAL pin
   (#225) and the `linux3` tooling. No verdict changes except R41's. Some
   evidence does: C2-R25 and C2-R27 now also fail `test_boot_layout`
   (the loader's new test) in M1; C2-W12r's M1 and C3-H1's M2 and M4
   differ only in build-log lines; **C2-W32's M6 lost `CPU 2 PPI 27:
   missing interrupt binding`** while its M7 kept it. W32 runs partition
   B with `cpu_partition` saying A; HAVEN disables B's timer PPI when it
   first fires with the wrong owner, so whether the snapshot sees it
   still enabled depends on timing [I]; the verdict (CPU assignment
   drift) is unchanged.
5. **Untested path.** The "observation, no finding" path for an image
   without the table is not tested in `main`, and no run used such an
   image; only `read_ownership` returning None is tested.
6. **"Every method"** in V2 counts only the methods that apply; corrected
   above.

