# AI Workflow Playbook

This playbook defines practical AI execution techniques for a 12+ month thesis roadmap.

Project target:
- Primary: NXP i.MX95 Dev Kit.
- Secondary: QEMU arm64 and additional validated platforms.

## Core techniques

1. Specialized agents by task class
- Isolation Guardian for memory/interrupt/scheduler safety.
- Platform Bring-up for board and firmware enablement.
- Thesis Evidence workflow for chapter and milestone traceability.

2. Deterministic hooks for quality gates
- pre-commit: style and config validation.
- commit-msg: commit message policy.
- pre-push: build and test verification.

3. Evidence-first development loop
- Implement minimal scoped change.
- Add or update tests.
- Update traceability and methodology documents.
- Record residual risk and next milestone gate.

## Annual execution cadence

Phase A (Months 1-3): Foundation
- Stabilize repository contracts, hooks, CI, and baseline interfaces.
- Lock i.MX95 bring-up prerequisites and assumptions.

Phase B (Months 4-7): Isolation implementation
- Stage-2, interrupt ownership, and budget scheduler maturation.
- Build adversarial isolation tests and run repeatable benchmarks.

Phase C (Months 8-10): Platform validation
- Execute i.MX95 validation loops and secondary platform checks.
- Refine performance bounds and failure behavior documentation.

Phase D (Months 11-12+): Thesis evidence packaging
- Freeze milestone evidence.
- Consolidate chapter traceability and final evaluation assets.
- Produce reproducibility appendix and handoff artifacts.

## Definition of done per milestone

1. Code is merged with tests.
2. Build/test/config checks pass locally and in CI.
3. Traceability entries are updated.
4. Safety assumptions and non-goals remain explicit.
