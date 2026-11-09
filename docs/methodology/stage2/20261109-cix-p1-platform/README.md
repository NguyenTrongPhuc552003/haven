# D4: a draft CIX P1 platform, a wider emulated GIC, and dt_check.py's D3 gaps

**Status:** step D4 of `../../../porting/cix-p1-device-tree.md` (owner-approved
2026-11-09, scope fixed). Design and expectations by Claude, committed and
merged before any code. No board exists: everything here is checked by
building and by host tests. Labels: **[F]** fact from our code or records,
**[O]** source observation in `data/` (file and revision named), **[A]**
assumption (no source in `data/` or our records), **[I]** inference, **[P]**
proposal. Nothing here is a hardware observation.

## Problem

D3 (`../20261109-board-partition-tree/`) produced partition A's device tree
for the Orange Pi 6 Plus and left four findings. HAVEN has no CIX P1
platform at all, so nothing yet states the board's CPU map, GIC, console
UART and timer in code; its emulated GIC reports 64 interrupt IDs, so
UART2's INTID 330 could never reach Linux (finding 1); and `dt_check.py`
does not compare CPU identities (finding 2), assumes 3-cell interrupt
specifiers (finding 3) and ignores `/psci` (finding 4). D4 does four things,
and only these:

1. a draft `src/platform/cix-p1/` (memory.h, platform.c) selected by CMake
   `PLATFORM=cix-p1`, that builds;
2. the emulated GIC's interrupt range made a per-platform value, 352 IDs on
   cix-p1, 64 (unchanged) on qemu-virt;
3. `dt_check.py`: CPU identity, specifier width, a `/psci` information line,
   and an emulated-GIC range option;
4. D3's seeded trees run again through the new `dt_check.py`.

## Design

### Part 1: the platform draft

Data sources (read only): Linux `v7.3` `sky1.dtsi` in
`data/common/linux-dts-cix/v7.3/` (D1); CIX BIOS `data/common/cix-bios`,
`edk2-platforms` at `e5cb70f` ("CIX P1 2026Q3 RC2.3 RELEASE"):
`Platform/CIX/Sky1/Sky1Common.dsc.inc` (below: `dsc`),
`Silicon/CIX/Sky1/Include/MemoryMap.h`, `InterruptId.h`,
`Platform/CIX/Sky1/Drivers/AcpiSocTables/Iort.aslc`; the ledger
(`../../../porting/cix-p1-ledger.md`) and the partition plan.

