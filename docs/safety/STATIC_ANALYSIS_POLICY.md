# Static Analysis Policy

**Project:** Haven - Static Partition Hypervisor for AMP  
**Version:** 0.6.0  
**Policy owner:** Haven project team  
**Review cycle:** Per release tag  
**Last reviewed:** 2025-01

---

## 1. Policy Goals

1. Ensure the trusted computing base (TCB) is analysed by at least one automated static
   analysis tool on every CI run.
2. Prevent new defects in isolation-critical paths from being merged without explicit
   reviewer acknowledgement.
3. Maintain a false-positive registry so that suppressions are traceable and reversible.
4. Align analysis scope with MISRA-C:2012 requirements (see `MISRA_DEVIATION_RECORD.md`).

---

## 2. Tool Selection

### Primary tool: cppcheck

| Property          | Value                                                                     |
| ----------------- | ------------------------------------------------------------------------- |
| Version           | >= 2.13                                                                   |
| Invocation        | `cppcheck --language=c --std=c11 --enable=all --error-exitcode=1 <paths>` |
| MISRA integration | `--misra-c=misra-c:2012 --addon=misra.py` (if available)                  |
| CI gate           | job "quality gate", cppcheck runner (`tools/gate/`)                        |
| Scope             | `src/core/`, `arch/arm64/`, `drivers/iommu/`, `drivers/irqchip/`          |

cppcheck is the primary gate because it is available in all CI environments without
licensing cost and supports MISRA addon configuration.

### Secondary tool: Clang static analyser

| Property   | Value                                                      |
| ---------- | ---------------------------------------------------------- |
| Version    | >= 17 (bundled with LLVM toolchain)                        |
| Invocation | `clang --analyze -Xanalyzer -analyzer-output=text <paths>` |
| CI gate    | job "quality gate", clang-tidy `clang-analyzer-*` checks   |
| Scope      | Same as cppcheck                                           |

Clang is run as a secondary gate. Its findings complement cppcheck with
inter-procedural analysis and null-pointer dereference detection.

### Tertiary (manual, periodic): Coverity / CodeQL

Run on demand at milestone boundaries:

- Coverity: manual submission at major milestone sign-off

These are **advisory** - findings do not block merges but must be triaged within
30 days of the report.

---

## 3. Scope Definition

### In scope (analysed on every CI push)

| Path                    | Rationale                              |
| ----------------------- | -------------------------------------- |
| `src/core/`             | Partition lifecycle, policy evaluation |
| `arch/arm64/cpu.c`      | CPU setup at EL2                       |
| `arch/arm64/irq.c`      | Interrupt controller management        |
| `arch/arm64/mm.c`       | Stage-2 MMU                            |
| `arch/arm64/timer.c`    | Per-partition timer budget             |
| `drivers/iommu/smmu.c`  | SMMU DMA policy                        |
| `drivers/irqchip/gic.c` | GIC configuration                      |

### Out of scope

| Path                     | Rationale                                       |
| ------------------------ | ----------------------------------------------- |
| `tests/`                 | Test code; relaxed coding standard              |
| `tools/`                 | Host tools; separate language/environment       |
| `drivers/linux/`         | Linux kernel module; covered by kernel analysis |
| `drivers/uart/`          | Serial driver; low isolation impact             |
| `arch/arm64/boot.S`      | Assembly                                        |
| `arch/arm64/context.S`   | Assembly                                        |
| `arch/arm64/partition.S` | Assembly                                        |
| `arch/arm64/entry.S`     | Assembly                                        |

Scope additions require a PR that updates this document and the gate's
profile (`quality/gate.json`) together.

---

## 4. Defect Classification

| Class        | Definition                                                                                              | CI action                          |
| ------------ | ------------------------------------------------------------------------------------------------------- | ---------------------------------- |
| **Blocker**  | Memory safety, null deref, buffer overflow, use-after-free, integer overflow in isolation-critical path | Build fails; PR cannot merge       |
| **Major**    | Resource leak, uninitialized variable, incorrect type in protocol                                       | Warning; reviewer must acknowledge |
| **Minor**    | Style, unreachable code, unused variable                                                                | Warning; no block                  |
| **Advisory** | MISRA advisory rule                                                                                     | Logged; no CI action               |

