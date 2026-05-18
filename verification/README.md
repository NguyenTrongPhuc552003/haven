# Haven Formal Verification

## Overview

Haven's assurance argument rests on three layers:

1. **Policy proofs** - machine-checked theorems in Coq and Isabelle/HOL that
   cover Haven's isolation invariants at the portable C model level.
2. **Hardware binding tests** - unit, integration, and negative-path test suites
   that validate the ARM64 arch layer and drivers against the policy contracts.
3. **Benchmark evidence** - latency measurements establishing that the isolation
   mechanisms meet the thesis acceptance thresholds (stage-2 check < 100 µs,
   RTOS deadline miss < 0.1%).

The formal proofs do *not* include the hardware binding; this follows the seL4
model where a verified abstract specification is connected to hardware through a
separate (tested but unverified) hardware abstraction layer.

---

## Subdirectories

### `coq/`

Three Coq 8.18 proof files:

| File                | Theorem                                                     |
| ------------------- | ----------------------------------------------------------- |
| `IsolationModel.v`  | `spatial_isolation` - no two distinct partitions share a PA |
| `Stage2Policy.v`    | `add_partition_preserves_isolation` - map operation is safe |
| `BudgetScheduler.v` | `consume_preserves_valid` - budget ≤ period invariant       |

See `coq/README.md` for build instructions and correspondence with C source.

### `isabelle/`

One Isabelle/HOL 2023 theory file `HavenIsolation.thy` that independently
cross-validates the Coq isolation model in 10 sections.  Key theorems:
`spatial_no_overlap`, `isolation_no_shared_pa`, `consume_budget_preserves_valid`.

See `isabelle/README.md` for build instructions.

---

## TCB Story

The trusted computing base for Haven's formal assurance comprises:

- The **Coq 8.18 proof kernel** (a small, widely-audited term checker).
- The **Isabelle/HOL 2023 kernel** (independent second checker).
- The **portable policy contracts** in `src/core/` that are modelled by both
  provers.

Hardware-specific code (`arch/arm64/`, `drivers/`) sits *outside* the formal
TCB and is validated by the test suite.

---

## Toolchain Requirements

| Tool         | Version | Install                       |
| ------------ | ------- | ----------------------------- |
| Coq          | 8.18+   | `opam install coq.8.18.0`     |
| Isabelle/HOL | 2023    | <https://isabelle.in.tum.de/> |
| Python 3     | 3.10+   | For `tools/analysis/` scripts |

---

## CI Integration

The Coq proofs are checked in CI via `.github/workflows/ci.yml`.  Isabelle
builds are run on demand (the full Isabelle image is large) and results are
attached as workflow artefacts.
