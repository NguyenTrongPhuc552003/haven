# The core evaluation campaign in QEMU: design (step C0)

**Status:** owner-approved item (2026-11-13: "the core evaluation campaign
in QEMU: seeded faults across stage-2, SMMU and GIC, every detection method
compared, an owner-written held-out set"). This is the design, written
before any case, tool change or run. Labels: **[F]** fact from our records,
**[I]** inference, **[P]** proposal, **[?]** for the owner.

## Why another case set

- [F] Case set v3 (`../20261102-a1-cases-v3/`): 11 cases written by
  Claude, 4 drawn at random, three methods (read-back, host unit tests,
  QEMU boot check). Its own threats section: "Claude wrote the cases, the
  tool and the runner; the owner ... did not choose the faults", and
  "four is a small sample".
- [F] The Linux chain set (`../20261106-linux-chain-cases/`) added the
  demand check and the Linux boot; the dt_check blind test
  (`../20261107-dt-check-blind/`) the structural check. Each set ran its
  own tool versions; no table compares every method on the same faults.
- [F] Read-back changed since v3 (now v14: priorities, CPU assignment,
  HAVEN's EL2 map and timer, vCPU tables), so v3's blind spots (I7, P1)
  may no longer hold.
- [I] So the evidence for the charter's hypothesis (section 3: at least one
  independently justified, non-structural error class that the strong
  baseline misses, with no accepted unauthorised operation and no false
  rejection of valid controls) is scattered, small and mostly
  author-chosen. The campaign gathers it into one frozen, scored
  comparison.

## Design

### 1. A frozen fault taxonomy (step C1)

Classes by enforcement point and what goes wrong, each with the files
`../20261102-a1-cases-v3/HELDOUT-GUIDE.md` lists:

| Point | Classes |
| - | - |
| Stage 2 | extent; permission (read, write, execute); memory type and shareability; owner (association); missing binding; extra binding |
| SMMU | stream not bound; window extent; window permission; stage or VMID tag |
| GIC | routing target; enable; group; priority; PPI ownership per CPU |
| CPU and EL2 | CPU assignment; HAVEN's own EL2 map; HAVEN's timer and budget |
| Controls | semantically equivalent changes (no enforced state changes) |

Shapes (the same guide): wrong constant or bit, size off by a page or a
block, skipped check, missing call, two steps reordered, right value to
the wrong target.

### 2. Three sources of cases (steps C2 and C3)

| Source | Who | How many [P] | Expectations |
| - | - | - | - |
| Written | Claude | at least one per class, about 30 | written per case and approved by the owner before the run, as in v3 |
| Random | a seeded generator (`heldout_random.py`'s successor): a shape applied at a random enforcement site; mutants that do not build are dropped and counted | about 60 | none per case (blind); only the class, recorded after the draw by reading the diff |
| Held out | the owner, without Claude seeing them | at least 8 | none; sealed (below) |

**Sealing the owner's cases.** The owner writes each case as a patch,
puts them in one archive, and commits only its SHA-256 (and the count)
before the run; the archive reaches the runner as a private input (not a
public file), and its patches enter the record after scoring. The
campaign's tools, runner, grant and case lists are hashed and committed
before the run too (v3's `freeze/` method).

**Equivalent mutants.** A random mutant that changes nothing any method
can observe may be equivalent (mutation-testing's oracle problem). Such
cases are listed and counted apart, never in a rate's denominator; a
sample of them is checked by reading the diff.

### 3. Every method on every case (step C4)

| Method | What it checks | Tool |
| - | - | - |
| M1 | host unit tests | the CI host-test job |
| M2 | QEMU boot checks (partition B, direct boot) | `scripts/qemu/*-check.*` |
| M3 | structural configuration check of the device tree and the grant | `tools/readback/dt_check.py`, grant schema |
| M4 | the partitions' own checks (guest test results) | UART logs |
| M5 | demand check (Linux cases) | `tools/readback/demand_check.py` |
| M6 | read-back through the QEMU monitor | `tools/readback/readback.py` |
| M7 | read-back through the EL3 dump (TF-A profile), on the cases it applies to | `tools/el3-readback/` |

M1 to M5 are the baselines (charter section 8: current policy tests and
structural validation); M6 and M7 the method. Every method sees the same
build and the same grant.

### 4. Where it runs [P]

In CI, on demand: one workflow, cases sharded across jobs like the stress
workflow (`../20261114-console-drain/`), each case one HAVEN build and its
QEMU runs on two CPU models (cortex-a72, max); results as JSON per case
and one summary table. Nothing runs on the Mac.

### 5. Metrics

Per class and method: detection rate with its exact 95 % interval
(Clopper-Pearson); the cases only read-back detects; false findings on
controls (each method); oracle disagreement (a method contradicting the
approved expectation); read-back's run time. Written, random and held-out
cases are reported apart and together.

## Expectations of the campaign (fixed now)

| Row | Expected |
| - | - |
| E1 | every control: no finding from any method (no false rejection) |
| E2 | at least one class that M1 to M5 miss in every case and M6 detects in most (the hypothesis); which class is not predicted here |
| E3 | v3's blind spots I7 (priority) and P1 (BAR placement): I7 now detected by M6 (v7 reads priorities); P1 still missed by every method |
| E4 | M6 and M7 agree on every case M7 applies to, except where the Non-secure view cannot see groups (the EL3 record's known difference) |
| E5 | every miss of M6 has a named cause (blind spot, timing, or defect), recorded before anything changes |

## Steps

| Step | What | Gate |
| - | - | - |
| C0 | this design | owner's yes |
| C1 | taxonomy file, generator, runner workflow; their own expectations | merged before C2 |
| C2 | written and random cases, frozen with their hashes; owner approves the written expectations | owner's yes |
| C3 | owner's held-out archive, its hash committed | the owner |
| C4 | the run in CI | after C2 and C3 |
| C5 | results and threats to validity | |

## For the owner [?]

1. Are about 30 written, 60 random and at least 8 held-out cases the right
   size? With 10 cases of a class all detected, the rate's lower 95 %
   bound is 69 %; with 20, 83 %.
2. When can the held-out cases be written? `HELDOUT-GUIDE.md` is the map;
   about 5 minutes per case.
3. Should M7 (EL3 read-back) be in the campaign, given that on the board
   it is uncertain (`../../../porting/cix-p1-ledger.md`, "Without J21")?
