# Haven

**Hypervisor for Asymmetric Virtualization ENforcement and isolation**

[![CI](https://github.com/NguyenTrongPhuc552003/haven/actions/workflows/ci.yml/badge.svg)](https://github.com/NguyenTrongPhuc552003/haven/actions/workflows/ci.yml)
[![Docs](https://github.com/NguyenTrongPhuc552003/haven/actions/workflows/vercel.yml/badge.svg)](https://haven-tau-eight.vercel.app/)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.0--dev-orange.svg)](VERSION)
[![Platform](https://img.shields.io/badge/platform-ARM64%20AArch64-lightgrey.svg)](configs/arm64/)

> **Master's Thesis** - Computer Engineering  
> *Static Partition Hypervisor for Asymmetric Multiprocessing: Enforcing Spatial and Temporal Isolation Between Linux and RTOS on a Heterogeneous SoC*

**Live documentation → [haven-tau-eight.vercel.app](https://haven-tau-eight.vercel.app/)**

---

## Overview

Haven is a static partition hypervisor research artifact that runs at **EL2** on ARM64 SoCs. It enforces hard spatial and temporal isolation between a Linux domain and an RTOS domain on heterogeneous SoCs where application-class cores (Cortex-A) and microcontroller cores (Cortex-M) coexist on a single die.

```
┌─────────────────────────────────────────────────────────────┐
│                  Guest Partitions (EL1/EL0)                 │
│  ┌───────────────────────┐   ┌───────────────────────────┐  │
│  │   Linux  (A55 × 4)    │   │  FreeRTOS  (M7 / A55)     │  │
│  └───────────┬───────────┘   └─────────────┬─────────────┘  │
├──────────────┼─────────────────────────────┼────────────────┤
│              │          EL2 - Haven        │                │
│  ┌───────────▼─────────────────────────────▼─────────────┐  │
│  │  Stage-2 MMU │ IRQ Ownership   │ Budget Scheduler     │  │
│  │  SMMU Policy │ EL2 Exc Handler │ Time Accounting      │  │
│  └───────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│              Hardware - NXP i.MX95 (ARM64 SoC)              │
│     Cortex-A55 × 4  │  Cortex-M7  │  GIC-700  │  SMMU-700   │
└─────────────────────────────────────────────────────────────┘
```

### Core Guarantees

| Property               | Mechanism                                     | Status           |
| ---------------------- | --------------------------------------------- | ---------------- |
| **Spatial isolation**  | Stage-2 page tables, SMMU stream policy       | Stub implemented |
| **Temporal isolation** | Budget-based EL2 scheduler, overrun detection | Stub implemented |
| **IRQ ownership**      | Deny-by-default interrupt routing table       | Stub implemented |
| **DMA isolation**      | Per-partition SMMU context                    | Stub implemented |
| **Minimal TCB**        | No dynamic alloc, no FP, bounded loops        | By design        |

---

## Quick Start

**Prerequisites:** `gcc` or `clang`, `cmake` ≥ 3.22, `ninja-build`, `python3`, `qemu-system-aarch64` (optional)

```sh
git clone https://github.com/NguyenTrongPhuc552003/haven.git
cd haven
```

**Host unit + integration tests:**

```sh
cmake --preset host-tests
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

**ARM64 cross-compile (QEMU):**

```sh
# Requires: aarch64-linux-gnu-gcc or aarch64-elf-gcc
cmake --preset arm64-qemu
cmake --build build
# → build/haven.elf, build/haven.bin, build/guest_a.bin, build/guest_b.bin
```

**ARM64 cross-compile (i.MX95 — thesis primary board):**

```sh
cmake --preset arm64-imx95
cmake --build build-imx95
```

**Other targets:**

```sh
cmake --build <dir> --target style-check   # style + config checks
cmake --build <dir> --target verification  # Coq isolation proofs
cmake --build <dir> --target evidence      # package evaluation artifacts
cmake --build <dir> --target qemu-run      # launch Haven on QEMU (ARM64 build only)
```

See [CMakePresets.json](CMakePresets.json) for all available presets.

---

## Repository Structure

```
haven/
├── src/core/           # EL2 hypervisor core (mm, irq, sched, dma, exc)
├── src/guest/          # Guest-side drivers (UART, FreeRTOS integration)
├── src/platform/       # Board-specific bring-up (i.MX95, QEMU)
├── include/haven/      # Public API headers
├── configs/arm64/      # Static partition configs (YAML)
├── drivers/            # Linux and guest tool drivers
├── tests/              # Unit, integration, and isolation tests
├── docs/               # Architecture, methodology, roadmap, safety docs
├── docs/thesis/        # Master thesis working documents
├── verification/       # Formal verification artifacts (Coq, Isabelle)
├── website/            # Astro Starlight documentation portal
├── scripts/            # Build, test, CI, and release automation
└── .github/workflows/  # CI matrix (gcc/clang, QEMU smoke, Vercel deploy)
```

**Key source modules:**

| Module           | File                                    | Purpose                                     |
| ---------------- | --------------------------------------- | ------------------------------------------- |
| Stage-2 MMU      | `src/core/mm/stage2.c`                  | Partition memory mapping and access control |
| IRQ Ownership    | `src/core/irq/ownership.c`              | Interrupt routing table, deny-by-default    |
| Budget Scheduler | `src/core/sched/budget.c`               | CPU time budget tracking and enforcement    |
| SMMU / DMA       | `src/core/dma/smmu.c`                   | Stream-ID-based DMA partition isolation     |
| EL2 Exceptions   | `src/core/exc/el2_exceptions.c`         | EL2 exception dispatch and fault handling   |
| Guest UART       | `src/guest/drivers/uart.c`              | Isolated guest UART port abstraction        |
| FreeRTOS Port    | `src/guest/rtos/freertos_integration.c` | FreeRTOS partition state management         |

---

## Validation Platforms

| Board                  | SoC     | CPU Topology               | Role                      |
| ---------------------- | ------- | -------------------------- | ------------------------- |
| **NXP i.MX95 Dev Kit** | i.MX95  | Cortex-A55 × 4 + Cortex-M7 | Primary - thesis evidence |
| **QEMU virt (arm64)**  | virtual | Cortex-A57 (emulated)      | CI gating - reproducible  |
| **NXP i.MX8QM-MEK**    | i.MX8QM | Cortex-A53 × 4 + Cortex-M4 | Secondary validation      |

---

## CI / CD

| Workflow            | Trigger      | Purpose                                                    |
| ------------------- | ------------ | ---------------------------------------------------------- |
| `ci.yml`            | push / PR    | Build (gcc + clang), style check, unit + integration tests |
| `nightly.yml`       | nightly      | Extended test matrix                                       |
| `benchmark.yml`     | push to main | Baseline performance capture                               |
| `cross-os.yml`      | push / PR    | QEMU smoke test on ubuntu/macos                            |
| `evidence-pack.yml` | manual       | Package thesis evidence archive                            |
| `vercel.yml`        | push to main | Build and deploy documentation portal                      |

---

## Thesis Context

**Research questions:**
1. Can stage-2 page tables and SMMU stream policy enforce hard spatial isolation between Linux and FreeRTOS on a real AMP SoC under deliberate violation attempts?
2. Can a budget-based EL2 scheduler bound RTOS latency under worst-case Linux load on shared cores?

**Chapter-to-artifact traceability:** [`docs/methodology/CHAPTER_TRACEABILITY.md`](docs/methodology/CHAPTER_TRACEABILITY.md)  
**Evaluation plan:** [`docs/methodology/EVALUATION_PLAN.md`](docs/methodology/EVALUATION_PLAN.md)  
**One-year roadmap:** [`docs/roadmap/ONE_YEAR_RELEASE_PLAN.md`](docs/roadmap/ONE_YEAR_RELEASE_PLAN.md)

---

## Governance

| Document                                                     | Purpose                             |
| ------------------------------------------------------------ | ----------------------------------- |
| [`CONTRIBUTING.md`](CONTRIBUTING.md)                         | Contribution workflow and DCO       |
| [`SECURITY.md`](SECURITY.md)                                 | Vulnerability reporting             |
| [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)                   | Community standards                 |
| [`docs/safety/THREAT_MODEL.md`](docs/safety/THREAT_MODEL.md) | Adversary model and security claims |
| [`docs/safety/ASSUMPTIONS.md`](docs/safety/ASSUMPTIONS.md)   | Hardware and platform assumptions   |
| [`AGENTS.md`](AGENTS.md)                                     | AI agent roles and policies         |

---

## License

[Apache License 2.0](LICENSE) - see the file for full terms.
