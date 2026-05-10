# Haven Copilot Instructions

Project context:
- Thesis: Static Partition Hypervisor for Asymmetric Multiprocessing.
- Primary board: NXP i.MX95 Dev Kit.
- Secondary validation: QEMU arm64 and other supported heterogeneous platforms.

Execution priorities:
1. Protect spatial and temporal isolation invariants first.
2. Prefer deterministic behavior over feature breadth.
3. Keep trusted computing base small and auditable.
4. Tie implementation claims to measurable evidence.

Engineering expectations:
1. Keep changes minimal, scoped, and reversible.
2. Add tests for all behavior changes in core isolation code.
3. Update docs when assumptions, interfaces, or timing models change.
4. Use static analysis and syntax checks before pushing.

Isolation review checklist:
1. Does memory mapping logic preserve partition boundaries?
2. Are interrupt routes strictly owned and non-leaking?
3. Are scheduling budgets validated and bounded?
4. Are failure modes explicit and fail-closed?

Platform strategy:
1. i.MX95 is the source-of-truth target for thesis milestones.
2. QEMU arm64 is required for reproducible CI gating.
3. Additional boards may be added only with explicit config and tests.

Evidence strategy:
1. Every milestone should map to chapter traceability artifacts.
2. Performance claims require measured data and documented setup.
3. Security/safety claims must state assumptions and non-goals.
