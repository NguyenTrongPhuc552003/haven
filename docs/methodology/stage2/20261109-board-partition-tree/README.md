# D3: partition A's device tree for the Orange Pi 6 Plus, checked offline

**Status:** step D3 of `../../../porting/cix-p1-device-tree.md` (owner-approved
2026-11-09). Design and expectations by Claude, committed before the
generator, the grant and any check run. No board yet: everything here is
offline. Labels: **[F]** fact from our code or records, **[O]** source
observation (file and revision named), **[I]** inference, **[P]** proposal.
Nothing here is a hardware observation.

## Problem

On the board HAVEN must hand partition A's Linux a device tree that
describes exactly what the grant gives it: its CPUs, its RAM window, its
devices, HAVEN's emulated GIC instead of the GIC-700, PSCI through HAVEN,
and nothing HAVEN owns (its console, UART3). In QEMU this tree is written
by hand (`../20261104-linux-partition/partition-a.dts`). For the board it
should be derived from the board's own tree, mainline `sky1-xcp.dts`
(Orange Pi 6 Plus, Linux `v7.3`, `data/common/linux-dts-cix/v7.3/`, D1),
and from the board grant, and checked against that grant with
`tools/readback/dt_check.py` before any boot. This record fixes the design
of that tree and predicts what `dt_check.py` reports for it and for seeded
faults.

Deviation from the porting note's D3 row: the row keeps PCIe; the owner's
D3 instruction (2026-11-09) leaves PCIe for later, so the first tree has no
PCIe.

## Design

### Inputs and outputs

- Board tree: `sky1-xcp.dts` preprocessed with `clang -E` (the command in
  `../../../porting/cix-p1-device-tree.md`, section 3) and compiled with
  `dtc`, then read with `dt_check.parse_fdt` [P]. The generator copies
  facts from it (CPU compatibles, MPIDRs, capacities, UART2's node, timer
  PPIs, GIC addresses) and fails if a fact it relies on differs from what
  this record states (for example UART3 not at 0x040E0000, GICD not at
  0x0E010000).
- Board grant: `grant-cixp1-v0.json` (format 1, draft, a `source` field
  giving every assumption).
- Generator: `gen_partition_a.py` (Python 3.9, standard library, calls
  `clang` and `dtc`) writes `partition-a.dts` and `partition-a.dtb` here,
  and with `--seeds` the seeded trees under `seeds/`. A runner,
  `run_d3.py`, runs `dt_check.py` on each and keeps the output in `runs/`.
- The tree is built from an allow-list, not by deleting nodes from the
  board tree: a node is in partition A's tree only if this design names it
  [P]. What the board tree has and the list does not name is left out.

### Decisions