Blocker-class defects in TCB scope are **hard gates** - they block merge even if the
defect is in a dead code path.

---

## 5. Suppression Policy

### Allowed suppressions

A finding may be suppressed only if:

1. It is a **documented false positive** (see Section 6), OR
2. It corresponds to a **documented MISRA deviation** in `MISRA_DEVIATION_RECORD.md`

### Suppression format

All inline suppressions must follow this format:

```c
/* static-analysis: <tool> - FP-NNN or DR-NNN: <one-line explanation> */
// cppcheck-suppress <check-id>
```

Or for Clang:

```c
// NOLINTNEXTLINE(<clang-analyzer-check>)  // FP-NNN: <reason>
```

**Suppressions without a corresponding FP or DR entry are rejected** during PR review.

### Suppression review

All suppressions are reviewed by the Isolation Guardian agent at each milestone
sign-off. Suppressions that are no longer needed (false positive resolved upstream,
deviation removed) must be deleted within one milestone cycle.

---

## 6. False-Positive Registry

Known false positives are recorded here. Each entry has an ID, a description of the
false positive, the tool version that introduced it, and the expected resolution.

| ID     | Tool          | Check                  | File                   | Description                                                                                                           | Resolution                                              |
| ------ | ------------- | ---------------------- | ---------------------- | --------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| FP-001 | cppcheck      | `missingReturn`        | `arch/arm64/irq.c`     | cppcheck does not model inline assembly `ret` at end of naked functions                                               | Waiting for cppcheck >= 2.14 fix                        |
| FP-002 | cppcheck      | `unusedFunction`       | `src/core/partition.c` | `hv_partition_get_state()` is only called from test code, which is excluded from scope                                | Suppress; function is part of the public API            |
| FP-003 | clang-analyze | `core.NullDereference` | `drivers/iommu/smmu.c` | Clang does not track that `smmu_find_stream_entry()` always returns non-null after `hv_partition_configure()` barrier | Suppress; guarded by partition initialization invariant |

---

## 7. Coverage Targets

At each milestone sign-off, the following coverage targets must be met:

| Metric                                             | Target                                      |
| -------------------------------------------------- | ------------------------------------------- |
| TCB source lines analysed                          | 100% of in-scope files                      |
| Blocker-class findings                             | 0 unresolved                                |
| Major-class findings                               | 0 unresolved (or all acknowledged in PR)    |
| Suppression ratio                                  | < 5% of total findings (suppressions/total) |
| False-positive registry entries without resolution | < 3 open at any time                        |

---

## 8. CI Workflow Integration

Static analysis runs in the quality gate (`scripts/gate`, design in
`docs/quality/gate.md`): cppcheck, clang-tidy (including the Clang static
analyzer's checks), lizard, jscpd, ruff, shellcheck, a secret and
personal-name scan and gitleaks, rated BLOCKER to INFO. The CI job
"quality gate" runs it on the whole tree and is a required check on
`main`; the git hooks run the same checks on the changed files before a
push; a weekly CI workflow adds the sanitizer builds. Findings that existed
when the gate started are in `quality/baseline.json`; a new CRITICAL or
MAJOR finding blocks the change.

### Adding a new tool

A tool is added to the gate's engine (`tools/gate/`): a runner, its
severity table and unit tests with recorded output, then the profile
(`quality/gate.json`) names it per stage and pins its version.

## 9. Relationship to MISRA Deviation Record

This policy and `MISRA_DEVIATION_RECORD.md` are companion documents:

- **This document** defines what tools run, what counts as a blocker, and how
  suppressions are justified.
- **MISRA_DEVIATION_RECORD.md** lists each specific rule deviation with individual
  rationale and Isolation Guardian sign-off.

A MISRA finding that does not have a corresponding DR entry in
`MISRA_DEVIATION_RECORD.md` is treated as a **Blocker** (see Section 4).

---

## 10. Policy Revision Log

| Date    | Version | Change                  | Author     |
| ------- | ------- | ----------------------- | ---------- |
| 2025-01 | 0.6.0   | Initial policy document | Haven team |
