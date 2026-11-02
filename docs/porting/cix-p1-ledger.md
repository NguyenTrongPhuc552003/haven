# CIX P1 (Sky1) weakness ledger

**Status:** source-derived (deep research report, 2026-11-01). No row is
observed on our hardware yet. Rows 1 to 5 are the most important and also the
least certain: treat them as hypotheses to test on a board.
**Boards:** Radxa Orion O6 / O6N, Orange Pi 6 / 6 Plus.
**Evidence:** V = vendor or upstream source, C = community report (2+
sources), S = single source or disputed.

| # | Weakness | Ev. | Why it matters to HAVEN | How we test it (board) |
| - | - | - | - | - |
| 1 | GPU (ACPI `CIXH5000`) has no IORT/SMMU entry, so its DMA is untranslated | V (firmware source, see note) | GPU DMA bypasses every stage-2/SMMU grant; a GPU partition must be identity-mapped and trusted. Direct A1 case: granted is not enforced | `acpidump`, `iasl -d` IORT; read SMMU stream tables from HAVEN |
| 2 | GPU access gated by an EL3 interconnect (IDM) grant; violations cause an EL3 RAS print storm that stalls all cores | C | EL3 can stall every partition; GPU security state is invisible at EL2 | GPU reset from a non-root partition, watch EL3 log |
| 3 | SMMU0 event storm (`C_BAD_STREAMID`, ~130 IRQ/s) on some firmware; cause disputed | S | SMMU fault reporting unreliable; read-back must report `SMMU_CR0`, `STRTAB_BASE_CFG`, `GBPA` itself | Read both SMMUs per firmware version |
| 4 | PCIe needs bypass stream entries at boot; enabling PRI causes a storm | C | Bypass entries are granted-but-unenforced DMA; must appear in read-back | Read STEs, list bypass entries |
| 5 | Radxa firmware 1.3.x: PCIe no longer behind the OS-visible SMMU | S | No DMA isolation for PCIe devices; pin firmware version | Compare IORT and SMMU state across firmware versions |
| 6 | TF-A v2.7 lacks Cortex-A720/A520 errata, incl. A720 3699561 (GIC virtualisation register read can return wrong data) | C | Wrong `ICH_*` reads would corrupt vGIC read-back; HAVEN must apply its own EL2 errata | Check SDEN-2439421 / SDEN-2444153; test `ICH_*` reads |
| 7 | Firmware forks years behind upstream (EDK2 base 202208) | C | Harder to add a custom EL2 boot path or fix ACPI | n/a (record) |
| 8 | TF-A, OP-TEE, EDK2 rebuildable with OEM key; SE and SCP firmware closed | C | We can modify EL3/BL33 for experiments; SCP and SE stay black boxes | Rebuild BL31 once, record result |
| 9 | EL3 locks `IMP_CPUECTLR_EL1` writes (write hangs) | S | Limits prefetcher tuning for real-time | Do not touch without rebuilt BL31 |
| 10 | Mainline 6.19 unpatched: serial and NVMe only | C | Linux partition kernel must carry the CIX or Sky1-Linux patch set | Pin kernel and patch set |
| 11 | Upstream device tree is minimal (no GPU/DPU/VPU nodes) | V | A DT-based HAVEN must write or convert its own platform DT | Convert ACPI to DT, diff against vendor DT |
| 12 | Vendor direction is ACPI; mainline-track kernels require ACPI mode | V | Static partitioners assume DT; per-partition ACPI is hard. Early decision: DT or ACPI | Decide before board bring-up |
| 13 | `clk_ignore_unused` required to boot | V | Clock ownership fragile; one partition may gate another's clocks | Record clock tree per partition |
| 14 | GPU DVFS churn can wedge the shared SCMI mailbox, taking down all SCMI users | C | Shared SCMI channel is a cross-partition failure path; keep GPU in the partition that owns SCMI | Stress DVFS, watch other SCMI users |
| 15 | Intermittent cold-boot hang at EDK2 splash | S | Slows board test cycles | Count boots, record rate |
| 16 | CPU enumeration interleaves A520 cores; PPTT cache info wrong | C | Core assignment and cache colouring must use MPIDR and measured cache geometry, not ACPI | Read MPIDR per core, measure L3 sets |
| 17 | Enabling MTE caps usable RAM (~29 GiB of 64 GB) | C | Memory-map surprises; validate the EFI memory map | Disable MTE; dump memory map |
| 18 | HYM8563 RTC missing from ACPI; PXE broken (HTTP boot works) | C | Network boot of HAVEN images needs HTTP | n/a |
| 19 | Some firmware exposed 8 of 12 cores; post-launch firmware changed performance | S | Firmware version is a controlled variable in every experiment | Record firmware version in each run |
| 20 | DP/HDMI rendering glitches, worst on VPU-to-GL video | V | Demo load: glmark2 or vkcube, not video | n/a |
| 21 | PCIe Gen4 freezes; x16 slot reportedly broken on some revisions | S | No discrete-GPU plans | n/a |
| 22 | Firmware enters the OS at EL2 with VHE (`HCR_EL2.E2H` may be set) | S | Positive: HAVEN can start at EL2, but must handle E2H at entry | Dump `HCR_EL2`, `ID_AA64MMFR*` at HAVEN entry |
| 23 | Armbian support alpha | S | Low | n/a |

