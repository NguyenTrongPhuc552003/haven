# Partition B on CIX P1: its own record (design)

**Status:** design and expectations, 2026-11-23, before any code.
Owner-approved ("Approve", 2026-11-23: partition B's own record, the
first open item of G3). Owner decision of 2026-11-09: partition B is not
started on cix-p1 until it has its own record; this is that record. The
code it leads to is compile-only, like the rest of cix-p1, until the
board exists.

## Facts [F]

- CPUs (`sky1.dtsi`, v7.3, `data/common/linux-dts-cix/`): cores 0x000
  to 0x300 are Cortex-A520 (capacity 403), 0x400 to 0xB00 Cortex-A720
  (1024); one `cpu-map` cluster. HAVEN's logical map
  (`src/platform/cix-p1/platform.c`) puts cores 8 to 11 at logical 8 to
  11, reserved for partition B by the owner (D3, 2026-11-09). Partition A
  holds logical 0 to 7 (cores 4, 0 to 3, 5 to 7: four A720 and four A520).
- The architected timer's virtual PPI is GIC PPI 11, INTID 27
  (`sky1.dtsi` `timer`), as on qemu-virt, so B's timer route is the same.
- Free DRAM on paper (`docs/porting/cix-p1-loading.md`, 5.2 and the
  layout table): 0xD100_0000 to 0xF000_0000 (496 MiB), or above
  0x1_8000_0000; HAVEN at 0xF000_0000 + 8 MiB; partition A at
  0x1_0000_0000 + 2 GiB. The board's memory map (probe P1) is not read yet.
- `haven-boot.efi` loads every range of a per-platform table
  (`boot/efi/layout.h`), qemu-virt only today; partition B's range and
  `guest_b.bin` are rows of it.
- On qemu-virt B is one CPU, 64 MiB at IPA 0x6000_0000, no UART, its
  timer PPI on its CPU, a 2 ms in 10 ms budget, and it reports through
  `guest_b_status` in its own RAM.

## Design [P]

| Item | Proposal | Why |
| - | - | - |
| CPU | one CPU, logical 8 (core 0x800, A720); logical 9 to 11 stay off | as on qemu-virt (one CPU); an A720 like A's vCPU 0, so interference cells compare like cores; more CPUs for B are a later record |
| RAM | 64 MiB at PA 0xE000_0000, IPA 0x6000_0000 | inside the free range, 2 MiB aligned, 256 MiB above the highest fixed reservation and 192 MiB below HAVEN; the IPA as on qemu-virt, so `guest_b.bin` is the same image |
| Shared memory | none on cix-p1 | nothing uses it yet; `SHARED_*` stay qemu-virt's |
| Interrupts | the virtual timer PPI 27 on logical 8; no SPI, no UART | as on qemu-virt; B reports through `guest_b_status` |
| Budget | 2 ms in 10 ms, as on qemu-virt | the interference plan's countermeasure cells use it |
| Start | PSCI `CPU_ON` through the vendor firmware; the redistributor replay (`20261122-firmware-redistributor/`) applies | board check P9 |
| Loader | cix-p1 rows in `boot/efi/layout.h`: HAVEN, partition A's ranges as the Linux layout needs them, partition B with `guest_b.bin` | the loader claims every range with `AllocateAddress` before HAVEN starts |
| Read-back | B's range and CPU in the board grant `grant-cixp1-v1.json` (v0 plus B) | part C of the board plan reads the board grant |

The RAM address is a proposal on paper: if probe P1 shows 0xE000_0000 +
64 MiB is not conventional memory, the fallback is another 2 MiB aligned
range in 0xD100_0000 to 0xF000_0000, recorded as a deviation.

## Expectations

| Row | Check | Expected |
| - | - | - |
| P1 | cix-p1 builds (the CI's two cix-p1 steps, demo and Linux layout) with partition B defined | both build; compile-time checks hold: B's RAM inside 0xD100_0000 to 0xF000_0000 and disjoint from HAVEN and A; `PART_B_CPU` not one of A's vCPU CPUs |
| P2 | qemu-virt | every image byte-identical with a fixed `SOURCE_DATE_EPOCH` |
| P3 | `grant-cixp1-v1.json` | passes M3 (format 1 to 2 and back); lists B's RAM and CPU 8, the rest as v0 |
| P4 | the loader's cix-p1 table | a host check: every range 2 MiB aligned, no two overlap, none overlaps a fixed reservation of the loading study's table |
| P5 | the board (later, part A3 of the board plan) | B's status record reached through the chosen read-back channel shows stage 5 and at least 8 timer interrupts; not judged before the board |
