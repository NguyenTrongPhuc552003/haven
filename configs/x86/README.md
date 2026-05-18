# Haven - x86-64 Target Configuration

## Status

**Planned - Phase 3+ (stretch goal)**

x86-64 support is not yet implemented.  This directory contains the planned
QEMU virtual-platform configuration file (`qemu-x86_64.yaml`) to be used
if and when the x86-64 port is attempted.  All items in this README describe
the intended design; they are subject to change pending thesis scope decisions.

---

## Architecture: Intel VT-x (VMX)

Haven's x86-64 port would target **Intel VT-x (VMX)**, which defines:

| VMX Mode     | ARM64 Equivalent | Haven Role                 |
| ------------ | ---------------- | -------------------------- |
| VMX root     | EL2              | Haven hypervisor runs here |
| VMX non-root | EL1/EL0 (guest)  | Guest OS and userspace     |

Key VT-x structures and mechanisms:

- **VMCS (Virtual Machine Control Structure)** - per-vCPU data structure that
  controls guest entry/exit, analogous to ARM64's `HCR_EL2` + `VTTBR_EL2`
  combined.  Managed via `VMREAD`/`VMWRITE` instructions.
- **EPT (Extended Page Tables)** - Intel's second-stage address translation,
  analogous to ARM64 stage-2.  Configured via the EPT pointer field in the VMCS.
- **APIC virtualisation** - x2APIC + virtual APIC page replace GICv3.  IRQ
  routing is controlled via the VMCS interrupt-control fields.
- **VM exits** - trap to the hypervisor on sensitive operations
  (analogous to EL2 traps from `HCR_EL2` bits).

---

## Key Differences from ARM64

| Aspect               | ARM64                   | Intel VT-x (x86-64)                             |
| -------------------- | ----------------------- | ----------------------------------------------- |
| Stage-2 table        | Stage-2 (`VTTBR_EL2`)   | EPT (VMCS EPT pointer)                          |
| Stage-2 fault        | `FAR_EL2`, `ESR_EL2`    | EPT violation VM exit, `GUEST_PHYSICAL_ADDRESS` |
| Interrupt controller | GICv3                   | x2APIC + IOAPIC                                 |
| Timer                | Generic Timer           | HPET / TSC deadline (`VMCS PREEMPT_TIMER`)      |
| IOMMU                | SMMUv3                  | Intel VT-d (DMAR)                               |
| Hypercall            | `HVC #0`                | `VMCALL` instruction                            |
| Trap mechanism       | `HCR_EL2` bits          | VMCS execution controls                         |
| Context switch       | `VMID` + TTBR switching | VPID + VMCS switching (`VMPTRLD`)               |

---

## Target Platform

- **QEMU `q35` machine + KVM** - `qemu-system-x86_64 -machine q35,accel=kvm`
- Config file: `configs/x86/qemu-x86_64.yaml`
- Toolchain: native `gcc` (x86-64 host) or `x86_64-linux-gnu-gcc`
- Note: Running Haven under QEMU/KVM on an x86-64 host requires nested
  virtualisation (`kvm-intel nested=1` or `kvm_intel.nested=1`).

---

## Roadmap

The x86-64 port is a **Phase 3+ stretch goal** and may not be pursued if the
thesis scope is satisfied by ARM64 and RISC-V results alone:

1. Implement `arch/x86/` entry/exit stubs, EPT table management, and
   `VMLAUNCH`/`VMRESUME` flow.
2. Integrate Intel VT-d IOMMU driver (`drivers/iommu/vtd.c`).
3. Validate on `qemu-system-x86_64 -machine q35,accel=kvm`.
4. Run the existing test suite under QEMU x86-64.
