# A1 feasibility probe: results (2026-11-01)

**Status:** EXPERIMENT, QEMU only, feasibility probe as specified in
`DESIGN.md` section 7. Not a scored experiment, not a G1 decision, and not a
research distinction (`DESIGN.md` section 6 prior work is not yet rechecked).

## 1. Setup (reproducible)

| Item                  | Value                                                                              |
| --------------------- | ---------------------------------------------------------------------------------- |
| HAVEN source          | branch `stage1/evidence-and-bugfixes`, commit `eab5288`                            |
| Build                 | CMake + Ninja, `cmake/arm64.cmake`, `MinSizeRel`, `HAVEN_BOOT_PROFILE=qemu-direct` |
| Toolchain             | aarch64-unknown-linux-gnu GCC 15.2.0 (Homebrew)                                    |
| Emulator              | QEMU 11.1.3, `virt,virtualization=on,gic-version=3,iommu=smmuv3`, 4 CPUs, 2 GiB    |
| CPU models            | `cortex-a72`, `max`                                                                |
| Probe                 | `readback.py` (this folder), Python 3.9.6 on macOS                                 |
| Oracle                | `expected-G.json`, derived from G and the MM1 proposal section 4 translation table |
| Exact QEMU command    | `runs/*/qemu-command.txt`                                                          |
| Build and dump hashes | `runs/BUILD_SHA256SUMS`, `runs/DUMP_SHA256SUMS` (8 MiB dumps not committed)        |

Command per run:

```
python3 readback.py <build-dir> <aarch64 nm> expected-G.json runs/<run> <cpu>
```

The probe waits for `PARTITION_A: Post-fault check OK` on the UART, then over
QMP issues `stop`, `pmemsave` of PA 0x8000_0000 + 8 MiB and `quit`
(`runs/*/qmp.json` shows every command returned `{}`). It finds `pt_roots`
with `nm` and walks each partition's tables with the Arm descriptor format.

Mutant: a disposable copy of `eab5288` with one line changed in
`arch/arm64/include/asm/page.h` (`runs/mutant-*/mutant.diff`):
`S2_XN_NEVER` from `2UL << 53` (0b10) back to `3UL << 53` (0b11), which is the
pre-MM2 encoding. Built the same way into a separate directory; it never
touched the repository.

## 2. Observations

| Run                  | Read-back H (partition: IPA, PA, size, S2AP, MemAttr, SH, XN, AF)                                                                                                            | Verdict                                          | Exit |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------ | ---- |
| baseline, cortex-a72 | 1: 0x0900_0000 -> 0x0900_0000, 4 KiB, 3, 0x1, 0, 0b10, 1; 0x4000_0000 -> 0x8080_0000, 512 MiB, 3, 0xF, 3, 0b00, 1. 2: 0x6000_0000 -> 0xA080_0000, 64 MiB, 3, 0xF, 3, 0b00, 1 | H matches G                                      | 0    |
| baseline, max        | identical to cortex-a72                                                                                                                                                      | H matches G                                      | 0    |
| mutant, cortex-a72   | as baseline except partition 1 UART XN = 0b11                                                                                                                                | execute drift on A UART (XN 0b11, expected 0b10) | 1    |
| mutant, max          | identical to mutant cortex-a72                                                                                                                                               | same                                             | 1    |

Supporting observations:

- The dumped image is byte-identical between the two CPU models for the same
  build (`DUMP_SHA256SUMS`), so the CPU model did not change the enforced state.
- UART output of all four runs is identical except the build timestamp line
  and the reported PARange (4 on cortex-a72, 6 on max). In particular the
  mutant prints the same `Denied stage-2 violation IPA=0x60000000` and
  `Post-fault check OK` lines as the baseline.
- Partition 2 is never started in qemu-direct (`PSCI CPU_ON failed for CPU 2 -
  partition B inactive`, expected for the NONE conduit), yet its tables are
  built and are read back; external read-back does not need the guest to run.
- No extra bindings and no cross-partition alias were reported in any run.

## 3. Interpretation (inference, not yet a claim)

1. External read-back is practical in QEMU: no HAVEN code change, about 20 s
   per run, and the decoder sees exactly the bindings G describes.
