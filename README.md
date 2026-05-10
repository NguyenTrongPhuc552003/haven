# haven

Hypervisor for Asymmetric Virtualization ENforcement and isolation.

Haven is a static partition hypervisor research repository for heterogeneous SoCs,
focused on enforcing spatial and temporal isolation between Linux and RTOS domains.

## Thesis Context

Title:
Static Partition Hypervisor for Asymmetric Multiprocessing: Enforcing Spatial and Temporal Isolation Between Linux and RTOS on a Heterogeneous SoC

Core claims:
- Spatial isolation: a partition cannot access memory/devices assigned to another.
- Temporal isolation: overload in one partition cannot violate bounded latency in another.

## Scope

In scope:
- Static partition definition at build/deploy time.
- Stage-2 memory isolation policy.
- Device and interrupt partitioning policy.
- CPU budget and deterministic scheduling for mixed criticality.
- Linux plus RTOS coexistence on heterogeneous SoCs.

Out of scope:
- Dynamic VM creation or migration.
- Cloud orchestration features.
- Overcommit-based scheduling models.

## Repository Map

- src/: hypervisor core and platform code.
- include/: interface and architecture headers.
- configs/: static partition configurations per target.
- drivers/: Linux and guest-side integration helpers.
- tests/: unit, integration, and isolation validation.
- docs/: architecture, safety, methodology, and roadmap.
- verification/: placeholders for formal artifacts.
- scripts/: build, test, style, and release helpers.

Key baseline artifacts:
- docs/methodology/CHAPTER_TRACEABILITY.md
- docs/methodology/AI_WORKFLOW_PLAYBOOK.md
- docs/methodology/CI_STRATEGY.md
- docs/methodology/BRANCH_PROTECTION_POLICY.md
- docs/methodology/GITHUB_PROTECTION_SETUP.md
- docs/methodology/CI_TROUBLESHOOTING.md
- docs/methodology/IMX95_EVIDENCE_TEMPLATE.md
- docs/architecture/THESIS_DEEP_DIVE.md
- docs/architecture/NAMING_RATIONALE.md
- docs/porting/IMX95_VALIDATION_RUNBOOK.md
- include/haven/stage2.h
- include/haven/irq_ownership.h
- include/haven/budget_sched.h
- configs/arm64/qemu-virt.yaml
- configs/arm64/imx8qm-mek.yaml

## Quick Start

Prerequisites:
- clang or gcc toolchain
- make
- qemu-system-aarch64 (recommended for initial validation)

Bootstrap:
- ./scripts/build.sh
- ./scripts/test.sh

## Evaluation Plan

Primary metrics:
- Memory isolation violation attempts blocked.
- Interrupt routing correctness.
- Worst-case latency/jitter under Linux stress.
- RTOS deadline miss rate under interference.

## Governance

Please read:
- CONTRIBUTING.md
- SECURITY.md
- CODE_OF_CONDUCT.md
- docs/safety/THREAT_MODEL.md
- .github/copilot-instructions.md
- AGENTS.md

Evidence packaging:
- make evidence
- .github/workflows/evidence-pack.yml
