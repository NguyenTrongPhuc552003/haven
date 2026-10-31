# MM1/MM2 QEMU tests: readiness audit

**Status:** AUDIT, read-only. No code changed. Written 2026-10-31.
**Owner repo:** HAVEN. **Read at:** working tree on `f775304` with the
[MM1 BUGFIX](../20261031-mm1-bugfix/README.md) applied.
**Question:** what stands between the MM1/MM2 fix and its L2/L3 QEMU tests
(read-only write fault, execute-never fetch fault, on `cortex-a72` and
`-cpu max`)?

## 1. MM2 encoding: second independent source (source-derived)

Linux KVM writes stage-2 descriptors for real hardware. In
`arch/arm64/kvm/hyp/pgtable.c` (GitHub `torvalds/linux` master, read
2026-10-31), `stage2_set_xn_attr` encodes `XN[1:0]` (`KVM_PTE_LEAF_ATTR_HI_S2_XN
= GENMASK(54, 53)` in `asm/kvm_pgtable.h`) as: both may execute `0b00`; EL0
only `0b01`; neither `0b10`; privileged only `0b11`. Different PX/UX values
are rejected unless the CPU has `ARM64_HAS_XNX`. `kvm_pgtable_stage2_pte_prot`
decodes the same table.

This agrees with QEMU's TTS2UXN implementation. Two independent
implementations confirm that HAVEN's old `S2_XN_ALL` (`0b11`) was
EL1-executable on FEAT_XNX CPUs, and that the fix (`0b10`) is correct. The Arm
ARM itself has still not been read; the MM2 entry keeps that as open.

## 2. HAVEN's reporting path for guest stage-2 faults (source-derived)

`hv_handle_sync` (`src/core/exc/el2_exceptions.c`) handles a lower-EL
Instruction Abort (EC `0x20`) or Data Abort (EC `0x24`) with FSC `0x04-0x0F`
and `S1PTW = 0`: it prints `HAVEN: Denied stage-2 violation IPA=... FAR=...
ESR=...`, adds 4 to the saved ELR and returns to the guest. Stage 0's TF-A
experiments observed guest A and guest B denials through this path
(`stages/STAGE0_BASELINE.md`), so a read-only **write** test is observable from
the UART log today. Two problems affect the new tests:

### 2a. The logged IPA is unreliable for permission faults (proposed finding EX1)

- **Source-derived:** HAVEN computes `IPA` from `HPFAR_EL2` for every fault
  code in `0x04-0x0F`, including permission faults (`0x0C-0x0F`).
- **Source-derived (Linux KVM):** `__get_fault_info` / `__hpfar_valid` in
  `arch/arm64/kvm/hyp/include/hyp/fault.h` trust `HPFAR_EL2` only for
  translation, access-flag and address-size faults, and for permission faults
  during a stage-1 walk. For a direct stage-2 permission fault, KVM recomputes
  the IPA from `FAR_EL2` with an `AT` instruction instead.
- **Inference:** the `IPA=` value HAVEN logs for exactly the faults MM1 now
  makes possible may be UNKNOWN. Confirm the `HPFAR_EL2` validity rule in the
  Arm ARM.
- **Consequence for tests:** the L3 test must judge a permission fault by
  `ESR` (EC and FSC) and `FAR`, not by the logged `IPA`.

### 2b. Instruction-abort recovery repeats the fault (proposed finding EX2)

- **Source-derived:** the handler always skips the faulting instruction by
  `ELR += 4`.
- **Inference (architecture):** for an Instruction Abort, `ELR` is the address
  of the instruction that could not be fetched. `ELR + 4` is the next
  instruction in the same execute-never page, so the guest faults again,
  once per instruction, until execution leaves the page: up to 1,024 log
  lines per 4 KB page, then execution continues at whatever follows.
- **Consequence for tests:** an XN fetch test would observe the first fault
  correctly, but the guest's behaviour afterwards is not meaningful. A clean
  L3 test needs a different recovery for instruction aborts (for example,
  return to the guest's link register, or stop the partition and report).
  That is a HAVEN design choice, not part of the approved MM1 BUGFIX.

## 3. Booting HAVEN under QEMU (source-derived)

- `src/core/partition.c` calls `hv_arch_psci_cpu_on(2, ...)` unconditionally
  after mapping both partitions. With direct loading
  (`virt,virtualization=on`) there is no EL3 PSCI service, so the `hvc #0` from
  EL2 traps to HAVEN's own fatal handler (`stages/STAGE0_BASELINE.md`, ESR EC
  `0x16`). A single-CPU run does not avoid this.
- The TF-A BL1/BL2/BL31 path (`virt,secure=on,...`) reached the guests, but
  only with a disposable `hvc` → `smc` change. The conduit is undecided
  (FINDINGS B1, `stages/STRATEGY.md` Phase 1), and the full QEMU command line
  was not preserved.

## 4. What the L2/L3 tests need, in order

| # | Prerequisite | Owner | Type | Blocked on |
| - | - | - | - | - |
| 1 | PSCI conduit / boot-profile decision (B1): TF-A + SMC, or a profile that doesn't start CPU 2 | HAVEN | decision, then BUGFIX | owner |
| 2 | Reproducible QEMU launch recorded verbatim (TF-A build, QEMU command, expected log) | ELMOS | EXPERIMENT | 1; needs macOS (QEMU is not in the Linux sandbox) |
| 3 | Instruction-abort recovery that doesn't repeat the fault (EX2) | HAVEN | decision, then BUGFIX | owner |
| 4 | Test partition config with an `HV_MEM_R` region and an `HV_MEM_R\|W` (non-X) region, and a test guest that writes to the first and branches into the second | HAVEN | FEATURE (test-only) | 1-3 |
| 5 | Run on `-cpu cortex-a72` and `-cpu max`; judge by ESR/FAR (EX1) | HAVEN + ELMOS | EXPERIMENT | 2-4 |

The read-only **write** test needs only rows 1, 2, 4 and 5. The XN fetch test
also needs row 3.

## 5. Decisions (approved by the workspace owner, 2026-10-31)

All three approved as recommended: **E1** EX1 and EX2 recorded in the finding
ledger; **E2** QEMU tests boot through TF-A with an SMC conduit, implemented
as FINDINGS B1's explicit boot profiles (not a global HVC→SMC swap); **E3**
on an instruction abort from a guest, HAVEN stops the partition and logs.
Implementation of E2 and E3 needs its own BUGFIX approval.

Original questions:

- **E1:** record EX1 and EX2 in the finding ledger as source-derived/inference,
  pending the Arm ARM check?
- **E2:** which boot path for the QEMU tests: TF-A with an SMC conduit (closest
  to real firmware, matches Stage 0's working run), or a test profile without
  CPU 2 (simplest)? This is FINDINGS B1.
- **E3:** for EX2, prefer "return to the guest's link register" (guest can
  continue and report) or "stop the partition and log" (simplest, fail-safe)?
