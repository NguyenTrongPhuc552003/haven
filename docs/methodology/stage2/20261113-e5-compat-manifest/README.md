# E5: one compatibility manifest for the cross-repository pins

**Status:** owner-approved step (2026-11-11, confirmed 2026-11-13). Design
and expectations by Claude, merged before any code; results added by a
later pull request, expectations never edited after them. Labels:
**[F]** fact from our code, records or CI logs, **[I]** inference,
**[P]** proposal.

## Problem

[F] After E4 the pins that tie HAVEN's CI to the other repositories are
literals spread over `.github/workflows/ci.yml` (job `linux-partition`):
the ELMOS SDK tag `linux-6.18-4`, the KDAL commit `f11e87e48f91...`, the
runner `ubuntu-24.04`; and some are not written anywhere a tool can read:
the SDK's manifest digest, the ELMOS commit it was built from, the QEMU
version the runner provides, the HAVEN commit since which these pins are
tested. Records cite them by hand. A second copy of a pin can drift from
the first without anything failing.

## Design

**File:** `ecosystem.lock.json` at the root of haven. Why this name and
place: it is a lock file in the usual sense (exact versions of things
HAVEN depends on but does not build), at the root where the other
repositories, the records and a reader of the clone find it; JSON because
CI and every tool here read it with Python's standard library (no YAML
parser on the runner is assumed). No umbrella repository (decision of
2026-11-06: pins live in haven).

Content (every value from a CI log or a record, below):