2. The mutant is invisible to the existing guest-level evidence: the boot log
   and the deny-and-continue check are unchanged, and G and E are unchanged
   because the mutant edits only the arch encoding. Only H-ext shows it. This
   is the MM2 situation reproduced on purpose, and it is the kind of drift
   P-A0 could only model, not observe.
3. On a cortex-a72 (no FEAT_XNX) XN 0b11 still means "not executable", so this
   drift has no behavioural effect on this CPU model; a behavioural probe
   (H-probe) would need FEAT_XNX hardware or an EL0 execution test to show it.
   Read-back reports the encoding difference regardless of CPU, which is both
   its strength (it sees latent drift) and a scoring question for later
   (should latent drift count as a fault?).

## 4. Limits and threats to validity

- One mutant, one field, one fault class. The other classes in
  `DESIGN.md` section 5 are untested.
- The oracle (`expected-G.json`) and the probe were written by Claude. Before
  any scored A1 experiment, the owner must review the oracle and seed or
  approve held-out mutants, as in P-A0.
- The probe trusts the ELF symbol table to locate `pt_roots`; a mismatched ELF
  would mislead it. Build hashes are recorded to make the pairing checkable.
- Snapshot only: one read after partition A's post-fault line. Stale bindings
  after unmap, and races during reconfiguration, are not covered.
- QEMU only. On a board, H-ext needs JTAG/SWD memory access; that path is
  unproven for the CIX P1 boards.

## 5. Next steps (proposed, owner decides)

1. Owner review of `expected-G.json` and of the drift taxonomy.
2. Extend seeding to association, extent, memory-type and missing-binding
   drift (descriptor corruption after mapping), then freeze cases and score.
3. Add the GIC view (IRQ route drift) through the same QMP path if QEMU
   exposes the redistributor/distributor state usably.
4. Recheck prior work (`DESIGN.md` section 6) before stating any distinction.

## 6. Scored case set (2026-11-01)

**Status:** EXPERIMENT, observed. Frozen case set (`CASES.md`,
`freeze/SHA256SUMS`, owner-approved), run with `run_cases.py` on haven
`b1dabd8`, cortex-a72 and max (identical results on both). Full table:
`runs/cases/results.md`; logs per case in `runs/cases/<case>/`; memory dumps
removed after hashing (`runs/cases/DUMP_SHA256SUMS`).

One runner fix before the scored run: the D2 and D4 edit first matched two
places in `mm.c` (map and unmap); the runner refused, and the edit was
anchored to `hv_arch_stage2_map`, the place `CASES.md` describes. The frozen
files did not change.

| Case | Read-back | Unit tests | Boot check | Caught only by read-back |
| - | - | - | - | - |
| D1 association | no verdict (probe failure) | pass | **fail** | no |
| D2 extent | expected drift, plus cross-partition alias | pass | **fail** | no |
| D3 memory type | expected drift | **fail** | pass | no |
| D4 missing binding | expected drift (A RAM, and also B RAM) | pass | pass | **yes** |
| D5 access | expected drift | **fail** | pass | no |
| D6 access flag | no verdict (probe failure) | pass | **fail** | no |
| D7 execute | expected drift | **fail** | pass | no |
| C1 baseline | H matches G | pass | pass | control |
| C2 MM1 revert | H matches G (as expected) | **fail** | pass | control |
| C3 refactor | H matches G | pass | pass | control |

**Observations**

- Read-back gave the expected verdict in 5 of 7 seeded faults and no false
  drift in the 3 controls.
- Read-back was the only check to catch 1 of 7 faults (D4). The existing checks
  together caught the other 6: unit tests D3, D5, D7; boot check D1, D2, D6.
- D1 and D6 break partition A before its post-fault line, so the frozen probe,
  which waits for that line, never dumps memory. The boot check catches both.
- D2 and D4 affect every region of 2 MiB or more, so read-back also reported B
  RAM (D4) and a cross-partition alias (D2), beyond the approved verdict.
- Two predictions in `CASES.md` were wrong: D2 fails the boot check (the extra
  block maps the IPA that partition A probes to show a denial), and D5 passes
  it (HAVEN skips denied data accesses, so read-only RAM goes unnoticed).
- C2 behaves as predicted: unit tests catch the MM1 revert, read-back cannot,
  because the current grant has no read-only region.

