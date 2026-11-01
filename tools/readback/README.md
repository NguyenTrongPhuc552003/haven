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
  (`{"<SID>": {"partition": N}}`) and `allowed_unenforced` devices.
- Exit status: 0 enforced state matches the grant, 1 findings, 2 probe failure.

Finding classes: missing binding, extent, association, access, execute,
memory-type, shareability and access-flag drift, extra binding,
cross-partition alias, SMMU enable drift, DMA enforcement drift, and
unenforced DMA master (a device that can do DMA with no SMMU in its path).

Needs `qemu-system-aarch64`, an aarch64 `nm` and `dtc`. The method and its
evaluation are in `docs/methodology/stage1/20261101-a1-readback/`; the frozen
v1 used there stays unchanged.
