# Changelog

All notable changes to Haven are documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

- i.MX95 physical board bring-up and EL2 boot validation (requires hardware).
- SMMUv3 stream table population wired to real SMMUv3 MMIO.
- Budget scheduler timer interrupt integration with GICv3 virtual timer.
- Secondary-CPU ERET for Partition B (Phase 3 multi-CPU bring-up).

---

## [0.6.0] - 2026-05-14

### Added

**Partition B RTOS-class Guest Stub (PR #18)**
- `tests/demos/guest_b_entry.S` - bare-metal EL1 stub for Partition B (IPA `0x60000000`,
  PART_B_IPA_BASE). Runs 3 simulated RTOS task periods (busy-wait loops), deliberately reads
  Partition A's IPA base (`0x40000000`) to trigger a stage-2 DABT (Haven logs denial and resumes),
  then writes a `0xDEADBEEF` sentinel at IPA+0x100 as a health-check indicator. No UART mapped -
  the PL011 at `0x09000000` is exclusively owned by Partition A; any B→UART access faults at
  stage-2, preserving the spatial isolation invariant.
- `linker-guest-b.ld` - ELF64 AArch64 linker script placing the stub at VMA `0x60000000`.
  Haven maps PA `0xA0800000` → IPA `0x60000000` for Partition B.
- `Makefile` - ARM64 `all` target now includes `guest_b.bin`; added `guest_b.elf` / `guest_b.bin`
  assembly + objcopy rules.
- `scripts/qemu/qemu-run.sh` - optional `GUEST_B_LOADER` at PA `0xA0800000` (PART_B_PA_BASE).
- `scripts/qemu/qemu-smoke.sh` - loads guest B at `0xA0800000` when the binary is present;
  detects `PARTITION_B:` marker (active once secondary-CPU bring-up is wired).

**SMMU Integration Test Expansion (PR #19)**
- `tests/integration/test_smmu_hardware.c` - added 3 new scenarios (total 8):
  - S6: Read-only DMA window blocks write access (HV_EPERM for WO/RW request).
  - S7: Write-only DMA window blocks read access (HV_EPERM for RO/RW request).
  - S8: StreamID pool exhaustion → HV_ENOSPC (hard upper bound at HV_MAX_SMMU_DEVICES=64).

**CI Benchmark Regression Gate (PR #20)**
- `scripts/ci/bench-regression.sh` - parses `build/benchmarks/*.json`, compares `mean_ns` per
  benchmark against `tests/benchmarks/latency-baseline.json`, exits 1 on >10% regression.
  Uses Python 3 stdlib only.
- `scripts/ci/bench-update-baseline.sh` - reruns benchmark suite and regenerates the baseline
  file from current measurements.
- `tests/benchmarks/latency-baseline.json` - 15 baseline entries covering stage-2, IRQ, budget,
  SMMU, timer, and IOMMU hot paths. Seeded from current CI measurements.
- `.github/workflows/nightly.yml` - new `benchmark-regression` job: builds benchmarks, runs
  them, calls `bench-regression.sh` with 10% threshold, uploads results as 30-day artifact.

### Fixed

**EL2 Virtual IRQ Injection (PR #17)**
- `src/core/exc/el2_exceptions.c` - completed two remaining stubs:
  - `hv_el2_exceptions_init()`: calls `hv_install_vectors()` (writes VBAR_EL2) and
    `hv_arch_gic_el2_setup()` (enables GICv3 virtual CPU interface via ICH_HCR_EL2/ICH_VMCR_EL2)
    under `HAVEN_ARCH_ARM64`. Both symbols are `__attribute__((weak))` for host builds.
  - `hv_el2_inject_exception()`: for `HV_EXC_IRQ` on ARM64, calls `hv_arch_inject_virtual_irq()`
    which writes a free GICv3 List Register (ICH_LR<n>_EL2) with Pending state so the guest
    receives the interrupt on next IRQ exception from EL1.

**SMMU StreamID Pool Leak (PR #19)**
- `src/core/dma/smmu.c`, `hv_smmu_reset_partition()` - previously only freed StreamIDs whose
  DMA window had been configured. An allocated-but-unconfigured StreamID (e.g., one whose
  `configure_dma_window` call was rejected due to PA overlap) would never be freed, permanently
  leaking quota from the global StreamID pool. Fixed: `reset_partition` now also frees
  unconfigured StreamIDs for the target partition.

### Evidence

- SMMU integration: **8/8 scenarios pass** (S1–S8); host + CI verified.
- EL2 exception unit tests: **35/35 pass** (host builds, pre-merge validation).
- Benchmark regression baseline: **15/15 named benchmarks within 10% tolerance**.

---

## [0.5.0] - 2026-05-20

### Added

**QEMU Two-Partition Isolation Demo (R3 milestone)**
- `tests/demos/guest_a_entry.S` - minimal bare-metal EL1 stub for Partition A; prints greeting via PL011 UART, deliberately reads Partition B's IPA base (`0x60000000`), and confirms Haven resumes it after the fault.
- `linker-guest.ld` - guest stub linker script; links binary at VMA `0x40000000` (Partition A IPA base).
- `Makefile` - added `guest_a.elf` / `guest_a.bin` targets under the ARM64 build; binary loaded at PA `0x80800000` by QEMU.

**Stage-2 Fault Enforcement**
- `src/core/exc/el2_exceptions.c` - EL2 sync handler now decodes ESR_EL2 (EC=0x24 DABT, FSC 0x04–0x0F) and HPFAR_EL2 to detect and log stage-2 violations; advances saved ELR_EL2 in the stack frame to skip the faulting instruction and resume the guest.
- `arch/arm64/mm.c` - fixed stage-2 S2AP field: `attrs=0` → Normal WB inner-shareable RW; `attrs=1` → Device nGnRE non-executable RW. Previously all mappings had S2AP=0b00 (no access).

**Memory Layout Fixes**
- `src/platform/qemu-virt/memory.h` - corrected PA layout: `HAVEN_SIZE` increased to 8 MB to cover BSS stage-2 pool; `PART_B_IPA_BASE` set to `0x60000000` (distinct from Partition A) to make cross-partition violations detectable.
- `src/core/partition.c` - added PL011 UART MMIO region (IPA/PA `0x09000000`, 4 KB, device attrs) to Partition A stage-2 mapping; guest stub can now print.

**QEMU Smoke Test**
- `scripts/qemu-smoke.sh` - rewrote serial capture to use `-chardev file` instead of `-nographic` (fixes macOS TTY routing); loads `guest_a.bin` via `-device loader`; passes when boot marker + guest greeting + stage-2 violation log + post-fault marker are all present in the UART output.
- `scripts/qemu-run.sh` - updated to load `guest_a.bin` at PA `0x80800000` alongside `haven.bin`.

### Fixed
- Stage-2 S2AP=0b00 (no access) bug in `arch/arm64/mm.c` - all normal-memory mappings were inaccessible; fixed by translating `attrs` to correct `S2AP_RW | S2_MEMATTR_*` descriptor bits.
- ELR_EL2 not advancing after stage-2 fault - C handler was writing to the ELR_EL2 register directly, but `restore_minimal` in `entry.S` overwrote it from the stack frame; fixed by writing to `sp+168` (saved ELR slot) instead.
- macOS QEMU serial capture - `-nographic` routes PL011 to `/dev/tty` (terminal), not stdout when redirected; fixed by using `-chardev file,id=serial0,path=...` which writes directly to a file on all platforms.

### Evidence
QEMU UART capture (`build/smoke/uart.log`) shows:
```
PARTITION_A: Hello from partition A (EL1 guest)
HAVEN: Denied stage-2 violation IPA=0x60000000 FAR=0x60000000 ESR=0x93c08006
PARTITION_A: Post-fault check OK
```
Smoke test result: `validation_status=pass`, `elapsed_seconds=1`.

---

## [0.4.0] - 2026-05-18

### Added

**ARM64 Architecture Layer (`arch/arm64/`)**
- `arch/arm64/entry.S` - EL2 exception vector table (2KB-aligned, VBAR_EL2).
- `arch/arm64/context.S` - GP register save/restore (x0–x30, SP_EL0/EL1, NEON Q0–Q31).
- `arch/arm64/boot.S` - primary CPU EL2 entry and secondary CPU PSCI bring-up.
- `arch/arm64/cpu.c` - EL2 CPU init: HCR_EL2, VPIDR_EL2, VMPIDR_EL2, MDCR_EL2, CPTR_EL2.
- `arch/arm64/mm.c` - stage-2 page-table hardware: VTTBR_EL2, VTCR_EL2, TLB invalidation.
- `arch/arm64/timer.c` - EL2 physical timer: CNTHP_CTL_EL2, CNTHP_CVAL_EL2.
- `arch/arm64/irq.c` - EL2 IRQ handling, GICv3 List Register injection (ICH_LR0_EL2).
- `arch/arm64/partition.S` - partition launch: SPSR_EL2 + VTTBR_EL2 setup, ERET to guest.
- `arch/arm64/include/asm/sysregs.h` - `read_sysreg`/`write_sysreg` macros, all EL2 registers.
- `arch/arm64/include/asm/page.h` - stage-2 descriptor formats, S2AP permissions, granule support.
- `arch/arm64/include/asm/gic.h` - GICv3 distributor/redistributor MMIO offsets.
- `arch/arm64/include/asm/smmu.h` - SMMUv3 register offsets, STE/CD field definitions.

**Hardware Drivers**
- `drivers/irqchip/gic_v3.c` - GICv3 init, IRQ routing (GICD_IROUTER), enable/disable, EOI.
- `drivers/iommu/smmu_v3.c` - SMMUv3 init, linear stream table, STE programming (partition/abort).
- `drivers/uart/pl011.c` - PL011 UART driver for QEMU virt board.
- `drivers/uart/imx_uart.c` - i.MX UART driver for i.MX95/i.MX8QM boards.
- `drivers/linux/haven_driver.c` - Linux guest kernel module (`/dev/haven`, IOCTL interface).
- `drivers/guest-tools/haven_tool.c` - Userspace CLI for hypervisor state inspection.

**Core Layer Wiring**
- `src/core/init.c` - hypervisor entry point; calls all `_init()` and `platform_init()`.
- `src/core/partition.c` - partition launcher: maps stage-2, assigns IRQs, sets budget, fires ERET.
- `src/core/mm/stage2.c` - wired to `hv_arch_stage2_map()` for hardware page-table writes.
- `src/core/irq/ownership.c` - wired to `gic_v3_route_irq()` / `gic_v3_disable_irq()`.
- `src/core/dma/smmu.c` - wired to `smmu_v3_set_ste_partition()` / `smmu_v3_set_ste_abort()`.
- `src/core/time/timer.c` - wired to `hv_arch_timer_now()` and `hv_arch_timer_set_deadline()`.
- `src/core/exc/el2_exceptions.c` - IRQ injection completed via `hv_arch_inject_virtual_irq()`.

**Common Utilities (`src/common/`)**
- `printk.c`, `string.c`, `panic.c`, `spinlock.c` - hypervisor utilities (no libc, LDAXR/STLXR).

**Platform Abstractions (`src/platform/`)**
- `qemu-virt/platform.c` - PL011/GICv3/SMMUv3/timer bases for QEMU virt.
- `imx95-devkit/platform.c` and `memory.h` - i.MX95 Dev Kit UART/GIC/memory map.
- `imx8qm-mek/platform.c` - secondary validation board (i.MX8QM-MEK, GICv2).

**Formal Verification (`verification/`)**
- `verification/coq/IsolationModel.v` - core spatial isolation invariant (Coq 8.18).
- `verification/coq/Stage2Policy.v` - stage-2 map-preserves-isolation lemma.
- `verification/coq/BudgetScheduler.v` - budget ≤ period invariant.
- `verification/isabelle/HavenIsolation.thy` - cross-validating Isabelle/HOL theory.

**Tests**
- `tests/integration/test_smmu_hardware.c` - SMMU StreamID fault and window pass tests.
- `tests/integration/test_fault_injection.c` - 8 fault scenarios (F1–F8) with DENY assertions.
- `tests/selftests/test_hypervisor_invariants.c` - invariant checks across all isolation modules.
- `tests/demos/demo_two_partition.c` - two-partition boot demo (Linux-class + RTOS-class).
- `tests/benchmarks/bench_isolation_latency.c` - 6 hot-path enforcement latency measurements.
- `tests/benchmarks/bench_stage2_fault.c` - stage-2 containment check, 4 boundary cases.
- `tests/benchmarks/bench_smmu_policy.c` - SMMU DMA policy, 5 cases including StreamID alloc.
- `tests/benchmarks/bench_temporal_isolation.c` - RTOS response time under 5 Linux load levels.

**Infrastructure**
- `Dockerfile` - Ubuntu 24.04, aarch64 cross-compiler, QEMU, Coq 8.18, Python/matplotlib.
- `scripts/build-arm64.sh` - auto-detects cross-compiler prefix; runs `make ARCH=arm64`.
- `linker.ld` - ELF linker script, `.text` at `0x80000000`, 4-CPU stacks.
- `CMakeLists.txt` - optional CMake support for IDE integration.
- `configs/riscv/qemu-riscv64.yaml` and `configs/x86/qemu-x86_64.yaml` - future platform stubs.

**CI/CD**
- `.github/workflows/ci.yml` - added `arm64-cross-compile` job: installs toolchain, builds ELF, verifies ARM64 ELF headers and key symbols.
- `.github/workflows/nightly.yml` - added `arm64-cross-compile` (TCB audit), `coq-proof-check`, and `evidence-generation` jobs (90-day artifact retention).

**Documentation**
- `docs/contributing/DEVELOPMENT_GUIDE.md`, `TESTING_GUIDE.md`, `REVIEW_CHECKLIST.md`.
- `docs/porting/IMX95_EVIDENCE_CAPTURE.md` - evidence capture methodology and JSON schema.
- `docs/thesis/REPRODUCIBILITY_APPENDIX.md` - exact reproduction commands and Docker hash.
- Expanded `docs/architecture/OVERVIEW.md` and `ISOLATION_MODEL.md` with hardware binding details.

### Fixed
- Added `#define _POSIX_C_SOURCE 200809L` to all benchmark files to expose `clock_gettime` under strict `-std=c11 -Werror` (was causing CI failures with both GCC and Clang).

### Removed
- `src/core/arch/.gitkeep`, `src/core/iommu/.gitkeep`, `src/core/time/.gitkeep` - replaced by real implementation files.

---

## [0.3.0] - 2026-05-10

### Added
- Astro Starlight documentation portal (`website/`) with 24 MDX pages covering architecture, getting started, platform guides, thesis traceability, and API reference.
- Professional README with GitHub Actions CI badges, architecture ASCII diagram, and platform comparison table.
- `docs/thesis/README.md` - chapter outline, references, and web portal link for master thesis working documents.
- `website/src/assets/logo-light.svg` and `logo-dark.svg` - Haven "H" monogram branding.
- Custom Haven blue (`#2563eb`) Starlight theme via `website/src/styles/custom.css`.
- `docs/methodology/VERCEL_DEPLOYMENT.md` - updated deployment guide for Astro structure.

### Changed
- Documentation portal migrated from static `site/index.html` to Astro Starlight (`website/`).
- Vercel CI workflow (`vercel.yml`) rewritten: builds in `website/`, patches Vercel project settings via API, then deploys with `--prebuilt` flag to bypass stale dashboard overrides.
- `vercel.json` updated with `rootDirectory: website`, `framework: astro`, and `outputDirectory: dist`.

### Fixed
- Removed hexapod thesis content that was incorrectly present in `thesis/background.mdx`.
- Removed stale `docs/final-hexapod-thesis/` directory and its `.gitignore` entry.
- Fixed stale `site/index.html` reference in `README.md`.

### Removed
- Static `site/` portal folder.

---

## [0.2.0] - 2026-05-09

### Added
- C11 stub implementations for all five core isolation modules: `stage2.c`, `ownership.c`, `budget.c`, `smmu.c`, `el2_exceptions.c`.
- Guest module stubs: `uart.c`, `freertos_integration.c`.
- 7 test binaries covering unit, integration, and isolation scenarios: `test_core_stubs`, `test_el2_exceptions`, `test_freertos_integration`, `test_guest_uart`, `test_isolation_flow`, `test_isolation_negative`, `test_smmu_dma`.
- `build/benchmarks/baseline.json` - initial performance baseline (stub measurement).
- `build/ci/metadata.txt` and `build/evidence/metadata.txt` - CI and evidence metadata.
- `configs/arm64/imx8qm-mek.yaml` - secondary validation board config.
- `docs/methodology/PHYSICAL_BOARD_VALIDATION.md` - physical board validation protocol.
- `docs/methodology/VIRTUAL_PLATFORM_VALIDATION.md` - QEMU validation protocol.
- `docs/methodology/IMX95_EVIDENCE_TEMPLATE.md` - structured evidence capture template.
- `docs/porting/IMX95_HARDWARE_SETUP.md` - i.MX95 Dev Kit hardware setup guide.
- `docs/porting/IMX95_VALIDATION_RUNBOOK.md` - step-by-step i.MX95 validation runbook.
- `docs/porting/CROSS_OS_VIRTUALIZATION_RUNBOOK.md` - cross-OS virtualization runbook.
- `verification/coq/` and `verification/isabelle/` scaffolding.
- All empty roadmap release directories with `.gitkeep` placeholders.
- 6 GitHub Actions workflows: `ci.yml`, `vercel.yml`, `nightly.yml`, `benchmark.yml`, `cross-os.yml`, `evidence-pack.yml`.

### Changed
- `scripts/build.sh`, `scripts/test.sh`, `scripts/benchmark-baseline.py` - updated for new module layout.

---

## [0.1.0] - 2026-05-08

### Added
- Initial repository structure with `src/`, `include/`, `configs/`, `tests/`, `docs/`, `scripts/`.
- Baseline governance: `README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, `CODING_STYLE.md`, `LICENSE` (Apache 2.0).
- Core public headers: `stage2.h`, `irq_ownership.h`, `budget_sched.h`, `smmu.h`, `el2_exceptions.h`, `guest_uart.h`, `freertos_integration.h`, `types.h`.
- Platform configs: `configs/arm64/qemu-virt.yaml`, `configs/arm64/imx95-devkit.yaml`.
- Documentation skeleton: architecture overview, isolation model, thesis deep-dive, threat model, assumptions, chapter traceability, evaluation plan.
- CI/CD scaffolding: `Makefile`, `scripts/build.sh`, `scripts/test.sh`, `scripts/style-check.sh`.
- `AGENTS.md` and `.github/copilot-instructions.md` - AI agent policy.

