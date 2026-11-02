# G2: real DMA from a partition through the SMMU (QEMU)

**Status:** EXPERIMENT, observed 2026-11-02 at haven `f4da7e8`. Approved by
the owner as the next experiment after the SMMU bugfix. Not a scored case
set: Claude wrote the checks and expected outcomes; G1 and G2 were tried
before the runner was written, so nothing here is blind.

**Question.** With the SM1 to SM3 fixes, does a device granted to a
partition really reach its window, and is it really stopped outside it? Do
the partition's own view (what landed in memory) and the external read-back
agree?

## Change under test

- `src/core/partition.c`: DMA grants come from the partition configuration
  (`dma_grants[]`). QEMU: PCIe function 00:01.0, StreamID 8, granted to
  partition A with a 1 MiB read-write window at IPA `0x5F000000`; its config
  page (`0x40_1000_8000`) and a 16 KiB BAR (`0x1000_0000`) are mapped to A
  only after the SMMU enforces the window. If it cannot, the device is
  withheld. Partition A learns this from bit 0 of x0.
- `src/core/mm/stage2.c`: `hv_stage2_add_regions()` adds regions to a mapped
  partition with the same ownership checks.
- `drivers/iommu/smmu_v3.c`: event queue (64 entries) enabled.
- `tests/demos/guest_a_dma.c`: partition A places the BAR, initialises the
  device as a modern virtio device (virtio 1.2, sections 3.1 and 4.1), and
  asks it twice for 64 random bytes: into a buffer in the window, and into a
  buffer in A's RAM outside the window. Each buffer is pre-filled with a
  canary; the verdict is what changed in memory.
- `tools/readback`: SMMU event queue decoded; virtio devices without
  `VIRTIO_F_ACCESS_PLATFORM` flagged; `--marker`.

Grant file: `grant-g2.json`. Runner: `run_g2.py`; results:
`runs/results.md` and `runs/results.json`, logs per check, memory dumps
replaced by `runs/DUMP_SHA256SUMS`. QEMU 11.1.3, cortex-a72 and max.

## Results (both CPUs identical)

| Check | Setup | Partition A reports | Read-back | SMMU events |
| - | - | - | - | - |
| G0 | no device | device absent | matches the grant | none |
| G1 | virtio-rng-pci with `iommu_platform=on` | inside window: 64 bytes written; outside: buffer unchanged | matches the grant; SID 8 translated through its own 1 MiB table | 17 translation faults, SID 8, writes to page `0x5E000000`; none in the window |
| G2 | virtio-rng-pci, QEMU default | device bypasses the IOMMU, not started | **unenforced DMA master** (virtio without `VIRTIO_F_ACCESS_PLATFORM`) | none |
| G3 | stage-1-only SMMU (`virt-9.1`) | no DMA device granted; HAVEN: grant withheld (`rc=-95`) | missing binding for the config page and BAR; DMA enforcement drift (SID 8 denied, grant says translated) | none |
| G4 | G1 with the window widened to 32 MiB in HAVEN's configuration | inside OK; **outside NOT blocked**, 64 bytes written | **DMA extent drift**: reach `0x2000000` bytes for a `0x100000` window | none |

## Observations

- DMA works through the per-stream table and the SMMU stops writes outside
  the window: QEMU traces each access as a stage-2 translation for SID 8,
  and the faults land in HAVEN's event queue.
- QEMU's virtio device still marks the outside request as completed
  (inference: QEMU maps the faulting buffer through a bounce buffer whose
  write-back is dropped; not traced). Completion is not evidence that a DMA
  landed; the memory check is.
- **A virtio device in QEMU bypasses the SMMU unless it offers
  `VIRTIO_F_ACCESS_PLATFORM`** (`iommu_platform=on`, which needs
  `disable-legacy=on`). In that case its DMA reaches physical memory
  directly, whatever the stream table entry says. Read-back v2 and the
  SMMU bugfix checks classified such a device as translated from its stream
  table entry alone; v3 classifies it as an unenforced DMA master. The
  bugfix checks never ran DMA, so their verdicts on the tables stand, but
  the "translated" label there describes the entry, not the device's path.
- When HAVEN's own configuration is wrong (G4), partition A observes the
  consequence (memory outside the window overwritten) and read-back names
  the cause (reach larger than the grant), while HAVEN boots normally.

## Interpretation (inference)

This is the first end-to-end evidence for the A1 claim with a working
enforcement mechanism: the enforced state read from outside predicts what
the device can do (G1, G4), and it also catches a path the enforcement
mechanism does not cover at all (G2). G2 is the QEMU counterpart of the CIX
P1 GPU and PCIe findings: a DMA master whose path has no SMMU.

## Limits

- One emulated device; QEMU's SMMU and virtio models are not hardware. The
  `iommu_platform` behaviour is a QEMU modelling choice for paravirtual
  devices; real PCIe devices behind an SMMU cannot opt out.
- HAVEN does not consume SMMU events yet (no interrupt handling); they are
  read from memory by read-back.
- Partition A can still write its device's BAR registers and so move the
  BAR inside the PCIe window; HAVEN maps only the configured 16 KiB. A
  production design would let HAVEN place BARs and filter config writes.
- CI runs an older QEMU whose SMMU has stage 1 only, so grants are withheld
  there (as in G3); the boot check does not use a device.
