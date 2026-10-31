# MM1 - Stage-2 permission semantics: proposal

**Status:** DOCUMENTATION. Decisions D1-D4 approved by the workspace owner
2026-10-31 (section 6). Implementation not started; it needs its own BUGFIX
approval. No code changed.
**Owner repo:** HAVEN. **Revision read:** `f775304` (`arch/arm64/mm.c`,
`arch/arm64/include/asm/page.h`, `src/core/mm/stage2.c`,
`include/haven/stage2.h`, `src/core/partition.c` unmodified in the working tree).
**Related:** [MM1](../../stage0/20261025-corrections/FINDINGS.md) ·
`stages/STRATEGY.md` Phase 1 · `stages/CARRIED_RISKS.md` (MM1 row).

## 1. What the code does today (source-derived, f775304)

- `struct hv_mem_region` (`include/haven/stage2.h`) carries an opaque
  `hv_u64 attrs`. `hv_stage2_map_partition` (`src/core/mm/stage2.c:167`)
  validates alignment, overflow and PA overlap, then passes `attrs` to
  `hv_arch_stage2_map` unchecked.
- `hv_arch_stage2_map` (`arch/arm64/mm.c:99`) treats `flags` as a two-way
  switch: `1` → Device nGnRE, `S2AP_RW`, `S2_XN_ALL`; anything else → Normal WB,
  `S2AP_RW`, executable. Its header comment claims `S2AP_* | S2_MEMATTR_* |
  S2_SH_*` bits.
- It is the only caller of `hv_arch_stage2_map`. Callers of the core API pass
  only 0 or 1: `src/core/partition.c` (guest RAM 0, UART 1) and about ten test
  files (`tests/unit`, `tests/integration`, `tests/isolation`,
  `tests/selftests`, `tests/benchmarks`).
- **The SMMU shares these tables.** `drivers/iommu/smmu_v3.c` programs
  `STE.S2TTB` with the partition's stage-2 root (`mm.c:289-301`). Whatever
  permissions stage 2 encodes also govern device DMA for that partition.

Consequence: no caller can obtain a read-only, no-access or non-executable
Normal mapping; least privilege cannot be expressed for CPU or DMA.

## 2. New candidate finding: stage-2 execute-never encoding (MM2)

- **Source-derived:** `page.h:80-86` names bit 53 "EL1 execute never" and bit 54
  "EL0 execute never", and defines `S2_XN_ALL` as both bits. `mm.c:121` uses
  `S2_XN_ALL` for device mappings; `page.h:103,108` use it in `S2_PAGE_RO/RW`.
