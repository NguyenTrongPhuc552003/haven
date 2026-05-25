# Static Analysis Policy

**Project:** Haven — Static Partition Hypervisor for AMP  
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

| Property | Value |
| -------- | ----- |
| Version | >= 2.13 |
| Invocation | `cppcheck --language=c --std=c11 --enable=all --error-exitcode=1 <paths>` |
| MISRA integration | `--misra-c=misra-c:2012 --addon=misra.py` (if available) |
| CI gate | `static-analysis.yml` job `cppcheck` |
| Scope | `src/core/`, `arch/arm64/`, `drivers/iommu/`, `drivers/irqchip/` |

cppcheck is the primary gate because it is available in all CI environments without
licensing cost and supports MISRA addon configuration.

### Secondary tool: Clang static analyser

| Property | Value |
| -------- | ----- |
| Version | >= 17 (bundled with LLVM toolchain) |
| Invocation | `clang --analyze -Xanalyzer -analyzer-output=text <paths>` |
| CI gate | `static-analysis.yml` job `clang-analyze` |
| Scope | Same as cppcheck |

Clang is run as a secondary gate. Its findings complement cppcheck with
inter-procedural analysis and null-pointer dereference detection.

### Tertiary (manual, periodic): Coverity / CodeQL

Run on demand at milestone boundaries:

- CodeQL: triggered via `github/codeql-action` on version tags
- Coverity: manual submission at major milestone sign-off

These are **advisory** — findings do not block merges but must be triaged within
30 days of the report.

---

## 3. Scope Definition

### In scope (analysed on every CI push)

| Path | Rationale |
| ---- | --------- |
| `src/core/` | Partition lifecycle, policy evaluation |
| `arch/arm64/cpu.c` | CPU setup at EL2 |
| `arch/arm64/irq.c` | Interrupt controller management |
| `arch/arm64/mm.c` | Stage-2 MMU |
| `arch/arm64/timer.c` | Per-partition timer budget |
| `drivers/iommu/smmu.c` | SMMU DMA policy |
| `drivers/irqchip/gic.c` | GIC configuration |

### Out of scope

| Path | Rationale |
| ---- | --------- |
| `tests/` | Test code; relaxed coding standard |
| `tools/` | Host tools; separate language/environment |
| `drivers/linux/` | Linux kernel module; covered by kernel analysis |
| `drivers/uart/` | Serial driver; low isolation impact |
| `arch/arm64/boot.S` | Assembly |
| `arch/arm64/context.S` | Assembly |
| `arch/arm64/partition.S` | Assembly |
| `arch/arm64/entry.S` | Assembly |

Scope additions require a PR that updates this document and the CI workflow
`static-analysis.yml` simultaneously.

---

## 4. Defect Classification

| Class | Definition | CI action |
| ----- | ---------- | --------- |
| **Blocker** | Memory safety, null deref, buffer overflow, use-after-free, integer overflow in isolation-critical path | Build fails; PR cannot merge |
| **Major** | Resource leak, uninitialized variable, incorrect type in protocol | Warning; reviewer must acknowledge |
| **Minor** | Style, unreachable code, unused variable | Warning; no block |
| **Advisory** | MISRA advisory rule | Logged; no CI action |

Blocker-class defects in TCB scope are **hard gates** — they block merge even if the
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

| ID | Tool | Check | File | Description | Resolution |
| -- | ---- | ----- | ---- | ----------- | ---------- |
| FP-001 | cppcheck | `missingReturn` | `arch/arm64/irq.c` | cppcheck does not model inline assembly `ret` at end of naked functions | Waiting for cppcheck >= 2.14 fix |
| FP-002 | cppcheck | `unusedFunction` | `src/core/partition.c` | `hv_partition_get_state()` is only called from test code, which is excluded from scope | Suppress; function is part of the public API |
| FP-003 | clang-analyze | `core.NullDereference` | `drivers/iommu/smmu.c` | Clang does not track that `smmu_find_stream_entry()` always returns non-null after `hv_partition_configure()` barrier | Suppress; guarded by partition initialization invariant |

---

## 7. Coverage Targets

At each milestone sign-off, the following coverage targets must be met:

| Metric | Target |
| ------ | ------ |
| TCB source lines analysed | 100% of in-scope files |
| Blocker-class findings | 0 unresolved |
| Major-class findings | 0 unresolved (or all acknowledged in PR) |
| Suppression ratio | < 5% of total findings (suppressions/total) |
| False-positive registry entries without resolution | < 3 open at any time |

---

## 8. CI Workflow Integration

The static analysis CI gate is defined in `.github/workflows/static-analysis.yml`.

### Current gate configuration

```yaml
# Excerpt from static-analysis.yml
jobs:
  cppcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install cppcheck
        run: sudo apt-get install -y cppcheck
      - name: Run cppcheck
        run: |
          cppcheck --language=c --std=c11 \
            --enable=warning,portability,performance \
            --error-exitcode=1 \
            arch/arm64/ src/core/ drivers/iommu/ drivers/irqchip/

  clang-analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run clang --analyze
        run: |
          clang --analyze -Xanalyzer -analyzer-output=text \
            arch/arm64/*.c src/core/*.c drivers/iommu/*.c drivers/irqchip/*.c
```

### Adding a new tool

To add a new static analysis tool:

1. Add a new job to `static-analysis.yml`
2. Update the scope definition in Section 3 of this document
3. Run the tool locally and triage initial findings before merging
4. Add any false positives to Section 6

---

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

| Date | Version | Change | Author |
| ---- | ------- | ------ | ------ |
| 2025-01 | 0.6.0 | Initial policy document | Haven team |
