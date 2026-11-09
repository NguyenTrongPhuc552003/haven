# CIX P1 board: how HAVEN is loaded and where it lives

**Status:** study, 2026-11-09 (owner-approved task: a study, not code). No
board yet, nothing here is a hardware observation. It answers open question
5 of `cix-p1-device-tree.md` and open question 1 of the D4 record
(`../methodology/stage2/20261109-cix-p1-platform/`), and ends with a
recommendation for the owner to decide.

Labels: **[F]** established fact (primary source: file and line, manual page,
specification); **[O]** observation of vendor files in `data/` (decoded
bytes, not hardware); **[S]** source-derived claim from a community or single
source; **[I]** inference; **[H]** hypothesis to test; **[P]** proposal.
Abbreviations: `cix-bios` = `data/common/cix-bios` at `7799770` ("CIX P1 BIOS
2026Q3 RC2.3 RELEASE"), with `edk2` at `9212cf3`, `edk2-platforms` at
`e5cb70f`, `edk2-non-osi` at `e725a1d`; `dsc` =
`edk2-platforms/Platform/CIX/Sky1/Sky1Common.dsc.inc`; "BIOS manual" =
`data/orange-pi-6/plus/opi-6plus-bios User Manual v1.4.pdf` (PDF page
numbers). The Orange Pi 6 Plus firmware itself is a binary built from CIX's
SDK (ledger row 26); CIX source facts apply to it only where its images
agree, which is said each time.

## 1. Short answer

- The board boots SE firmware, then TF-A v2.7 (BL2, BL31) with OP-TEE, then
  EDK2 UEFI as BL33. UEFI runs in the non-secure world at EL2 or EL1; every
  piece of evidence points to EL2, none proves it (section 3).
- HAVEN cannot run where it is linked today: 0x8000_0000 is the firmware's
  secure reservation, which UEFI marks reserved, so UEFI would refuse to
  place an image there (section 5).
- **Recommendation [P]:** a small UEFI application, separate from HAVEN,
  started from the firmware's built-in UEFI Shell on a USB stick or SD card.
  It does on the board what QEMU's `-device loader` does today: it places
  HAVEN and the partition images at fixed physical addresses, leaves boot
  services, turns the MMU and data cache off and jumps to HAVEN at EL2.
  HAVEN keeps its entry contract and gets a new fixed link address,
  0xF000_0000. Nothing is flashed; removing the stick undoes everything.
  Runner-up: the same hand-off built into HAVEN's own image as an EFI stub
  (the Xen and Linux packaging). Firmware integration needs flashing and is
  not recommended.

## 2. What HAVEN expects at entry today

| Item | Today | Source |
| - | - | - |
| Who starts HAVEN | firmware "U-Boot / ATF" loads `haven.bin` and jumps to it; in QEMU, `-device loader,...,cpu-num=0` places it at 0x8000_0000 and starts CPU 0 there | [F] `arch/arm64/boot.S` lines 5-11; `scripts/qemu/qemu-run.sh` line 69 |
| Exception level | EL2, checked; another EL halts | [F] `boot.S` lines 42-47 |
| x0 | "physical address of DTB"; passed to `haven_init`, which ignores it | [F] `boot.S` lines 7, 80-81; `src/core/init.c` line 153 |
| MMU and caches | assumed off: `hv_arch_el2_mmu_enable` invalidates (not cleans) every cache line over the image because "until now every data access was Device (translation off)" | [F] `arch/arm64/mmu_el2.c` lines 112-120 |
| HCR_EL2 | overwritten with `VM, RW, IMO, FMO, AMO, TSC` (E2H = 0); the EL2 map assumes the single-range regime, E2H = 0 | [F] `arch/arm64/cpu.c` lines 30-31; `asm/sysregs.h` lines 102-103; `asm/el2_mmu.h` line 14 |
| Address | linked at `HV_LOAD_ADDR`, default 0x8000_0000, overridable with `--defsym`; built `-fno-pie`, static: function pointers and pointers in data are absolute, so the image must run at its link address | [F] `linker.ld` line 21; `cmake/flags.cmake` line 13; `src/platform/cix-p1/platform.c` (`platform = &cix_p1_ops`) |
| EL2 map | every region 2 MiB aligned, or the map is rejected | [F] `mmu_el2.c`, check `(g->pa \| g->size) & (SZ_2M - 1U)` |
| Partition images | not loaded by HAVEN: QEMU places each guest image (and a Linux guest's DTB) at a fixed address before HAVEN runs | [F] `scripts/qemu/qemu-run.sh` lines 54-57; `CMakeLists.txt` line 171 (`HAVEN_PART_A_DTB_IPA`) |
| Secondary CPUs | started by HAVEN with PSCI CPU_ON (SMC conduit on cix-p1) | [F] `boot.S` `hv_arch_psci_cpu_on`; D4 record |
| Read-back | maps ELF symbols to memory with `nm`, so the image must sit where the ELF says | [F] `tools/readback/readback.py` line 494 |

So HAVEN's contract is the one Linux documents for a raw arm64 image: EL2,
MMU off, image cleaned to the point of coherency, x0 = a DTB address. [F]
Linux `Documentation/arch/arm64/booting.rst` (6.9): "The MMU must be off",
the image range "must be cleaned to the PoC", EL2 "RECOMMENDED", interrupts
masked in DAIF.

## 3. The firmware chain and the exception level

| Stage | What | EL | Evidence |
| - | - | - | - |
| Security engine | `sfh_fw.bin`, SE firmware ("SE Version beta2.0-...") | its own core | [F] `edk2-non-osi/.../PackageTool/spi_flash_config_all.json` (image type 8); BIOS manual PDF page 8 (Firmware Information) |
| BL2 ("PBL") | `bootloader1.img`: strings `v2.7(release):Beta_2.1.5_release`, `BL2: Booting BL31` | EL3 | [O] strings of the CIX prebuilt (SPI type 1); manual page 8: "PBL Version v2.7-...", "ATF Version v2.7-..." |
| BL31 + BL32 | `bootloader2.img`: TF-A v2.7 BL31 and OP-TEE ("OP-TEE cix version") | EL3, S-EL1 | [O] strings (SPI type 2); manual page 8 "TEE Version"; "STMM Version" (a Standalone MM secure partition) |
| BL33 | `bootloader3.img`: a FIP holding only `SKY1_BL33_UEFI.fd` and its certificates, signed with the OEM key | non-secure EL2 or EL1 | [F] `edk2-non-osi/.../build_and_package.sh` lines 276-293; SPI offset 0x406000 (type 7) |
| OS loader | the vendor Linux image: GRUB as `\EFI\BOOT\BOOTAA64.EFI`, then `Image` and the DTBs on the same FAT partition | as UEFI | [F] `orangepi-build` `a95129b`, `scripts/build-cix-image.sh` lines 133-153 |

**At which EL does UEFI run?**

- [F] UEFI 2.10 section 2.3.6: AArch64 firmware runs at the highest
  non-secure level available, "Non-secure EL2 (Hyp) or Non-secure
  EL1(Kernel)"; loaded images run in the same state; the MMU is on with "any
  RAM defined by the UEFI memory map identity mapped", caches on; secondary
  cores are off or held; the image gets x0 = image handle, x1 = system table.
- [F] CIX's UEFI handles EL2: `Platform/CIX/Sky1/PrePi/AArch64/ArchPrePi.c`
  lines 24-26 write `HCR_EL2 = ARM_HCR_TGE` (bit 27 only, `edk2`
  `ArmPkg/Include/Chipset/AArch64.h` line 59) when `CurrentEL == EL2`;
  `edk2` `ArmExceptionLib/AArch64/AArch64Exception.c` lines 43-49 only ORs
  TGE in again. No CIX source writes E2H. TF-A decides the BL33 entry level
  and its BL31 is a closed binary here.
- [F] The firmware describes EL2 resources: GTDT in all four Orange Pi 6
  Plus images has the non-secure EL2 timer at GSIV 26 and the virtual EL2
  timer at 28 [O, decoded below]; the CIX MADT gives every core a VGIC
  maintenance interrupt, GSIV 25 (`Drivers/AcpiSocTables/Madt.aslc` lines
  59-111).
- [F] The vendor kernels are built with `CONFIG_VIRTUALIZATION=y`,
  `CONFIG_KVM=y` (`orangepi-build` `external/config/kernel/
  linux-6.6-cix-p1-next.config` lines 710-711; `linux-6.1-cix-p1-current`
  lines 681-682). A config option does not show that KVM starts.
- [S] A review of the Orange Pi 6 Plus with "Cix Technology Group UEFI,
  version 1.3" reports `/dev/kvm` present and Proxmox "creating ARM VMs"
  (taoofmac.com, 2026-04-11). KVM needs the kernel entered at EL2, so this
  is indirect evidence that this firmware hands its loaded image EL2. One
  source, firmware version 1.3, not our board.

[I] UEFI almost certainly runs at EL2 with E2H = 0 and TGE = 1, and an EFI
application inherits that. This corrects the reading of ledger row 22
("firmware enters the OS at EL2 with VHE"): nothing in the firmware sets
E2H; VHE is what Linux turns on by itself when it starts at EL2. Probe P2
settles it.

## 4. Options to load HAVEN

| Option | What it needs | Reversible? | Risk | Effort [estimate] | HAVEN changes |
| - | - | - | - | - | - |
| **A2. Separate EFI loader** (recommended) | a PE/COFF EFI application (`haven-boot.efi`) on a FAT USB stick or SD card, run from the built-in UEFI Shell, `startup.nsh`, or a GRUB `chainloader` entry; it loads raw `haven.bin` and partition images to fixed addresses, calls ExitBootServices, cleans to PoC, turns SCTLR_EL2.M and C off, jumps | **yes, fully**: no flash, no NVRAM change needed (a one-time Boot Manager choice); pull the stick | low: a loader bug hangs one boot; power-cycle recovers | loader about one week; probe mode first (P2) a day or two | link address (section 5), entry checks for E2H = 0 and MMU off; contract unchanged |
| A1. HAVEN as its own EFI application | a PE/COFF header and an EFI entry stub in HAVEN's image doing A2's work; Xen does this on arm64 ("the PE/COFF header is open-coded in assembly ... a single binary may be booted as an Image file or as an EFI application", Xen `docs/misc/efi.pandoc`, 4.19) | yes, same as A2 | low; the UEFI-facing code lives in HAVEN's image and is counted by `tcb-image-loc.py` although it is dead after the jump | similar to A2 | header in `boot.S` or a new `efi-entry.S`, EFI calls in the image, CMake objcopy step |
| B. A "kernel" booted by GRUB | GRUB on arm64 EFI refuses non-PE images: "plain image kernel not supported - rebuild with CONFIG_(U)EFI_STUB enabled" [F, GRUB `loader/arm64/efi/linux.c`, quoted in the openSUSE grub2 patch "Remove magic number header field check"] | yes | as A1 | reduces to A1 | reduces to A1; GRUB is only a menu in front of A1 or A2 |
| C. In the firmware image (BL33 replacement or extra FIP stage) | rebuilt FIP signed with the OEM key, written to SPI NOR at 0x406000 with `FlashUpdate.efi` or an SPI programmer [F, `cix-bios` `Readme.md`]; whether the retail board checks the NT-FW certificate against a fused key is not in `data/` (`EfusePublicKeyLib` exists, effect unknown) | **no: requires flashing the board** (standing rule: never flash). Recovery needs an SPI programmer | high: brick until reflashed; key may not match | weeks (vendor build, signing, recovery setup) | HAVEN becomes BL33 (as the `qemu-tfa` profile in QEMU) but must also replace UEFI's role (no ACPI or DT hand-over, no file loading) |
| D. kexec from the vendor Linux | vendor kernels have `CONFIG_KEXEC=y`, `CONFIG_KEXEC_FILE=y` [F, configs lines 275-277 and 429-430]; Linux v6.6 `relocate_kernel.S` turns the MMU off and jumps with x0 = DTB, at EL2 through `HVC_SOFT_RESTART` for nVHE, or directly at EL2 under VHE [F, lines 80-100] | yes (SD card only) | medium: HAVEN inherits a used machine (GIC LPI tables, SMMU and PCIe set up by Linux, GPU and clocks running), and under VHE it is entered with E2H = 1, TGE = 1; [H] kexec-tools chooses the load address, so a non-relocatable HAVEN may not land at its link address | small for a first test | arm64 Image header (64 bytes); entry handling of E2H = 1; probably relocation |
| E. Late launch from Linux (Jailhouse style) | a kernel module installs HAVEN at EL2 through the hyp stub after Linux booted | yes | changes HAVEN's model: Linux would build the machine HAVEN partitions, against the static, boot-first design the research questions assume | large | architectural |

Notes:

- [F] The firmware has the UEFI Shell as a Boot Manager entry (BIOS manual
  PDF pages 26-27, device path `FvFile(7C04A583-9E3E-4F1C-AD65-
  E05268D0B4D1)`, the EDK2 Shell file GUID) and the shell runs `startup.nsh`
  after five seconds (photo on PDF page 27). The vendor's own flashing
  procedure runs EFI programs from a FAT32 USB stick this way; we use the
  same path only to run our programs, never `FlashUpdate.efi`.
- [F] EDK2 places a PE image at its preferred `ImageBase` when it is at or
  above 1 MiB and free, falls back to any address only if the image keeps
  relocations, and refuses otherwise (`edk2` `MdeModulePkg/Core/Dxe/Image/
  Image.c` lines 654-697). So A1 with relocations stripped either lands at
  HAVEN's link address or fails cleanly; A2's loader gets the same
  guarantee from `AllocatePages(AllocateAddress)` (UEFI 2.10, 7.2).
- [I] Why HAVEN cannot simply keep running in UEFI's state: UEFI leaves the
  MMU and data cache on (spec above). HAVEN zeroes BSS and sets its stack
  with caches on, then `mmu_el2.c` invalidates those lines, which would
  throw away the zeroed BSS, the stack and data the loader copied. The
  loader must therefore clean to PoC and switch translation and the data
  cache off before the jump, exactly as Linux's EFI stub did: [F] Linux
  v5.15 `arch/arm64/kernel/efi-entry.S` lines 26-67 (clean the image to
  PoC, invalidate the I-cache, clear SCTLR_EL2.M and C at EL2, `br` with x0
  = DTB, x1 to x3 = 0).
- [F] At ExitBootServices EDK2 disables every GIC interrupt source, the CPU
  interface and the distributor (`edk2` `ArmPkg/Drivers/ArmGic/GicV3/
  ArmGicV3Dxe.c` lines 339-356, the driver the `dsc` builds, line 773) and
  stops the generic timer it used (`ArmPkg/Drivers/TimerDxe/TimerDxe.c`
  lines 92-98). HAVEN starts from a quiet GIC, as in QEMU.

**Why A2 over A1 [I]:** both use the same hand-off; A2 keeps every UEFI call
out of HAVEN's image, so HAVEN's trusted-code count and its QEMU entry stay
exactly as they are, and the loader is replaceable (QEMU `-device loader`,
the EFI loader, or later a firmware stage) without touching HAVEN. The
loader is still in the trusted boot path (it decides what gets loaded); the
thesis should say so and the loader should print what it loaded (sizes,
SHA-256) for each run record. A1 wins only if a single artifact matters
more than that separation.

## 5. Memory

### 5.1 DRAM extent on the board

- [F] Boards built with `UPDATE_MEMORY_TOP` (all four CIX board packages:
  `Alcor.dsc` line 260, `Edge.dsc` 263, `Merak.dsc` 267, Radxa `O6.dsc`
  249) take the top of DRAM at boot from the memory-init result, not from
  `PcdSystemMemorySize`: `PrePi/AArch64/ModuleEntryPoint.S` lines 28-58 read
  `AvailableSize` (MiB) at `PcdMemInfoBaseAddr` 0x83C0_0000 and set the end
  to 0x8000_0000 + min(size, 0x7800 MiB) - 1. Anything above 30 GiB is a
  second range at `PcdDramHighSpaceBase` 0x80_0000_0000
  (`Library/PlatformLib/ArmPlatformLibMem.c` lines 27-48, 183-197).
- [I] With 16 GB, DRAM is one range, 0x8000_0000 to about 0x4_7FFF_FFFF
  (less if memory harvesting or inline ECC lowers `AvailableSize`; BIOS
  manual PDF pages 15-16 show both options). This also corrects the D3 record's
  "every DRAM source agrees on 4 GiB": `PcdSystemMemorySize` (0x1_0000_0000,
  `dsc` line 494) is a build default that these boards override. Below
  0x8000_0000 the firmware maps device space (`ArmPlatformLibMem.c` line
  148). Whether the Orange Pi build also defines `UPDATE_MEMORY_TOP` is not
  in `data/`: probe P1.

### 5.2 Fixed reservations below 4 GiB

| Range | What | Kept from the OS how | Source |
| - | - | - | - |
| 0x8000_0000 + 0x250_0000 | secure memory (TF-A, OP-TEE) | `EfiReservedMemoryType` HOB; mapped uncached | [F] `dsc` line 498; `MemoryInitPeiLib.c` line 237; `ArmPlatformLibMem.c` line 159 |
| 0x8250_0000 + 0x1F0_0000 | shared non-secure memory with firmware (SCMI/SMC buffers; memory-init info at 0x83C0_0000) | `EfiRuntimeServicesData` | [F] `dsc` lines 500-501; `MemoryInitPeiLib.c` line 243 |
| 0x8440_0000 + 0x1F_0000 | UEFI firmware volume (boot time) | `EfiBootServicesData` | [F] `Merak.fdf` line 28; `MemoryInitPeiLib.c` line 186 |
| 0x8480_0000 + 0x80_0000 | GOP frame buffer | `EfiReservedMemoryType` | [F] `dsc` lines 502-503; `MemoryInitPeiLib.c` line 224 |
| 0x85F0_0000 + 0x10_0000 | firmware shared memory | `EfiRuntimeServicesData` | [F] `Sky1.dec` lines 85-86; `MemoryInitPeiLib.c` line 249 |
| 0xA000_0000 + 0x800_0000 | RAM-disk boot image, only if `PcdRamdiskEnable` | `EfiReservedMemoryType` | [F] `dsc` line 496; `MemoryInitPeiLib.c` lines 192-197 |
| 0xBCE0_0000 + 0x100_0000 | GPU reservation, only if `PcdGpuEnable` | `EfiReservedMemoryType` | [F] `CixPkg.dec` lines 409-410; `MemoryInitPeiLib.c` lines 212-217 |
| 0xCDE0_0000 + 0x320_0000 | audio DSP | `EfiReservedMemoryType` | [F] `dsc` lines 506-507; `MemoryInitPeiLib.c` line 255 |
| 0x1_4000_0000 + 0x1_2000_0000 | Android reservation | **not reserved**: its HOB is commented out | [F] `MemoryInitPeiLib.c` lines 230-235 |

[O] The vendor Orion O6 tree's `reserved-memory` (`edk2-platforms/
Platform/Radxa/Orion/O6/DeviceTree/sky1-orion-o6.dtb`, decompiled) adds
Linux-side `no-map` regions from 0x8250_0000 to 0xD0E0_0000 and one 512 KiB
region at 0x1C_A000_0000; none lies in 0xD100_0000 to 0x1_8000_0000.
Mainline `sky1-xcp.dts` (v7.3) reserves only a 640 MiB CMA pool by size.

### 5.3 Proposed placement [P]

| What | Where | Why |
| - | - | - |
| HAVEN image | **0xF000_0000**, 8 MiB (`HAVEN_LOAD_PA`, `HV_LOAD_ADDR`) | in DRAM for every memory size; 2 MiB aligned (EL2 map rule); above every fixed reservation found (highest ends at 0xD100_0000) and outside the RAM-disk and GPU ranges; 248 MiB below partition A's window; below 4 GiB, so the same address works on a 4 GiB variant |
| Partition A RAM | 0x1_0000_0000 + 2 GiB, identity mapped (owner decision, D3) | inside DRAM on the 16 GB board [I, 5.1]; no fixed reservation overlaps it (5.2); the loader claims it with `AllocateAddress` before writing the kernel, DTB and initrd into it, so nothing of UEFI's can be there when HAVEN starts |
| Partition B RAM | not decided here | free on paper: 0xD100_0000 to 0xF000_0000 (496 MiB) or above 0x1_8000_0000 (12 GiB on 16 GB) |
| Firmware ranges | never granted as RAM | secure, shared and firmware-shared memory stay out of every partition; a partition driver that needs one (for example SCMI's shared memory at 0x8438_0000 in `sky1.dtsi`, inside the shared range) needs an explicit identity-mapped grant, which D3's grant does not have yet [I] |

**Fixed address, not relocatable [I]:** HAVEN has absolute pointers in data
and read-back maps ELF symbols to memory (section 2), so a fixed link
address keeps the image, the ELF and read-back in agreement with no new
code. Making HAVEN position independent would add self-relocation to its
trusted code and an offset to every read-back. UEFI does not force
relocation: both the loader (`AllocateAddress`) and EDK2's own image loader
(Image.c above) honour a fixed address or fail visibly. If probe P1 shows
0xF000_0000 taken by boot-time allocations, the fallback is another fixed
address from the free ranges above, not relocation.

What the current draft does instead: `src/platform/cix-p1/memory.h` lines
52-57 keep 0x8000_0000, inside the secure reservation; EDK2 would refuse
that range (it is an `EfiReservedMemoryType` allocation), and a CPU write
there from the non-secure world would hit TF-A's protected memory [I].

## 6. Console, timer and interrupts at hand-off

| Item | What the sources say | Status |
| - | - | - |
| UEFI console | UART2, PL011 at 0x040D_0000, 115200 baud, 100 MHz reference clock on boards | [F] `dsc` lines 522, 544-545; BIOS manual page 30: UART2 carries "BIOS and kernel logs" |
| UART3 (HAVEN's console) | CIX's build names it the debug UART (`PcdSerialDbgRegisterBase` 0x040E_0000, INTID 331, `dsc` lines 525-526) and describes it in SPCR (`Spcr.aslc` line 28). If the setup option "Support SPCR" is enabled, UEFI initialises UART3 at 115200 at ReadyToBoot and keeps SPCR; if disabled, it uninstalls SPCR and leaves UART3 alone (`Drivers/AcpiSocDxe/AcpiSocDxe.c` lines 35, 524-549, 591-615). Default `PcdAcpiSpcrEnable` is FALSE (`Sky1.dec` line 161) and the manual shows "Support SPCR <Disabled>" (PDF page 19) | [F] |
| SPCR and DBG2 in the Orange Pi images | all four images (`bios_v1.0.bin` and `bios_40pin_v1.0.bin` from the Linux release image, General 1.4, Android 1.3) carry the same SPCR (PL011 at 0x040E_0000, GSIV 331, 115200) and DBG2 (PL011 at 0x040D_0000) | [O] decoded from the LZMA firmware volumes, read-only, with the method of `tools/fwscan/iort_scan.py` (SPCR and DBG2 table hashes `265b1aa77efa`, `3e7165797fa9`) |
| UART3 pins | not set by the CIX source paths read; Orange Pi ships a separate "40pin" BIOS variant (`bios_40pin_v1.0.bin`) whose difference is not documented | [I]; probe P5 |
| Counter | CNTFRQ programmed by firmware (required by UEFI 2.10, 2.3.6); CIX builds 1 GHz for boards (`dsc` line 547); HAVEN reads CNTFRQ_EL0 | [F] |
| Timers | GTDT in all four images: secure EL1 29, non-secure EL1 30, virtual 27, non-secure EL2 26, virtual EL2 28, the PPIs HAVEN already uses (26 budget, 27 guest) | [O] decoded; [F] HAVEN D4 record |
| UEFI's timer at EL2 | TimerDxe registers the EL2 (hyp) timer interrupt (`TimerDxe.c` line 392) and disables the timer at ExitBootServices (lines 92-98) | [F] |
| Watchdog | an SBSA generic watchdog at 0x1600_3000 (EL2 interrupt 408, `dsc`) and a setup option "SOC Watchdog Timer <Enabled>" (PDF page 19) whose value goes to the power-management configuration (`PlatformSetupVariableInitDxe.c` line 146). Whether a watchdog runs after ExitBootServices is not in the source | open; probe P6 |
| Display | the GOP frame buffer at 0x8480_0000 is reserved and the IORT has an RMR node (ledger) | [I] the display controller may keep scanning it out after ExitBootServices: a DMA master alive at HAVEN's start; probe P7 |

## 7. Recommendation for the owner [P]

1. **Choose A2** (separate EFI loader, started from the UEFI Shell),
   runner-up A1. Not C (flashing), not D or E as the main path.
2. Set cix-p1's link address to 0xF000_0000 (memory.h `HAVEN_LOAD_PA` and
   the CMake `HV_LOAD_ADDR` for `PLATFORM=cix-p1`), keeping qemu-virt at
   0x8000_0000. Add two entry checks to `boot.S`, fail closed: HCR_EL2.E2H =
   0 and SCTLR_EL2.M = 0, else park.
3. Steps, each small and observable:
   - **L0 probe** (read-only, A2's first half): an EFI application that
     prints and saves to the stick what probe P2 lists, then returns to
     the shell. No HAVEN, no ExitBootServices.
   - **L1 rehearsal in QEMU**: QEMU `virt` with `virtualization=on` and
     EDK2 firmware runs UEFI at EL2 [I, to verify]; the loader starts the
     qemu-virt HAVEN build from a FAT drive and the existing QEMU checks
     must pass unchanged. Runs in CI, before the board.
   - **L2 loader on the board**: HAVEN's banner on UART3, then partition A.

Loader behaviour, in order [P]: check CurrentEL = 2 (else return
`EFI_UNSUPPORTED` to the shell, nothing changed); claim HAVEN's range and
the partition windows with `AllocateAddress` (any failure: print and
return); read the files to their addresses and print sizes and SHA-256; get
the memory map and call ExitBootServices (retry on a stale map key); mask
DAIF; clean the loaded ranges and the hand-off code to PoC; clear
SCTLR_EL2.M and C; invalidate the I-cache and EL2 TLBs; x0 = 0 (HAVEN reads
nothing from it today); branch to 0xF000_0000. Secondary cores stay off
(UEFI 2.10, 2.3.6) for HAVEN's PSCI CPU_ON.

Open toolchain point [H]: GNU objcopy can emit an AArch64 EFI application
(`efi-app-aarch64`) only in recent binutils; if the CI and macOS toolchains
lack it, the PE/COFF header can be written by hand in assembly as Linux and
Xen do. No EDK2 or vendor build is needed either way.

## 8. What must be confirmed on the board

| # | Probe | How | Decides |
| - | - | - | - |
| P1 | UEFI memory map | UEFI Shell `memmap -b` (save with `> fsN:\memmap.txt`); in Linux `/proc/iomem` | 0xF000_0000 + 8 MiB and 0x1_0000_0000 + 2 GiB are conventional memory; DRAM top (5.1) |
| P2 | Entry state | L0 probe prints CurrentEL, HCR_EL2, SCTLR_EL2, TCR_EL2, ID_AA64MMFR1_EL1.VH, ID_AA64MMFR4_EL1.E2H0, MPIDR (boot core 0x400?), MIDR, CNTFRQ_EL0, CNTHCTL_EL2, ICC_SRE_EL2, DAIF; lists the EFI configuration tables (ACPI, device tree) | EL2 and E2H assumption (section 3); boot CPU = entry 0 of HAVEN's CPU map |
| P3 | Both system-table modes | P1 and P2 with "System Table Selection" ACPI and Device Tree, and "Support SPCR" on and off (setup options, restored with F9; no flash) | whether DT mode changes memory or UART3 set-up |
| P4 | Vendor Linux | `dmesg` lines "CPU: All CPU(s) started at EL2" and "kvm [1]: ... initialized"; dump `/sys/firmware/fdt`; `/proc/cmdline` | cross-check of P2; the firmware's tree (ledger row 26) |
| P5 | UART3 pins | L0 probe writes a line to UART3; watch with a 3.3 V adapter on the 40-pin header, general and "40pin" BIOS behaviour compared only by reading, never flashing | whether HAVEN needs its own pinmux |
| P6 | Watchdog after ExitBootServices | a loader test that leaves boot services and prints a counter on UART2 by polling for ten minutes | whether HAVEN must service or stop a watchdog |
| P7 | Display after ExitBootServices | does the screen keep UEFI's last picture while HAVEN runs | a live DMA master at hand-off (section 6) |
| P8 | MMU-off hand-off | the loader prints one character on UART2 after clearing SCTLR_EL2.M and C, before the jump | the cache-clean sequence is right |

## 9. What Orange Pi (or CIX) could answer

1. Does the retail firmware enter UEFI (BL33) at non-secure EL2, and do
   EFI applications run at EL2 with HCR_EL2.E2H = 0?
2. Is the Orange Pi build's secure reservation 0x8000_0000 + 0x250_0000 as
   in CIX's source, and are `PcdRamdiskEnable`, `PcdGpuEnable` and
   `UPDATE_MEMORY_TOP` set? Any other fixed reservation below 4 GiB?
3. What does `bios_40pin_v1.0.bin` change compared with `bios_v1.0.bin`:
   only the pinmux of UART1 and UART3 on the 40-pin header?
4. Is any watchdog left running after ExitBootServices ("SOC Watchdog
   Timer"), and who must service it?
5. Are the Orange Pi 6 Plus board package sources ("CIX Phecda Board" in
   the setup screen) published?

## 10. Sources outside `data/`

- UEFI Specification 2.10, section 2.3.6 "AArch64 Platforms" and 7.2
  (AllocatePages): https://uefi.org/specs/UEFI/2.10/02_Overview.html
- Linux, "Booting AArch64 Linux": https://docs.kernel.org/6.9/arch/arm64/booting.html
- Linux v5.15 `arch/arm64/kernel/efi-entry.S`; v6.6
  `arch/arm64/kernel/relocate_kernel.S` and `machine_kexec.c`
  (github.com/torvalds/linux, tags v5.15 and v6.6)
- Xen 4.19 `docs/misc/efi.pandoc` (github.com/xen-project/xen, tag
  RELEASE-4.19.0)
- GRUB arm64 EFI loader check, as quoted in openSUSE grub2
  `0001-loader-arm64-efi-linux-Remove-magic-number-header-fi.patch`
  (src.opensuse.org/rpm/grub2)
- Orange Pi 6 Plus review (KVM, Proxmox; single source):
  https://taoofmac.com/space/reviews/2026/04/11/1900
