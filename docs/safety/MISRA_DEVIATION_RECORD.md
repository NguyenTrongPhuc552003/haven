# MISRA-C:2012 Deviation Record

**Project:** Haven — Static Partition Hypervisor for AMP  
**Version:** 0.6.0  
**TCB Scope:** `arch/arm64/`, `src/core/`, `drivers/irqchip/`, `drivers/iommu/`  
**Standard:** MISRA-C:2012 (with Amendment 1, 2016; Amendment 2, 2020)  
**Tooling:** cppcheck with `--language=c --std=c11 --misra-c=misra-c:2012`  
**Maintainer:** Haven project team  
**Last reviewed:** 2025-01

---

## 1. Rule Selection Basis

Haven applies MISRA-C:2012 rules selectively to the trusted computing base (TCB).
The TCB is defined as all code that executes at EL2 and directly manipulates
isolation-critical resources: stage-2 page tables, interrupt routing registers,
SMMU stream tables, and scheduling timer registers.

The following categories of files are **in scope**:

| Path | Rationale |
| ---- | --------- |
| `arch/arm64/cpu.c` | CPU initialization at EL2 |
| `arch/arm64/irq.c` | Interrupt controller configuration |
| `arch/arm64/mm.c` | Stage-2 MMU manipulation |
| `arch/arm64/timer.c` | Per-partition timer budget |
| `src/core/` | Partition lifecycle, policy evaluation |
| `drivers/iommu/smmu.c` | SMMU stream table and DMA policy |
| `drivers/irqchip/gic.c` | GIC-600 configuration (if present) |

The following are **out of scope** for MISRA:

| Path | Rationale |
| ---- | --------- |
| `tests/` | Test harness; correctness over style |
| `tools/` | Host-side tools; not in TCB |
| `drivers/linux/` | Linux userspace drivers; separate coding standard |
| `website/` | TypeScript/Astro; not C |
| `arch/arm64/boot.S` | Assembly; MISRA does not apply to asm |
| `arch/arm64/context.S` | Assembly; same |

---

## 2. Mandatory Rule Deviations

The following MISRA-C:2012 required rules are deviated from in the TCB.
Each deviation requires an ID, rationale, and Isolation Guardian sign-off.

### DR-001 — Rule 15.5: A function should have a single point of exit

**Location:** `src/core/partition.c` — `hv_partition_configure()`  
**Rule text:** "A function should have a single point of exit at the end."  
**Deviation rationale:** Early-exit guard clauses are used for parameter validation at
EL2 entry points. The alternative (deep nesting or `goto cleanup`) reduces readability
and increases audit complexity. Each early return path is fail-closed (returns
`HV_ERR_INVALID_ARG`). No resource is allocated before the first guard clause.  
**Risk assessment:** Low. All early exits are on the error path; the success path has
a single exit.  
**Isolation impact:** None. Guard clauses execute before any partition state mutation.  
**Isolation Guardian sign-off:** Pending

---

### DR-002 — Rule 17.7: The value returned by a function call shall be used

**Location:** `arch/arm64/irq.c` — GIC register write calls  
**Rule text:** "The value returned by a function having non-void return type shall be used."  
**Deviation rationale:** MMIO register write wrappers (`writel()`, `writeq()`) return void
by design. Some wrappers are thin macros over volatile pointer assignments and cannot
meaningfully return an error code — the hardware register write either succeeds
(acknowledged by hardware) or the system is in a fatal state. Error handling for
individual GIC register writes would add noise without improving safety.  
**Risk assessment:** Medium. A hardware-fault detector (e.g., RAS) would be needed
for true detection of write failures; this is out of scope for v1.0.  
**Isolation impact:** Low. GIC register writes are confined to the EL2 initialization
path; isolation invariants are verified after the full initialization sequence.  
**Isolation Guardian sign-off:** Pending

---

### DR-003 — Rule 21.3: The memory allocation and deallocation functions of \<stdlib.h\> shall not be used

**Location:** `src/core/partition.c`, `drivers/iommu/smmu.c`  
**Rule text:** "The memory allocation and deallocation functions of \<stdlib.h\> shall not be used."  
**Deviation rationale:** Haven uses a static memory allocator (`src/common/alloc.c`) for
all EL2 heap operations. `malloc()` and `free()` from `<stdlib.h>` are not used in TCB code.
This rule is **satisfied** — recorded here as an explicit confirmation that no dynamic
allocation from the C standard library is present.  
**Risk assessment:** None (rule satisfied).  
**Isolation Guardian sign-off:** N/A (compliant)

