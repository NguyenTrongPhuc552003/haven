# Haven Coq Formal Verification

## Overview

This directory contains three Coq 8.18 proof files that formalise Haven's
isolation policy invariants at the *policy layer* — the portable C contracts
that hold regardless of the underlying hardware implementation.

The proof strategy mirrors the seL4 approach: verify the abstract policy model
in a theorem prover, then validate the hardware binding (arch/arm64/,
drivers/) through testing.  Together, these two artefacts give a strong
end-to-end assurance argument for the thesis.

---

## Files

| File                  | Content                                                               |
|-----------------------|-----------------------------------------------------------------------|
| `IsolationModel.v`    | Core spatial isolation model.  Defines `PartitionState`, `HvState`,  |
|                       | `spatial_isolation_invariant`, and proves the primary                 |
|                       | `spatial_isolation` theorem: if `pa` belongs to partition p1 then it |
|                       | cannot belong to any distinct partition p2.                           |
| `Stage2Policy.v`      | Stage-2 map operation.  Proves that `hv_stage2_map_partition` —       |
|                       | modelled as `add_partition` — preserves `spatial_isolation_invariant`.|
| `BudgetScheduler.v`   | Temporal budget model.  Defines `BudgetState`, proves the             |
|                       | `budget_leq_period` invariant, and shows that the `consume` operation |
|                       | preserves it.                                                         |

---

## Key Theorems

```
(* IsolationModel.v *)
Theorem spatial_isolation :
  forall (s : HvState) (p1 p2 : PartitionId) (pa : PhysAddr),
    spatial_isolation_invariant s ->
    p1 <> p2 ->
    partition_contains_pa s p1 pa ->
    ~partition_contains_pa s p2 pa.

(* Stage2Policy.v — invariant preservation *)
Theorem add_partition_preserves_isolation :
  forall s p,
    spatial_isolation_invariant s ->
    partition_disjoint_from_all p s ->
    spatial_isolation_invariant (add_partition s p).

(* BudgetScheduler.v *)
Theorem budget_leq_period :
  forall bs : BudgetState,
    valid_budget bs ->
    bs.(budget) <= bs.(period).

Theorem consume_preserves_valid :
  forall (bs : BudgetState) (ticks : nat),
    valid_budget bs ->
    valid_budget (consume bs ticks).
```

---

## Admitted Sub-lemmas

A small number of sub-lemmas are currently `Admitted` — these are noted with
`(* DEFERRED *)` comments in the source.  They cover arithmetic edge cases in
the stage-2 overlap check that are mechanically verified in the C
implementation via unit tests (`tests/unit/test_core_stubs.c`).  Closing these
admissions is tracked as a Phase 2 proof obligation.

---

## Running the Proofs

**Install Coq 8.18 via opam:**

```bash
opam install coq.8.18.0
```

**Check all three files:**

```bash
coqc IsolationModel.v Stage2Policy.v BudgetScheduler.v
```

Expected output: no errors printed to stderr.  The `coqc` process exits with
code 0.

Alternatively, open the files interactively in CoqIDE or the VS Code
`vscoq` extension for step-by-step proof exploration.

---

## Correspondence with C Source

| Coq definition / theorem            | C source                                    |
|--------------------------------------|---------------------------------------------|
| `spatial_isolation_invariant`        | `hv_stage2_map_partition` (stage2.c:121–133)|
| `regions_disjoint`                   | `hv_range_overlaps` (stage2.c)              |
| `partition_contains_pa`              | `hv_stage2_contains` (stage2.c)             |
| `BudgetState`, `consume`             | `hv_budget_tick` (budget.c)                 |
| `budget_leq_period`                  | assertion in `hv_sched_init` (budget.c)     |

---

## TCB Story

These proofs cover the *policy* layer only.  The trusted computing base (TCB)
for the Haven thesis comprises:

1. **Policy proofs** (this directory) — machine-checked by Coq 8.18.
2. **Hardware binding** (`arch/arm64/`, `drivers/`) — validated by unit,
   integration, and negative-path tests.
3. **Coq kernel itself** — a small, widely-audited proof checker.

This matches the seL4 assurance model: a verified abstract specification plus
a tested (but unverified) hardware abstraction layer.

See also: `verification/isabelle/` for cross-validation of the isolation model
in Isabelle/HOL 2023.
