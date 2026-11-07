# EL3 read-back service (prototype, QEMU)

The board-side fallback B of `docs/architecture/board-readback.md`: when no
external debugger can read the board, the EL3 firmware dumps HAVEN's
enforced state on a UART that HAVEN and the partitions do not use. HAVEN
takes no part. This is the QEMU prototype, built into TF-A's BL31.

## What it does

`tf-a-v2.12.0-el3-readback.patch` adds `plat/qemu/common/qemu_el3_readback.c`
to TF-A v2.12.0 (QEMU platform) behind the build option `EL3_READBACK=1`:

- at the end of BL31 setup it arms the secure physical timer (PPI 29,
  configured as a Group 0 interrupt routed to EL3) to fire after
  `EL3RB_DELAY_MS` (default 8000 ms);
- when it fires, BL31 writes on the secure UART (UART1, 0x09040000) the
  interrupted CPU's EL2 translation registers, the GIC distributor and
  redistributor registers read-back needs, the SMMU registers, and every
  non-zero 32-byte line of HAVEN's region (PA 0x80000000, 8 MiB, mapped
  read-only into BL31);
- `tools/readback/readback.py --tfa DIR --el3` parses that dump and runs the
  same comparison as with the QEMU monitor.

Format: one record per line (`RB BEGIN`, `RB S` system register, `RB W`/`RB Q`
32/64-bit register, `RB M` 32 bytes of memory, `RB END`), described in the
source file's header.

## Build and run

```sh
git clone --depth 1 --branch v2.12.0 https://github.com/ARM-software/arm-trusted-firmware.git tf-a
cd tf-a && git apply <haven>/tools/el3-readback/tf-a-v2.12.0-el3-readback.patch
make CROSS_COMPILE=aarch64-elf- PLAT=qemu QEMU_USE_GIC_DRIVER=QEMU_GICV3 \
     PRELOADED_BL33_BASE=0x80000000 EL3_READBACK=1 bl1 bl2 bl31
# on macOS also: AR=aarch64-elf-ar OC=aarch64-elf-objcopy OD=aarch64-elf-objdump (GNU make: gmake)
cmake --preset arm64-qemu -B build-tfa -DHAVEN_BOOT_PROFILE=qemu-tfa && cmake --build build-tfa
python3 tools/readback/readback.py build-tfa GRANT.json out --tfa tf-a/build/qemu/release --el3 --wait 40
```

## Limits

- QEMU only. On the CIX P1 the same handler needs the board's BL31
  (rebuildable with the OEM key per community reports, ledger row 8),
  the board's UART, GIC and SMMU addresses, and the owner's approval to
  flash firmware.
- The dump runs on one CPU while the others continue; it is not an atomic
  snapshot of the whole system (the QEMU monitor stops every CPU).
- The SMMU stream table is taken from the dumped region; one outside
  HAVEN's 8 MiB region is reported as zeros.
- The read moment is a timer, not a chosen instruction.
- BL31 now contains code we wrote that reads HAVEN's memory: it moves the
  trust from HAVEN to our firmware change, it does not remove it.