| Item | Value in the draft | Source |
| - | - | - |
| CPU map (`hv_cpu_mpidr`, boot CPU first) | 0x400, 0x000, 0x100, 0x200, 0x300, 0x500, 0x600, 0x700, 0x800, 0x900, 0xA00, 0xB00; `hv_cpu_count` = 12 | [O] MADT order and `GET_MPID(core, 0)` (ledger "CPU numbering"); `PcdArmPrimaryCore` 0x400 and `PcdCoreCount` 12 (`dsc`); the 12 cpu `reg` values of `sky1.dtsi`. Logical 0 to 7 are partition A's, 8 to 11 partition B's (owner) |
| GIC distributor | 0x0E01_0000 | [O] `sky1.dtsi` `gic` reg; `PcdGicDistributorBase` (`dsc`) |
| GIC redistributors | base 0x0E09_0000, region 0x30_0000 (12 frames of 256 KiB) | [O] `sky1.dtsi` (`/* GICR * 12 */`); `PcdGicRedistributorsBase`. HAVEN finds each CPU's frame by GICR_TYPER affinity (D2) [F] |
| HAVEN's console | PL011 UART3 at 0x040E_0000 (INTID 331, not enabled by HAVEN: polled output) | [O] `sky1.dtsi` `uart3` (SPI 299); `UART3_BASE` (`MemoryMap.h`); `FCH_INTR_UART3_INTERRUPT_ID` 331 (`InterruptId.h`); owner decision (partition plan, decision 5) |
| UART3 reference clock | 100 MHz, 115200 baud | [O] `PL011UartClkInHz` 100000000 for post-silicon boards (`dsc`), returned for every PL011 by `PL011UartClockLib.c` outside FPGA builds. [A] that the Orange Pi 6 Plus firmware keeps it for UART3 (D3 open question 2 asks the same for UART2) |
| UART3 pins | not set by HAVEN | [A] the vendor's "40pin" BIOS variant routes them (partition plan, decision 5); HAVEN has no pinmux code. Board check |
| Linux's console (recorded, not used by HAVEN) | UART2 0x040D_0000, INTID 330 | [O] `sky1.dtsi`; `PcdSerialRegisterBase`, `PL011UartInterrupt` 330 (`dsc`) |
| Timer frequency | read from CNTFRQ_EL0 (as qemu-virt) | [F] firmware programs it; [O] `PcdArmArchTimerFreqInHz` 1000000000 for boards (`dsc`), noted in a comment only |
| SMMU HAVEN owns | SMMUv3 0x0B01_0000 (PCIe SMMU), event-queue INTID 107 | [O] `SMMU0_BASE` (`MemoryMap.h`); `Iort.aslc` gives SMMU0 the event interrupt `PCIEHUB_EXT_TCU_EVENT_Q_IRPT_NS_INTERRUPT_ID` = 107 (`InterruptId.h`); [F] the IORT of the Orange Pi images describes this SMMU (ledger). HAVEN owns the SMMU (partition plan, section 2); `smmu_v3_init` refuses base 0, so a base is required [F] |
| PSCI to firmware | SMC conduit (`HAVEN_PSCI_CONDUIT_SMC`) | [O] `sky1.dtsi` `/psci` `method = "smc"`. Guests keep PSCI over HVC, emulated by HAVEN (owner) |
| EL2 map (`hv_el2_regions`) | Device 0x0 + 1 GiB (UART3, SMMU0 and the GIC all lie below 0x4000_0000); Normal: HAVEN's image | [F] same shape as qemu-virt |
| HAVEN's load address | the linker default, 0x8000_0000, 8 MiB | [A] `data/` does not say how HAVEN is loaded (porting note, open question 5). This default lies inside `PcdReservedSecureMemoryBase/Size` 0x8000_0000 + 0x250_0000 (`dsc`) [O], so **the draft image cannot boot on the board as linked**; kept so the build has one consistent address until the load method is known |
| Partition A window and emulated GIC IPAs | constants only: RAM 0x1_0000_0000 + 0x8000_0000; vGICD 0x0E01_0000; vGICR 0x0E09_0000 + 8 x 0x2_0000 | owner decisions and D3's grant [F]; RAM window is [A] until the board's EFI memory map. Not consumed yet (next paragraph) |

What the draft does not do [F]: `src/core/partition.c` includes the
qemu-virt `memory.h` unconditionally and holds QEMU's partition tables and
vGIC IPAs (0x0800_0000, 0x080A_0000). D4's scope is the platform files, so
partition.c is not changed: a `PLATFORM=cix-p1` build carries QEMU's
partition layout and is a compile-only check of the platform code. Taking
the partition table from the platform is a later step [P]. The i.MX
platforms build the same way today [F].

CMake: `PLATFORM=cix-p1` selects `src/platform/cix-p1/platform.c`, the PL011
driver, the SMC conduit and the vGIC width below; a preset `arm64-cix-p1`
(build directory `build-cix-p1`); CI's ARM64 job gets one more step that
builds it (no boot).

### Part 2: the emulated GIC's interrupt range

Today `HV_VGIC_NR_IRQS` is 64 in `include/haven/vgic.h` and
`GICD_TYPER_VALUE` hard-codes ITLinesNumber = 1 in `vgic.c` [F]. Linux
enables INTID 330 by writing GICD_ISENABLER10; with 64 IDs that word is
read-as-zero, write-ignored, so the physical SPI is never enabled.

Design:

