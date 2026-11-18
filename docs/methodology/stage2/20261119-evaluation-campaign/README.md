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

## Owner's answers (2026-11-19)

The owner approved the design and its three questions as proposed: about
30 written, 60 random and at least 8 held-out cases; the held-out cases
written with `HELDOUT-GUIDE.md` when step C3 comes; M7 (EL3 read-back) in
the campaign.

## Step C1: the tools, design and expectations (before code)

`tools/campaign/` (Python standard library, like `tools/readback/`):

- **`taxonomy.json`**: the classes of design item 1, each with an id
  (for example `S2-EXTENT`), its enforcement point and the files where it
  lives; frozen by hash with the case lists in C2.
- **`mutate.py`**: `heldout_random.py`'s enumeration and draw, moved out
  of the v3 record and freed from the Mac: the same five operators
  (numeric constant +1 or one page; comparison swap; `&&`/`||` swap;
  deletion of a call-only statement; `|`/`&` swap), the same fixed order,
  the same shuffle by the SHA-256 of a seed text; the build check uses the
  toolchain on `PATH` (CI's `aarch64-linux-gnu-`), not a Mac path. The
  class of a drawn mutant is recorded after the draw by reading its diff.
- **`run_case.py`**: one case: a patch applied to a clean copy of the
  repository at the frozen commit, HAVEN built, methods M1 to M7 run,
  one JSON file with each method's verdict and its evidence lines.
- **`score.py`**: case files and expectations into the per-class tables of
  design item 5, with exact (Clopper-Pearson) intervals.
- **`.github/workflows/campaign.yml`**: on demand; cases sharded across
  jobs; summary as the run's summary. The owner's held-out archive
  reaches it as a repository secret (base64; GitHub masks secrets in
  logs), checked against the SHA-256 committed in C3 before any case runs;
  its patches are never printed until the results record.

| Row | Check | Expected |
| - | - | - |
| K1 | `mutate.py` at commit `e47b722` with v3's seed text | 1,062 mutants; the first five of the shuffled order are R1 to R4 (indices 267, 781, 617, 43) and the one mutant v3 skipped, as `heldout-random/draw.json` records |
| K2 | unit tests | each operator on a small C sample gives the expected mutants; two draws with one seed are identical; comments and non-`#define` preprocessor lines are never mutated |
| K3 | `run_case.py` on the unchanged tree (no patch), in CI | every method: no finding and pass (the baseline is clean) |
| K4 | `run_case.py` on v3's case M5 (partition B RAM 2 MiB larger) with today's tools | M6 reports extent drift on B RAM; M1 and M2 pass, as v3 found |
| K5 | `score.py` unit tests | a synthetic set gives the expected table; 10 of 10 gives a lower bound of 0.692, 20 of 20 0.832 |
| K6 | a 10-case dry run of the workflow | completes; its time extrapolated to the campaign's size is under 45 minutes |
| K7 | the secret channel, with a test archive (not the owner's cases) | a wrong hash stops the job before any case; the right hash runs it; no patch text appears in the log |

## Results of step C1, first run (2026-11-19)

The tools merged in #149 (commit `5b6ff79`); `campaign.yml` ran the cases
file `tools/campaign/cases/k3-k4.json` on demand, two shards on
`ubuntu-24.04` (Ubuntu's QEMU), and scored them into the run's summary.
**[F]** fact from a log or a rerun, **[I]** inference.

| Row | Result | Evidence |
| - | - | - |
| K1 | holds | the quality-gate job of #149: `test_k1_reproduces_case_set_v3 ... ok` (commit `e47b722` fetched, not skipped) [F] |
| K2 | holds | the host-test jobs (Ubuntu, macOS, Windows) of #149 [F] |
| K5 | holds | the same jobs: 10 of 10 gives 0.692, 20 of 20 0.832, 5 of 10 [0.1871, 0.8129] [F] |
| K3 | **fails** | M1 to M4 pass, but **M6 reports a finding on the unchanged tree** [F]. Not a false finding of read-back: a defect of the campaign's runner (below) |
| K4 | **fails, two ways** | M2, M3 and M4 pass [F]. **M1 has a finding**: `test_partition_layout` fails at `tests/unit/test_partition_layout.c:127` (the map's IPA and PA), reproduced on a clean copy with the patch [F]. M6's finding is not valid evidence, for K3's reason [I] |
| K6 | not run | waits for the runner fix |
| K7 | not run | a later step |

**Cause of K3 (and of K4's M6 verdict)** [I, to be confirmed by the
rerun]: `campaign.yml` installs no `dtc`. After reading the memory,
`readback.py` checks DMA masters against QEMU's own device tree
(`device_tree()`, called before the result is written), which runs `dtc`
on QEMU's `dumpdtb` output; without `dtc` Python stops with an error, exit
code 1, and `run_case.py` reads every exit code 1 as a finding. What
points there: the other QEMU workflows install `device-tree-compiler`
(`try-release.yml`) or `dtc` (`r1-demo.yml`) and this one does not [F];
`readback.py`'s usage says it needs `dtc` on `PATH` [F]. The job keeps the
read-back output in files (by design), and those files were not read for
this record.

**Cause of K4's M1 finding** [F]: the expectation copied v3's result
without checking today's tests. `test_partition_layout` came after v3
(commit `3473290`, 2026-11-11; v3 ran at `e47b722`, 2026-11-02) and checks
partition B's layout, so today's unit tests catch case M5. This is a
prediction error on the tree's state, not a defect: the baseline got
stronger, and M5 is no longer a case only read-back detects.

**Two defects of `run_case.py` found** [F]:

1. An exit code 1 without a `readback.json` that lists findings counts as
   a finding. It must count as an error (the method could not judge),
   with the log's last lines as evidence.
2. M1's evidence is only ctest's exit code; it must name the failing
   tests.

**Next**: the fix (`dtc` in the workflow; defects 1 and 2), then K3, K4
(with M1's expectation corrected to a finding, `test_partition_layout`)
and K6 run again. Nothing else in the rows changes.
