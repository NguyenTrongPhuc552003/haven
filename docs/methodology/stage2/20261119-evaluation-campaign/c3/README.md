# Campaign step C3: the sealed channel and the owner's held-out cases

**Status:** design and expectations, 2026-11-21, written before any code.
Owner-approved as a step on 2026-11-21 (C3 after read-back v15).

The campaign design (`../README.md`, "Sealing the owner's cases") says:
the owner writes each held-out case as a patch, puts them in one archive
and commits only its SHA-256 and the count before the run; the archive
reaches the runner as a private input; its patches enter the record after
scoring. Row K7 tests that channel. This record fixes how it works.

## Facts that shape it [F]

- haven is a public repository: every job log can be read by anyone, and
  the workflow artifacts by any signed-in GitHub user.
- GitHub masks a secret's exact value in job logs, not text derived from
  it (the decoded archive is not masked). A secret holds at most 48 KB.
- `run_case.py` already prints only verdicts for `heldout` cases, but
  writes build, patch and UART logs to `OUT/CASE/`, and a build failure's
  log can quote source lines; `campaign.yml` uploads all of `out/`.
- The design gives held-out cases no expectations ("none; sealed"); they
  are scored per class like random cases.

## Design [P]

1. **Archive.** A gzip tar with `cases.json` (`haven-campaign-cases/1`,
   every case `source: heldout`, a class from `taxonomy.json`, `patch`
   naming a file in the archive) and the patches. The owner may add
   `expect` (the same form as the written cases'); scored if present.
2. **`tools/campaign/seal.py DIR`** (owner, on the Mac): reads `H*.patch`
   and `H*.json` (`{"class": ..., "expect": {...}}`, expect optional),
   checks the classes, writes `cases.json` and a byte-reproducible archive
   (sorted names, zero times, fixed owner), and prints only the case
   count, each case's id and class, and the archive's SHA-256; writes
   `DIR.b64`. `--build-check` applies each patch to an export of HEAD and
   builds the host tests, qemu-direct and qemu-psci, printing only
   `H1: builds` or `H1: does not build`.
3. **The owner** sets the repository secret `HELDOUT_ARCHIVE_B64` from
   `DIR.b64` and sends the hash and count; Claude commits them as
   `c3/HELDOUT-SHA256` (`<sha256>  <count>`) before any run. Claude never
   sets the secret.
4. **`tools/campaign/unseal.py`**: reads the base64 text from an
   environment variable (the secret) or, for K7 only, a committed file;
   checks the decoded bytes against the hash file **before** writing
   anything; refuses an archive member that is absolute, has `..`, or is
   not a regular file; extracts to a directory outside the checkout;
   prints only `sealed archive: hash matches, N cases` or the reason it
   stops (never a name inside the archive).
5. **`campaign.yml`** gains two inputs: `sealed` (the hash file; when
   set, `cases` is ignored and the case list is the archive's) and
   `sealed_source` (`secret`, the default, or `test` for K7's committed
   archive). Each job that needs the cases (plan, run, summary) unseals
   them itself; the plan job does it first, so a wrong hash stops the run
   before any case job starts. With `sealed` set, the run jobs upload only
   `out/*.json` (verdicts and evidence), with 7 days' retention; logs stay
   in the job's scratch directory and go with it.
6. **Opening.** After the run, Claude reads the artifacts, the owner
   gives the archive's contents for the results record, and the hash is
   checked once more against the committed one.

**K7's test archive.** `tools/campaign/cases/k7/make_test_archive.py`
writes two test cases (not the owner's): T1 adds a comment holding a
unique marker string to an enforcement file (builds; every method as the
baseline), T2 adds the marker in a line that does not compile (every
build fails, so its build log quotes the line). The committed
`k7/test-archive.b64`, `k7/TEST-SHA256` (right) and `k7/WRONG-SHA256`
(one hex digit changed) feed the rows below. The marker appears in the
repository only inside the generator and, encoded, the base64 file.

## Expectations

| Row | Check | Expected |
| - | - | - |
| K7a | unit tests of `unseal.py` and `seal.py` (`tools/campaign/test_seal.py`) | a wrong hash exits non-zero and writes nothing; `..`, absolute and link members are refused; a case that is not `heldout` or has an unknown class is refused; two seals of one directory give the same bytes; nothing printed names a patch or holds its text |
| K7b | `campaign.yml`, `sealed=.../k7/WRONG-SHA256`, `sealed_source=test` | the plan job fails at the unseal step with a hash mismatch; no case job starts |
| K7c | the same with `k7/TEST-SHA256` | two cases run; T1 as the baseline (M6 matches the grant), T2 a finding from M1, M2, M4 and M6 (builds fail); no disagreement with the test archive's expectations; the artifacts hold only `T1.json` and `T2.json` |
| K7d | the logs of K7b and K7c (`gh run view --log`) and their artifacts | the marker appears 0 times in the logs; in the artifacts only if a build-failure evidence line quotes it (then recorded, and held-out evidence is cut to exit codes before C4) |

**Correction before any run (2026-11-21, from reading `run_case.py`):**
when the qemu-psci build fails, M6's verdict is `error` ("nothing to
read"), not a finding; K7c's T2 row is therefore M1, M2 and M4 findings,
M3 pass and M6 error, as the test archive's expectations say.

K7d's second clause is a prediction I cannot settle by reading: M1's and
M2's evidence for a failed build includes the log's last lines
(`tail()`), which may quote the bad line. If they do, held-out cases get
exit codes only in their evidence, as its own change with its own row.

## Limits [I]

- K7 does not pass through GitHub's secret store: K7b and K7c read the
  committed test archive. The secret path differs only in where
  `unseal.py` reads the text; if it fails at C4, the hash check fails
  first and no case runs, so the failure is safe.
- The secret's 48 KB limit holds about 30 small patches after gzip and
  base64 [I]; `seal.py` stops above 46 KB.
- Anyone who can run the workflow can run the sealed cases, but not read
  them: the logs carry verdicts only. Only the owner and collaborators can
  dispatch workflows.

## For the owner [?] (superseded 2026-11-22)

The steps that stood here used folders outside the workspace (`~`); the
owner ruled that nothing outside `workspace/` is touched, and the
section below replaces them. `seal.py` takes any folder, so a sealed run,
if ever wanted, uses one under `workspace/.work/`.

## Owner decision (2026-11-22): historical faults instead of held-out cases

The owner left C3 to Claude ("please do those in yourself"). Cases
Claude writes cannot be held out from Claude, the author of read-back,
so the held-out slot is filled instead by **historical faults**: real
defects of HAVEN's enforcement code, put back by reverting their fixes.
Neither the owner nor Claude chooses them; a rule picks them, fixed here
before any is extracted or run [P, delegated]:

1. Every non-merge commit on `main` up to `f1da2b1` whose subject starts
   with `fix:` and that changes the 11 enforcement files of
   `taxonomy.json` (`mutate.py`'s default list): 14 commits [F].
2. Commits changing more than 100 lines in those files are left out as
   rewrites, not single faults (`7ec513e`, 638 lines; `4f0d9ea`, 137) [F].
3. Each case is the reverse of its commit's diff, limited to the 11 files,
   applied to the tree at the run's commit (`git apply`, then
   `git apply -3`); one that does not apply is counted as "code moved"
   and dropped, as the random draw drops mutants that do not build.
4. Classes are named from the reverse diffs before the run; cases are
   blind (no per-case expectations), source `historical`, reported apart
   from written, random and held-out cases (design item 5).

**Threat to validity [I].** Claude wrote most of these fixes, so Claude
knows the faults; this set is independent of the choice of cases (a
rule picks them, and the faults were real, not invented for the
campaign), not of the author's knowledge. The held-out property the C0
design wanted is therefore **not met**; it is stated as a limit of the
evaluation. The sealed channel stays in the repository, tested (K7), for
a reviewer or the owner to use later.

| Row | Check | Expected |
| - | - | - |
| H1 | extraction (`tools/campaign/history.py`) | 12 commits; each gives a patch or "code moved"; the list and every patch committed with their classes before the run |
| H2 | the run (`campaign.yml`, the cases file `historical.json`) | every case a verdict per method; cases that do not build counted apart |
| H3 | read-back's share | no prediction per case; the record names, for every case read-back misses, a cause (E5) |

## Also found: C2's random cases were not drawn [F]

The design's step C2 is "written and random cases" (about 60 random,
owner-approved 2026-11-19); C2 ran only the 40 written cases, and
`tools/campaign/cases/` has no random set. Recorded here; the draw gets
its own record (seed text committed before the draw, the draw in CI,
classes read from the diffs after it, frozen by hash) before C4.

## K7 results (2026-11-21)

`campaign.yml` at `0025e9d` (the merge of #173) [F]:

| Row | Result |
| - | - |
| K7a | `test_seal.py`, 10 tests, pass in CI on Linux, macOS and Windows (#173); holds |
| K7b | `WRONG-SHA256`: the plan job stops at the unseal step (`unseal: hash mismatch: the archive is not the committed one; no case runs`); both case jobs skipped; holds |
| K7c | `TEST-SHA256`: each job prints `sealed archive: hash matches, 2 cases`; T1 as the baseline (M6 pass), T2 M1, M2, M4 finding, M3 pass, M6 error; no disagreement; the artifacts hold only `T1.json`, `T2.json` and the report; holds |
| K7d | the marker appears 0 times in both runs' logs; in the artifacts once, in `T2.json`: M1's evidence quotes the compiler's error, the bad line included. The second clause applies |

So the logs are clean, but a held-out case that does not build would put
its patch text in a downloadable artifact before the results record.
M2's and M4's tails happened to show only build progress lines here; a
different patch could make them quote source too [I].

**Change and row before C4 [P].** `run_case.py` quotes no log lines for
held-out cases: evidence keeps its first line (`host-tests build exit 1`,
`qemu-psci build failed`, an exit code) and drops the `tail()` lines;
written and random cases unchanged.

| Row | Check | Expected |
| - | - | - |
| K7e | the change's unit test, then K7c again (`TEST-SHA256`) | the unit test: a held-out case's evidence has no log line; K7c: the same verdicts, the marker 0 times in the logs **and** the artifacts |

## K7e result (2026-11-21)

The change merged as #175; K7c again at `f0a5973` [F]: T1 and T2 with
the same verdicts as before, no disagreement; T2's M1 evidence is now
`host-tests build exit 1` alone; the marker appears 0 times in the log
and 0 times in the artifacts (`T1.json`, `T2.json`, the report). K7e
holds.

**K7 holds, so step C1 is complete.** The sealed channel is ready for
the owner's cases (section "For the owner" above). Until the owner's
hash is committed as `c3/HELDOUT-SHA256`, no sealed run with
`sealed_source=secret` can start.

## Historical faults: extraction (row H1, 2026-11-22)

`tools/campaign/history.py . OUT f1da2b1` on the Mac (git only, no
build), HEAD `0fc2296`; `history.json` here [F].

**Correction to the rule's counts above:** 13 commits match rule 1, not
14 (`798f152` starts with `feat:`; it was counted from a search for
"fix" anywhere in the subject); rule 2 leaves out 2, so 11 are eligible,
not 12. The rule itself is unchanged.

| Commit | Lines | Outcome | Case | Class | What the revert puts back |
| - | - | - | - | - | - |
| `a4c2dc1` | 45 | code moved | - | - | enforce irq ownership boundaries |
| `cfdf566` | 69 | code moved | - | - | harden stage2 boundary and mapping state checks |
| `604c17f` | 64 | code moved | - | - | address PR review feedback across scripts and isolation paths |
| `4f0d9ea` | 137 | rewrite (left out) | - | - | harden SMMUv3 command queue with circular depth tracking |
| `8b02d33` | 88 | code moved | - | - | portable stage-2 permission bits; fix execute-never encoding (MM1, MM2 |
| `7ec513e` | 638 | rewrite (left out) | - | - | SMMU command queue, hardware stream IDs and per-stream DMA tables |
| `de6291c` | 7 | kept | H1 | GIC-GROUP | GICD_CTLR written with the Secure layout's bits (G1NS, G0) |
| `3fbafae` | 13 | code moved | - | - | disable an SPI that cannot be delivered instead of letting it fire aga |
| `2b6f5ea` | 5 | kept | H2 | SMMU-TAG | the VMID left in a stream table entry's stage-2 table address |
| `4ccee17` | 41 | kept | H3 | GIC-ENABLE | virtual interface: list registers neither bounded by ICH_VTR_EL2 nor cleared per CPU |
| `be4eb32` | 10 | kept | H4 | CPU-ASSIGN | CPU_ON issued for a CPU the CPU map lacks |
| `93cf892` | 30 | kept | H5 | GIC-PPI | redistributor frames by fixed stride instead of GICR_TYPER affinity |
| `7dcbad5` | 2 | kept | H6 | CONTROL | a redundant zero initializer of a static array (no change in behaviour) |

6 kept, 5 "code moved" (the code they fixed has since been rewritten,
mostly the May 2026 fixes), 2 rewrites left out. H1 holds with the
corrected count. Classes named from the reverse diffs before the run;
H6 is a historical control (the fix removed a redundant initializer, so
its revert changes nothing). Patches: `tools/campaign/cases/hist/`;
cases file: `tools/campaign/cases/historical.json` (source
`historical`, new in `score.py` and `run_case.py`).

Six cases are too few for a rate; they are reported as cases, beside the
written and random sets.

## Historical faults: the run (rows H2, H3, 2026-11-22)

`campaign.yml` on `historical.json`, three shards, at `e9e2014` [F]:

| Case | Class | M1 | M2 | M3 | M4 | M6 | Cause, when every method passes [I] |
| - | - | - | - | - | - | - | - |
| H1 | GIC-GROUP | pass | build fails | pass | build fails | error | does not build: the GICD_CTLR macros it uses were renamed since; counted apart |
| H2 | SMMU-TAG | pass | pass | pass | pass | pass | not exercised: the revert lets VMID bits 3:0 into S2TTB bits 51:48, and the one DMA stream's VMID is 0xC0, whose low four bits are zero; read-back compares bits 51:4, so a second stream (VMID 0xC1) would show [I] |
| H3 | GIC-ENABLE | pass | pass | pass | pass | pass | equivalent under QEMU: the list registers are not cleared, but QEMU resets them to 0; on hardware they are UNKNOWN at reset |
| H4 | CPU-ASSIGN | pass | pass | pass | pass | pass | not exercised: CPU 2 is in QEMU's CPU map, so the missing check never matters |
| H5 | GIC-PPI | pass | pass | pass | pass | pass | equivalent under QEMU: QEMU's redistributor frames sit at the fixed stride the revert assumes and the boot CPU is core 0; the fix was for the board |
| H6 | CONTROL | pass | pass | pass | pass | pass | control: no finding from any method, as it should |

No disagreement (no expectations); H2 and H3 hold: every miss has a
named cause. **Every historical fault that builds is one QEMU or the
grant cannot show**: they were found and fixed because of the board's
documents (frames, list registers) or a second stream, which is why
they were bugs worth fixing and why a QEMU campaign misses them. Two of
them (H3, H5) are board questions for G3; H2 is a case for scenario 2
or a two-stream grant.
