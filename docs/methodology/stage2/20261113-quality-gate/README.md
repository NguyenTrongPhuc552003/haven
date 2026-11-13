# Quality gate for HAVEN, ELMOS and KDAL (Q, G0 to G5)

**Status:** design and expectations for G1 (HAVEN), merged before any gate
code and before any measurement of the tree (owner-approved 2026-11-13: the
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

## Results

Added after G1's runs.
