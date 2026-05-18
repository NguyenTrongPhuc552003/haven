# Review Checklist

Use this checklist for every pull request. Items marked **[REQUIRED]** must
be satisfied before merge. All other items are strong recommendations.

---

## General PR hygiene

- [ ] PR title follows `<type>(<scope>): <summary>` convention.
- [ ] Each commit is atomic — one logical change per commit.
- [ ] No debug prints (`printf`, `printk` with `HAVEN_TRACE_*`) left in
      production paths unless guarded by a compile-time flag.
- [ ] No new compiler warnings with `-Wall -Wextra -Werror`.

---

## Isolation guardian checklist [REQUIRED for isolation-policy PRs]

Any PR that touches the following files **must** have at least two reviewers,
one of whom is the isolation guardian for the affected subsystem:

- `src/core/mm/stage2.c` — spatial isolation
- `src/core/sched/budget.c` — temporal isolation
- `src/core/dma/smmu.c` — DMA isolation
- `src/core/irq/ownership.c` — interrupt isolation
- `src/core/iommu/iommu_policy.c` — IOMMU group isolation
- `include/haven/stage2.h`, `include/haven/smmu.h`,
  `include/haven/budget_sched.h` — public API contracts

For each changed isolation-policy file, verify:

- [ ] **[REQUIRED]** Every public API function validates partition_id != 0
      and all pointer arguments are non-NULL before any state mutation.
- [ ] **[REQUIRED]** Negative test added or updated in
      `tests/integration/test_isolation_negative.c` covering the new
      rejection path.
- [ ] **[REQUIRED]** `docs/safety/ASSUMPTIONS.md` updated if a new
      assumption is introduced or an existing one is invalidated.
- [ ] **[REQUIRED]** `docs/safety/THREAT_MODEL.md` reviewed — does the
      change affect any threat boundary?

---

## TCB size review

Haven targets a core TCB under 5 000 lines of C. Check before merge:

```bash
wc -l src/core/**/*.c
```

- [ ] Total `src/core/` line count remains below 5 000 lines.
- [ ] Any new file added to `src/core/` has a justification in the PR
      description explaining why it belongs in the TCB.
- [ ] New helper code that is not strictly isolation-enforcement lives in
      `src/common/` or `src/guest/`, not `src/core/`.

---

## Test coverage requirements

- [ ] Every new public function has at least one unit test in
      `tests/unit/test_<subsystem>.c`.
- [ ] New isolation enforcement paths have a boundary test in
      `tests/isolation/`.
- [ ] `make test` passes locally before opening the PR.
- [ ] If a new benchmark is added, expected output ranges are documented
      in `docs/thesis/REPRODUCIBILITY_APPENDIX.md`.

---

## Documentation requirements

- [ ] Public header comments (`include/haven/*.h`) updated to reflect any
      changed preconditions, postconditions, or error codes.
- [ ] If the change introduces a new subsystem or significantly extends an
      existing one, `docs/architecture/` contains a matching description.
- [ ] Porting instructions in `docs/porting/` updated if the change touches
      platform-specific code (`src/platform/`).

---

## Safety assumption review

Before merging any change to `src/core/` or `include/haven/`:

- [ ] Review `docs/safety/ASSUMPTIONS.md` — does the change rely on a
      hardware feature not listed there?
- [ ] Review `docs/safety/THREAT_MODEL.md` — does the change open or close
      a threat boundary?
- [ ] If Coq proofs exist for the affected subsystem, proofs have been
      updated and still compile (`coqc`).
