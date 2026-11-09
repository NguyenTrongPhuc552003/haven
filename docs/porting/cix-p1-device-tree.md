# CIX P1 board: device tree preparation for the first bring-up

**Status:** preparation, 2026-11-08 (owner approved "device tree for the
first bring-up" at G2 and this preparation step). No board yet. Labels:
**[R]** reference fact (a manual), **[O]** source observation (a file in
`data/` or upstream source, with its revision), **[I]** inference,
**[P]** proposal. Nothing here is a hardware observation.

## 1. What the bring-up needs from device trees

Two different trees, often confused:

1. **The board's tree**, which describes the whole SoC and board. HAVEN
   reads facts from it for its own platform port (CPU map, GIC, its
   console UART, SMMUs, timer). HAVEN keeps these facts compiled in, as it
   does for QEMU and i.MX, so the tree is a source for the port, not
   something HAVEN parses at run time.
2. **Partition A's tree**, which HAVEN hands to Linux in x0 (as
   `HAVEN_PART_A_DTB_IPA` does in QEMU). It is the board tree cut down to
   what the grant gives partition A: its CPUs, its RAM window, its
   devices, HAVEN's emulated GIC instead of the real one, and no HAVEN
   console or HAVEN-owned device. `dt_check.py` already checks such a tree
   against a grant before boot.

## 2. Sources found

| Source | What it is | Status |
| - | - | - |
| Mainline Linux `arch/arm64/boot/dts/cix/sky1.dtsi` and `sky1-xcp.dts` | SoC tree, and the **Orange Pi 6 Plus** board tree (`model = "Xunlong,OrangePi 6 Plus"`, 83 lines on top of `sky1.dtsi`: UART2 console, PCIe x8, x2 and x1 root complexes, a 640 MiB CMA pool, a few pin hogs), posted by CIX (v4, 2026-01-10) and pulled for v6.20-rc1 | [O] in `data/common/linux-dts-cix/v7.3/` (D1, fetched 2026-11-08 at Linux `v7.3`, commit `c03c1a1`; 13 files with SHA-256 in its README) |
| CIX BIOS `edk2-platforms/Platform/Radxa/Orion/O6/DeviceTree/sky1-orion-o6.dtb` | the vendor firmware's tree for the Radxa Orion O6: same SoC, another board | [O] `data/common/cix-bios` at `7799770`; decompiled with `dtc`, 5,060 lines; facts in section 3 |
| Orange Pi 6 Plus firmware | BIOS manual v1.4, section 3.6: "System Table Selection: ACPI and Device Tree" | [R]; the firmware can hand over a device tree |
| Orange Pi firmware images in `data/` | `opi6plus_bios_image_for_linux_20251031.img`, `OPI6PLUS_BIOS_1.4_General.bin`: no uncompressed flattened tree found (search for the FDT header); strings show a build from CIX's ACPI SDK (`cix_sdk_acpi`) | [O]; a tree, if present, is inside a compressed firmware volume [I] |
| Orange Pi kernel trees (`orange-pi-6.1-cix`, `orange-pi-6.6-cix` on gitee) | named in `orangepi-build/external/config/sources/families/cix.conf` (`a95129b`) | [O]; not fetched |

## 3. SoC facts from the Orion O6 tree, cross-checked

Source: `sky1-orion-o6.dtb` (above). The partition plan's column comes
from ACPI tables in the CIX BIOS source (`cix-p1-partition-plan.md`).

| Item | Orion O6 tree [O] | Agrees with | HAVEN today | Consequence |
| - | - | - | - | - |
| CPUs | 12, MPIDR Aff1 = 0 to 11, Aff0 = 0, `compatible = "arm,armv8"`; capacity 403 for Aff1 0 to 3, 1024 for 4 to 11 | HAVEN's CPU-map note (core number in Aff1) | logical map per platform (`asm/cpu_map.h`) | cpu map from MPIDR, as designed; which cores are A520 or A720 is still open: the tree says 4 small then 8 big, ledger row 16 reports interleaved numbering from the community; MIDR on the board decides |
| PSCI | `arm,psci-1.0`, method `smc` | | `HAVEN_PSCI_CONDUIT_SMC` exists | qemu-psci style conduit works on the board [I] |
| GIC | GIC-700 (`arm,gic-700`, `arm,gic-v3`); GICD 0x0E010000 (64 KiB); 12 redistributor regions of 256 KiB from 0x0E090000 (0x0E090000 + n x 0x40000); ITS 0x0E050000; maintenance interrupt PPI 9 (INTID 25) | | `gic_v3.c`: redistributor at `gicr_base + logical CPU x GICR_STRIDE` | **two porting items** (ledger row 25): the 256 KiB stride (GICv4 frames), and the frame must be found by GICR_TYPER affinity, because HAVEN's logical CPU 0 is the boot core (core 4 on CIX, per the CPU-map note), not core 0 |
| UARTs | PL011 at 0x040B0000, 0x040C0000, 0x040D0000, 0x040E0000 (SPIs 296 to 299, INTIDs 328 to 331), aliases serial0 to serial3; UART2 (0x040D0000) enabled and `stdout-path`; two more PL011 at 0x05020000 (SPI 387) and 0x06540000 (no interrupt), disabled | partition plan (UART0 to UART3 at these addresses; UART2 firmware console) | QEMU PL011 at 0x09000000 | HAVEN's console UART3 = 0x040E0000, INTID 331 [I, from the alias order]; partition A keeps UART2 as its console, as `sky1-xcp.dts` does |
| SMMUs | SMMUv3 at 0x0B0E0000 (disabled), 0x0B1B0000 (enabled, the multimedia hub), 0x0B010000 (disabled), plus PMCG nodes | partition plan (multimedia SMMU 0x0B1B0000) | `smmu_v3.c`, one SMMU in QEMU | which SMMU serves PCIe is not in this tree [I: the disabled ones]; IORT on the board decides |
| Timer | PPIs 13, 14, 11, 10 (INTIDs 29, 30, 27, 26), level-low; `clock-frequency` 1 GHz | | budget timer on PPI 26 (CNTHP), guest virtual timer PPI 27 | same PPI numbers as QEMU; the 1 GHz counter shortens nothing HAVEN computes from CNTFRQ [I] |
| Memory | `memory@80000000`, 4 GiB in this tree | | | board-specific: the Orange Pi 6 Plus has 16 GB (and ledger row 17); the board's memory map decides partition A's RAM window |

