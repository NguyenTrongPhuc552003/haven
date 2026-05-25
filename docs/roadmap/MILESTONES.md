# Milestones

M1: Repository and architecture baseline ✅
- Deliver full structure, governance, and initial docs.
- **Status:** Complete — repo structure, CMake build system, CI workflows, architecture docs.

M2: Minimal boot and partition bring-up ✅
- Boot hypervisor and launch Linux plus RTOS domains.
- **Status:** Complete — EL2 boot, two-partition static config, PSCI CPU_ON, ERET transitions.

M3: Isolation enforcement ✅
- Implement and validate memory/device separation.
- **Status:** Complete — stage-2 page tables, GICv3 IRQ routing, SMMUv3 DMA policy,
  QEMU demo with deliberate cross-partition fault injection (F1–F8 matrix, 8/8 pass).

M4: Temporal guarantees 🔄 In Progress
- Implement budget controls and measure latency bounds.
- **Status:** Budget scheduler and EL2 timer implemented. Benchmark baselines captured
  (`build/benchmarks/`). Formal measurement documentation in progress.

M5: Thesis evidence package 📋 Planned
- Consolidate results, threats, limitations, and reproducibility kit.
- **Status:** Not started. Depends on M4 measurement documentation completion.

Traceability reference:
- docs/methodology/CHAPTER_TRACEABILITY.md maps thesis chapters to deliverables.