- `vgic.h` takes the width from `HAVEN_VGIC_NR_IRQS` when the build defines
  it and keeps 64 otherwise; a static assertion requires a multiple of 32
  between 64 and 1024. `GICD_TYPER.ITLinesNumber` becomes
  `HV_VGIC_NR_IRQS / 32 - 1` (1 for 64 IDs, so qemu-virt is unchanged;
  10 for 352). Every array and range check in `vgic.c` already uses
  `HV_VGIC_NR_IRQS` [F].
- CMake defines `HAVEN_VGIC_NR_IRQS=352U` for the `haven` target when
  `PLATFORM=cix-p1`, target-wide, so every translation unit sees the same
  `struct hv_vgic`. Nothing is defined for qemu-virt: its records stay valid.
- Why 352 (ITLinesNumber 10, INTIDs 0 to 351): it covers every SPI of the
  draft grant (330 for A, 331 for HAVEN), as D3 proposed. It does not cover
  the whole SoC: `sky1.dtsi` names SPIs up to 448 (INTID 480, PCIe INTx) and
  `InterruptId.h` INTIDs up to 506 [O]; a grant with those needs 512 IDs
  (ITLinesNumber 15) [I]. The new `dt_check.py` option reports exactly that
  case before boot.
- Memory: `struct hv_vgic` keeps per-vCPU copies of the distributor state
  (`irouter` is 8 bytes per ID). From its layout (LP64): 664 bytes at 64
  IDs, 3,440 bytes at 352, so +2,776 bytes per vCPU; 27,520 bytes for
  partition A's 8 vCPUs [I, computed]. No allocation: static arrays in BSS.
  Read-back offset 40 of `enabled` is unchanged (the arrays follow it).
- Host test: a second build of `vgic.c` with `HAVEN_VGIC_NR_IRQS=352U`
  (`tests/unit/test_vgic_wide.c`, in CTest and `scripts/dev/test.sh`):
  ITLinesNumber 10, an owned INTID 330 enabled through GICD_ISENABLER10
  reaches the hardware hook, its IROUTER, priority and configuration are
  stored, INTID 352 and above read as zero, and `sizeof(struct hv_vgic)` is
  printed. `test_vgic.c` (64 IDs) is unchanged.

Not changed: `readback.py` reads the model's enables for INTIDs 0 to 63
only (`read_vgic_enables`) [F]; for qemu-virt that is the whole model. On
cix-p1 it would miss INTID 330; widening read-back is a later step [P].

### Part 3: dt_check.py

Each addition only acts on input the old tool did not use, so every
existing test and every D3 result for a 3-cell tree without the new grant
field or option stays as it was (expected, rows A below).

| D3 finding | Change | Output (exact text) |
| - | - | - |
| 2, CPU identity | grant format 1 gains an optional `gic.mpidr`: `{"<logical cpu>": "<guest MPIDR>"}`. If present, every cpu node's `reg` (read with the `/cpus` `#address-cells`, compared on MPIDR Aff3..Aff0) must be the MPIDR of one of the partition's CPUs. Absent: no check, as today. `grant.py` carries it through format 2 (`haven,mpidr` per partition, `haven.mpidr` for CPUs of no partition) so the round-trip test keeps it | finding `/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1`, after the CPU-count finding, in node order |
| 3, specifier width | the width is the interrupt parent's `#interrupt-cells`: 3 as before, or 4 (the 4th cell, 0 or a PPI-partition phandle, is not checked). Any other width, a missing `#interrupt-cells`, or an `interrupts` length that is not a multiple of the width: the node's interrupts are not read and a finding is reported (fail closed) | `/timer: interrupt specifiers not readable: the parent's #interrupt-cells is 2 (3 or 4 expected)`; for a length: `...: 13 cells is not a multiple of #interrupt-cells 4`; for none: `...: the parent has no #interrupt-cells` |
| 4, PSCI conduit | information only (Linux profile): `/psci` with a `method` other than `"hvc"`; or no `/psci` while a cpu node has `enable-method = "psci"`. Nothing is printed for `method = "hvc"` | `info /psci: method "smc"; HAVEN serves PSCI to partitions over HVC`; `info /psci: none, but cpu nodes use enable-method "psci"` |
| 1, emulated range | option `--vgic-intids N`: every SPI or PPI the check reads (not the timer entries listed as information) must be below N. Absent: no check | finding `/soc@0/serial@40d0000: SPI 330 beyond the emulated GIC (64 interrupt IDs)`, after that interrupt's grant finding if both apply |

