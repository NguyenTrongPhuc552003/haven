# E4: Linux in partition A on every pull request (CI, ELMOS SDK kernel)

**Status:** owner-approved step (2026-11-11). Design and expectations by
Claude, merged before any code; results added by a later pull request,
expectations never edited after them. Labels: **[F]** fact from our code,
records or CI logs, **[I]** inference, **[P]** proposal.

## Problem

[F] HAVEN's CI boots HAVEN in QEMU with its demo guests only. Every run
with Linux in partition A (L4, the chain case set, SMP1 to SMP3, the N-vCPU
step) ran on the owner's Mac, with a kernel built there by hand
(`Linux version 6.18.0-dirty`, Homebrew clang) and QEMU 11.1.3. A change
that breaks the Linux partition is therefore seen only when someone runs
those scripts. ELMOS now publishes a Linux 6.18 kernel from its own CI as
the private package `ghcr.io/nguyentrongphuc552003/elmos-sdk` (ELMOS
`docs/developer/sdk-ci.md`), which KDAL's CI already uses (KDAL
`docs/ci-sdk.md`), and HAVEN has Read access to it.

## Design

A new job in `.github/workflows/ci.yml`, `linux-partition`, on its own
(no step is added to the existing jobs), `runs-on: ubuntu-24.04`:

1. `permissions: contents: read, packages: read` on this job only.
2. `oras` (`oras-project/setup-oras`), log in to ghcr.io with
   `GITHUB_TOKEN`, pull the pinned tag `elmos-sdk:linux-6.18-4` (as KDAL:
   a new SDK build reaches HAVEN only through a reviewed change), `oras
   logout`; print the manifest's version string, ELMOS commit and run.
3. Check out KDAL at a pinned commit, `f11e87e` (KDAL `main`, pull request
   27), into `kdal/` (public repository, no token needed). Its
   `scripts/ci/kernel-prepare.sh` checks the SDK's `SHA256SUMS`, fetches
   the v6.18 source from kernel.org and runs `modules_prepare`; then
   `kdal.ko` is built against that tree, as the Mac's runs used it (no
   KUnit). The init program and the initramfs come from the same KDAL
   scripts (`kunit-init.c`, `mkcpio.py`), as on the Mac.
