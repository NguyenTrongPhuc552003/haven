# P-A0 evidence package: candidate A rejection pilot

**Status: COMPLETE (steps 1-7). Verdict under the frozen rules: candidate A as
framed is REJECT/REFRAME; stop as inconclusive on the device question.** The
final decision is the owner's at gate G1. See "Steps 4-7" below. Classification:
EXPERIMENT (host/model only). Owner: HAVEN. Protocol:
[NEXT_EXPERIMENT.md](../20261025-decision/NEXT_EXPERIMENT.md).

Everything here is a host/model result. Addresses, IRQ 40 and registers are
fictional. Nothing here is QEMU, board, device, DMA or isolation evidence.

## Inputs

| Repo | Revision | Working tree |
| --- | --- | --- |
| haven | `f775304ce9443dc9861ba1880e4208c7ae19b314` | 9 entries, unchanged by this run |
| kdal | `1ab0dce6dedada4162325ba2101db63214ed896c` | clean |
| elmos | `5ef5cd7fb974fca961e1af7f5a7f08ff47f70e95` | 1 untracked `CLAUDE.md` |

Host: Darwin 27.0.0, Apple clang 21.0.0. Disposable copy under the session
scratchpad. Hashes of the frozen draft are in `freeze/SHA256SUMS`.

## Baseline results (experimental observation)

`baseline/harness.c` calls the real `hv_stage2_map_partition` and `hv_irq_assign`
(host build) after structural, range, version and hash checks. Two runs were
byte-identical (`baseline/run1.txt`, `run2.txt`).

| Case | Oracle | Baseline | Checkpoint |
| --- | --- | --- | --- |
| V1, V2 | ACCEPT | ACCEPT | - |
| F1 | REJECT | REJECT | structural range |
| F2 | REJECT | REJECT | `hv_stage2_map_partition` |
| F3 | REJECT | **ACCEPT** | - |
| F4 | REJECT | REJECT | structural version |
| F5 | REJECT | **ACCEPT** | - |
| F6 | REJECT | **ACCEPT** | - |
| F7 | N/A | N/A | non-scoring |
| F8 | REJECT | **ACCEPT** | - |
| F9 | REJECT | **ACCEPT** | - |
| F10 | REJECT | **ACCEPT** | - |

These escapes are not a distinguishing result. Candidate A has not been run.

## Existing host suite

- Unmodified working tree: 6 objects fail `-Werror`, so 6 test binaries do not
  build (`baseline/build-k0.log`, `ctest-k0.log`): of 20 tests, 13 passed, 6 did
  not run and 1 timed out.
- Disposable copy with only `el2_exceptions.c` restored to HEAD: 0 build
  failures, 19 of 19 selected tests pass (`baseline/ctest-head.log`).
- `bench_temporal_isolation` hit the 10s limit once and was then excluded. It is
  not diagnosed.

## Findings

1. **Working-tree host build regression (confirmed).** The Stage 0 diagnostic
   diff removed `(void)esr/far/elr`, and the print using them is ARM64-only.
   Since fixed under separate approval, uncommitted: see the
   [fix record](../20261031-el2-host-build/README.md). The host-suite results
   above are the pre-fix capture.
2. **Stage-2 permissions are not enforceable (source-derived).**
   `arch/arm64/mm.c::hv_arch_stage2_map` documents `S2AP_*` flags, but maps
   `flags == 1` to Device RW and every other value to Normal WB RW. A read-only
   grant cannot be expressed, and an unknown value becomes cacheable RW. Not
   runtime-tested. Recorded as
   [MM1](../../stage0/20261025-corrections/FINDINGS.md) in the finding ledger.
3. **F3 is not an existing control as NEXT_EXPERIMENT.md says (observed).**
   `hv_stage2_map_partition` takes no authorised-owner input and refuses only
   conflicts with existing mappings, so the wrong owner on an unowned page passes.

## Limits on interpreting this

