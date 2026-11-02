# readback

External read-back of HAVEN's enforced state in QEMU: stage-2 tables of every
partition and, optionally, the SMMU stream table and the DMA path of every
emulated device. Nothing runs inside HAVEN; the tool stops QEMU and reads
memory and SMMU registers through QMP, then compares them with a grant file.

```sh
tools/readback/readback.py build grant.json out/ --cpu cortex-a72 \
    --device virtio-rng-pci --device virtio-rng-device
```

- `build`: a qemu-direct build (`haven.bin`, `guest_a.bin`, `guest_b.bin`, `haven.elf`).
- `grant.json`: the expected binding. `partitions` lists each partition's
  regions (`ipa`, `pa`, `size`, `attrs` as in `include/haven/stage2.h`); the
  optional `smmu` section gives `enabled`, the granted `streams`
  (`{"<SID>": {"partition": N, "window": {"ipa": ..., "size": ...},
  "access": "R" | "W" | "RW"}}`, window optional, access default `RW`; the
  window is in the partition's address space) and `allowed_unenforced`
  devices.
- `--smmu-stage`: QEMU's SMMU model. `nested` (default) is what QEMU 9.2
  and later give the `virt` machine (stage 1 and stage 2); `1` and `2` run
  the `virt-9.1` machine, whose SMMU has stage 1 only unless `2` is given.
- Exit status: 0 enforced state matches the grant, 1 findings, 2 probe failure.

Finding classes: missing binding, extent, association, access, execute,
memory-type, shareability and access-flag drift, extra binding,
cross-partition alias, SMMU enable drift, DMA enforcement drift, extra stream
binding (a stream entry that lets DMA through without a grant), DMA extent
drift (a translated device reaches more than its granted window), DMA missing
binding (part of the window is not mapped), DMA association drift (a window
address leads to another physical address than the partition's own stage-2
gives it), DMA access drift (rights differ from the grant), SMMU command queue
fault, and unenforced DMA master (a device that can do DMA with no SMMU in its
path).

With a `gic` section (`{"cpus": {"<cpu>": partition}, "spis": {"<INTID>":
{"partition": N, "cpu": C}}, "ppis": {"<cpu>": [INTID, ...]}}`), read-back
also checks interrupt ownership as the GICv3 enforces it: GIC distributor
drift (affinity routing or Group 1 off), interrupt enable drift (an SPI or
PPI enabled without a grant), interrupt routing drift (1-of-N, or routed to
a CPU of another partition or another CPU), interrupt group drift and
missing interrupt binding. GIC registers are read one access at a time:
QEMU's GIC model answers the wider accesses of a multi-value `xp` with
zeros.

The SMMU event queue is decoded and listed (observations, not findings). A
virtio PCI device without `VIRTIO_F_ACCESS_PLATFORM` (QEMU
`iommu_platform=false`) counts as an unenforced DMA master: QEMU lets such a
device reach physical memory without the SMMU, whatever its stream table
entry says. `--marker TEXT` reads the memory after that UART line (default
`Post-fault check OK`).

A stream's table is walked wherever its entry points: a partition's root, or
a table of its own, which is attributed to the partition owning the physical
memory it maps.

Needs `qemu-system-aarch64`, an aarch64 `nm` and `dtc`. The method and its
evaluation are in `docs/methodology/stage1/20261101-a1-readback/`; the frozen
v1 used there stays unchanged, and case set v2 was scored with this tool at
haven `3517927`.
