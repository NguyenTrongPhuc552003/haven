/* SPDX-License-Identifier: Apache-2.0 */
/* CIX P1 (Sky1) memory map for HAVEN: Orange Pi 6 Plus, DRAFT (D4).
 *
 * No board yet: compile-only. Every value cites its source in the
 * workspace's read-only data/ folder or in a HAVEN record; "ASSUMPTION"
 * marks a value data/ does not give (docs/methodology/stage2/
 * 20261109-cix-p1-platform/README.md, Part 1). Abbreviations:
 *   sky1.dtsi = data/common/linux-dts-cix/v7.3/arch/arm64/boot/dts/cix/
 *               sky1.dtsi (Linux v7.3)
 *   dsc       = data/common/cix-bios/edk2-platforms (e5cb70f)
 *               Platform/CIX/Sky1/Sky1Common.dsc.inc
 *   MemoryMap.h, InterruptId.h = the same tree, Silicon/CIX/Sky1/Include/
 */
#ifndef HAVEN_PLATFORM_CIX_P1_MEMORY_H
#define HAVEN_PLATFORM_CIX_P1_MEMORY_H

/* PL011 UARTs of the FCH block (sky1.dtsi uart2/uart3; MemoryMap.h
 * UART2_BASE/UART3_BASE; InterruptId.h FCH_INTR_UART2/3_INTERRUPT_ID).
 * UART3 is HAVEN's console (partition plan, decision 5); UART2 is the
 * firmware console and partition A's (D3 grant), never touched by HAVEN. */
#define CIX_UART2_BASE 0x040D0000UL
#define CIX_UART2_INTID 330U /* SPI 298 */
#define CIX_UART3_BASE 0x040E0000UL
/* SPI 299; HAVEN prints polled and never enables it */
#define CIX_UART3_INTID 331U

/* PL011 reference clock: dsc PL011UartClkInHz = 100000000 for post-silicon
 * boards, which PL011UartClockLib.c returns for every PL011 outside FPGA
 * builds. ASSUMPTION: the Orange Pi 6 Plus firmware keeps it for UART3. */
#define CIX_UART_CLK_HZ 100000000U
#define CIX_UART_BAUD 115200U

/* GIC-700 (sky1.dtsi gic reg; dsc PcdGicDistributorBase and
 * PcdGicRedistributorsBase): 12 redistributor frames of 256 KiB (GICv4
 * frames), found by GICR_TYPER affinity (D2), not by index. */
#define CIX_GIC_DIST_BASE 0x0E010000UL
#define CIX_GIC_REDIST_BASE 0x0E090000UL
#define CIX_GIC_REDIST_SIZE 0x00300000UL

/* SMMUv3 HAVEN owns: the PCIe SMMU (MemoryMap.h SMMU0_BASE), described by
 * the IORT of the Orange Pi firmware images (porting ledger). Event-queue
 * interrupt: Iort.aslc gives SMMU0 PCIEHUB_EXT_TCU_EVENT_Q_IRPT_NS_
 * INTERRUPT_ID = 107 (InterruptId.h). */
#define CIX_SMMU0_BASE 0x0B010000UL
#define CIX_SMMU0_EVTQ_INTID 107U

/* CPUs: 12 (dsc PcdCoreCount; sky1.dtsi cpu@0 to cpu@b00). MPIDR has the
 * core number in Aff1, Aff0 = 0; firmware boots core 4 (dsc
 * PcdArmPrimaryCore 0x400). The logical map is in platform.c. */
#define CIX_NR_CPUS 12U

/* HAVEN's image at 0xF0000000, 8 MiB (owner decision 2026-11-09, loading
 * study docs/porting/cix-p1-loading.md): in DRAM for every memory size,
 * 2 MiB aligned, above every fixed firmware reservation found. CMake links
 * the image there (HV_LOAD_ADDR). */
#define HAVEN_LOAD_PA 0xF0000000UL
#define HAVEN_SIZE 0x00800000UL /* 8 MiB, as qemu-virt */

/* Partition A, owner decisions (D3 grant-cixp1-v0.json), used by
 * src/core/partition.c since D5. RAM: identity mapped, 2 GiB at 4 GiB;
 * ASSUMPTION until the 16 GB board's EFI memory map. Emulated GIC at the
 * GIC-700's addresses, one 128 KiB redistributor frame per vCPU. Target:
 * 8 vCPUs on logical CPUs 0 to 7; HAVEN supports 1 or 2 today (8 is the
 * step after D5, docs/architecture/multi-vcpu.md step 4). */
#define CIX_PART_A_RAM_BASE 0x100000000UL
#define CIX_PART_A_RAM_SIZE 0x80000000UL
#define CIX_PART_A_VGICD_IPA CIX_GIC_DIST_BASE
#define CIX_PART_A_VGICR_IPA CIX_GIC_REDIST_BASE
#define CIX_PART_A_VCPUS 8U

/* The names partition.c takes. UART2 is Linux's console; no read-only
 * region, no DMA grant (D3: no SMMU section) and no partition B (owner:
 * not mapped or started until it has its own record). */
#define PART_A_IPA_BASE CIX_PART_A_RAM_BASE
#define PART_A_PA_BASE CIX_PART_A_RAM_BASE
#define PART_A_SIZE CIX_PART_A_RAM_SIZE
#define PART_A_UART_BASE CIX_UART2_BASE
#define PART_A_UART_INTID 330 /* CIX_UART2_INTID, a plain decimal */
#define PART_A_VGICD_IPA CIX_PART_A_VGICD_IPA
#define PART_A_VGICR_IPA CIX_PART_A_VGICR_IPA
#define PART_A_LAST_CPU 7U /* A's CPUs are logical 0 to 7 */

#endif /* HAVEN_PLATFORM_CIX_P1_MEMORY_H */
