# BUGFIX: SMMU command queue (SM3), hardware stream IDs (SM1), DMA windows in hardware (SM2)

**Status:** fixed in haven `7ec513e`; verified 2026-11-02 in QEMU with
external read-back v3 (`d5bb678`). Fix verification, not a scored case set:
Claude wrote the checks and their expected verdicts after writing the fix, so
nothing here is blind. The defects are recorded in
`../../stage0/20261025-corrections/FINDINGS.md` section 6.

## Change

| Defect | Change | Files |
| - | - | - |
| SM3 | Command queue in memory (256 x 16 B, 4 KiB aligned) programmed through `SMMU_CMDQ_BASE`; PROD/CONS at 0x98/0x9c; `CMDQEN` enabled and acknowledged before `SMMUEN`; CFGI_ALL + TLBI_NSNH_ALL + SYNC at init; CFGI_STE with SID in dw0 [63:32] and LEAF in dw1 bit 0, TLBI_S12_VMALL for the VMIDs involved, CMD_SYNC with CS=NONE; wrap at bit log2size; every wait bounded, errors return `HV_EIO` (new status) instead of `b .`; CR1 write-back inner shareable | `drivers/iommu/smmu_v3.c`, `arch/arm64/include/asm/smmu.h`, `include/haven/types.h` |
| SM3 (header) | `STRTAB_BASE_ADDR_MASK` [51:6], `STE_3_S2TTB_MASK` [51:4], CR1 field layout, word-1 SHCFG moved from word 0, `smmu_wait_cr0ack` compares every enable bit with a time limit | `arch/arm64/include/asm/smmu.h` |
| SM1 | `hv_smmu_assign_streamid(partition, hw_streamid)` binds the device's StreamID; StreamID space 256 (`HV_MAX_SMMU_STREAMS`), at most 64 bound (`HV_MAX_SMMU_DEVICES`); `allocate` kept and documented as policy-test only | `src/core/dma/smmu.c`, `include/haven/smmu.h` |
| SM2 | Per-stream stage-2 table mapping exactly the window, Normal WB inner shareable, never executable, S2AP from the access (RO 01, WO 10, RW 11); own VMID per window (0xC0 to 0xFF); hardware bound before policy state is committed and unwound on failure | `arch/arm64/mm.c`, `src/core/dma/smmu.c` |
| No stage 2 | An SMMU without stage 2 is still enabled with every stream aborted; grants return `HV_ENOTSUP` | `drivers/iommu/smmu_v3.c` |

Every field was checked against Linux
`drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h` (CR1, queue base, CMDQ
encodings, STE masks) and QEMU 11.1 `hw/arm/smmuv3.c` (command handling).
CR1 cacheable accesses are valid only for a coherent SMMU: the CIX P1
firmware sets `EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE` on all three SMMU
nodes (`data/common/cix-bios/edk2-platforms`, `Platform/CIX/Sky1/Drivers/AcpiSocTables/Iort.aslc`).

Tests: `tests/unit/test_smmu_dma.c` gains hardware-StreamID binding and DMA
VMID slot tests; the two boundary tests that used `HV_MAX_SMMU_DEVICES + 1`
as an invalid StreamID now use `HV_MAX_SMMU_STREAMS`, because the StreamID
space grew. Host tests: 20 of 20 (benchmark excluded).

Read-back v3 (`tools/readback`): walks a stream's table wherever S2TTB points,
attributes a table of its own to the partition owning the physical memory it
maps, checks window extent, coverage, address association and access, reads
the command queue registers, and reports a queue error as a finding.

## QEMU's SMMU model (source-derived)

QEMU 9.2 and later give the `virt` machine's SMMU both stages: `hw/arm/virt.c`
(v11.1.0, line 2075) sets `stage = "nested"` unless the machine type is 9.1 or
older (line 4494), and this overrides `-global arm-smmuv3.stage`. Observed:
`SMMU_IDR0 = 0x0d44101b` (S1P and S2P) on `virt`, `0x0d44101a` (S1P only) on
`virt-9.1`. Case sets v1 and v2 therefore already ran on a stage-2-capable
SMMU. Checks marked stage `1` below use `virt-9.1`.

## Validation (experimental observation, 2026-11-02)

Runner: `run_checks.py` (each check edits a disposable `git archive` copy of
HEAD); results: `runs/results.md`, `runs/results.json`, logs per check;
memory dumps replaced by `runs/DUMP_SHA256SUMS`. QEMU 11.1.3, virtio-rng-pci
(hardware SID 8), cortex-a72 and max.

| Check | Change in the copy | SMMU | Expected | Observed (both CPUs) |
| - | - | - | - | - |
| B0 | none, grant v2 | nested | matches the grant | matches; boot pass |
| B1 | none, grant v2 | stage 1 | matches the grant | matches; boot pass |
| F1 | SID 8 assigned to A, 1 MiB window, RW | nested | matches `grant-sid8-A-rw.json` | matches; `rc=0`; SID 8 translated through its own table (`0x100000` bytes); boot pass |
| F2 | as F1, read-only | nested | matches `grant-sid8-A-r.json` | matches; boot pass |
| F3 | as F1 | stage 1 | grant refused, SID 8 stays denied (DMA enforcement drift against the grant) | `rc=-95` (`HV_ENOTSUP`), SID 8 denied; boot pass |
| M1 | F1 with the STE pointed at A's partition table (SM2 back) | nested | DMA extent drift | DMA extent drift |
| M2 | F1 with `allocate` instead of `assign` (SM1 back) | nested | DMA enforcement drift; extra stream binding | both; HAVEN bound SID 0 |
| M3 | F2 with S2AP forced to RW | nested | DMA access drift | DMA access drift |
| M4 | F1 with the window mapped 1 MiB further into A | nested | DMA association drift | DMA association drift |

QEMU's command trace (`-trace 'smmuv3_cmdq*'`, `runs/F1/smmu-trace.log`)
shows init consuming CFGI_STE_RANGE (all SIDs), TLBI_NSNH_ALL and SYNC, then
the grant consuming CFGI_STE for StreamID 8, TLBI_S12_VMALL for VMID 192
(0xC0) and SYNC, with no guest error. After the grant `CMDQ_PROD = CMDQ_CONS
= 6`, `GERROR = 0`. Before the fix the same grant hung HAVEN (case set v2,
S5 to S7).

## Interpretation (inference)

The grant path now reaches the hardware as the policy describes it: the
right StreamID, a window-sized table with the granted rights, and
invalidation that the SMMU completes. Read-back confirms the enforced state
matches the grant and flags each fix when it is reverted.

## Not done

- No device has done DMA through the new tables: no partition drives
  virtio-rng-pci, so QEMU never walks them. A DMA test from a partition is
  the next experiment (G2).
- Event queue not enabled: SMMU faults are not reported to HAVEN.
- Production configuration grants no DMA device yet; the grant calls exist
  only in the verification copies.
- Two-level stream tables and StreamIDs above 255 (needed for real PCIe
  topologies) are not supported.
- On CIX P1 the vendor default firmware builds without the PCIe SMMU
  (`PCIE_SMMU_ENABLE 0` in `CommonDefines.h`); see
  `../../../porting/cix-p1-ledger.md`.