### Mainline `sky1-xcp.dts` at v7.3, compiled (D1) [O]

Preprocessed with `clang -E` and compiled with `dtc` (834 lines out):
CPUs Aff1 0 to 3 are `arm,cortex-a520`, 4 to 11 `arm,cortex-a720`
(contiguous, as the vendor tree's capacities say; ledger row 16's
interleaving is not in either tree, MIDR on the board stays the test);
GICv3 with one redistributor region of 3 MiB at 0x0E090000 (12 x 256 KiB,
as the vendor tree's 12 regions); UART2 (0x040D0000) `okay` and
`stdout-path`, UART3 (0x040E0000, SPI 299, INTID 331) disabled, so free
for HAVEN; PSCI over SMC; a fifth timer PPI 12 (INTID 28, the EL2 virtual
timer). No memory node (firmware fills it in) and no SMMU nodes in this
tree at v7.3: partition A's tree gets its memory from the grant, and SMMU
facts stay with the vendor tree and IORT.

## 4. Open questions for the board

1. Which tree the Orange Pi firmware hands over in Device Tree mode, and
   whether it matches mainline `sky1-xcp.dts` (dump `/sys/firmware/fdt`
   from a vendor Linux boot).
2. The memory map with 16 GB, and the boot CPU's MPIDR.
3. MIDR per core (ledger row 16).
4. UART3's pins on the 40-pin header (the vendor's "40pin" BIOS variant,
   partition plan).
5. How HAVEN is loaded (UEFI application, or a firmware image change),
   and at which EL with which `HCR_EL2.E2H` (ledger row 22).

## 5. Steps proposed

| Step | What | Needs |
| - | - | - |
| D1 | Fetch mainline `sky1.dtsi`, `sky1-xcp.dts`, `sky1-orion-o6.dts` and the dt-bindings headers they include, at a pinned Linux tag, into `data/common/linux-dts-cix/` with a README giving the tag and commit | **done** 2026-11-08 (`v7.3`, owner-approved) |
| D2 | HAVEN GIC driver: redistributor found by GICR_TYPER affinity, per-platform stride; host unit test with a fake 12-frame layout; QEMU checks unchanged | **done** 2026-11-08 (`93cf892`, `../methodology/stage2/20261108-gicr-affinity/`) |
| D3 | Partition A tree generator: from `sky1-xcp.dts` and the board grant v0 (Linux on 8 cores), keep the granted CPUs, RAM window, UART2, PCIe; replace the GIC by HAVEN's emulated one; drop UART3 and HAVEN-owned nodes; check it with `dt_check.py` offline | **done** 2026-11-09 without PCIe (owner's D3 instruction; `../methodology/stage2/20261109-board-partition-tree/`): matches the draft grant, 4 findings and proposals for `dt_check.py` and the vGIC |
| D4 | `src/platform/cix-p1/` draft (CPU map, GIC, UART3, timer), compile-only until the board | **done** 2026-11-09 (`1ec9b8a`, `../methodology/stage2/20261109-cix-p1-platform/`): builds (CI step), vGIC at 352 IDs on cix-p1 only, `dt_check.py` closes D3's findings 1 to 4; not bootable yet (load address, partition.c still QEMU's) |

Sources (upstream patch and pull):
https://patchew.org/linux/20260110093406.2700505-1-gary.yang@cixtech.com/20260110093406.2700505-3-gary.yang@cixtech.com,
https://lists.infradead.org/pipermail/linux-arm-kernel/2026-January/1097934.html.
