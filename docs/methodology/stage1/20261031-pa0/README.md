# P-A0 evidence package: candidate A rejection pilot

**Status: PAUSED at the independence gate. Steps 1-3 done; steps 4-7 not run.**
No candidate checker exists. No verdict has been reached. Classification:
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

## Gate

The workspace owner decided on 2026-10-31 to keep P-A0 paused here and supply
sealed held-out cases. Do not read or create anything under a `held-out/` folder,
and do not start step 4 until the owner releases those cases.

A changed freeze means rerunning step 3.