**Interpretation (inference)**

- In this seeded set, read-back adds narrow but real coverage: layout drift
  that no guest touches and that attribute-level unit tests do not model
  (D4). It also names region and field for every fault it sees, where the
  other checks give pass or fail.
- It is not a replacement for unit tests (C2, D3, D5, D7) or for behavioural
  checks.
- The skip-on-deny policy (EX2) masks write-permission drift in the boot check
  (D5): a reason to keep a structural check that does not depend on guest
  behaviour.

**Threats to validity**

- Claude wrote the seeded faults, the expected verdicts, the probe and
  `test_stage2_attrs` (part of the MM1 fix), so the unit tests are not an
  independent baseline for D3, D5 and D7. No owner-authored held-out case.
- Seven faults, one platform model, one grant; the result is a feasibility
  signal, not a rate.

**Next**

- Probe v2: dump after a fixed time even if the guest stalls (would give
  verdicts for D1 and D6), as a new version with its own freeze.
- Add a read-only region to the grant so access drift such as C2 becomes
  visible; SMMU read-back (experiment G1) next.

## 7. Probe v2 and SMMU read-back (G1), feasibility (2026-11-02)

**Status:** EXPERIMENT, observed; feasibility runs, not a frozen or scored
case set. Tool: `tools/readback/readback.py` (v2); grant:
`grant-qemu-g1.json`; mutants: `try_v2_mutants.sh`; logs: `runs/v2/` (memory,
stream-table and device-tree dumps replaced by `runs/v2/DUMP_SHA256SUMS`).
haven `1c5daf7`, QEMU 11.1.3, cortex-a72 and max (identical results).

**What v2 changes.** Memory is read when partition A reaches its post-fault
line or after a time limit, whichever comes first. New SMMU view: SMMU_CR0ACK,
GBPA and the stream table found through SMMU_STRTAB_BASE, and every DMA-capable
device QEMU emulates, classified as denied, translated, bypassed or
unenforced. Which buses are behind the SMMU comes from the device tree QEMU
generates (`dumpdtb`), not from HAVEN.

| Run | Seeded change | Read-back | Unit tests | Boot check |
| - | - | - | - | - |
| G1 baseline (+ virtio-rng-pci, virtio-rng-device) | none | SMMU on, 256 entries; PCIe device SID 8 denied (STE abort); virtio-mmio device **unenforced** | not run | not run |
| S1 | default stream entry BYPASS instead of ABORT | **DMA enforcement drift**: SID 8 bypassed | pass | pass |
| S2 | SMMU never enabled | **SMMU enable drift** and SID 8 bypassed (GBPA bypass) | pass | pass |
| D1 (v2) | stage-2 output address +2 MiB | association drift on A RAM and B RAM (read at the time limit) | pass (case set) | fail (case set) |
| D6 (v2) | access flag cleared on blocks | access-flag drift on A RAM and B RAM (read at the time limit) | pass (case set) | fail (case set) |

Unit tests and boot check for S1 and S2 were run with the same edits (host
tests via ctest, `scripts/qemu/qemu-direct-check.sh` on cortex-a72): all 20
tests and the boot check passed.

**Observations**

- G1 pass condition met: the PCIe device is classified as enforced (denied)
  and the virtio-mmio device as unenforced; nothing is falsely called enforced.
  None of the 32 virtio-mmio nodes in the QEMU device tree is behind the SMMU.
- S1 and S2 are caught by read-back only: neither guest does DMA, so no
  behavioural check can notice that DMA is no longer enforced.
- v2 gives verdicts for D1 and D6, which v1 could not. D1 shows no
  cross-partition alias, because both partitions move by the same 2 MiB; the
  alias predicted in `CASES.md` for D1 was wrong.

**Interpretation (inference).** DMA enforcement is where read-back adds most:
a missing or bypassed SMMU stream has no behavioural symptom until a device
misbehaves. This is the CIX GPU situation (`docs/architecture/gpu-partition.md`),
where the GPU appears to have no SMMU stream at all.

**Threats to validity.** Claude wrote the mutants and the tool; two SMMU
mutants only; one QEMU machine model; no device actually performs DMA. Before
any claim, these SMMU cases need an owner-reviewed, frozen case set like
sections 5 and 6.
