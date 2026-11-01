# A1 case set (proposal, needs owner approval)

**Status:** proposed 2026-11-01, not frozen, nothing run. Claude drafted the
cases, so the expected read-back verdicts must be approved (or changed) by the
owner before any run is scored, as in P-A0.

**Method.** Each case is a disposable copy of `stage1/integration` with one
seeded change in the arch layer (`arch/arm64/mm.c` or `asm/s2_attrs.h`). The
grant G (`src/core/partition.c`) and the envelope E never change. Each case is
built with the qemu-direct profile and run on cortex-a72 and max through:

1. **Read-back** (`readback.py` against `expected-G.json`): the method under test.
2. **Host unit tests** (`ctest`, as in CI): existing check.
3. **QEMU boot check** (`scripts/qemu/qemu-direct-check.sh`): existing check.

A case counts for A1 only if read-back reports the expected drift **and**
neither existing check fails. The existing-check columns below are
predictions to be measured, not part of the oracle.

## Seeded faults

| Case | Seeded change (all blocks unless stated) | Expected read-back verdict (owner approves) | Unit tests (predicted) | Boot check (predicted) |
| - | - | - | - | - |
| D1 | L2 block output address shifted by +2 MiB | association drift on A RAM; cross-partition alias with B RAM (last block lands at 0xA080_0000) | pass | pass, or guest faults if code moves |
| D2 | Mapping loop maps one extra 2 MiB block past the region end | extent drift / extra binding after A RAM | pass | pass |
| D3 | `hv_s2_hw_attrs` ignores `HV_MEM_DEVICE` | memory-type drift and shareability drift on A UART | **fail** (`test_stage2_attrs` covers this function) | pass |
| D4 | Last 2 MiB block of a region skipped | missing binding on A RAM (last 2 MiB unmapped) | pass | pass |
| D5 | S2AP forced read-only on Normal memory | access drift on A RAM and B RAM | **fail** (`test_stage2_attrs`) | fail (guest writes fault) |
| D6 | Access flag cleared on L2 blocks | access-flag drift on A RAM and B RAM | pass | fail (access-flag faults) |
| D7 | MM2 revert: XN = 0b11 (already observed in the feasibility probe) | execute drift on A UART | **fail** (`test_stage2_attrs`) | pass |

## Controls (expected: no drift)

| Case | Change | Expected read-back verdict | Why it is here |
| - | - | - | - |
| C1 | none (baseline) | H matches G | false-positive check |
| C2 | MM1 revert (S2AP always read/write) | **H matches G**, because the current G grants no read-only region | shows a limit: read-back only sees drift that the grant makes visible |
| C3 | behaviour-preserving refactor of the mapping loop (same descriptors) | H matches G | read-back must not flag code changes that keep enforcement equal |

## What the owner reviews

1. Is each expected read-back verdict right? Mark Y, N (and what it should
   be) or ? per case.
2. Is any important drift missing? One line each; it becomes your own held-out
   case, built from your description without Claude seeing the expected
   verdict until scoring.
3. Is `expected-G.json` a correct statement of the grant?

After approval the case list and expected verdicts are frozen with SHA-256
sums, then run and scored with the same rules as P-A0.