---

### DR-004 — Rule 21.6: The standard library I/O functions shall not be used

**Location:** `arch/arm64/cpu.c`, `src/core/partition.c` — debug logging  
**Rule text:** "The standard library input/output functions shall not be used."  
**Deviation rationale:** In debug builds (`-DHAVEN_DEBUG`), `printf()` is used via
`drivers/uart/uart.c` to write to the UART FIFO. This is guarded by `#ifndef NDEBUG`
and compiled out in release builds. Production builds (`cmake --preset arm64-imx95`)
define `NDEBUG` and do not link `printf`.  
**Risk assessment:** Low. Debug output is one-way (UART TX only); no input functions
(`scanf`, `fgets`) are used anywhere in the TCB.  
**Isolation impact:** None in release builds. In debug builds, UART output is a
non-isolated side channel — acceptable for development, not for production.  
**Isolation Guardian sign-off:** Pending

---

### DR-005 — Rule 14.4: The controlling expression of an if statement and the controlling expression of an iteration-statement shall be essentially Boolean

**Location:** `src/core/irq_policy.c` — IRQ ownership checks  
**Rule text:** "The controlling expression shall be essentially Boolean."  
**Deviation rationale:** IRQ ownership predicates return `int` (0/non-zero) rather than
`_Bool` to maintain compatibility with the existing `hv_irq_is_owned_by()` interface
contract. Changing the return type would require updating all call sites and tests.
The values returned are always 0 or 1.  
**Risk assessment:** Low. The non-zero test is semantically equivalent to a Boolean test
at all call sites.  
**Isolation Guardian sign-off:** Pending

---

### DR-006 — Rule 11.1–11.8: Conversions shall not be performed between a pointer to a function and any other type

**Location:** `arch/arm64/boot.S` linker-exported symbols, `src/core/partition.c`  
**Rule text:** MISRA Rule 11.1 (function pointer cast), Rule 11.4 (object pointer to integer),
Rule 11.6 (void pointer arithmetic).  
**Deviation rationale:** EL2 partition entry points are passed as physical addresses via
`uint64_t` and then cast to function pointers at the partition launch callsite. This is
required to support dynamic partition configuration (different guest entry points per YAML
config). The cast is performed in a single, audited location (`hv_partition_launch()`).
All addresses are validated against the stage-2 map before the cast.  
**Risk assessment:** Medium. Incorrect addresses here would cause an EL1 exception.
Mitigation: address validation asserts are present; the test suite covers invalid
address paths.  
**Isolation Guardian sign-off:** Pending

---

## 3. Advisory Rule Deviations

The following MISRA-C:2012 advisory rules are deviated from.

### DA-001 — Rule 2.7: There should be no unused parameters in functions

**Location:** Various IRQ handler stubs and weak symbols  
**Rationale:** Weak symbol implementations must match their declared prototype even
when parameters are not used in the default (no-op) implementation.  
**Mitigation:** `(void)param;` cast is used at all unused parameter sites.

---

### DA-002 — Rule 8.13: A pointer should point to a const-qualified type whenever possible

**Location:** `src/core/partition.c` — partition state machine  
**Rationale:** Several internal state mutation functions pass pointers to mutable
`hv_partition_t` objects that are const for most of the function body but mutated at
the end. Splitting these into read-path and write-path functions would increase
code size and complexity.  
**Mitigation:** Functions are documented with the specific field(s) they mutate.

---

## 4. Suppression Policy

MISRA deviations in the TCB **must** be documented in this file before being suppressed
in source code. The suppression format is:

```c
/* MISRA-C:2012 Rule X.Y deviation: DR-NNN - <one-line reason> */
// cppcheck-suppress misraX.Y
```

Suppressions **without** a corresponding DR entry in this file are flagged as
policy violations in the static analysis CI gate (`static-analysis.yml`).

---

## 5. Isolation Guardian Sign-Off Template

When a DR entry is reviewed, the Isolation Guardian agent records:

```
Isolation Guardian Sign-Off
DR-NNN: <rule>
Reviewed: <YYYY-MM-DD>
Reviewer: Isolation Guardian agent (session: <session-id>)
Verdict: ACCEPTED | REJECTED | DEFERRED
Notes: <any caveats or required follow-up>
```

All open sign-offs (marked "Pending") must be resolved before the Phase 6 milestone
gate closes.
