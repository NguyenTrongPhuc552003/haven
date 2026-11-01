# A1 case set v2 (proposal, needs owner approval)

**Status:** proposed 2026-11-02, not frozen, nothing run as a scored set.
Claude drafted the cases; the owner approves or changes each expected
read-back verdict before the run, as for case set v1 (`CASES.md`).

**Method.** As v1, with probe v2 (`tools/readback/readback.py`) and grant v2
(`grant-qemu-v2.json`: case set v1 grant plus the partition A read-only region
and an `smmu` section). Every case boots with `-device virtio-rng-pci`
(hardware SID 8, behind the SMMU) on cortex-a72 and max, and is also run
through the host unit tests and the QEMU boot check.

**Blind or not.** R1, S1, S2, S5 and S6 were already run as feasibility runs
(`RESULTS.md` sections 7 and 8), so their expected verdicts are not blind;
they are scored as regression checks. S7, U1, C1 and C5 have not been run.

| Case | Seeded change | Expected read-back verdict (owner approves) | Unit tests (predicted) | Boot check (predicted) |
| - | - | - | - | - |
| R1 | MM1 revert: S2AP always read/write | access drift on A RO | fail | pass |
| S1 | default stream entry BYPASS | DMA enforcement drift on SID 8; extra stream binding (all 256) | pass | pass |
| S2 | SMMU never enabled | SMMU enable drift; DMA enforcement drift on SID 8 (GBPA bypass) | pass | pass |
| S5 | grant the device to A through the current API (SM1) | DMA enforcement drift on SID 8 (still denied); extra stream binding on SID 0 | pass | fail (SM3 hang) |
| S6 | as S5 with the allocator forced to SID 8 (SM2) | DMA extent drift on SID 8 (reach is all of partition A, window 1 MiB) | pass | fail (SM3 hang) |
| S7 | as S6 but the entry points at partition B | DMA enforcement drift on SID 8 (translated to partition 2, expected 1) | pass | fail (SM3 hang) |
| U1 | none; adds `-device virtio-rng-device` | unenforced DMA master (virtio-rng-device) | pass | pass |
| C1 | none (baseline, grant v2) | enforced state matches the grant | pass | pass |
| C5 | behaviour-preserving refactor of `ste_write_abort` (same STE bytes) | enforced state matches the grant | pass | pass |

S5, S6 and S7 use the grant that gives SID 8 to partition A with a 1 MiB
window (`runs/v2b/grant-sid8-to-A.json`). Their boot-check failures come from
the SM3 hang, not from the seeded change itself.

## What the owner reviews

1. Is each expected read-back verdict right? Mark Y, N (and what it should
   be) or ? per case.
2. Is any important drift missing? One line each; it becomes an owner-authored
   held-out case.
3. Is `grant-qemu-v2.json` a correct statement of the grant?

After approval: freeze with SHA-256 sums, run, score, report in `RESULTS.md`.
