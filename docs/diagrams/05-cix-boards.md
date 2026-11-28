# What is the physical target, and what is unverified?

CIX P1 (Sky1). No board is in hand: everything on the hardware side is
taken from manuals, firmware source and community reports, and nothing is
observed. Sources: `docs/architecture/cix-p1-partition-plan.md` (labels
[F] primary source, [S] community or single source), `src/platform/cix-p1/`,
`boot/efi/`, `.github/workflows/ci.yml` (jobs "Cross-compile (cix-p1
draft)" and "UEFI probe and loader at EL2 in QEMU").

```mermaid
flowchart TB
  subgraph SOC["CIX P1 SoC, on the boards below (not in hand)"]
    CPU["12 cores: 4 + 4 Cortex-A720, 4 Cortex-A520<br/>MPIDR core number in Aff1 [F]"]
    GIC["GIC-700, GICv4.1 (reported)"]
    SMMU["SMMUv3 instances: PCIe at 0x0B010000,<br/>multimedia at 0x0B1B0000 [F]"]
    GPU["Immortalis-G720 GPU<br/>no SMMU entry: DMA untranslated [F]"]
  end
  FW["Vendor firmware: TF-A, UEFI (EDK2)<br/>ACPI by default; PSCI over SMC"]
  subgraph HV["HAVEN for the board"]
    BUILD["cix-p1 build<br/>compiles in CI (draft); no board run<br/>(implemented)"]
    LOADER["UEFI loader that starts HAVEN at EL2"]
    PLAN["Partition plan: CPUs, consoles,<br/>GPU and NPU placement<br/>(proposed)"]
  end
  subgraph BOARDS["Boards (one SoC, different wiring and firmware)"]
    OP6P["Orange Pi 6 Plus<br/>thesis board, planned"]
    OP6["Orange Pi 6"]
    O6["Radxa Orion O6 / O6N"]
  end

  FW -->|UEFI application| LOADER
  LOADER --> BUILD
  BUILD -.->|programs stage-2, GIC, SMMU| SOC
  PLAN -.-> BUILD
  BOARDS --- SOC

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef implemented stroke:#fab219,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class LOADER observed
  class BUILD implemented
  class PLAN proposed
```

- The UEFI loader is observed only in QEMU (edk2 firmware, CI job "UEFI
  probe and loader at EL2", rows L0 and L1b), not on a board.
- DMA isolation claims for these boards stay experimental: the GPU has no
  SMMU in its path, and the firmware's IORT is reported wrong in places.
- Before any port: board revision, firmware hash, exception level at
  entry, ACPI or device tree, GIC and SMMU register dumps, a recovery path.
