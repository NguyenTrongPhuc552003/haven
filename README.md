# Haven

**Hypervisor for Asymmetric Virtualization ENforcement and isolation**

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.6.2-green.svg)](VERSION)
[![Platform](https://img.shields.io/badge/platform-ARM64%20AArch64-lightgrey.svg)](configs/arm64/)

> Master's thesis artifact, Computer Engineering (UIT). Defense planned
> around mid-2028.

Haven is a small static-partitioning hypervisor that runs at EL2 on ARM64.
Each partition gets its own CPUs, memory, interrupts and DMA-capable devices,
fixed at build time. The thesis studies whether the hardware really enforces
what Haven grants: an external **read-back** tool reads the enforced state
(stage-2 tables, SMMUv3 stream tables, GICv3 routing and priorities) from
outside Haven and compares it with the grant.

**Target SoC: CIX P1 (Sky1) only.** Twelve Cortex-A720 and A520 cores, an
Immortalis-G720 GPU, an NPU, and two SMMUv3 instances (PCIe and multimedia).
Thesis board: Orange Pi 6 Plus (planned, end of 2026); the Orange Pi 6 and
Radxa Orion O6 / O6N are reference variants. All current evidence comes from
QEMU; no board result exists yet.

## Working research question

Can independent read-back of enforced hardware state (stage-2, SMMU, GIC)
find enforcement drift that policy tests and configuration checks cannot,
without growing Haven's trusted code?

## What works today (QEMU `virt`, observed)

| Area | State | Records |
| - | - | - |
| Stage-2 memory isolation | Per-partition tables with permissions and memory types; cross-partition access denied and logged | `docs/methodology/stage1/` |
| DMA isolation | SMMUv3 with one stage-2 table per granted stream (own VMID); real virtio DMA allowed inside its window, blocked outside; blocked DMA logged from the event queue | `stage2/20261102-g2-dma/`, `stage2/20261102-dma-hardening/` |
| Interrupts | SPIs and per-CPU PPIs owned per partition; delivered as hardware-linked virtual interrupts; undeliverable interrupts disabled | `stage2/20261102-irq-delivery/`, `stage2/20261102-partition-b/` |
| Two partitions, two CPUs | Partition A on CPU 0; partition B on CPU 2, started through PSCI after a ready handshake, with its own virtual timer | `stage2/20261102-partition-b/`, `stage2/20261102-secondary-cpu/` |
| CPU time budgets | Enforced per CPU with the EL2 timer (A 8 ms, B 2 ms per 10 ms); partitions measure their own duty cycle | `stage2/20261102-budgets/` |
| Haven's own memory | EL2 stage-1 identity map: Haven's image cacheable, MMIO device, partition RAM unmapped; read back | `stage2/20261102-el2-memory/` |
| Read-back v6 | Stage-2, SMMU, GIC (including Haven's own timer interrupt) and Haven's EL2 map compared with a grant; seeded-fault case sets and randomly drawn held-out faults | `tools/readback/`, `stage2/20261102-a1-cases-v3/`, `stage2/20261102-readback-v4/` |
| Interference pipeline | Both demo guests cacheable; partition B measures timer latency, partition A runs CPU or memory load; checked in QEMU (numbers host-bound) | `stage2/20261102-interference/` |

**Not done yet:** interference measurements (they need the board, see
`docs/methodology/stage2/20261102-interference/PLAN.md`);
no Linux guest; no board bring-up. Every known defect and its status is
in [`FINDINGS.md`](docs/methodology/stage0/20261025-corrections/FINDINGS.md).

## Try it without a cross compiler

`scripts/release/make-try-bundle.sh` builds a small archive (about 70 KB)
with prebuilt HAVEN images, the read-back tool and `try.sh`. Unpacked
anywhere, `./try.sh` needs only `qemu-system-aarch64`, `python3` and `dtc`:
it boots both partitions, reads back the enforced state, then boots a
deliberately faulty image that passes the boot check and is caught only
by read-back. Download it from the
[try-20261103 release](https://github.com/NguyenTrongPhuc552003/haven/releases/tag/try-20261103)
or build it yourself; see `scripts/release/try/README.md`.

## Quick start

Prerequisites: `cmake` 3.22 or later, `ninja`, a host C compiler, `python3`;
for ARM64 builds an `aarch64-linux-gnu` (or `aarch64-unknown-linux-gnu`)
toolchain and `qemu-system-aarch64` (9.2 or later for SMMU stage 2).

```sh
git clone https://github.com/NguyenTrongPhuc552003/haven.git
cd haven

# Host unit and integration tests
cmake --preset host-tests && cmake --build build-host
ctest --test-dir build-host --output-on-failure -E bench_temporal_isolation

# ARM64, partition A only (QEMU provides no PSCI in this profile)
cmake --preset arm64-qemu -DHAVEN_BOOT_PROFILE=qemu-direct && cmake --build build
./scripts/qemu/qemu-direct-check.sh build cortex-a72

# ARM64, both partitions (QEMU's built-in PSCI starts CPU 2)
cmake --preset arm64-qemu -B build-psci -DHAVEN_BOOT_PROFILE=qemu-psci
cmake --build build-psci
python3 scripts/qemu/partition-b-check.py build-psci cortex-a72

# Read back the enforced state and compare it with a grant
python3 tools/readback/readback.py build-psci \
    docs/methodology/stage2/20261102-readback-v4/grant-partb-v4.json /tmp/rb \
    --device virtio-rng-pci,disable-legacy=on,iommu_platform=on --marker "IRQ test done"
```

`bench_temporal_isolation` is excluded because of a known, undiagnosed
timeout. Boot profiles (`HAVEN_BOOT_PROFILE`, qemu-virt only): `qemu-direct`
(QEMU loads Haven at EL2, no PSCI), `qemu-psci` (QEMU answers PSCI over
SMC), `qemu-tfa` (TF-A provides PSCI at EL3).
[ELMOS](https://github.com/NguyenTrongPhuc552003/elmos) wraps these steps
on macOS (`task haven:all`).

## Repository layout

```
arch/arm64/        EL2 entry, vectors, stage-2, GIC virtual interface, timer
src/core/          portable policy: memory regions, IRQ ownership, DMA grants, budgets, partitions
src/platform/      qemu-virt (current); imx95-devkit, imx8qm-mek (legacy, not maintained)
drivers/           GICv3, SMMUv3, PCI ECAM
tests/             host unit, integration and isolation tests; demo guests A and B
tools/readback/    external read-back of enforced state (QEMU)
tools/fwscan/      IORT scanner for CIX UEFI images
scripts/           build, test and QEMU checks
docs/              see docs/README.md
verification/      Coq and Isabelle models from an earlier project phase (not current evidence)
```

## Documentation

- Website: [haven-docs-five.vercel.app](https://haven-docs-five.vercel.app/)
  (source: [haven-docs](https://github.com/NguyenTrongPhuc552003/haven-docs)),
  with the overview, guides and these records rendered.
- [`docs/README.md`](docs/README.md): which documents are current and which
  are kept only for history.
- [`docs/porting/cix-p1-ledger.md`](docs/porting/cix-p1-ledger.md): CIX P1
  platform facts and weaknesses (IORT, SMMU, GPU, boards).
- [`docs/architecture/gpu-partition.md`](docs/architecture/gpu-partition.md):
  GPU partition design and board experiments.
- `docs/methodology/stage0` to `stage2`: dated experiment records, each with
  its changes, expectations, raw runs and limits.

## Platforms

| Platform | Role |
| - | - |
| QEMU `virt` (cortex-a72, max) | All current evidence; CI on every push |
| CIX P1: Orange Pi 6 Plus | Thesis board (planned purchase, end of 2026) |
| CIX P1: Orange Pi 6, Radxa Orion O6 / O6N | Reference variants (schematics, firmware) |
| NXP i.MX95, i.MX8QM | Legacy from an earlier plan: code and presets kept, not maintained or tested |

## Related repositories

- [ELMOS](https://github.com/NguyenTrongPhuc552003/elmos): macOS toolkit
  that builds and runs Haven and Linux images.
- [KDAL](https://github.com/NguyenTrongPhuc552003/kdal): device abstraction
  layer for the Linux partition.

## Contributing and license

See [`CONTRIBUTING.md`](CONTRIBUTING.md), [`CODING_STYLE.md`](CODING_STYLE.md)
and [`SECURITY.md`](SECURITY.md). Licensed under the
[Apache License 2.0](LICENSE).