New grant for the runs: `grant-cixp1-v1.json` in this folder, D3's
`grant-cixp1-v0.json` (left unchanged) plus `gic.mpidr` for the 12 logical
CPUs (guest MPIDR = physical, D3 decision): 0 = 0x400, 1 to 4 = 0x000 to
0x300, 5 to 11 = 0x500 to 0xB00.

Unit tests: `test_dt_check.py` gains tests that need no `dtc` (they read
D3's committed `.dtb` files: S2b with an MPIDR map, S5, S6, the clean tree
with `--vgic-intids 64`), so they run in CI, where `dtc` is not installed
and the existing dts-based tests are skipped [F, `ci.yml`]; and dts-based
tests (4 cells, width 2, length mismatch, `/psci` absent or `smc`,
MPIDR mismatch, emulated range) that run where `dtc` exists.

### Part 4: D3's trees again

`run_d4.py` in this folder runs `dt_check.py` on D3's committed trees
(`partition-a.dtb` and `seeds/*.dtb`, not regenerated) in three sets, and
on one new seed, S7: `partition-a.dts` with the GIC's `#interrupt-cells`
changed to `<2>`, compiled with `dtc` (run without `-q`, stderr kept).
Emulated ranges as in D3 in every run: `--emulated 0x0E010000+0x10000
--emulated 0x0E090000+0x100000`, `--partition 1`.

- Set A: D3's command unchanged (grant v0, no new option).
- Set B: grant v1 and `--vgic-intids 352`.
- Row V: the clean tree, grant v1, `--vgic-intids 64` (finding 1).
- Row S7: grant v1, `--vgic-intids 352`.

## Expectations

Baselines are built from `main` at `0569797` (pull request 57) with the same
toolchain before any D4 code: `aarch64-unknown-linux-gnu` GCC 15.2.0 on the
macOS host, `cloc` for `tools/scripts/tcb-image-loc.py`. At that commit the
default qemu-virt image is text 43,140, data 144, bss 5,251,064 bytes, and
its trusted code is 66 files, 7,448 code lines.

### Builds, tests, trusted code

