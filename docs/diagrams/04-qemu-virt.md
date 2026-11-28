# What exactly runs in QEMU?

The QEMU `virt` evaluator: boot profiles, load addresses and the partition
memory map. Sources: `CMakeLists.txt` (boot profiles),
`src/platform/qemu-virt/memory.h`, `scripts/qemu/qemu-direct-check.sh`,
`scripts/qemu/partition-b-check.py`, `tools/readback/README.md`,
`tools/el3-readback/README.md`.

```mermaid
flowchart TB
  subgraph HOST["Host: CI runner (Ubuntu) or a Mac"]
    RUN["Run scripts<br/>qemu-direct-check.sh, partition-b-check.py,<br/>campaign run_case.py"]
    RB["readback.py<br/>stops QEMU, reads memory and<br/>registers through QMP"]
    LOG[("UART logs<br/>and verdicts")]
    subgraph QEMU["qemu-system-aarch64 -M virt,virtualization=on,gic-version=3,iommu=smmuv3<br/>-cpu cortex-a72 or max, -smp 4, -m 2G"]
      HB["haven.bin at PA 0x8000_0000<br/>8 MiB, with its stage-2 table pool"]
      GA["Partition A at PA 0x8080_0000<br/>IPA 0x4000_0000, 512 MiB"]
      GB["Partition B at PA 0xA080_0000<br/>IPA 0x6000_0000, 64 MiB, CPU 2"]
      UART["PL011 UART 0x0900_0000<br/>shared by HAVEN's log and partition A"]
      DMA["virtio-rng devices (read-back runs)<br/>DMA through the SMMUv3"]
    end
  end
  subgraph PROFILES["Boot profiles (HAVEN_BOOT_PROFILE)"]
    PD["qemu-direct: entered at EL2,<br/>no PSCI, partition B not started"]
    PP["qemu-psci: entered at EL2,<br/>QEMU answers PSCI over SMC"]
    PT["qemu-tfa: TF-A v2.12.0 at EL3,<br/>secure=on, EL3 read-back dump"]
  end

  RUN --> QEMU
  QEMU --> LOG
  RB -->|QMP| QEMU
  RB --> LOG
  HB -->|stage-2, VMID 1| GA
  HB -->|stage-2, VMID 2, after CPU_ON| GB
  PROFILES -.- HB

  classDef observed stroke:#0ca30c,stroke-width:2px
  class RUN,RB,LOG,HB,GA,GB,UART,DMA,PD,PP,PT observed
```

- Linux in partition A uses the same layout, the qemu-psci profile and a
  device tree; its CI job pins QEMU 8.2.2, whose SMMU has no stage 2, so
  the DMA grant is withheld there.
- QEMU's TCG models function, not timing or caches: QEMU results never
  stand in for a board.
