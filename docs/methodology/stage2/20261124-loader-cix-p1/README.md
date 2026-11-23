# haven-boot.efi on CIX P1: the loader's table (design)

**Status:** design and expectations, 2026-11-24, before any code.
Owner-approved ("Approve", 2026-11-24: the loader's cix-p1 table, the last
loader item before board bring-up).

## Facts [F]

- `boot/efi/boot.c` claims every range of `BOOT_RANGES` with
  `AllocateAddress`, zeroes it, and loads at most one file at each
  range's base (`load_file`: the file must fit the range; its SHA-256 is
  printed). `boot/efi/build.sh` builds it from qemu-virt's `memory.h`
  only; `scripts/qemu/uefi-boot-check.py` runs it under QEMU's EDK2 in CI.
- On cix-p1 partition A is Linux: HAVEN enters it at `PART_A_IPA_BASE`
  (identity mapped, 0x1_0000_0000) with the device tree's IPA
  (`HAVEN_PART_A_DTB_IPA`) in x0. CI builds that layout with
  `HAVEN_PART_A_DTB_IPA=0x110000000`, a placeholder (the D5 record).
- Linux's arm64 boot protocol: the kernel `Image` (uncompressed) at a
  2 MiB aligned base; the device tree 8-byte aligned, at most 2 MiB; an
  initramfs given by `/chosen linux,initrd-start` and `-end`.
- The generated partition A tree (`20261109-board-partition-tree/`,
  `gen_partition_a.py`) has `/chosen` with `bootargs` and `stdout-path`
  only: no initramfs yet.
- Partition B's range and image: `20261123-cix-p1-partition-b/`.

## Design [P]

1. **Files inside a claimed range.** A second table, `BOOT_LOADS`, of the
   same `struct boot_range`: each row is a file loaded at its base, at
   most its size, inside a range `BOOT_RANGES` claims. `BOOT_RANGES` rows
   keep their own optional file. Loads run after every claim and zero.
2. **qemu-virt uses it too**, so CI exercises the mechanism: its three
   images (`haven.bin`, `guest_a.bin`, `guest_b.bin`) move from the range
   rows to `BOOT_LOADS` rows at the same bases and sizes; the console
   lines stay the same.
3. **cix-p1's table** (`HAVEN_PLATFORM_CIX_P1`, `build.sh PLATFORM=cix-p1`):

   | Kind | Name | Base | Size | File |
   | - | - | - | - | - |
   | claim | haven | 0xF000_0000 | 8 MiB | `haven.bin` |
   | claim | part-a | 0x1_0000_0000 | 2 GiB | |
   | claim | part-b | 0xE000_0000 | 64 MiB | `guest_b.bin` |
   | load | a-kernel | 0x1_0000_0000 | 128 MiB | `Image` |
   | load | a-dtb | 0x1_1000_0000 | 2 MiB | `partition-a.dtb` |
   | load | a-initrd | 0x1_1100_0000 | 512 MiB | `initramfs.cpio` |

   The device tree's address becomes a cix-p1 constant
   (`CIX_PART_A_DTB_IPA`), and HAVEN's build checks that
   `HAVEN_PART_A_DTB_IPA` equals it on cix-p1, so the loader and HAVEN
   cannot disagree. `BOOT_UART` is UART3 (HAVEN's console).
4. **The initramfs in the tree**: `gen_partition_a.py --initrd ADDR SIZE`
   writes `linux,initrd-start` and `-end` into `/chosen`; the size is the
   built initramfs's, so the tree is generated with it.

Not in scope: running the cix-p1 loader (no QEMU machine for the board);
how the initramfs is built for the board (ELMOS's SDK root file system
and KDAL's module, as for the Linux CI job, is the obvious source).

## Expectations

| Row | Check | Expected |
| - | - | - |
| L1 | the qemu-virt loader in CI (`uefi-boot-check.py`, four runs) | all pass; the same `claim` and `load` console lines as before, in the same order of files |
| L2 | `build.sh` with `PLATFORM=cix-p1` | builds `haven-boot.efi` with no relocation and no `.got`, as for qemu-virt |
| L3 | a host test of both tables | every load lies inside one claimed range; no two loads overlap; claims 2 MiB aligned; the kernel at partition A's base; the device tree 8-byte aligned and at most 2 MiB |
| L4 | HAVEN's cix-p1 Linux build | builds with `HAVEN_PART_A_DTB_IPA` equal to `CIX_PART_A_DTB_IPA`; fails to build with another value |
| L5 | `gen_partition_a.py --initrd` | the tree's `/chosen` has both properties; `dt_check.py` still passes on it |

## Code and L1 to L5 (2026-11-24, on the Mac) [F]

`boot/efi/layout.h` (both tables per platform), `boot.c` (the loads
after the claims, through the same `load_file`), `build.sh` (`PLATFORM`),
cix-p1's `memory.h` (`CIX_PART_A_DTB_IPA` 0x1_1000_0000,
`CIX_PART_A_INITRD_IPA` 0x1_1100_0000, the maximum sizes, and the check
on `HAVEN_PART_A_DTB_IPA`), `tests/unit/test_boot_layout.c` (built for
each platform), `gen_partition_a.py --initrd`.

| Row | Result |
| - | - |
| L1 | `uefi-boot-check.py` on cortex-a72 and max (QEMU 11.1.3, Homebrew's EDK2): PASS; the console lists the same five claims and the same three loads (`haven.bin`, `guest_a.bin`, `guest_b.bin`) at the same addresses as before. **Holds** on the Mac; CI runs all four |
| L2 | `PLATFORM=cix-p1 build.sh`: all four EFI files build with the relocation and `.got` checks; `haven-boot.efi` differs from qemu-virt's. **Holds** |
| L3 | `test_boot_layout` and `test_boot_layout_cix` pass. **Holds** |
| L4 | cix-p1 Linux build with `HAVEN_PART_A_DTB_IPA=0x110000000`: builds; with `0x120000000`: stops at the static assertion. **Holds** |
| L5 | with `--initrd 0x111000000 12345678`, `/chosen` has `linux,initrd-start = <0x1 0x11000000>` and `-end = <0x1 0x11bc614e>`; `dt_check.py` against grant v1: matches the grant; without `--initrd` the generated tree is the committed one. **Holds** |

## L1 in CI (2026-11-24) [F]

#220's head ran CI twice (push run 45234819011, pull-request run
45234882003). In both, the UEFI step passes all four checks: the probe,
then `uefi-boot-check` on cortex-a72, max, and max with the seeded MMU.
The console shows the five `claim` lines and then the three `load` lines
(`haven.bin` at 0x8000_0000, `guest_a.bin` at 0x8080_0000, `guest_b.bin`
at 0xA080_0000), now from the load table. **L1 holds.** Merged as
`e6445e1`.
