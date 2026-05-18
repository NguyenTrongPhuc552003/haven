# Assumptions and Constraints

This document records the system-level assumptions under which Haven's isolation
claims hold. Any claim made in the thesis is conditioned on these assumptions.

## Hardware assumptions

1. AArch64 Stage-2 MMU is present, enabled, and configured exclusively by EL2.
2. SMMU (SMMUv3 or equivalent) is present and enforces per-StreamID DMA
   window policy before any device can issue a bus transaction.
3. A GICv3-compatible interrupt controller is present and all SPI routing
   registers are writable only from EL2.
4. The platform timer (EL1 virtual timer) is accessible per-partition and
   cannot be reprogrammed by a guest to affect another partition's deadline.
5. Hardware virtualization extensions are enabled prior to hypervisor entry.
6. The CPU does not execute speculative memory accesses across Stage-2 domain
   boundaries in a way observable to the adversary (microarchitectural
   side-channels are out of scope; see THREAT_MODEL.md).

## Software and boot assumptions

1. The secure boot chain establishes a trusted hypervisor image before any
   partition is launched.
2. Platform firmware provides correct hardware configuration metadata
   (e.g., memory map, SMMU StreamID assignments, interrupt routing table).
3. Partition configurations (configs/arm64/*.yaml) are provided by a trusted
   operator and are not modifiable at runtime by any guest partition.
4. The hypervisor TCB consists only of the files listed in `src/core/` and
   `include/haven/`. Guest code in `src/guest/` is untrusted by definition.

## Evaluation scope constraints

1. Safety and isolation claims are limited to the hardware and software
   configurations evaluated on QEMU arm64 and the NXP i.MX95 Dev Kit.
2. Performance bounds (latency, budget jitter) are derived from measurements
   taken on these platforms; they do not generalise to other SoCs without
   re-measurement.
3. The isolation properties demonstrated are policy-correct: the enforcement
   logic does what the policy specifies. Physical hardware correctness
   (e.g., correct SMMU silicon behaviour) is assumed and not re-verified.

## Non-goals

1. Confidentiality against microarchitectural side-channels
   (cache timing, Spectre-class attacks).
2. Runtime dynamic partition migration or live resizing of partition budgets.
3. Fault-tolerant recovery from hardware failures within a partition's domain.
4. Formal verification of the C implementation (formal modelling in
   `verification/` targets the policy logic only).
