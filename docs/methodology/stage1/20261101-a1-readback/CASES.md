# A1 case set (proposal, needs owner approval)

**Status:** approved by the owner and frozen 2026-11-01 (`freeze/SHA256SUMS`);
not yet run. Claude drafted the cases; the owner approved every expected
read-back verdict, as required by the P-A0 protocol.

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

## Owner review (2026-11-01)

| Question | Owner answer |
| - | - |
| Is each expected read-back verdict right? | Yes, all ten cases (D1 to D7, C1 to C3) |
| Is any important drift missing (held-out case)? | No case added for now |
| Is `expected-G.json` a correct statement of the grant? | Yes |

The case list, expected verdicts and `expected-G.json` are frozen by
`freeze/SHA256SUMS`. Changing any of them requires a new owner review and a new
freeze. No owner-authored held-out case exists, so results are reported as
seeded-mutant results, not as independent held-out validation.
