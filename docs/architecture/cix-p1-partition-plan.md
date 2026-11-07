# CIX P1 partition plan, version 0 (G2 input)

**Status:** accepted at G2 (2026-11-07); decisions 4 and 5 recorded below. It extends `gpu-partition.md` (2026-11-01) with the NPU,
CPUs, consoles and the open questions. Nothing is observed on hardware.
Labels: **[F]** primary source or our record, **[S]** community or single
source, **[I]** inference, **[P]** proposal.

## 1. What the SoC offers (from `data/` and the ledger)

| Resource | Fact | Source |
| - | - | - |
| CPUs | 12 cores: 4 Cortex-A720 ("large"), 4 Cortex-A720, 4 Cortex-A520 | [F] Orange Pi 6 Plus Linux manual v0.7, sections 1.1 and 1.3 |
| CPU numbering | MPIDR has the core number 0 to 11 in Aff1, Aff0 = 0; firmware boots core 4 first | [F] CIX BIOS source, ledger "CPU numbering" |
| Which core is which type | community reports interleaved A520 numbering and wrong cache data in ACPI | [S] ledger row 16; to read on the board (MIDR, MPIDR per core) |
| Memory | 128-bit LPDDR5, 16, 32 or 64 GB | [F] manual 1.3 |
| GPU | Immortalis-G720 MC10; no IORT or SMMU entry, so its DMA is untranslated; gated by an EL3 interconnect grant | [F] firmware source and images (row 1); [S] row 2 |
| NPU | up to 28.8 TOPS; ACPI device `CIXH4000`; named component `NPU0` behind the multimedia SMMU at 0x0B1B0000 | [F] manual 1.3; CIX BIOS `Dsdt-NPU.asl`; IORT in firmware images |
| NPU software | no working Linux driver reported for the Orion O6 | [S] Radxa hardware notes (`orion.md`, 2026-09) |
| PCIe (2x NVMe x4, M.2 E-key, 2x 5 GbE RTL8126) | behind the PCIe SMMU at 0x0B010000, StreamID = requester ID | [F] manual 1.3; IORT |
| Display, ISP, VPU, audio, DSP | display and ISP behind the multimedia SMMU; a DSP device `CIXH6000` in ACPI | [F] IORT; `Dsdt-Dsp.asl` |
| Power and clocks | through SCMI over a shared mailbox; GPU DVFS can wedge it; `clk_ignore_unused` needed | [S] row 14; [F] row 13 |
| Debug UARTs | UART2 (firmware and kernel log), UART4 (power management), UART5 (security engine), UART6 (POST codes), on the 10-pin header | [F] manual 1.3 |
| UARTs the application cores own | UART0 to UART3 in the FCH block at 0x040B0000, 0x040C0000, 0x040D0000, 0x040E0000 (PL011, ACPI `ARMH0011`); UART2 is the firmware console | [F] CIX BIOS `MemoryMap.h`, `Dsdt-Fch-Uart.asl`, `Dbg2.aslc` |
| UART6 | its pads are named `UART6_CSU_SE_TXD/RXD`: it belongs to the security engine's domain, not to the FCH UARTs | [F] CIX BIOS `IoConfig.h`, `sky1-iomux.h` |
| 40-pin header UARTs | UART1 and UART3 with the vendor's "40pin" BIOS variant | [F] manual section on BIOS variants |

## 2. Proposed layout [P]

| Partition | CPUs | Owns | Memory | Why |
| - | - | - | - | - |
| A: Linux | one A720 cluster and the four A520s (8 cores) | GPU, display, SCMI, PCIe (NVMe, Ethernet), USB, audio, **NPU** | identity-mapped (IPA = PA), because GPU DMA is untranslated | GPU and SCMI together (row 14); the demo's desktop and GPU load |
| B: real-time | 2 to 4 cores of the other A720 cluster | its timers, one UART | small, cache-coloured where possible | the victim of the interference study |
| HAVEN | EL2 on every core | stage-2, SMMU, GIC | its own image | |
| Not assigned | | DSP, ISP, VPU (powered down or left to Linux later) | | no driver or no need yet |

**The NPU in partition A** gives read-back a useful pair on one board: the
GPU, whose DMA no SMMU translates (granted but not enforced), and the NPU,
whose DMA goes through the multimedia SMMU (granted and enforced, if the
stream table entry is right). Read-back can then show both kinds. Its
Linux driver is the risk [S]; if none works, the NPU stays unassigned and
read-back still checks its stream table entry.

## 3. What HAVEN lacks for this layout

| Gap | Today | Needed | Where tracked |
| - | - | - | - |
| Several CPUs per partition | one vCPU per partition; CPU_ON refused | Linux on 8 cores, B on 2 to 4 | parking lot; G2 item 1 |
| Device tree or ACPI for Linux | device tree in QEMU | the vendor kernel expects ACPI (row 12) | G2, early decision |
| Identity mapping for partition A | IPA offset in QEMU | IPA = PA for the GPU | `gpu-partition.md` |
| CPU map from the board | QEMU's | MPIDR order with core 4 first | ledger "CPU numbering" |
| Console policy | HAVEN prints on partition A's UART | HAVEN on its own UART: **UART3** proposed (FCH, on the 40-pin header); UART6, approved at G2 as Claude's first suggestion, belongs to the security engine and is not usable from the application cores | G2 follow-up |

[I] An alternative to multi-vCPU for B is several single-vCPU real-time
partitions (one core each). Linux still needs several cores to be a
credible general-purpose partition, so multi-vCPU support is needed
either way.

## 4. Decisions for G2

1. Accept this layout as the target, or change the core split.
2. NPU: partition A (proposed), or unassigned.
3. Multi-vCPU partitions: promote from the parking lot to committed work,
   with Linux SMP as its first target.
4. Device tree or ACPI for the Linux partition on the board: **device tree for the first bring-up** (owner, 2026-11-07); ACPI revisited later.
5. HAVEN's console: approved as UART6 (2026-11-07), then found to be the security engine's UART; **UART3 proposed instead**, UART1 for the EL3 read-back dump (owner to confirm).
