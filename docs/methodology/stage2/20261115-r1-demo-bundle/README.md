# R1 demo bundle: every scene's inputs built and checked by CI

Owner-approved 2026-11-15 (the R1 demo plan, preparation step "a CI job
that builds the demo bundle and checks every scene's markers"). Design and
expectations before code. Labels: **[F]** fact, **[O]** observation,
**[I]** inference, **[P]** proposal.

## Problem

- [F] The R1 demo (end of January 2027) runs live on the owner's Mac:
  scenes A to C are the try bundle (published as `try-20261115.1`, E7);
  scene D is Linux with KDAL in partition A on 2 and 3 vCPUs, which today
  runs only in CI (`linux-partition` job), because it needs the private
  ELMOS SDK kernel and `kdal.ko` built against it.
- [F] The live demo uses Homebrew's QEMU on the Mac (11.1.3 now); CI's
  Linux job uses Ubuntu's QEMU 8.2.2. [I] Scene D has never run on the QEMU
  the demo will use.
- [F] The SDK kernel is a private package (owner's choice, 2026-11-09), so
  scene D's inputs must not become public.

## Design

1. **One boot loop.** QEMU's free boot with its stop condition (wait for
   "partition stopped by PSCI" or "EL2 FATAL", then stop QEMU) moves from
   `20261108-multi-vcpu/run_smp1.py` into `scripts/qemu/free_boot.py`
   (standard library only); `run_smp1.py` builds the QEMU command and calls
   it, so `run_n.py`, the CI job and the stress workflow are unchanged.
2. **The bundle** (`scripts/release/make-demo-bundle.py`): the try bundle
   archive (from the published release, checked against its
   `SHA256SUMS`); for N = 2 and N = 3 HAVEN's `haven.bin` and `guest_b.bin`,
   partition A's DTB and the QEMU command as JSON with file names only; the
   SDK kernel `Image`, the initramfs (KDAL's `/init` and `kdal.ko`); the
   boot loop, `linux-partition-check.py` and a runner `run-scene-d.py`; a
   copy of `ecosystem.lock.json` (scene E); `MANIFEST.sha256` over every
   file.
3. **`.github/workflows/r1-demo.yml`** (`workflow_dispatch`, input `tag`,
   form `r1-demo-YYYYMMDD[.N]`, and `try-tag`, default `try-20261115.1`):
   - **build** (pinned runner, the `linux-partition` setup action): makes
     the bundle, runs scene D from the bundle's own files on Ubuntu's QEMU,
     and creates a **draft** release titled `... [private: do not
     publish]` with the bundle. The draft is the private channel; it is
     never published.
   - **test-macos** (`macos-15`, Homebrew QEMU and dtc): scenes A to C with
     the published try release (the E7 test action), scene D from the
     draft's bundle.
   - **mark**: writes both results into the draft's notes.
   Nothing publishes a release, uploads an Actions artifact or deletes
   anything.

## Expectations

| Row | Check | Expected |
| - | - | - |
| D1 | build: the bundle | every file of design item 2 present and listed in `MANIFEST.sha256`; the try archive's SHA-256 equals the release's `SHA256SUMS` |
| D2 | build: scene D from the bundle, Ubuntu's QEMU 8.2.2 | `linux-partition-check`: N = 2 and N = 3 OK (the CI job's markers) |
| D3 | test-macos: scenes A to C, Homebrew QEMU | as E7 T5: both partition checks PASS, step 2 clean, the EL2 finding in step 3 |
| D4 | test-macos: scene D from the draft, Homebrew QEMU (11.x predicted) | N = 2 and N = 3 OK. Prediction with low confidence: QEMU 9.2 and later emulate the SMMU stage HAVEN uses, so partition A's DMA grant is given where 8.2.2 withholds it; the markers checked do not depend on it |
| D5 | the draft | `draft: true`, no Git tag, title ends `[private: do not publish]`; no Actions artifact for the run |
| D6 | after design item 1 | the `linux-partition` CI job passes and prints the same 16 result lines as before (apart from timestamps and interrupt counts) |
| D7 | the whole run | under 25 minutes |

Not shown by this record: that the bundle runs on the owner's Mac itself
(the rehearsals in January do that), or on an Intel Mac.

## Results (haven #139; run 43721059747 at `29d1afa`, draft `r1-demo-20261115`)

| Row | Observed | Verdict |
| - | - | - |
| D1 | `haven-r1-demo-v0.6.0-435-g29d1afa.tar.gz`, 15,281,672 bytes; every file of design item 2 present; `MANIFEST.sha256` checked after unpacking (build and macOS); the try archive `haven-try-v0.6.0-425-g13dd128.tar.gz: OK` against the release's `SHA256SUMS` | holds |
| D2 | Ubuntu, QEMU 8.2.2, from the bundle: `N = 2: OK (5 markers)`, `N = 3: OK (6 markers)` | holds |
| D3 | macOS, QEMU 11.1.3: both partition checks PASS, step 2 `RESULT enforced state matches the grant`, step 3 the EL2 finding (`RESULT 1 finding(s)`) | holds |
| D4 | macOS, QEMU 11.1.3, from the draft: `N = 2: OK (5 markers)`, `N = 3: OK (6 markers)` | holds: scene D runs on the QEMU the demo uses |
| D5 | draft 446095445, `draft: true`, title `... [private: do not publish]`; no `r1-demo` Git tag; no Actions artifact | holds |
| D6 | the `linux-partition` job on `main` after #139: the same 16 result lines as before (kernel timestamps and interrupt counts removed) | holds |
| D7 | 2 min 59 s (13:50:04 to 13:53:03 UTC) | holds |

**For the owner [P].** Working rule 2 of `PLAN-2027.md` keeps CI outputs in
private GHCR packages; this design used a never-published draft release,
as E7 did for a bundle meant for publication. Both are private (a draft
is visible only with write access), but the draft is not the mechanism
the rule names and drafts collect in the release list. Proposed: the next
run pushes the bundle as a private package (`oras push`, as ELMOS does
for the SDK) instead of a draft, after the owner's choice.