| Item | Decision | Why |
| - | - | - |
| CPUs | **[P]** cores 0 to 7: the four Cortex-A520 (MPIDR 0x000 to 0x300, power domain `SKY1_PERF_CPU_L`) and the A720 groups `M0` and `M1` (MPIDR 0x400 to 0x700). Partition B (later) gets cores 8 to 11 (`B0`, `B1`) | layout v0 gives Linux "one A720 cluster and the four A520s" without naming the cores [F, `cix-p1-partition-plan.md`]. `sky1.dtsi` puts all 12 cores in one cpu-map cluster, so "cluster" is read as the power-domain groups: `M0`/`M1` (medium) for Linux, `B0`/`B1` (big, by the name only [I]) for the real-time partition. Core 4 is the firmware's boot core and HAVEN's logical CPU 0 [F, ledger "CPU numbering"], and in QEMU partition A starts on HAVEN's boot CPU, so A keeps core 4 |
| Grant CPU keys | HAVEN logical CPUs from the ledger's map (MADT order, core 4 first: logical 0 = core 4, 1 to 4 = cores 0 to 3, 5 to 11 = cores 5 to 11): A = logical 0 to 7, B = logical 8 to 11; logical 1 to 7 started on PSCI CPU_ON (`cpus_on_request`) | `gic.cpus` is keyed by HAVEN's logical CPU in read-back [F]; D4 fixes the map in code, the grant follows it |
| Guest MPIDRs | **[P]** equal to the physical ones (cpu `reg` 0x000 to 0x700, copied from the board tree) | HAVEN writes each vCPU's guest MPIDR to VMPIDR_EL2 from its vCPU table [F, `multi-vcpu.md` section 7]; keeping the physical values keeps cpu-map and capacities as the SoC tree has them |
| CPU node properties | kept: `compatible`, `reg`, `device_type`, `enable-method = "psci"`, `capacity-dmips-mhz`; dropped: `power-domains` (SCMI DVFS, not given to A yet), `cpu-idle-states` (HAVEN's CPU_SUSPEND returns at once [F, `psci.c`], so the idle states would only mislead the idle governor). cpu-map: one cluster, core0 to core7 | first bring-up without SCMI |
| RAM window | **[P, assumption to confirm on the board]** IPA = PA = 0x1_0000_0000, size 0x8000_0000 (2 GiB), RWX | identity mapping for partition A (GPU DMA is untranslated) [F, plan]. The board's 16 GB map is unknown. Every DRAM source agrees on 4 GiB from 0x8000_0000: the vendor tree's `memory@80000000` and `PcdSystemMemoryBase/Size` = 0x8000_0000 / 0x1_0000_0000 [O, cix-bios `7799770`, edk2-platforms `e5cb70f`, `Platform/CIX/Sky1/Sky1Common.dsc.inc`]. The window lies in the upper half of that, above every firmware reservation that file lists, all below 4 GiB: secure 0x8000_0000 + 0x250_0000, shared 0x8250_0000 + 0x1F0_0000 (holds TF-A's SCMI shared memory at 0x8438_0000 of `sky1.dtsi`), firmware volume 0x8440_0000, GOP frame buffer 0x8480_0000, boot image 0xA000_0000 + 0x800_0000, audio 0xCDE0_0000 + 0x320_0000, GPU reservation (a 32-bit PCD). One listed reservation overlaps: `PcdReservedAndroidBase` 0x1_4000_0000 + 0x1_2000_0000, for Android builds only (its HOB is commented out) [O]. Low DRAM stays for HAVEN's image and partition B. 2 GiB holds the board tree's 640 MiB CMA pool; it grows once the EFI memory map of the 16 GB board is read |
| Emulated GIC | GICv3 node `interrupt-controller@e010000`: GICD IPA 0x0E01_0000 + 0x1_0000; GICR IPA 0x0E09_0000 + 8 x 0x2_0000 (one 128 KiB frame per vCPU, 1 MiB); `#interrupt-cells = <3>`, `#address-cells = <0>`; no ITS, no maintenance interrupt, no `ppi-partitions` | **[P]** the physical GIC-700 addresses (`sky1.dtsi`; `PcdGicDistributorBase/RedistributorsBase` [O]), as HAVEN does in QEMU (emulation at the machine's GIC address). With IPA = PA nothing granted to A can sit there; QEMU's IPAs (0x0800_0000, 0x080A_0000) would collide with the board's mailboxes at 0x0809_0000 and 0x080A_0000. 128 KiB frames because HAVEN's vGIC reports GICR_TYPER without VLPIS [F, `vgic.c`], so Linux steps 128 KiB; the physical 256 KiB stride is HAVEN's own concern (D2). The 1 MiB lies inside the physical 3 MiB redistributor region, which A's stage-2 never maps. No LPIs in the vGIC, so no ITS; no PPI partitions in the vGIC, so 3 cells: the generator rewrites every kept 4-cell specifier to 3 cells by dropping the 4th cell (0 or a PPI-partition phandle) |
| PSCI | `compatible = "arm,psci-1.0", "arm,psci-0.2"`, `method = "hvc"` | HAVEN emulates PSCI over HVC; same node as the QEMU tree that boots [F] |
| Timer | `arm,armv8-timer` with the first four PPIs of the board tree (INTIDs 29, 30, 27, 26, level-low, 3 cells); the fifth (hyp-virt, INTID 28) and `interrupt-names` dropped | Linux at EL1 uses the virtual timer (27) [F, `dt_check.py`]; four entries as in the QEMU tree. Grant: PPI 27 on each of A's eight CPUs |
| Console | UART2 `serial@40d0000` (PL011, SPI 298 = INTID 330) kept under `soc@0`, `status = "okay"`, alias `serial2`, `stdout-path = "serial2:115200n8"`, `bootargs = "earlycon panic=-1"`; UART3 `serial@40e0000` (INTID 331) **not in the tree** | UART2 is the firmware console and the board tree's `stdout-path` [O]; UART3 is HAVEN's console (G2 decision 5) [F]. In the grant UART2 is `DEVICE_RW` for A with SPI 330 on logical CPU 0; SPI 331 is listed as HAVEN's (partition 0) |
| PL011 clock | a `fixed-clock` node, 100 MHz, given as both `uartclk` and `apb_pclk`, replacing the two SCMI clocks | same pattern as the QEMU tree [F], so no SCMI, mailbox or shared memory is needed for the console. 100 MHz is `PL011UartClkInHz` of the CIX BIOS for boards [O, `Sky1Common.dsc.inc`]; on the board, UART2's divisors after UEFI must read IBRD = 54, FBRD = 16 at 115200 baud if that is true [I]. `dt_check.py` has no rule about clocks (a node with no `reg` and no `interrupts` cannot produce a finding) |
| Memory nodes | `memory@100000000` from the grant's Normal region; `reserved-memory` with the board's `linux,cma` pool (640 MiB, no fixed address) | the board tree has no memory node (firmware fills it in) [O] |
| Left out for the first bring-up | PCIe root complexes (x8, x4, x2, x1_0, x1_1) and the ITS, SCMI (`firmware/scmi`, `scmi-1`), mailboxes and SCMI shared memory, PMUs (`pmu-a520`, `pmu-a720`: they need `ppi-partitions`), I2C 0 to 7, SPI 0 and 1, UART0, UART1, UART3, I3C 0 and 1, FCH and S5 GPIO, system controls (`syscon`, `s5_syscon`), pin controllers and their hogs, FCH DMA controller (DMA-350), audio clock and reset unit, idle states, the S5 GPIO fixed clock; not in the mainline tree at all: GPU, display, NPU, VPU, ISP, DSP, USB, SMMUs | each needs a grant region, SPIs, SCMI or an SMMU stream entry that the first bring-up does not have; they come back one by one with their grant entries |

### Known limits found while designing (not seeded faults)

1. HAVEN's vGIC reports 64 interrupt IDs (`GICD_TYPER` ITLinesNumber = 1,
   `HV_VGIC_NR_IRQS` = 64) [F, `vgic.c`, `vgic.h`]. UART2's INTID 330 is
   beyond it: Linux would get no UART2 interrupt (the console would still
   print through polled writes [I]). `dt_check.py` does not know the
   emulated GIC's range, so it cannot report this. D4 item.
2. HAVEN builds partition A with at most two vCPUs today
   (`HAVEN_PART_A_VCPUS`, `part_a_peers`) [F]; the tree describes eight,
   the target of `multi-vcpu.md` step 4.
3. `dt_check.py` counts cpu nodes but does not check which MPIDRs they
   carry, reads every interrupt specifier as 3 cells whatever the
   controller's `#interrupt-cells`, and has no rule for `/psci`. Seeds S2b,
   S5 and S6 measure these three; they are expected to be missed or misread.

## Expectations

Command for every row from C on (from the haven repository):
`python3 tools/readback/dt_check.py <grant> <tree>.dtb --partition 1
--emulated 0x0E010000+0x10000 --emulated 0x0E090000+0x100000`, grant
`grant-cixp1-v0.json`. Findings are listed as `dt_check.py` prints them
after `FINDING`, in the printed order.

| Row | Tree | Expected exit | Expected findings | Expected info |
| - | - | - | - | - |
| G | generator on `sky1-xcp.dts` v7.3 | 0 | `dtc` compiles `partition-a.dts` with no warning (run without `-q`, stderr kept) | generator summary: 8 cpu nodes, 1 memory node, 1 enabled device node with `reg` besides the GIC (UART2) |
| C | `partition-a.dtb` (clean) | 0 | none (`RESULT the device tree matches the grant`) | exactly 3: `/timer: timer interrupt 29 (entry 0) not used by an EL1 guest`, the same for `30 (entry 1)` and `26 (entry 3)` |
| S1 | UART3 left in, enabled: `soc@0/serial@40e0000` as in the board tree, with the fixed clock and 3 cells | 1 | `/soc@0/serial@40e0000: registers 0x40e0000+0x1000 not granted to partition 1`, `/soc@0/serial@40e0000: SPI 331 not granted to partition 1` | the 3 of C |
| S2a | a ninth CPU: core 8 (`cpu@800`) added | 1 | `/cpus: 9 CPU nodes, partition 1 has 8 CPU(s)` | the 3 of C |
| S2b | core 7 (`cpu@700`) replaced by core 8 (`cpu@800`), still 8 nodes | 0 | none: **expected miss** (the check counts CPUs, it does not compare MPIDRs) | the 3 of C |
| S3 | memory node twice the grant: `memory@100000000` 0x1_0000_0000 + 0x1_0000_0000 | 1 | `/memory@100000000: memory 0x100000000+0x100000000 not granted to partition 1` | the 3 of C |
| S3b | memory node in low DRAM over the firmware's reservations: `memory@80000000` 0x8000_0000 + 0x8000_0000 | 1 | `/memory@80000000: memory 0x80000000+0x80000000 not granted to partition 1` | the 3 of C |
| S4 | GIC node with the board's 12-frame redistributor region (0x0E09_0000 + 0x30_0000) | 1 | `/interrupt-controller@e010000: registers 0xe090000+0x300000 not granted to partition 1` | the 3 of C |
| S5 | limitation probe: 4-cell specifiers kept (GIC `#interrupt-cells = <4>`, timer and UART2 with their 4th cell 0), otherwise clean; a valid tree for Linux | 1 | `/timer: PPI 16 not granted to partition 1` (**false finding**: the 16 timer cells read in threes give a third entry `8 0 1`) | 4 timer lines: `29 (entry 0)`, `33 (entry 1)`, `24 (entry 3)`, `26 (entry 4)`; UART2 read correctly by chance (`0 298 4`, the 4th cell ignored) |
| S6 | PSCI left as the board has it: `method = "smc"` | 0 | none: **expected miss** (no rule for `/psci`) | the 3 of C |

Verdict rule: a row holds if exit status, findings (text and number) and
the number of info lines match. Any other result is a prediction error and
is reported as such; the expectations above are not edited after the runs.
