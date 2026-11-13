# The quality gate

One quality gate for HAVEN, ELMOS and KDAL. It runs the analysis tools each
repository needs, rates every finding on one scale, and decides whether a
commit, a push or a pull request may go ahead. The engine lives here
(`tools/gate/`); ELMOS and KDAL fetch it at a pinned HAVEN commit, so there is
one copy.

## Severity scale

| Level | Meaning | Examples |
| - | - | - |
| BLOCKER | must never reach the repository | build or test failure; a token or private key; the Mac's login or host name, a `/Users/` path; an analyzer-confirmed memory error |
| CRITICAL | very likely a bug | high-confidence analyzer reports (clang-tidy `bugprone`/`cert`, the Clang static analyzer), kernel `checkpatch` ERROR, a vulnerable Go dependency that is reached |
| MAJOR | makes the code harder to keep right | a function over the complexity limit, a duplicated block, a lint error |
| MINOR | style | formatting, fixed automatically where a formatter exists |
| INFO | measurement only | trusted-code delta, duplication percentage, complexity summary |

## What blocks where

| Stage | Command | Scope | Blocks on |
| - | - | - | - |
| pre-commit hook | `scripts/gate pre-commit` | staged files | BLOCKER |
| pre-push hook | `scripts/gate pre-push` | files changed against `main` | BLOCKER, CRITICAL, new MAJOR |
| CI, required check "quality gate" | `scripts/gate ci` | whole tree | BLOCKER, CRITICAL, new MAJOR |
| deep (local, on demand) | `scripts/gate deep` | whole tree, slow tools | reports only |

"New" means not in the repository's baseline (`quality/baseline.json`):
each finding is identified by tool, rule, file and a hash of the code it
points at, so moving code does not make an old finding new. Findings that
are fixed drop out of the baseline; the baseline never grows except through
a reviewed pull request that says why.

Hooks can be skipped with `git commit --no-verify`; the CI check cannot, so
it is what every contribution passes.

## Setting it up

    scripts/gate install      # sets the git hooks (once per clone)
    scripts/gate doctor       # lists the tools found and missing

A missing tool is reported as a finding of its own (MAJOR in CI, INFO on a
developer's machine), so a check is never skipped silently in CI.

## Configuration

`quality/gate.json` in each repository names the tools per stage, the paths
they cover and the limits (for example the complexity limit). The engine
maps each tool's output to the scale above; the mapping is in
`tools/gate/rules.py`, one table per tool.