| Key | Value | Source |
| - | - | - |
| `schema` | `haven-ecosystem-lock/1` | this record |
| `elmos_sdk.package` | `ghcr.io/nguyentrongphuc552003/elmos-sdk` | ELMOS `docs/developer/sdk-ci.md` |
| `elmos_sdk.tag` | `linux-6.18-4` | E4 |
| `elmos_sdk.digest` | `sha256:b2eaad501c6888a5dfa89323bedc89d45f5d0711508b94d1a3ce89f0d82126eb` | E4's CI log (`oras pull`, run 43083602847) |
| `elmos_sdk.elmos_commit` | `4eb897cb6e6f8f005b27e6049a452e55952b8525` | the package's `manifest.json` (same log) |
| `elmos_sdk.run` | `4` | the same manifest |
| `kdal.repository`, `kdal.commit` | `NguyenTrongPhuc552003/kdal`, `f11e87e48f91726e7391b7bc153ab63da5e00d8b` | E4 |
| `ci.runner` | `ubuntu-24.04` | `ci.yml` |
| `ci.qemu` | `8.2.2` (upstream version; the Ubuntu package revision seen in E4, `1:8.2.2+ds-0ubuntu1.18`, is printed, not pinned) | E4's CI log |
| `haven.tested_from` | `9d537420bf8c569ea203c695aef0afd1294e3429` (E4's change, the first commit CI booted Linux with these pins) | E4 |

The tested range is `haven.tested_from` up to every later commit that
carries the file unchanged, since CI runs the job on each pull request;
a change of any pin moves `tested_from` to that change's commit in the
same pull request.

Why the QEMU check uses the upstream version only [I]: it decides what
QEMU emulates (for example SMMU stage 2, E4 row E7); Ubuntu's package
revisions are fixes within that version, and failing CI on each of them
would make the pin noise rather than information.

**Tool:** `scripts/ci/ecosystem.py` (standard library only):

- `outputs`: writes the pins as `key=value` lines for `$GITHUB_OUTPUT`;
- `check-workflow ci.yml`: fails unless the `linux-partition` job's
  `runs-on` equals `ci.runner`, and unless `ci.yml` contains none of the
  pinned literals (KDAL commit, SDK tag, digest, ELMOS commit), so the
  file is the only copy;
- `check-sdk DIR`: fails unless the pulled `manifest.json` names
  `elmos_commit` and `run`;
- `check-qemu "<qemu-system-aarch64 --version, first line>"`: fails
  unless the version is `ci.qemu`.

**CI, job `linux-partition`:** after the checkout, a step `pins` runs
`check-workflow` and `outputs`; KDAL is checked out at
`steps.pins.outputs.kdal_commit`; the SDK is pulled **by digest**
(`package@digest`), and while logged in `oras resolve package:tag` must
give the same digest (a moved tag fails the job); then `check-sdk`; the
tool step runs `check-qemu`. Nothing else changes; the other jobs are
not touched.

## Expectations

| Row | Check | Expected result |
| - | - | - |
| C1 | `ecosystem.lock.json` | valid JSON with exactly the keys and values of the table above |
| C2 | `check-workflow` on the new `ci.yml` (macOS) | exit 0; exit 1 on a copy whose `linux-partition` job says `runs-on: ubuntu-latest`, and on a copy that names the KDAL commit literally |
| C3 | `ci.yml` | none of the four pinned literals appears (grep: 0 matches each) |
| C4 | `check-sdk` and `check-qemu` (macOS, on E4's values) | exit 0 on the right values, exit 1 on run 3 or on `QEMU emulator version 11.1.3` |
| C5 | the `linux-partition` job on the code pull request | passes: the pins step prints the pins, the pull is by digest, `oras resolve` gives the pinned digest, `check-sdk` and `check-qemu` pass, the runs show `N = 2: OK` and `N = 3: OK` as in E4 |
| C6 | job time | under 3 minutes (E4: under 2) |
| C7 | everything else | other jobs unchanged and green; no C or header file changes |

Verdict rule as before. Not shown: what happens when a pin is changed on
purpose (the first such change is the test), or the other repositories
reading this file (they do not yet).

## Results (2026-11-13, macOS host and CI)

Expectations merged first (haven `7b7f882`, pull request 81); the change
after that (commit `cea197d`, merged as `484271b`, pull request 83, CI
green on every job). C1 to C4 on the macOS host with a throwaway script
(copies of `ci.yml` and fake manifests outside the repository); C5 to C7
from CI run 43340529914 (pull request event), log read through the API.

| Row | Observed | Verdict |
| - | - | - |
| C1 | keys `schema`, `elmos_sdk` (`package`, `tag`, `digest`, `elmos_commit`, `run`), `kdal` (`repository`, `commit`), `ci` (`runner`, `qemu`), `haven` (`tested_from`), with the table's values; `outputs` prints them | as expected |
| C2 | new `ci.yml`: `OK ... (runner ubuntu-24.04)`, exit 0; `runs-on: ubuntu-latest`: `FAIL: linux-partition runs-on 'ubuntu-latest', pinned 'ubuntu-24.04'`, exit 1; KDAL commit written into `ref:`: `FAIL: ... names the KDAL commit (f11e87e48f91...) itself`, exit 1 | as expected |
| C3 | `f11e87e`, `linux-6.18-4`, `b2eaad5`, `4eb897c`: 0 matches each in `ci.yml` | as expected |
| C4 | `check-sdk`: run 4 exit 0, run 3 exit 1; `check-qemu`: the E4 line exit 0, the Mac's `QEMU emulator version 11.1.3` `FAIL: QEMU '11.1.3', pinned 8.2.2`, exit 1 | as expected |
| C5 | `OK: .github/workflows/ci.yml takes its pins ...`; `Pulled [registry] ghcr.io/nguyentrongphuc552003/elmos-sdk@sha256:b2eaad50...` (by digest); the resolve comparison passed (the step would have stopped with `FAIL: ... resolves to`); `OK: SDK built from ELMOS 4eb897c, run 4`; `OK: QEMU 8.2.2 (QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18))`; `N = 2: OK (5 markers)`, `N = 3: OK (6 markers)`; DMA grant withheld as in E4 | as expected |
| C6 | 1 min 31 s (00:52:37 to 00:54:08 UTC) | as expected |
| C7 | the other four jobs passed; the change is `ci.yml`, `ecosystem.lock.json` and `scripts/ci/ecosystem.py` only | as expected |

**Verdict.** 7 of 7 rows hold. `ecosystem.lock.json` is now the only
copy of the pins in haven: `ci.yml` reads them and CI fails if a copy
appears, the runner differs, the tag moves, the package's manifest or
the runner's QEMU disagrees. No token string in the job log. [I] `oras
logout` runs from the step's `trap` at exit; it prints nothing on
success (as in E4), so the log cannot show it.

Records cite the file from now on instead of repeating the values. Not
yet [P]: KDAL's and ELMOS's own CI pin the same package themselves (KDAL
`ci.yml`); reading this file from those repositories, or checking that
their pins agree with it, is a later step if wanted.