- The oracle and harness were written by Claude after reading `stage2.c`,
  `ownership.c` and `mm.c`, so they are not independent.
- F3, F5, F9 and F10 may reduce to comparing a grant with E. If so, they meet
  NEXT_EXPERIMENT's reject condition rather than supporting candidate A.
- F6 is accepted by construction: the portable API has no permission field.
  Finding 2 may also make it a known defect, which would be non-scoring.
- `hv_stage2_map_partition` runs with `HAVEN_ARCH_ARM64` off, so `mm.c` was not
  executed.

## Gate (resolved 2026-11-01)

Independent oracle review: on 2026-11-01 the workspace owner reviewed the 12
frozen expected verdicts (see `OWNER_REVIEW.md`) and approved all of them
unchanged, in conversation. Part 2 of the sheet (missing fault classes,
duplicate cases) was not answered, so independence rests on the owner's
verdict review, not on an owner-authored held-out case. No `held-out/` folder
was created or read. The freeze still verifies (`freeze/SHA256SUMS`).

## Steps 4-7 (2026-11-01, experimental observation, host/model only)

**Inputs.** Candidate checker `candidate/checker.c` (no expected verdicts in
it); inputs copied verbatim from `baseline/harness.c` plus E and its variants
from `freeze/cases.json`; H (effective binding) is a modelled input equal to G
except F6. Scoring by `candidate/compare.py`, the only reader of `expected`.
Toolchain in `candidate/toolchain.txt`; haven at `candidate/haven-revision.txt`.
The candidate method is the frozen baseline plus relations R1-R4 (fits,
within, granule, enforces).

**Repeatability.** Two candidate runs byte-identical (`candidate/run1.txt`,
`run2.txt`). The frozen baseline re-run on the current branch (MM1 applied;
its `attrs = 1` now means `HV_MEM_R`) gives output identical to the recorded
`baseline/run1.txt` (`candidate/baseline-current-branch.txt`).

**Scores** (`candidate/score.md`): oracle agreement baseline 5/11, candidate
11/11; no false rejection of valid controls. Candidate-only detections among
candidate cases, classified by the relation that fired:

| Case | Relation | Class | Counts toward acceptance? |
| - | - | - | - |
| F5 | R2 association | field comparison against E | No: "every effect is schema/range checking" |
| F9 | R2 IRQ route | field comparison against E | No: same |
| F10 | R2 outside envelope | field comparison against E | No: same |
| F6 | R4 effective access | effective binding | No: rediscovers known defect MM1 |
| F8 | R3 granularity | non-structural relation | Not as framed: its benefit depends on the invented model resource P (a fictional forbidden register), and no credible safe non-DMA device and authoritative map exist (protocol input 3) |

F3 (control) was also caught only by the candidate (R2 owner), confirming
finding 3; as a control it does not count toward the distinction.

**Verdict under the frozen rules.**
- Acceptance is not met: the only non-structural escape (F8) lacks an
  independently justified resource requirement and a credible enforcement
  probe on a real resource.
- Reject/reframe conditions met for candidate A as framed: the remaining
  effects are field comparisons against E or a known bug, and F8's benefit
  depends on invented device behaviour.
- Stop as inconclusive on the device question: no trustworthy resource map of
  a real resource-bearing backend was available.
- P1-P6 recheck for F8 not done; Jailhouse sub-page MMIO handling must be
  checked before any granularity claim.

**What the pilot does establish.** A frozen fault taxonomy, a scoring harness,
and the observation that HAVEN's current policy misses owner, association,
IRQ-route and envelope errors (5/11). These are useful engineering facts, not a
research distinction.

**Missing input to revise the question.** A real, authoritative resource map
whose agreement cannot be reached by comparing fields against E: either
enforcement read-back on QEMU (H observed, not modelled), or a vendor platform
description checked against hardware (for example the reported CIX P1
IORT/SMMU mismatch). Recorded for the G1 decision; nothing proceeds into
Linux/vGIC by default.