4. Install `qemu-system-arm` (Ubuntu's), the cross compiler, `clang`,
   `lld`, `llvm`, the kernel build tools and `dtc`; print the QEMU version.
5. Run `docs/methodology/stage2/20261112-multi-vcpu-n/run_n.py` (HAVEN
   built from the checked-out commit, `qemu-psci`, Linux with N = 2 and
   N = 3 vCPUs in partition A, partition B on CPU 2), then the new checker
   `scripts/qemu/linux-partition-check.py`, which fails the job unless
   each run's UART log has: Linux's `smp: Brought up 1 node, N CPUs`,
   `KDAL-DEMAND-END`, HAVEN's `CPU c ready, partition A vCPU i started`
   for every vCPU (CPU 1 for vCPU 1, CPU 3 for vCPU 2), `partition
   stopped by PSCI OFF`, `partition B started`, and no `not delivered`,
   `EL2 FATAL` or `failed to come online`. On failure the job prints the
   end of each UART log.

Why this run: it is the SMP1 F1 free boot (Linux, KDAL's demand, both
vCPUs, partition B beside it), it covers the N > 2 path, and it does not
depend on a DMA grant. [F] The records say Ubuntu 24.04's QEMU has an
SMMU with stage 1 only, so HAVEN's grant of the virtio device is
withheld there; partition A's tree has no PCI node, so Linux does not use
the device in either QEMU. No newer QEMU is pinned.

Not in CI (this step): read-back. Its comparison includes the SMMU
section and QEMU's device list, which differ on the older QEMU; making
read-back's result match there is not cheap and is left as a proposal.

Checker test: run on the N-vCPU record's committed logs
(`../20261112-multi-vcpu-n/runs/`) it must pass; on a copy with
`KDAL-DEMAND-END` removed from N3's log it must fail.

## Expectations

| Row | Check | Expected result |
| - | - | - |
| E1 | checker on the N-vCPU record's logs (macOS) | passes (exit 0); on the copy without `KDAL-DEMAND-END` in N3 it fails (exit 1) naming that marker and N = 3 |
| E2 | the new job on the code pull request | passes |
| E3 | the pulled package | manifest: `Linux version 6.18.0 (haven@ci)`, ELMOS commit `4eb897c`, run 4; `sha256sum -c` passes; `oras logout` runs in the same step |
| E4 | `kdal.ko` | built against the SDK tree: `kernel.release` 6.18.0 (the check in `kernel-prepare.sh`), vermagic `6.18.0 SMP preempt mod_unload aarch64` |
| E5 | N = 2 run in CI | every marker of the checker; Linux's banner names `haven@ci` |
| E6 | N = 3 run in CI | every marker; `CPU 3 ready, partition A vCPU 2 started` |
| E7 | HAVEN's DMA grant line on CI's QEMU | `withheld` (SMMU without stage 2); if it is granted, the records' statement about that QEMU is wrong and is reported as a prediction error |
| E8 | job time | under 15 minutes |
| E9 | other jobs, images, trusted code | unchanged and green; no C or header file changes, so trusted code stays 66 files / 7,543 lines (qemu) |
| E10 | the job's log | no token value printed (GitHub masks it); no URL of a CI output made public |

Verdict rule as before. Not shown: read-back in CI, a newer QEMU, or the
board.

## Results (2026-11-11 UTC, macOS host and CI)

Expectations merged first (haven `688407a`, pull request 77); the change
after that (commit `9d53742`, merged as `faecdd6`, pull request 78). The
new job ran twice on it (push and pull request event), 1 min 54 s and
1 min 38 s (run 43083602847, 19:03:36 to 19:05:14 UTC). Job log read
through the GitHub API, kept outside the repository.

| Row | Observed | Verdict |
| - | - | - |
| E1 | committed logs: `N = 2: OK (5 markers)`, `N = 3: OK (6 markers)`, exit 0; copy without `KDAL-DEMAND-END` in N3: `N = 3: FAIL`, `missing: KDAL-DEMAND-END`, exit 1 | as expected |
| E2 | `Linux in partition A (QEMU, ELMOS SDK kernel)` passed on both runs | as expected |
| E3 | `Login Succeeded`; `SDK Linux version 6.18.0 (haven@ci) (Ubuntu clang version 18.1.3 ...)`, `elmos 4eb897c`, `run 4`; `oras logout ghcr.io` in the same step; `kernel-prepare.sh` (which stops on a `sha256sum -c` or release mismatch) passed | as expected |
| E4 | `modinfo -F vermagic`: `6.18.0 SMP preempt mod_unload aarch64` | as expected |
| E5 | `N = 2: ... Linux version 6.18.0 (haven@ci) ...`, `N = 2: OK (5 markers)` | as expected |
| E6 | `N = 3: OK (6 markers)`, including `CPU 3 ready, partition A vCPU 2 started` | as expected |
| E7 | QEMU `8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18)`; `HAVEN: DMA grant SID 8 -> partition 1 withheld (rc=-95)` in both runs (-95: not supported) | as expected |
| E8 | under 2 minutes | as expected |
| E9 | every other job green; the change touches only `ci.yml` and the new checker (no C or header file) | as expected |
| E10 | no `ghs_` or `ghp_` string in the job log; the only `releases/download` URL is `setup-oras` fetching the ORAS CLI | as expected |

**Verdict.** 10 of 10 rows hold. From now on every pull request boots
Linux 6.18 (ELMOS SDK, `haven@ci`) with KDAL in partition A on 2 and 3
vCPUs beside partition B, in Ubuntu's QEMU 8.2.2, and fails if a vCPU,
KDAL's demand, the PSCI stop or partition B is missing.

Observations outside the rows [F]: on both E4 pull requests a
`host unit tests (macos-latest)` job of the pull-request run stayed
queued and was cancelled by GitHub; re-running it passed. The job is
not in this step's scope.

Limits [I]: the CI kernel and QEMU differ from the Mac's (6.18.0-dirty
built by hand, QEMU 11.1.3); the runs on the Mac stay the reference for
read-back and DMA grants. Next [P]: read-back in this job once its SMMU
expectations account for a QEMU without stage 2, or with a pinned newer
QEMU.
