# Haven — RISC-V Target Configuration

## Status

**Planned — Phase 3+ (future target)**

RISC-V support is not yet implemented.  This directory contains the planned
QEMU virtual-platform configuration file (`qemu-riscv64.yaml`) to be used
once the RISC-V port is underway.  All items in this README describe the
intended design; they are subject to change.

---

## Architecture: RISC-V H-extension

Haven's RISC-V port will target the **RISC-V Hypervisor (H) extension**,
which defines two new privilege modes:

| Mode | RISC-V Name | ARM64 Equivalent | Haven Role              |
|------|-------------|------------------|-------------------------|
| HS   | Hypervisor Supervisor | EL2     | Haven hypervisor runs here |
| VS   | Virtual Supervisor    | EL1 (guest) | Guest OS (Linux/RTOS)   |
| VU   | Virtual User          | EL0 (guest) | Guest userspace         |

The H-extension adds:
- **HGATP** — Hypervisor Guest Address Translation and Protection register
  (analogous to ARM64 `VTTBR_EL2`); controls the second-stage page table for
  guest physical-to-machine-physical translation.
- **HTVAL / HTINST** — guest fault information registers (analogous to
  `FAR_EL2` / `ESR_EL2`).
- **VS-mode CSRs** — shadow copies of supervisor CSRs (`vsatp`, `vsepc`, etc.)
  that are virtualised by the hypervisor.

---

## Key Differences from ARM64

| Aspect               | ARM64                        | RISC-V H-extension            |
|----------------------|------------------------------|-------------------------------|
| Stage-2 table reg    | `VTTBR_EL2`                  | `HGATP`                       |
| Stage-2 fault info   | `FAR_EL2`, `ESR_EL2`         | `HTVAL`, `HTINST`, `scause`   |
| Interrupt controller | GICv3 (`drivers/irqchip/`)   | PLIC (Platform-Level Interrupt Controller) or AIA (Advanced Interrupt Architecture) |
| Timer                | Generic Timer (`CNTHP_*`)    | SBI timer extension or `stimecmp` (Sstc extension) |
| IOMMU                | SMMUv3                       | RISC-V IOMMU (draft spec)     |
| Hypercall            | `HVC #0`                     | `ecall` from HS/VS mode       |
| CSR access           | System registers via `MRS/MSR` | CSR instructions (`csrr`, `csrw`) |

---

## Target Platform

- **QEMU `virt` machine (RISC-V 64-bit)** — `qemu-system-riscv64 -machine virt`
- Config file: `configs/riscv/qemu-riscv64.yaml`
- Toolchain: `riscv64-unknown-elf-gcc` or `riscv64-linux-gnu-gcc`

---

## Roadmap

The RISC-V port is planned for **Phase 3** of the Haven thesis project:

1. Port the arch layer: `arch/riscv/` (stage-2 table setup via HGATP,
   trap/exception entry, PLIC integration).
2. Adapt the IOMMU policy layer for the RISC-V IOMMU spec.
3. Validate on `qemu-system-riscv64 -machine virt` with the SBI firmware
   (`OpenSBI`).
4. Run the existing test suite under QEMU RISC-V.