## Row 1 checked against vendor firmware source (2026-11-02)

CIX UEFI source, `data/common/cix-bios/edk2-platforms` at `e5cb70f` ("CIX P1
2026Q3 RC2.3 RELEASE"), `Platform/CIX/Sky1/Drivers/AcpiSocTables/`:

- `Iort.aslc` declares 14 named components: `AEU0` to `AEU4`, `CBD0`, `CBD2`,
  `DPU0` to `DPU4`, `ISPM`, `NPU0`. None is the GPU.
- `Iort-NoSmmu.aslc`, the variant for firmware with the SMMU disabled,
  declares no named component.
- `Dsdt-GPU.asl` declares `Device (GPU)` with `_HID "CIXH5000"` and `_CCA 1`
  (cache-coherent DMA master) and its own MMU interrupt.

So under ACPI boot with this firmware release the GPU is a DMA master with no
SMMU stream (reference fact). Still to confirm on a board: that its firmware is
built from this release, and the device-tree boot path.

Same release, `CommonDefines.h` (checked 2026-11-02 for the SMMU bugfix):

- `PCIE_SMMU_ENABLE 0`, `SUB_SYSTEM_SMMU_ENABLE 0`, `MM_HUB_SMMU_ENABLE 1`
  (with DPU, NPU, ISP and CSI enabled). In the default build, PCIe devices
  have no SMMU in their path either; only the multimedia hub SMMU is
  described. A PCIe device in a partition would then be an unenforced DMA
  master, as the GPU is.
- `Iort.aslc` sets `EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE` on all three
  SMMU nodes (`SMMU0_BASE` PCIe, `SMMU2_BASE`, `SMMU1_BASE`): the SMMUs'
  table and queue accesses are coherent, which HAVEN's SMMU_CR1 setting
  relies on (`docs/methodology/stage1/20261102-smmu-bugfix/`).

## Orange Pi 6 firmware image checked (2026-11-02)

`data/orange-pi-6/base/BIOS/opi6_bios_image_20260708.img` (vendor image,
not flashed; SHA-256
`4ed78c5a4185e25bfcf47bfba833cf36e451bd1135c516020a604d5d1596fba4`). Its LZMA-compressed
firmware volumes hold an ACPI IORT (revision 5, OEM `CIXTEK SKY1EDK2`,
19 nodes), decoded from the image bytes:

- an SMMUv3 at `0x0B010000` (`SMMU0_BASE` in
  `Silicon/CIX/Sky1/Include/MemoryMap.h`, the PCIe SMMU) with the coherent
  flag set, and the PCIe root complex mapping requester IDs `0x0`-`0xFFFF`
  to it one to one (StreamID = requester ID, as in QEMU `virt`);
- an SMMUv3 at `0x0B1B0000` (`SMMU2_BASE`) for the same 14 named components
  as the CIX source (DPU, AEU, NPU0, ISPM, CBD0, CBD2); no GPU entry;
- an RMR node.

So unlike the CIX RC2.3 source default (`PCIE_SMMU_ENABLE 0`), this Orange Pi
6 release describes the PCIe SMMU (reference fact for this image). The
GPU has no SMMU stream here either (row 1 holds). No Orange Pi 6 Plus
firmware image is in `data/`, so the same check has not been done for the
Plus.

## GPU facts used by the thesis (source-derived)

- GPU: Arm Immortalis-G720 MC10 (arch v12, CSF). Upstream Panthor knows it
  since Linux 6.18; Mesa 25.1 added PanVK v12 and Panfrost G720 (GLES 3.1,
  non-conformant). CIX power and clock glue is still out of tree.
- Working stacks reported on kernels 6.18 to 7.1 with Panthor or vendor kbase
  r54p1; Panthor on 7.2 fails (issue cixtech/cix-linux-main#59, unresolved).
- No published Jailhouse, Bao, Xen or pKVM port to CIX P1 was found.

## Demo decision input

Primary: Linux partition (identity-mapped) owns GPU, display and SCMI and runs
glmark2 or vkcube; a bare-metal real-time partition records jitter under GPU
and CPU memory load, with and without mitigation; HAVEN read-back reports
stage-2, SMMU and GIC state, including devices with no SMMU stream (row 1).
Fallback: the same without display, with a headless Vulkan compute load.

## Main sources

- cixtech/cix-linux-main README and issues #59, #70
- Sky1-Linux (github.com/Sky1-Linux), cix-gpu-kmd README
- orion-o6/firmware issues #1, #2, #5, #6, #7, #8, #10, #12
- radxa-pkg/edk2-cix issues #24, #39; Radxa forum O6N BIOS changelog thread
- Mesa `docs/drivers/panfrost.rst`; Panthor Gx20 series (LKML 2025-07)
- Martins and Pinto, "Shedding Light on Static Partitioning Hypervisors",
  arXiv 2303.11186
