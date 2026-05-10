# Repository Structure

This document defines the expected hierarchy and purpose of each area.

## Top level

- .github/: issue templates and CI workflows.
- src/: hypervisor implementation.
- include/: shared interfaces and headers.
- configs/: static partition and platform configurations.
- drivers/: Linux and guest integration components.
- tools/: code generation and analysis utilities.
- tests/: unit, integration, and isolation test suites.
- docs/: architecture, safety, methodology, and roadmap documents.
- verification/: formal-method placeholders and artifacts.
- scripts/: reproducible build/test/style/release commands.

## Source hierarchy

- src/core/arch: architecture-specific EL2 logic.
- src/core/mm: stage-2 mapping and memory isolation.
- src/core/sched: temporal partitioning and budget control.
- src/core/iommu: DMA isolation policy hooks.
- src/core/irq: interrupt partitioning and routing logic.
- src/core/time: timers and accounting for deterministic execution.
- src/platform: SoC and board-specific bring-up.
- src/common: shared low-level utility code.

## Test hierarchy

- tests/unit: isolated module tests.
- tests/integration: multi-partition system behavior tests.
- tests/isolation: adversarial spatial and temporal isolation checks.
- tests/demos: reproducible demo scenarios.
- tests/selftests: runtime consistency checks.

## Documentation hierarchy

- docs/architecture: system model and technical design.
- docs/safety: threat model, assumptions, and claims boundaries.
- docs/methodology: benchmark and evaluation protocols.
- docs/porting: platform adaptation instructions.
- docs/api: control and interface references.
- docs/roadmap: staged project milestones.

## Governance and process

- CONTRIBUTING.md: contribution and review process.
- SECURITY.md: vulnerability reporting and disclosure window.
- MAINTAINERS: ownership and review areas.
- CODING_STYLE.md: coding and determinism-oriented constraints.
- CHANGELOG.md: release notes and evolution history.
