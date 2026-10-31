# A1 feasibility probe: results (2026-11-01)

**Status:** EXPERIMENT, QEMU only, feasibility probe as specified in
`DESIGN.md` section 7. Not a scored experiment, not a G1 decision, and not a
research distinction (`DESIGN.md` section 6 prior work is not yet rechecked).

## 1. Setup (reproducible)

| Item | Value |
| - | - |
| HAVEN source | branch `stage1/evidence-and-bugfixes`, commit `eab5288` |
| Build | CMake + Ninja, `cmake/arm64.cmake`, `MinSizeRel`, `HAVEN_BOOT_PROFILE=qemu-direct` |
| Toolchain | aarch64-unknown-linux-gnu GCC 15.2.0 (Homebrew) |
| Emulator | QEMU 11.1.3, `virt,virtualization=on,gic-version=3,iommu=smmuv3`, 4 CPUs, 2 GiB |
| CPU models | `cortex-a72`, `max` |
| Probe | `readback.py` (this folder), Python 3.9.6 on macOS |
| Oracle | `expected-G.json`, derived from G and the MM1 proposal section 4 translation table |
| Exact QEMU command | `runs/*/qemu-command.txt` |
| Build and dump hashes | `runs/BUILD_SHA256SUMS`, `runs/DUMP_SHA256SUMS` (8 MiB dumps not committed) |

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

| Run | Read-back H (partition: IPA, PA, size, S2AP, MemAttr, SH, XN, AF) | Verdict | Exit |
| - | - | - | - |
| baseline, cortex-a72 | 1: 0x0900_0000 -> 0x0900_0000, 4 KiB, 3, 0x1, 0, 0b10, 1; 0x4000_0000 -> 0x8080_0000, 512 MiB, 3, 0xF, 3, 0b00, 1. 2: 0x6000_0000 -> 0xA080_0000, 64 MiB, 3, 0xF, 3, 0b00, 1 | H matches G | 0 |
| baseline, max | identical to cortex-a72 | H matches G | 0 |
| mutant, cortex-a72 | as baseline except partition 1 UART XN = 0b11 | execute drift on A UART (XN 0b11, expected 0b10) | 1 |
| mutant, max | identical to mutant cortex-a72 | same | 1 |

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
