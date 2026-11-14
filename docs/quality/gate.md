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
developer's machine), so a check is never skipped silently in CI. A tool
that is installed but fails is MAJOR everywhere (`gate/tool-failed`).

    tools/gate/ci-setup.sh    # installs every tool the profile pins (Linux CI;
                              # its checkpatch step also works on macOS)

## Configuration

`quality/gate.json` in each repository names the tools per stage, the paths
they cover and the limits (for example the complexity limit). The engine
maps each tool's output to the scale above; the mapping is in
`tools/gate/rules.py`, one table per tool.

A tool reads the paths of its language (`c_paths`, `go_paths`, `py_paths`,
`sh_paths`) unless the profile gives it its own under `tool_paths`; KDAL uses
this to run `checkpatch.pl` on the kernel module only. Tools per language:

| Language | Tools |
| - | - |
| C | cppcheck (a profile may set its `enable` list and suppressions), clang-tidy (needs a compile database), checkpatch.pl (kernel C), lizard, jscpd |
| Go | gofmt, golangci-lint (govet, staticcheck, errcheck, ineffassign, unused), govulncheck, lizard, jscpd |
| Python | ruff, jscpd |
| Shell | shellcheck |
| Every file | the engine's scan (secrets, personal names, home paths), gitleaks |

## Other repositories

ELMOS and KDAL run this engine without a copy of it: their
`quality/gate.json` names a haven commit (`engine.repo`, `engine.commit`),
and their `scripts/gate` fetches only `tools/gate/` at that commit into
`~/.cache/haven-gate/<commit>/`, once, then runs it on their own tree with
their own profile and baseline. A newer engine is taken by changing the
pinned commit in a reviewed pull request.

## The deep tier

`scripts/gate deep` adds checks too slow for every pull request. It runs
weekly in CI in each repository (workflow "weekly deep checks", Mondays and
on demand), so it does not depend on anyone's machine being on, and it can
also be run by hand. It blocks no pull request; it lists every finding the
baseline does not hold, and with `--fail-on-new` (as in the weekly
workflows) it exits 1 when one of them is BLOCKER, CRITICAL or MAJOR, so the
scheduled run shows red. Its checks:

| Repository | Deep checks |
| - | - |
| HAVEN | the host tests built with AddressSanitizer and UBSan (`build-deep/`) |
| KDAL | the compiler's e2e test against `kdalc` built with AddressSanitizer and UBSan |
| ELMOS | `go test -race`, and 60 s of fuzzing for each target in the profile; a failing input is kept under the package's `testdata/fuzz/`, so plain `go test` replays it |

Sanitizer reports go to log files under `build-deep/sanitizer-logs/`, so a
test script that hides its error output does not hide them. LeakSanitizer
runs on Linux (the CI runs) and not on macOS on Apple silicon. KDAL's kernel
checks need its pinned kernel tree: sparse runs in the required "quality
gate" job, Coccinelle in the weekly one.

ELMOS and KDAL use this repository's `.github/actions/gate-tools` at the
commit their profile pins, so the CI setup has one copy.

The wrapper `scripts/gate` and the two hooks ELMOS and KDAL keep are copies
of `tools/gate/templates/`; the `copies` check in their `ci` stage fails
(MAJOR) when a copy differs from the templates of the engine they pin.
gitleaks scans a copy of the tracked files, so a developer's untracked
files give no findings. A profile may give cppcheck a header of macro
definitions (`cppcheck.include`), as KDAL does for the kernel's list
macros.

