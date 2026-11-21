# Campaign step C2, the random cases: design and expectations

**Status:** design and expectations, 2026-11-21, written before the
draw. The random cases are part of step C2 as the owner approved it on
2026-11-19 (about 60; `../../README.md`, design item 2); C2 ran only the
written cases, a gap found on 2026-11-21 (`../../c3/README.md`).

## What is drawn [P]

- **Generator.** `tools/campaign/mutate.py` as merged (row K1 holds): all
  single-point mutants of its default files (the 11 enforcement files of
  `taxonomy.json`), five operators, shuffled by the seed, each kept only
  if the host tests and the qemu-direct image build; skipped ones are
  counted in `draw.json`.
- **Count.** 60 kept mutants.
- **Seed text, fixed by a rule Claude cannot tune:**
  `haven <H> | campaign C2 random, 60 cases`, where `<H>` is the full
  hash of the merge commit of the pull request that adds this record.
  Nobody knows `<H>` before the merge, so the draw cannot be chosen by
  trying seeds. The draw runs at `<H>`.
- **Where.** A new on-demand workflow, `campaign-draw.yml` (Debian
  trixie container, as `campaign.yml`), runs `mutate.py REPO draw SEED
  60` and uploads `draw/` (patches and `draw.json`). Nothing on the Mac.
- **After the draw, before any run:** Claude names each mutant's class
  from its diff (`taxonomy.json`; `CONTROL` is never used for a random
  case; a mutant outside every class is named `OUTSIDE` and counted
  apart), writes `tools/campaign/cases/c2-random.json` (source `random`,
  no expectations: the cases are blind), and freezes the patches, the
  cases file and the tools as `c2/freeze/SHA256SUMS-C2R`.
- **The run.** `campaign.yml` on `c2-random.json`, eight shards.

## Expectations

| Row | Check | Expected |
| - | - | - |
| D1 | the draw | `draw.json` names the seed text with `<H>`, 60 kept mutants and the skipped ones; the mutant total is near 1,062 (v3's count, K1) if the 11 files changed little since `e47b722` |
| D2 | the draw again with the same seed (same workflow) | the same 60 mutants, the same order (K2's claim, at the scale of the campaign) |
| D3 | classes, before the run | every mutant has a class or `OUTSIDE`; the table of classes is in this record before the run |
| D4 | the run | every case gives a verdict per method; a method's `error` is counted apart from pass and finding |
| D5 | candidates for equivalence: random cases where every method passes | listed; each checked by reading its diff (all if 10 or fewer, else 10 drawn with the same seed), recorded as equivalent (counted apart, design item 2) or a miss (with a named cause, E5) |

No per-case verdict is predicted (blind cases). What the run measures is
the per-class detection rate of each method over mutants Claude did not
write. v3's four random cases (`../../../20261102-a1-cases-v3/RESULTS.md`)
[F]: R4 found by read-back and the boot check, R1 by a unit test alone,
R2 by no method (a priority: the I7 blind spot, closed since by v7), and
R3 crashed read-back. Four cases are too few to predict a rate [I].

## Threats to validity [I]

- Claude names the classes and judges equivalence; Claude also wrote
  read-back. The diffs and the verdicts are committed, so the owner or a
  reviewer can re-check the classes and the equivalence calls; D5's
  sample is drawn, not chosen.
- Single-point mutants are not the faults a developer makes (they are
  small and syntactic); the written and held-out sets cover the other
  kind. The three sets are reported apart (design item 5).