- **Architecture semantics (secondary source):** QEMU's implementation of
  ARMv8.2-TTS2UXN (FEAT_XNX) decodes bits 54:53 as a two-bit `XN[1:0]`:
  `00` executable at EL1 and EL0; `01` EL0 only; `10` neither; `11` **EL1 only**.
  Without FEAT_XNX, bit 53 is ignored and bit 54 alone means execute-never.
  ([QEMU patch](https://patchwork.kernel.org/patch/11466337))
- **Inference:** on a CPU without FEAT_XNX (QEMU `-cpu cortex-a72`, used in the
  Stage 0 runs) `S2_XN_ALL` works as intended. On a CPU with FEAT_XNX, the same
  value leaves device memory **executable at EL1**. Whether the CIX P1 cores
  implement FEAT_XNX is unknown; read `ID_AA64MMFR1_EL1.XNX` on the board.
- **Verify before relying on this:** confirm the `XN[1:0]` table in the Arm ARM
  (stage 2 Block and Page descriptor attributes) and record the section.
- **Correct encoding for "never execute":** bit 54 set, bit 53 clear (`XN = 10`).
  That value means non-executable at both ELs with or without FEAT_XNX.
- **Proposed:** record as MM2 in the finding ledger, owner's OK required.

## 3. Options for the meaning of `attrs`

|                                | Option A: portable permission bits (recommended)                             | Option B: raw descriptor bits                                 | Option C: keep 0/1, fix the comment |
| ------------------------------ | ---------------------------------------------------------------------------- | ------------------------------------------------------------- | ----------------------------------- |
| What `attrs` means             | `HV_MEM_R`, `HV_MEM_W`, `HV_MEM_X`, `HV_MEM_DEVICE` (portable, arch-neutral) | `S2AP_* \| S2_MEMATTR_* \| S2_SH_* \| XN` as the comment says | memory type only                    |
| Least privilege                | yes                                                                          | yes                                                           | **no**                              |
| Core can validate              | yes: unknown bits, forbidden combinations                                    | no: core would need ARM64 knowledge                           | nothing to validate                 |
| Arch leakage into config/tests | none                                                                         | ARM64 encodings in partition configs and host tests           | none                                |
| MM2 fixed                      | in the arch translation, once                                                | every caller must pass the right XN bits                      | separately                          |
| P-A0 F6 (no permission field)  | becomes expressible                                                          | expressible, but not portable                                 | stays unexpressible                 |
| Thesis claims supported        | per-region R/W/X isolation for CPU and DMA                                   | same, harder to audit                                         | type isolation only                 |

Option C would rule out every read-only-sharing or least-privilege claim. B
contradicts HAVEN's existing split, where `src/core` is arch-neutral and
`#ifdef HAVEN_ARCH_ARM64` guards the arch calls.

## 4. Proposed design (Option A)

**Portable bits** in `include/haven/stage2.h`:

```c
#define HV_MEM_R       (1ULL << 0)
#define HV_MEM_W       (1ULL << 1)
#define HV_MEM_X       (1ULL << 2)
#define HV_MEM_DEVICE  (1ULL << 3)   /* clear = Normal WB cacheable */
#define HV_MEM_VALID_MASK (HV_MEM_R | HV_MEM_W | HV_MEM_X | HV_MEM_DEVICE)
```

**Core validation** in `hv_stage2_map_partition`, before any state changes
(fail closed):
- unknown bits → `HV_EINVAL`;
- `attrs == 0` → `HV_EINVAL` (see decision D2);
- `HV_MEM_DEVICE | HV_MEM_X` → `HV_EINVAL` (executing from MMIO is never valid);
- `HV_MEM_W` without `HV_MEM_R` → `HV_EINVAL` (write-only is not a use case here).

**Arch translation** as a pure function, so it can be tested on the host
without compiling the rest of `mm.c`:

```c
/* arch/arm64/include/asm/s2_attrs.h */
static inline uint64_t hv_s2_hw_attrs(uint64_t attrs);
```

| `attrs`                   | S2AP | MemAttr / SH      | XN (bits 54:53) |
| ------------------------- | ---- | ----------------- | --------------- |
| R                         | `RO` | Normal WB / IS    | `10`            |
| R\|W                      | `RW` | Normal WB / IS    | `10`            |
| R\|X                      | `RO` | Normal WB / IS    | `00`            |
| R\|W\|X (guest RAM today) | `RW` | Normal WB / IS    | `00`            |
| DEVICE\|R                 | `RO` | Device nGnRE / NS | `10`            |
| DEVICE\|R\|W (UART today) | `RW` | Device nGnRE / NS | `10`            |

`S2_XN_ALL` is redefined as bit 54 only, and the macros are renamed to the
`XN[1:0]` field (MM2). `hv_arch_stage2_map`'s comment is corrected to say it
takes portable bits.

**Callers updated in the same change:** `partition.c` guest RAM → `R|W|X`,
UART → `DEVICE|R|W`; each test site gets the explicit value that preserves its
current behaviour. Because 0 becomes invalid, a missed caller fails loudly at
map time instead of silently mapping RWX.

**DMA note:** because the SMMU walks the same tables, `R` without `W` also
makes the region read-only for that partition's devices. That is the desired
property for read-only sharing, but CPU and DMA permissions cannot differ per
region without separate tables. Record that as a design limit.

## 5. Acceptance tests (evidence ladder)

| Rung | Test                                                                                              | Expected                                                                                                                                              |
| ---- | ------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| L1   | Host unit: core rejects unknown bits, 0, `DEVICE\|X`, `W` without `R`; accepts the six valid rows | `HV_EINVAL` / `HV_OK`, no state change on rejection                                                                                                   |
| L1   | Host unit: `hv_s2_hw_attrs` against the table above, including XN = `10`                          | exact descriptor bits                                                                                                                                 |
| L1   | Existing host suite unchanged                                                                     | 19/20 as now (`bench_temporal_isolation` timeout tracked separately)                                                                                  |
| L2   | QEMU positive: guest reads an `R`-only page                                                       | read succeeds                                                                                                                                         |
| L3   | QEMU negative: guest writes the `R`-only page                                                     | stage-2 permission fault taken to EL2: Data Abort from a lower EL (ESR_EL2.EC 0x24), permission-fault DFSC; HAVEN records it, the write does not land |
| L3   | QEMU negative: guest executes from an `R\|W` page                                                 | Instruction Abort from a lower EL (EC 0x20), permission fault                                                                                         |
| L3   | Repeat both negatives with `-cpu max` (FEAT_XNX present) as well as `cortex-a72`                  | same faults on both; this is the MM2 regression                                                                                                       |

Check the ESR encodings against the Arm ARM when writing the test. The L3 tests
also need HAVEN's lower-EL abort path to report the fault rather than halt;
whether `el2_exceptions.c` does that today is unverified.

Out of scope here: SMMU DMA write to a read-only region (a later L3 test once
the SMMU path runs under QEMU), board runs (L4/L5).

## 6. Decisions (approved by the workspace owner, 2026-10-31)

All four approved as recommended: **D1** Option A; **D2** `attrs == 0` is
invalid; **D3** forbidden combinations as in section 4, no W^X rule for Normal
memory; **D4** MM2 recorded in the finding ledger.

Original questions:

- **D1:** adopt Option A? (Alternatives: B, C.)
- **D2:** make `attrs == 0` invalid (recommended; forces every caller to state
  permissions), or keep 0 as a legacy alias for `R|W|X`?
- **D3:** confirm the forbidden combinations in section 4. W^X for Normal
  memory is not proposed, because guest kernels normally need RWX at stage 2.
- **D4:** record MM2 in the finding ledger as source-derived, pending the Arm
  ARM check and a `-cpu max` run?

After D1-D3, the implementation is one approved BUGFIX in HAVEN: core
validation, the arch translation, MM2's XN fix, and caller updates, plus the L1
tests. The L2/L3 QEMU tests follow once the launch command is reproducible
(`stages/STRATEGY.md` Phase 1).
