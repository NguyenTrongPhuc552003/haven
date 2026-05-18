# Release 2 (Month 4-6)

**Status: COMPLETE** — merged 2026-05-18 via PR #5

Theme:
- Isolation mechanisms maturity and hardware binding layer.

## Deliverables

1. ARM64 architecture layer (`arch/arm64/`): exception vectors, context save/restore, stage-2 page tables, timer, IRQ. ✓
2. Hardware drivers: GICv3 (`drivers/irqchip/`), SMMUv3 (`drivers/iommu/`), UART (`drivers/uart/`). ✓
3. Core layer wired to hardware: `stage2.c`, `ownership.c`, `smmu.c`, `timer.c`, `el2_exceptions.c`. ✓
4. Formal Coq proofs: `IsolationModel.v`, `Stage2Policy.v`, `BudgetScheduler.v`. ✓
5. Common utilities: `printk.c`, `string.c`, `panic.c`, `spinlock.c`. ✓
6. Platform abstractions: QEMU virt, i.MX95 Dev Kit, i.MX8QM-MEK. ✓
7. Full benchmark suite: 4 benchmark programs with JSON output. ✓
8. Linux kernel module (`drivers/linux/`) and userspace CLI (`drivers/guest-tools/`). ✓
9. ARM64 cross-compile CI gate + nightly Coq proof verification. ✓

## Mandatory checks

1. Existing CI gates remain green. ✓
2. ARM64 binary (`build/haven.elf`) produced by CI. ✓
3. Coq proofs check under Coq 8.18 nightly. ✓
4. Benchmark baselines within acceptable regression bounds. ✓

## Exit criteria

1. Spatial and temporal control paths have positive and negative test coverage. ✓
2. ARM64 hardware binding layer compiles and links without errors. ✓
3. Formal proofs committed and verified by CI. ✓
4. TCB size (`src/core/`) stays within 5 KLOC target. ✓
