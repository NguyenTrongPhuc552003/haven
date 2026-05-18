# Haven Isabelle/HOL Verification

## Purpose

This directory contains an Isabelle/HOL 2023 formalisation of Haven's spatial
isolation model, serving as an independent cross-validation of the Coq proofs
in `verification/coq/`.  Proving the same theorems in two different proof
assistants increases confidence that the results do not depend on a bug in
either tool's kernel or library.

---

## Theory File

**`HavenIsolation.thy`** — 10 sections mirroring the Coq model in
`verification/coq/IsolationModel.v`:

| Section | Content                                                                     |
|---------|-----------------------------------------------------------------------------|
| 1       | Type definitions: `partition_id`, `phys_addr`, `mem_region`                 |
| 2       | `pa_in_region` predicate                                                    |
| 3       | `regions_disjoint` definition (matches Coq `regions_disjoint`)              |
| 4       | `partition_state` and `hv_state` record types                               |
| 5       | `partition_contains_pa` ownership predicate                                 |
| 6       | `spatial_isolation_invariant` definition                                    |
| 7       | Primary theorem: `spatial_no_overlap`                                       |
| 8       | No-shared-PA corollary: `isolation_no_shared_pa`                            |
| 9       | Budget model: `budget_state`, `consume`, `valid_budget`                     |
| 10      | Budget theorem: `consume_budget_preserves_valid`                            |

---

## Key Theorems

```isabelle
(* Section 7 — primary isolation result *)
theorem spatial_no_overlap:
  "spatial_isolation_invariant s ⟹
   p1 ≠ p2 ⟹
   partition_contains_pa s p1 pa ⟹
   ¬ partition_contains_pa s p2 pa"

(* Section 8 — corollary: no physical address is shared *)
theorem isolation_no_shared_pa:
  "spatial_isolation_invariant s ⟹
   ∀ pa. ¬ (partition_contains_pa s p1 pa ∧ partition_contains_pa s p2 pa)"

(* Section 10 — budget preservation *)
theorem consume_budget_preserves_valid:
  "valid_budget bs ⟹ valid_budget (consume bs ticks)"
```

---

## Running the Proofs

**Prerequisites:**

- Isabelle/HOL 2023 — download from <https://isabelle.in.tum.de/>
- The Isabelle `HOL` session image (included in the standard distribution)

**Build the theory:**

```bash
isabelle build -d . HavenIsolation
```

The `-d .` flag registers the current directory as an Isabelle component so
that `HavenIsolation` is discoverable as a named session.  Expected output:
`Finished HavenIsolation (0:XX elapsed)` with no `FAILED` entries.

**Interactive exploration:**

```bash
isabelle jedit -d . -l HavenIsolation
```

This opens the Isabelle/jEdit IDE with the theory preloaded.

---

## Correspondence with Coq Proofs

| Isabelle theorem                  | Coq theorem                          |
|-----------------------------------|--------------------------------------|
| `spatial_no_overlap`              | `spatial_isolation`                  |
| `isolation_no_shared_pa`          | (corollary of `spatial_isolation`)   |
| `consume_budget_preserves_valid`  | `consume_preserves_valid`            |
| `regions_disjoint_sym`            | `regions_disjoint_sym`               |
| `no_pa_in_zero_region`            | `no_pa_in_zero_region`               |

Minor definitional differences exist due to the distinct type systems
(Coq's `Prop` vs Isabelle's `bool`), but the mathematical content is
equivalent.

---

## Toolchain Requirements

| Tool          | Version  | Notes                            |
|---------------|----------|----------------------------------|
| Isabelle/HOL  | 2023     | Poly/ML 5.9 included             |
| Java (JRE)    | 11+      | Required by Isabelle's Scala/ML  |
