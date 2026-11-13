# Quality gate for HAVEN, ELMOS and KDAL (Q, G0 to G5)

**Status:** G1 (HAVEN) done, results below; limitations L1 and L2 fixed in
#99 (expectations C1 to C5 recorded before its code, all held). Design and G1
expectations were merged before any gate code and before any measurement of
the tree (owner-approved 2026-11-13: the
gate replaces item Q's plain audit; Q's audit is the first baseline).
Expectations for G2 to G5 are added here, each before its own code. The
user-facing description is `docs/quality/gate.md`.

## Problem

Nothing measures duplication, complexity or analyzer findings across the
three repositories, and nothing stops a change that makes them worse. The
owner asked for one gate that rates findings by severity and decides, at
commit, push and pull request, whether a change may go in, for the
project's own work and for outside contributors.

## Design (summary; detail in `docs/quality/gate.md`)

- One engine, `tools/gate/` (Python standard library only, Python 3.9 or
  later, so the Mac's system Python runs it): runs the tools named in the
  repository's `quality/gate.json`, maps each tool's output to BLOCKER,
  CRITICAL, MAJOR, MINOR or INFO (`tools/gate/rules.py`), compares with
  `quality/baseline.json`, applies the stage's policy, prints a report and
  exits non-zero when the change must not go ahead.
- Finding identity: tool, rule, file and the SHA-256 of the whitespace-
  normalised source line(s) the finding points at; line numbers are not
  part of it, so code that moves keeps its identity.
- Stages: `pre-commit` (staged files), `pre-push` (files changed against
  `main`), `ci` (whole tree), `deep` (whole tree, slow tools, local only).
  `baseline` writes the baseline from the current tree.
- HAVEN's tool set (G1): the existing clang-format check, cppcheck,
  clang-tidy (`bugprone-*`, `cert-*`, `clang-analyzer-*`) on C under
  `arch/`, `src/`, `drivers/`, `include/`, `boot/`; lizard (cyclomatic
  complexity limit 15, function length limit 100 lines); jscpd (C and
  Python, blocks of at least 15 lines); ruff (Python under `tools/`,
  `scripts/`, `docs/methodology/`); shellcheck (shell scripts); a secret
  scan (gitleaks when installed, otherwise the engine's own patterns for
  private keys and GitHub tokens); the engine's personal-name and
  absolute-path scan; the trusted-code delta from `tcb-image-loc.py` as
  INFO in CI.
- The gate adds no code to `haven.elf`.

## Expectations for G1 (HAVEN), before the tree is measured

Predictions about the tree are ranges, recorded either way.

| Row | Check | Expected |
| - | - | - |
| Q1 | lizard: functions over complexity 15 in HAVEN's C | 10 to 40 |
| Q2 | jscpd: duplicated lines in HAVEN's C | 1 % to 5 % of the C lines |
| Q3 | cppcheck findings at warning level or above | 0 to 20 |
| Q4 | clang-tidy findings (`bugprone`, `cert`, analyzer) | 5 to 60, of them analyzer-confirmed memory errors in the trusted code: 0 |
| Q5 | ruff findings (default rules, no ruff configuration so far) | 20 to 200 |
| Q6 | shellcheck findings | 10 to 80 |
| Q7 | secrets and personal names in the tree | 0 (the names were removed in #80) |
| S1 | seeded: a staged file with a GitHub token pattern | `pre-commit` exits 1, BLOCKER |
| S2 | seeded: a staged file with a `/Users/<name>/` path | `pre-commit` exits 1, BLOCKER |
| S3 | seeded: a new C function with complexity 20 | `pre-push` exits 1 (new MAJOR); `pre-commit` passes |
| S4 | seeded: a new 40-line block copied from an existing function | `pre-push` exits 1 (new MAJOR) |
| S5 | seeded: a clang-tidy `bugprone` finding in a changed file | `pre-push` exits 1 (CRITICAL) |
| S6 | the tree as it is, with its new baseline | `ci` exits 0; every finding is "known" |
| S7 | a finding fixed | it no longer appears; `baseline --check` reports the baseline can shrink |
| T1 | engine unit tests (recorded tool outputs mapped to severities, identity stable across a line move) | pass on macOS and Linux, standard library only |
| T2 | time on the Mac | `pre-commit` on a 3-file commit under 10 s; `pre-push` under 3 min |
| T3 | CI job "quality gate" | under 5 min |
| B1 | HAVEN images | byte-identical to `main` (no C change) |

## Results (G1, HAVEN; runs of 2026-11-13 on `main` 2605d18 and 89371e7)

Observed on the Mac with the pinned tool versions of `quality/gate.json`,
and in the CI job "quality gate" (#96, #97). The Mac and CI report the same
counts: CRITICAL 4, MAJOR 289, MINOR 76 (369 findings, 349 distinct keys,
see limitation L1).

| Row | Expected | Observed | Verdict |
| - | - | - | - |
| Q1 | 10 to 40 | 13 functions over complexity 15 (and 2 over 100 lines) | holds |
| Q2 | 1 % to 5 % of the C lines | C alone 0.18 %; C and Python 3.0 % (the copies are the run scripts under `docs/methodology/`) | prediction error (C has far less copying than predicted) |
| Q3 | 0 to 20 | 0 at warning level or above; 2 "could not analyse" reports (INFO) | holds |
| Q4 | 5 to 60; 0 analyzer-confirmed memory errors in the trusted code | 248 (CRITICAL 4, MAJOR 243, MINOR 1); 1 analyzer null dereference at `el2_exceptions.c:371` | prediction error on the count; the dereference is on a path that cannot run (`hv_psci_call` never returns START_VCPU), an inference from reading the code, to be removed by a guard (fix list) |
| Q5 | 20 to 200 | 74 under the explicit rule set of `ruff.toml` (388 under ruff's defaults, which change with the ruff version) | holds |
| Q6 | 10 to 80 | 7 | prediction error |
| Q7 | 0 | 4 absolute home-directory paths of a build container in two committed `host-ctest.log` files (BLOCKER); replaced by `/home/user/` in #96 | prediction error |
| S1 | `pre-commit` exits 1, BLOCKER | exit 1, BLOCKER `secret/github-token` | holds |
| S2 | `pre-commit` exits 1, BLOCKER | exit 1, BLOCKER `path/home` (first seed was a `*.log`, which is ignored by git; repeated as `.txt`) | holds |
| S3 | `pre-push` exits 1; `pre-commit` passes | as expected | holds |
| S4 | `pre-push` exits 1 (new MAJOR) | first run: not caught (jscpd paths relative to the scanned directory); after #97: exit 1, `src/seed/copy.c:6: 36 lines duplicated from src/core/partition.c:405` | held after a fix |
| S5 | `pre-push` exits 1 (CRITICAL) from a `bugprone` seed | the `bugprone` memset seeds did not fire (the checks do not match `__builtin_memset`, which HAVEN's code uses); a null-dereference seed gave CRITICAL from cppcheck and the analyzer and exit 1 | not as planned: the CRITICAL path is shown with a different seed |
| S6 | `ci` exits 0, every finding known | exit 0 | holds |
| S7 | fixed finding disappears; `baseline --check` reports it | as expected | holds |
| T1 | unit tests pass, standard library only | 12 tests pass on macOS (Python 3.9) and in CI (Ubuntu); a 12th test for the base branch was added in #97 | holds |
| T2 | `pre-commit` under 10 s; `pre-push` under 3 min | `pre-commit` 0.05 s with 3 staged C files; `pre-push` 1.0 s with 3 changed C files; whole-tree `ci` 5.7 s | holds |
| T3 | under 5 min | 2 min 39 s, most of it installing the pinned tools | holds |
| B1 | images byte-identical | no C, assembly or linker file changed in #96 or #97 | holds by construction, not measured |

Correction: the commit message of #98 counts these rows as "13 of the 21
held" and "5 prediction errors". There are 18 rows: 12 held, S4 held after a
fix, S5 was shown with another seed and 4 were prediction errors (Q2, Q4,
Q6, Q7). The table above is right; the pushed message is left as it is.

### Calibrations made after the first run (before the baseline was committed)

The severity map was changed once, after looking at the first whole-tree
run; each change is a judgement, recorded here so that it can be reviewed:

- `bugprone-signed-bitwise` (195 of the 248 clang-tidy findings) and
  `bugprone-easily-swappable-parameters` (39) are MAJOR, not CRITICAL: the
  first flags almost every register mask written with signed constants, the
  second almost every function taking two addresses. They stay in the
  baseline as debt rather than block.
- `clang-analyzer-optin.performance.*` and
  `clang-analyzer-security.insecureAPI.*` are MINOR (padding and
  `memcpy` advice, not defects in a freestanding kernel).
- cppcheck's "could not analyse" reports (`syntaxError`,
  `preprocessorErrorDirective`, `unknownMacro`) are INFO: they say the tool
  did not look, not that the code is wrong.
- ruff runs an explicit rule set (`ruff.toml`: E4, E7, E9, F), because the
  default set changed the count between versions. The ruff mapping first
  read any rule starting with "F" as Pyflakes and so mapped `FURB` rules to
  MAJOR; it now requires "F" followed by a digit.
- gitleaks reported the baseline's hashes and `__pycache__` files; they are
  allowlisted in `.gitleaks.toml`. gitleaks built with `go install` reports
  no version, so CI installs the release binary.

### Limitations found (not fixed in G1)

- **L1, identical findings share one key.** The key is tool, rule, file and
  the normalised source line, so two findings on identical lines in one
  file have one key: the tree has 369 findings and 349 keys. A new copy of
  an already-known flawed line in the same file is therefore treated as
  known. Proposed fix: the baseline stores a count per key and a stage
  blocks when the current count is higher.
- **L2, a tool that fails silently reports nothing.** The runners read the
  tool's output and ignore its exit status and error output, so a crash or
  a bad flag gives zero findings and the stage passes. Proposed fix: a
  failure exit with no parsed findings becomes a MAJOR `gate/tool-failed`
  finding (blocking in `pre-push` and `ci`).
- **L3, the base branch** (fixed in #97): `pre-push` compared with
  `origin/main` first, which on a clone with an old mirror as `origin`
  made old duplicates look new.

### Expectations for the L1 and L2 fix, before its code

| Row | Check | Expected |
| - | - | - |
| C1 | seeded: a second copy of a line that already has a known MAJOR finding, in the same file | `pre-push` exits 1 (count 2 against baseline count 1); before the fix it exits 0 |
| C2 | the tree as it is, baseline regenerated with counts | `ci` exits 0; 349 keys, counts summing to 369 |
| C3 | seeded: a tool run that exits with an error and prints nothing (a wrong flag in a scratch copy of `gate.json`) | `pre-push` exits 1 with `gate/tool-failed` (MAJOR) |
| C4 | engine unit tests | pass, with tests for C1 and C3 added |
| C5 | baseline schema | `haven-gate-baseline/2`; a `/1` baseline is still read (count 1 per key) |

### Results for C1 to C5 (#99)

| Row | Observed | Verdict |
| - | - | - |
| C1 | line 23 of `src/core/exc/mmio.c` (a known `bugprone-signed-bitwise` MAJOR) copied below itself: `pre-push` exit 1, both instances listed, "new CRITICAL or MAJOR 2"; the same branch with `main`'s engine and baseline: exit 0 | holds |
| C2 | `baseline` wrote 349 keys with counts summing to 369; `ci` exit 0, "new CRITICAL or MAJOR 0" | holds |
| C3 | `clang_tidy.checks` set to `-*` in a seeded `gate.json`: `pre-push` exit 1, `MAJOR gate/tool-failed: clang-tidy: exit 1: Error: no checks enabled.` | holds |
| C4 | 14 unit tests pass on the Mac (2 added: counts, tool failure) | holds |
| C5 | the baseline is written as `haven-gate-baseline/2`; a `/1` file is read at one per key (unit test) | holds |

## G2: ELMOS and KDAL profiles (design and expectations, before any run on their trees)

Owner-approved 2026-11-13. Known before writing this: ELMOS has 74 Go files
(11.2k lines, 10 test files), example C under `examples/` and vendored
headers under `assets/libraries/`; KDAL has 45 C and 12 header files
(11.6k lines): a kernel module (`src/`, `include/`, `tests/kunit/`,
`examples/`) and userspace C built with CMake (`compiler/`,
`tools/kdality/`), 34 shell scripts and 1 Python file. No gate tool has been
run on either tree.

### Design decisions

- **D1, one engine.** `tools/gate/` stays in haven. ELMOS and KDAL pin a
  haven commit in their `quality/gate.json` (`engine.commit`); their
  `scripts/gate` fetches only `tools/gate/` at that commit into the user's
  cache (`~/.cache/haven-gate/<commit>/`) and runs it. CI does the same. No
  copy of the engine lives in ELMOS or KDAL; moving to a newer engine is a
  one-line change of the pin, reviewed like any other.
- **D2, paths per tool.** A profile may give a tool its own paths
  (`tool_paths`), because KDAL's kernel C and userspace C need different
  tools. Without it a tool keeps its language's paths.
- **D3, tool installation from the profile.** `ci-setup.sh` installs only the
  tools a profile pins in `versions`, so each repository's CI installs what it
  uses.
- **D4, ELMOS's Go tools.** `gofmt` (an unformatted file is MAJOR);
  golangci-lint with an explicit linter set (govet, staticcheck, errcheck,
  ineffassign, unused) instead of separate `go vet` and `staticcheck` runs:
  the same checks in one run with one version pin. Severities: govet and
  staticcheck's `SA` checks CRITICAL; errcheck, ineffassign, unused MAJOR;
  staticcheck's `S1`, `ST1` and `QF` style checks MINOR. govulncheck: a
  vulnerability in code ELMOS calls is CRITICAL, one only in a required
  module is INFO. The Go version is pinned like the tools (it decides the
  standard library govulncheck checks). lizard and jscpd also read Go.
- **D5, KDAL's kernel C.** `checkpatch.pl` from Linux v6.18 (pinned by tag
  and the script's SHA-256) on the kernel module: ERROR MAJOR, WARNING
  MINOR, CHECK INFO. cppcheck, lizard and jscpd on all C; clang-tidy only on
  the userspace C, through CMake's compile database. The kernel C has no
  compile database without a kernel build, so clang-tidy and sparse on it
  belong to the deep tier (G5).
- **D6, scope of G2.** Profiles, baselines and a CI job "quality gate" in each
  repository. KDAL's existing lint job and both repositories' hooks are left
  alone until G3.

### Expectations

| Row | Check | Expected |
| - | - | - |
| E1 | ELMOS: files `gofmt` would change | 0 to 3 |
| E2 | ELMOS: golangci-lint findings | 10 to 120, errcheck the largest group, govet 0 to 5 |
| E3 | ELMOS: govulncheck, vulnerabilities in called code | 0 to 2 |
| E4 | ELMOS: Go functions over complexity 15 | 5 to 30 |
| E5 | ELMOS: duplicated Go lines | 1 % to 6 % |
| K1 | KDAL: checkpatch on the kernel module | ERROR 0 to 30, WARNING 20 to 200 |
| K2 | KDAL: cppcheck at warning level or above | 0 to 15 |
| K3 | KDAL: clang-tidy on the userspace C | 20 to 200 |
| K4 | KDAL: C functions over complexity 15 | 10 to 40 (the compiler's parser and the tools' command handlers) |
| K5 | KDAL: duplicated C lines | 1 % to 5 % |
| K6 | KDAL: shellcheck findings | 20 to 150 |
| P1 | both: secrets, personal names, home paths in the tree | 0 |
| P2 | both: Mac and CI counts | identical per severity |
| S1 | seeded, ELMOS: a new Go function ignoring an error return | `pre-push` exits 1 (errcheck, MAJOR) |
| S2 | seeded, ELMOS: an unformatted Go file | `pre-push` exits 1 (gofmt, MAJOR) |
| S3 | seeded, KDAL: a kernel C change with a checkpatch ERROR | `pre-push` exits 1 |
| S4 | seeded, KDAL: a 30-line block copied into a new file | `pre-push` exits 1 (jscpd) |
| S5 | seeded: `engine.commit` set to a commit that does not exist | `scripts/gate` exits 2 with a message naming the pin; it never passes |
| S6 | both trees as they are, with their new baselines | `ci` exits 0 |
| T1 | engine unit tests, with recorded output of each new tool | pass on macOS and in CI, standard library only |
| T2 | `pre-push` on the Mac with 3 changed files | under 30 s (golangci-lint checks whole packages) |
| T3 | first engine fetch on the Mac | under 10 s; later runs fetch nothing |
| T4 | CI job "quality gate" in ELMOS and in KDAL | under 6 min each |
| B1 | HAVEN images | byte-identical to `main` (the engine change is Python only) |

### Results (G2; engine haven #101 at `400500b`, ELMOS #22, KDAL #28)

The record has 24 G2 rows (the commit message of #100 says 25; that count
is wrong). 20 held, P2 held after a fix, 3 were prediction errors (E4, E5,
K3).

| Row | Expected | Observed | Verdict |
| - | - | - | - |
| E1 | 0 to 3 | 0 | holds |
| E2 | 10 to 120, errcheck largest, govet 0 to 5 | 22, all errcheck (8 of them in tests); govet, staticcheck, ineffassign, unused 0 | holds |
| E3 | 0 to 2 | 1: GO-2026-6865 (a panic on two-block frames) in `github.com/klauspost/compress` v1.19.2, reached from `demo.Untar` through the zstd encoder; fixed in v1.20.2 | holds |
| E4 | 5 to 30 | 3 (`writeCommandDocs` 17, `Untar` 16, `tarToExt4` 17) | prediction error |
| E5 | 1 % to 6 % | 0 duplicated lines of 8,224 (tests excluded) | prediction error |
| K1 | ERROR 0 to 30, WARNING 20 to 200 | ERROR 2, WARNING 60 (33 of them a missing SPDX tag) | holds |
| K2 | 0 to 15 | 0 at warning level or above; `src/core/registry.c` not analysed (`list_for_each_entry` unknown, INFO) | holds, with a gap (L5) |
| K3 | 20 to 200 | 337: `cert-err33-c` (unchecked `printf`-family returns) 314, 13 CRITICAL (4 null dereferences in `compiler/lexer.c`, a 0-byte `malloc` and a tainted index in `compiler/codegen.c`, a leak, tainted indexes in `tools/kdality/`) | prediction error |
| K4 | 10 to 40 | 28 over complexity 15 (and 10 over 100 lines) | holds |
| K5 | 1 % to 5 % | 3.76 % (392 of 10,427 lines) | holds |
| K6 | 20 to 150 | 41 | holds |
| P1 | 0 | 0 in both trees | holds |
| P2 | identical per severity | ELMOS identical (CRITICAL 1, MAJOR 25). KDAL's first CI run had 10 more `cert-err33-c` than the Mac: all `snprintf` and `vsnprintf`, which macOS's fortified headers turn into `__builtin___snprintf_chk` macros the check does not match. With `-D_FORTIFY_SOURCE=0` for clang-tidy both report CRITICAL 13, MAJOR 384, MINOR 102 | held after a fix |
| S1 | `pre-push` exit 1 (errcheck) | exit 1, `errcheck` on the seeded `os.Remove` (and `unused`) | holds |
| S2 | `pre-push` exit 1 (gofmt) | exit 1, `gofmt/unformatted` | holds |
| S3 | `pre-push` exit 1 | exit 1, checkpatch `OPEN_BRACE` and `SPACING` (ERROR, MAJOR) | holds |
| S4 | `pre-push` exit 1 (jscpd) | exit 1, 34 lines duplicated from `tools/kdality/lint.c:62` | holds |
| S5 | exit 2 naming the pin | exit 2: "cannot fetch the engine at 1111... (engine in quality/gate.json)" | holds |
| S6 | `ci` exit 0 | exit 0 in both, on the Mac and in CI | holds |
| T1 | unit tests pass | 19 pass on macOS and in CI | holds |
| T2 | under 30 s with 3 changed files | 0.8 s in ELMOS and in KDAL, measured with 2 changed files each (golangci-lint's cache warm) | holds, measured with 2 files |
| T3 | under 10 s, later runs fetch nothing | 1.9 s, then 0.03 s | holds |
| T4 | under 6 min each | ELMOS 2 min 41 s (1 min 51 s with the cppcheck cache), KDAL 2 min 49 s to 3 min | holds |
| B1 | HAVEN images byte-identical | #101 changed no C, assembly or linker file | holds by construction, not measured |

### Limitations found in G2

- **L4, host-dependent analysis of host C.** clang-tidy on userspace C reads
  the host's C library headers, so macOS and Linux can differ (P2). Fortify
  macros were the difference here; others may remain, and the Linux runner
  is the reference when they differ.
- **L5, cppcheck and kernel macros.** cppcheck stops on files using kernel
  list macros (`registry.c`), reported as INFO. A cppcheck configuration
  for the kernel macros, or the deep tier's sparse, would cover them.
- **L6, gitleaks reads untracked files.** `gitleaks dir` scans the working
  directory, so a developer's untracked build output can give findings that
  CI, with a clean checkout, does not. Build and cache directories are
  allowlisted.
- **L7, the wrapper is copied.** `scripts/gate` is the same 34-line
  bootstrap in ELMOS and KDAL. It is small and changes rarely, but nothing
  checks that the copies agree.

## G3: hooks and one lint path in ELMOS and KDAL (design and expectations, before code)

Owner-approved 2026-11-13, with G4 to G5 and the fix list, and with the
`cert-err33-c` rule left at MAJOR (it marks ignored `snprintf` truncation as
well as ignored `printf` output; the 314 existing reports stay in KDAL's
baseline and only new ones block). Known before writing this: KDAL's
ruleset requires the old lint job ("style and static analysis") by name;
its lint scripts are referred to by a CMake target, `scripts/dev/test.sh`,
the e2e developer-journey test, the pull-request template, CONTRIBUTING and
the website's contributing page; KDAL installs hooks by copying them into
`.git/hooks/`; ELMOS has no hooks and checks formatting twice (its own CI
step and the gate).

### Design decisions

- **D7, one hook mechanism.** All three repositories keep their hooks in
  `.githooks/` and set `core.hooksPath` with `scripts/gate install`.
  KDAL's `commit-msg` hook moves there; `scripts/dev/install-hooks.sh` and
  the copying into `.git/hooks/` are removed.
- **D8, one lint path.** KDAL's `scripts/ci/checkpatch.sh`,
  `scripts/ci/static_analysis.sh` and the CI job "style and static
  analysis" are removed; the CMake `lint` target, the dev test, the e2e
  test, the template and the documents point to `scripts/gate`. ELMOS's CI
  drops its own format check (the gate's gofmt covers it) and keeps `go vet`
  on its three operating systems (files built only for one of them); `task
  dev:lint` runs `scripts/gate pre-push`.
- **D9, cppcheck options per profile.** A profile may set cppcheck's
  `enable` list and suppressions, so KDAL keeps the style checks and
  suppressions of its old job. HAVEN's profile does not set them and keeps
  its behaviour.
- **D10, required checks.** KDAL's ruleset swaps "style and static
  analysis" for "quality gate" before the old job is removed (that part of
  G4 is done first for KDAL; haven and ELMOS add "quality gate" in G4).

### Expectations

| Row | Check | Expected |
| - | - | - |
| H1 | ELMOS: `scripts/gate install`, then a commit staging a file with a GitHub token pattern | `core.hooksPath` is `.githooks`; the commit is refused (hook exit 1) |
| H2 | ELMOS: `git push` of a branch adding an unformatted Go file | refused by the pre-push hook |
| H3 | KDAL: a commit with the subject "bad subject" after the move | refused by `.githooks/commit-msg` |
| H4 | KDAL: `git push` of a branch with a checkpatch ERROR in `src/` | refused by the pre-push hook |
| H5 | KDAL, the old job against the gate on `main` (`1a3818f`), before removal | (a) the old cppcheck command exits 0 and the gate with KDAL's style options adds 0 findings to the baseline; (b) the old shellcheck (warning and above, `scripts/` only) reports 0 and the gate reports 0 at MAJOR or above under `scripts/`; (c) every file the old `checkpatch.sh` fallback warns about also has a checkpatch finding in the gate |
| H6 | KDAL after D8 | `git grep` finds no `install-hooks.sh`, `checkpatch.sh` or `static_analysis.sh` outside the gate's baseline |
| H7 | KDAL's and ELMOS's other CI jobs after D7 and D8 | pass unchanged |
| H8 | HAVEN after D9 | `baseline --check`: 0 new, 0 fixed |
| H9 | KDAL with D9 | `baseline --check` after the style options: 0 new (the old job's suppressions hold) |
| H10 | ELMOS and KDAL CONTRIBUTING | say how to install the hooks, what each stage checks, how to update the baseline and the pinned engine, and link `docs/quality/gate.md` |

### Results (G3 and G4; haven #104, #105; ELMOS #23; KDAL #29)

All 10 G3 rows held; H5 (c) held only as worded (see below).

| Row | Observed | Verdict |
| - | - | - |
| H1 | ELMOS: `core.hooksPath` = `.githooks`; a commit staging a GitHub token pattern: `BLOCKER scan/github-token`, commit exit 1 | holds |
| H2 | ELMOS: `git push` with an unformatted Go file: `gofmt/unformatted`, "failed to push some refs"; nothing reached GitHub | holds |
| H3 | KDAL: subject "bad subject" refused by `.githooks/commit-msg` (exit 1) | holds |
| H4 | KDAL: `git push` with a checkpatch ERROR in `src/core/registry.c`: `OPEN_BRACE`, `SPACING`, push refused; nothing reached GitHub | holds |
| H5 | (a) old cppcheck command exit 0; gate with KDAL's style options: 0 new findings. (b) old shellcheck: 0; gate under `scripts/` at MAJOR or above: 0. (c) every file the old fallback warned about has a gate checkpatch finding, but not for the same reason: the fallback's 33 "lines over 120 characters" in 10 files were byte counts of UTF-8 box-drawing comment lines (`─` is 3 bytes), about 70 characters wide; checkpatch, now at the same 120-column limit instead of ignoring `LONG_LINE`, correctly reports none | holds; (c) was a weak row |
| H6 | `git grep` finds none of the removed names (the baseline was regenerated, so not even there) | holds |
| H7 | KDAL's build, smoke and KUnit jobs and ELMOS's vet, test, build and e2fsck jobs passed unchanged | holds |
| H8 | HAVEN `baseline --check` after #104: 0 new, 0 fixed | holds |
| H9 | KDAL with the style options: 0 new; 5 fixed (shellcheck findings in the removed scripts); baseline 494 findings, 418 keys | holds |
| H10 | both CONTRIBUTING files (and KDAL's website copy) describe install, stages, baseline and the pinned engine, and link `docs/quality/gate.md` | holds |

**Not predicted:** KDAL's first G3 run failed before any check, because
GitHub's raw files answered 429 (Too Many Requests) to the anonymous
checkpatch download from the runner; the Mac got 429 too that day, through
3.5 minutes of retries. `ci-setup.sh` now fetches from git.kernel.org first
(the pinned SHA-256 values match), sends a named User-Agent (git.kernel.org
answers 403 to Python's default), keeps an already verified copy, and KDAL's
job caches the files (#105, KDAL #29).

**G4:** "quality gate" is a required status check (from GitHub Actions) on
`main` in haven, ELMOS and KDAL since 2026-11-13; KDAL's "style and static
analysis" is no longer required and its job is gone. Checked by reading the
rulesets back. A pull request with a failing gate being refused a merge was
not tried (it would leave a public test pull request); the evidence is the
configuration and GitHub's documented meaning of a required check.

## G5: the deep tier (design and expectations, before code)

Owner-approved 2026-11-13. Known before writing this: scan-build 23.1.5,
sparse 0.6.4 and Coccinelle 1.3.3 are installed on the Mac, which has no
Linux kernel tree; KDAL's KUnit CI job already downloads the Linux v6.18
source and runs `modules_prepare`; HAVEN has a native `host-tests` CMake
preset run by CTest; KDAL has an `asan` preset for its userspace tools and
smoke tests in `scripts/ci/test.sh`; ELMOS has 31 Go tests and no fuzz
targets; `demo.Untar` and `demo.parseSums` read files downloaded from the
network.

### Design decisions

- **D11, no scan-build.** The `ci` stage already runs the Clang static
  analyzer through clang-tidy's `clang-analyzer-*` checks with the same
  LLVM; scan-build would report the same paths a second time.
- **D12, sanitizers (G5a, Mac, `deep`).** A `sanitizers` runner configures
  a profile's CMake preset into `build-deep/` with
  `-fsanitize=address,undefined -fno-omit-frame-pointer
  -fno-sanitize-recover=undefined`, builds it and runs the profile's test
  command (HAVEN: CTest on `host-tests`; KDAL: its smoke tests against the
  sanitized `kdalc` and `kdality`). Each AddressSanitizer or UBSan report
  is CRITICAL, a failing test MAJOR. A `go-race` runner runs
  `go test -race ./...` for ELMOS: a data race CRITICAL, a failing test
  MAJOR. LeakSanitizer does not run on macOS on Apple silicon, so leaks are
  not covered here.
- **D13, sparse and Coccinelle in CI (G5b).** They need a configured kernel
  tree, which only KDAL's CI has, and on that tree they take seconds to
  minutes; the owner's rule is to run in CI what can run there. KDAL's
  "quality gate" job prepares the same v6.18 tree (the KUnit job's script)
  and the gate runs `make C=2 CHECK=sparse` and `make coccicheck
  MODE=report` on the module. sparse warnings are MAJOR, its errors
  CRITICAL; Coccinelle's ERROR reports MAJOR, others MINOR. On the Mac,
  with no kernel tree, both report INFO "not run".
- **D14, fuzzing (G5c, Mac, `deep`).** Go fuzz targets for `demo.Untar`
  (with a property: no file is written outside the target directory) and
  `demo.parseSums`. `deep` fuzzes each for 60 s; a crash or a property
  failure is CRITICAL, and its input is kept under `testdata/fuzz/` so that
  plain `go test` (CI) replays it from then on.
- **D15, policy.** `deep` blocks nothing (as designed in G0); it prints the
  report and writes JSON when asked. The `pre-push` and `ci` findings of the
  three repositories do not change.

### Expectations

| Row | Check | Expected |
| - | - | - |
| Z1 | HAVEN host tests under ASan and UBSan | all pass; 0 to 2 sanitizer reports |
| Z2 | KDAL smoke tests against sanitized `kdalc` and `kdality` | 1 to 5 sanitizer reports (the analyzer's null dereferences in `compiler/lexer.c` among the candidates) |
| Z3 | ELMOS `go test -race` | 0 races; tests pass as in CI |
| Z4 | seeded: a HAVEN host test writing one element past an array | `deep` reports a CRITICAL AddressSanitizer finding naming the test file |
| Z5 | seeded: an ELMOS test with two goroutines writing one variable | `deep` reports a CRITICAL data race |
| Z6 | `deep` time on the Mac | HAVEN, KDAL and ELMOS each under 5 min (fuzzing adds 2 min to ELMOS) |
| Z7 | `baseline --check` in all three after G5 | 0 new, 0 fixed |
| K7 | KDAL CI: sparse on the module | 0 to 20 warnings |
| K8 | KDAL CI: Coccinelle report | 0 to 10 |
| K9 | seeded on a pushed branch (no pull request, deleted after): a `__user` pointer dereferenced directly in `src/` | the branch's "quality gate" run fails with a new sparse finding |
| K10 | KDAL "quality gate" job with the kernel tree | under 6 min |
| K11 | KDAL on the Mac | sparse and Coccinelle give INFO "not run"; every other count equals CI's |
| F1 | 60 s of fuzzing each for `Untar` and `parseSums` | 0 to 2 crashes or property failures |
| F2 | a kept fuzz input | replayed by plain `go test` in CI |
| G4b | owner-approved: a pull request on ELMOS whose gate fails | GitHub refuses the merge (`mergeStateStatus` BLOCKED, `gh pr merge` refused); the pull request is closed and its branch deleted afterwards |

