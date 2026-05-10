# Changelog

All notable changes to Haven are documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

- i.MX95 hardware bring-up and EL2 boot validation.
- SMMUv3 stream table population from static config.
- Budget scheduler integration with GICv3 virtual timer.

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