| Row | Check | Expected result |
| - | - | - |
| B1 | qemu-virt, default preset (`qemu-direct`) | builds; `haven.bin` byte-identical to the baseline's (same SHA-256); size 43,140 / 144 / 5,251,064 |
| B2 | qemu-virt, `qemu-psci` profile | builds; `haven.bin` byte-identical to the baseline's |
| B3 | cix-p1, preset `arm64-cix-p1` | builds with no compiler warning (only the two linker warnings the qemu-virt build already prints: RWX LOAD segment, build-id discarded); configure prints `Haven PSCI conduit: HAVEN_PSCI_CONDUIT_SMC`; bss within 64 bytes of B1's |
| B4 | cix-p1 tables in `haven.elf` (objdump of the symbols) | `hv_cpu_mpidr` = 0x400, 0x0, 0x100, 0x200, 0x300, 0x500, 0x600, 0x700, 0x800, 0x900, 0xa00, 0xb00; `hv_cpu_count` = 12 |
| B5 | Linux options (`-DHAVEN_PART_A_DTB_IPA=0x50000000 -DHAVEN_PART_A_VCPUS=2`) on qemu-virt and on cix-p1 | both build; bss of cix-p1 minus bss of qemu-virt between 5,552 (2 x 2,776, the two vCPU models) and 5,552 + 4,095 bytes (padding before a page-aligned object) |
| T1 | host CTest (as CI, `bench_temporal_isolation` excluded) | every test passes; one more test than on `main` (`test_vgic_wide`), which prints `sizeof(struct hv_vgic)` = 3440 |
| T2 | Python tests (`unittest discover -s tools/readback`) | all pass, none skipped on the macOS host (`dtc` present); the format round trip includes `grant-cixp1-v1.json` |
| T3 | CI on each pull request | all jobs green, including the new cix-p1 build step |
| K1 | trusted code, qemu-virt default | 66 files; 7,451 to 7,456 code lines (+3 to +8, `vgic.h` only) |
| K2 | trusted code, cix-p1 | 67 files (qemu-virt's platform.c out; cix-p1's platform.c and memory.h in; partition.c still includes qemu-virt's memory.h); 7,470 to 7,510 code lines |
| K3 | offline tools (not trusted code) | `dt_check.py` 158 to between 190 and 220 code lines; `grant.py` 156 to between 162 and 170 |

### dt_check.py on D3's trees

"As D3" means the same exit status and the same output lines as D3's
`runs/<row>.txt`. Timer information lines are the D3 three (29 entry 0,
30 entry 1, 26 entry 3) unless stated.

| Row | Set | Expected exit | Expected findings | Expected info | D3 verdict, then D4 |
| - | - | - | - | - | - |
| A-C, A-S1, A-S2a, A-S2b, A-S3, A-S3b, A-S4 | A | as D3 | as D3 | as D3 | unchanged (S2b still missed: grant v0 has no MPIDRs) |
| A-S5 | A | 0 | none | the 3 timer lines | misread, then **read correctly** |
| A-S6 | A | 0 | none | 4: the 3 timer lines, `/psci: method "smc"; HAVEN serves PSCI to partitions over HVC` | missed, then **reported as information** (exit stays 0, by design) |
| B-C | B | 0 | none | 3 | passes, unchanged |
| B-S1 | B | 1 | as D3 (registers 0x40e0000+0x1000, SPI 331) | 3 | caught, unchanged |
| B-S2a | B | 1 | 2: `/cpus: 9 CPU nodes, partition 1 has 8 CPU(s)`, `/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1` | 3 | caught, now with the core named |
| B-S2b | B | 1 | `/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1` | 3 | missed, then **caught** |
| B-S3, B-S3b, B-S4 | B | 1 | as D3 (one finding each) | 3 | caught, unchanged |
| B-S5 | B | 0 | none | 3 | misread, then **read correctly** |
| B-S6 | B | 0 | none | 4 (as A-S6) | missed, then **reported as information** |
| V | clean tree, grant v1, `--vgic-intids 64` | 1 | `/soc@0/serial@40d0000: SPI 330 beyond the emulated GIC (64 interrupt IDs)` | 3 | D3 finding 1: passed, then **caught** |
| S7-dtc | `dtc` on the S7 source | 0 | exactly one warning: `interrupts_property` on `/soc@0/serial@40d0000` (12 bytes, not a multiple of 8) | | new seed |
| S7 | grant v1, `--vgic-intids 352` | 1 | 2, in this order: `/timer: interrupt specifiers not readable: the parent's #interrupt-cells is 2 (3 or 4 expected)`, the same for `/soc@0/serial@40d0000` | none | new seed: **caught** (fail closed) |

Summary of the prediction: of D3's ten rows, S2b changes from missed to
caught (with an MPIDR map in the grant), S5 from misread to read
correctly, S6 from silent to an information line; D3's finding 1 becomes a
finding under `--vgic-intids 64`; every other D3 row keeps its result.

Verdict rule (as D3): a row holds if exit status, findings (text, number
and order) and the number of information lines match; build rows hold if
every stated number or identity matches. Anything else is a prediction
error and is reported as such. These expectations are not edited after
the runs; results go in a Results section added by a later pull request.

## Results (2026-11-09, macOS host and CI; no board)

Expectations merged first (haven `a47ebc0`, pull request 58); the code
after that (commit `b99c8ea`, merged as `1ec9b8a`, pull request 59, CI
green on every job). Tools: `aarch64-unknown-linux-gnu` GCC 15.2.0 on the
macOS host (CI: `aarch64-linux-gnu` GCC 15.2.0), DTC 1.6.1, Python 3.9.6,
`cloc`. Build directories outside the repository; commands as in the
Expectations (presets `arm64-qemu`, `arm64-cix-p1`, `host-tests`).
Part 4: `python3 docs/methodology/stage2/20261109-cix-p1-platform/run_d4.py`
from the haven repository; one log per row in `runs/`, the new seed in
`seeds/` (marked `SEEDED FAULT`).

### Builds, tests, trusted code

| Row | Observed | Verdict |
| - | - | - |
| B1 | builds; size 43,140 / 144 / 5,251,064 as predicted; `haven.bin` **not** byte-identical: 3 bytes differ, the build time in `Build: Nov  9 2026 12:21:17` against `12:31:43` (`src/core/init.c` prints `__DATE__ " " __TIME__`). With `SOURCE_DATE_EPOCH=1793318400` for both builds (baseline: D4's changes stashed, same tree), the images are byte-identical (SHA-256 `b8b4ea32...d337` both) | **prediction error** (the expectation overlooked the embedded build time); the property it meant to test holds |
| B2 | same: differs only in the build time; with the fixed epoch byte-identical (`ceffd618...60c9` both). Also checked, not a row: the Linux options build is byte-identical too (`c2308a15...ef`) | **prediction error**, same cause; property holds |
| B3 | builds; no compiler warning; the linker warnings are the same lines as the qemu-virt build's (RWX LOAD segment and build-id discarded, for haven, guest_a and guest_b); `Haven PSCI conduit: HAVEN_PSCI_CONDUIT_SMC`; bss 5,251,064, equal to B1's (text 43,196) | as expected |
| B4 | `hv_cpu_mpidr` (0x60 bytes, read-only data) = 0x400, 0x0, 0x100, 0x200, 0x300, 0x500, 0x600, 0x700, 0x800, 0x900, 0xa00, 0xb00; `hv_cpu_count` = 12 | as expected |
| B5 | qemu-virt bss 5,252,496, cix-p1 bss 5,258,048: difference **5,552** exactly (no padding) | as expected |
| T1 | local CTest 27 of 27 pass (`main`: 26 run), `test_vgic_wide` prints `sizeof(struct hv_vgic) = 3440`; `scripts/dev/test.sh` also passes. CI: 27 of 27 on Linux, macOS and Windows | as expected |
| T2 | local: 53 tests (`main`: 42), all pass, 0 skipped; `test_grant` reads `grant-cixp1-v1.json`. CI: 53 run, 12 skipped (the dts-based tests, no `dtc` there, as designed), the 5 blob-based board tests run | as expected |
| T3 | pull request 59: every job green, including `Cross-compile (cix-p1 draft, no boot)` | as expected |
| K1 | 66 files, 7,455 code lines (+7: `vgic.h` 45 to 52; `vgic.c` unchanged at 310) | as expected |
| K2 | 67 files, 7,479 code lines (main's qemu-virt 7,448, minus qemu-virt `platform.c` 59, plus cix-p1 `platform.c` 61 and `memory.h` 22, plus 7 in `vgic.h`) | as expected |
| K3 | `dt_check.py` 190 (the low end of the range), `grant.py` 167 | as expected |

Note on K1 and K2: `tcb-image-loc.py` takes the file list from a build
but counts the files as they are on disk, so the baseline (7,448) was
counted at `main` before any change.

### dt_check.py on D3's trees

| Row | Observed | Verdict |
| - | - | - |
| A-C, A-S1, A-S2a, A-S2b, A-S3, A-S3b, A-S4 | output equal to D3's logs, line for line (the runner compares them) | as expected (S2b still missed with grant v0) |
| A-S5 | exit 0, no finding, info 29 (entry 0), 30 (entry 1), 26 (entry 3) | as expected: **read correctly** |
| A-S6 | exit 0, no finding, the 3 timer lines and `/psci: method "smc"; HAVEN serves PSCI to partitions over HVC` | as expected: **reported as information** |
| B-C | exit 0, no finding, 3 info | as expected |
| B-S1 | exit 1: registers 0x40e0000+0x1000 and SPI 331 of `/soc@0/serial@40e0000`; 3 info | as expected |
| B-S2a | exit 1: `/cpus: 9 CPU nodes, partition 1 has 8 CPU(s)`, then `/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1`; 3 info | as expected |
| B-S2b | exit 1: `/cpus/cpu@800: MPIDR 0x800 is not a CPU of partition 1`; 3 info | as expected: **caught** |
| B-S3, B-S3b, B-S4 | exit 1, D3's single finding each; 3 info | as expected |
| B-S5 | exit 0, no finding, 3 info | as expected |
| B-S6 | exit 0, no finding, 4 info (the `/psci` line last) | as expected |
| V | exit 1: `/soc@0/serial@40d0000: SPI 330 beyond the emulated GIC (64 interrupt IDs)`; 3 info | as expected: D3 finding 1 **caught** |
| S7-dtc | exit 0, one warning: `Warning (interrupts_property): /soc@0/serial@40d0000:#interrupt-cells: size is (12), expected multiple of 8` | as expected |
| S7 | exit 1: `/timer: interrupt specifiers not readable: the parent's #interrupt-cells is 2 (3 or 4 expected)`, the same for `/soc@0/serial@40d0000`; no info | as expected: **caught** (fail closed) |

**Verdict.** 32 checks: 30 hold as pre-registered, 2 are prediction errors
(B1, B2), both from one cause: the expectation said "byte-identical"
without fixing the build time HAVEN prints at boot. With the time fixed
the qemu-virt images (default, `qemu-psci`, Linux options) are
byte-identical before and after D4, which is what those rows were meant to
show. Of D3's limits, S2b (wrong core, right count) is now caught when the
grant names MPIDRs, S5 (4-cell specifiers) is read correctly, S6 (SMC
conduit) is reported as information, and D3 finding 1 (an interrupt the
emulated GIC cannot deliver) is a finding under `--vgic-intids`.

What this shows [O]: the draft platform builds with every board fact
traced to `data/`; the emulated GIC at 352 IDs takes and gives back INTID
330's enable, routing, priority and configuration in a host test, at a
cost of 2,776 bytes per vCPU; qemu-virt is unchanged.

What this does not show: that HAVEN runs on the board. The cix-p1 image
links at 0x8000_0000, inside the firmware's secure reservation, carries
QEMU's partition tables (partition.c is unchanged), and has not executed
one instruction on a CIX P1. UART3's clock and pins, the RAM window and
the GIC-700's own interrupt range stay assumptions until the board.

### Findings and proposals

1. **Reproducibility of image comparisons** [O, I]: HAVEN's image embeds
   its build time, so two builds of the same source differ. Proposal [P]:
   records that compare images set `SOURCE_DATE_EPOCH` (GCC honours it for
   `__DATE__` and `__TIME__`); CI could set it too.
2. **Next step for the platform** [P]: partition.c takes partition A's
   regions, vGIC IPAs and vCPU count from the platform (the `CIX_PART_A_*`
   constants of `memory.h` are ready and unused), so a cix-p1 build carries
   the D3 grant instead of QEMU's layout.
3. **Read-back** [F]: `readback.py` reads the model's enables for INTIDs 0
   to 63 only; on cix-p1 it would not see INTID 330. Proposal [P]: read
   `HV_VGIC_NR_IRQS / 32` words, taken from the ELF (for example a
   constant symbol), before read-back runs on the board.
4. **vGIC width beyond the first grant** [I]: 352 IDs do not cover PCIe
   INTx (INTID up to 480) or the highest vendor INTID (506); granting
   those needs 512 IDs (+1,280 bytes more per vCPU at 8 bytes of `irouter`
   per ID, plus the bitmaps and priorities), which `dt_check.py
   --vgic-intids` will report first.

### Open questions for the owner

1. How HAVEN is loaded on the board (UEFI application or firmware image)
   and at which address: needed before the cix-p1 image can run (porting
   note, open question 5).
2. Approve the next platform step (finding 2) as D5, or another order.
3. Adopt `SOURCE_DATE_EPOCH` for image comparisons in records and CI
   (finding 1).
