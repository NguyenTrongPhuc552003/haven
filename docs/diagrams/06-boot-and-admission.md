# How does HAVEN reach running, isolated partitions?

Boot on CPU 0 of the QEMU `virt` machine, qemu-psci profile. Everything
shown is observed (every boot check in CI prints these steps). Sources:
`arch/arm64/boot.S`, `src/core/init.c` (`haven_init`),
`src/core/partition.c` (`partitions_launch`), `src/platform/qemu-virt/memory.h`.

```mermaid
sequenceDiagram
  participant LD as QEMU loader
  participant BOOT as boot.S (EL2)
  participant INIT as haven_init<br/>src/core/init.c
  participant POL as Admission<br/>stage2.c, ownership.c, budget.c
  participant HW as Stage-2 MMU, GICv3,<br/>SMMUv3, timer
  participant PA as Partition A (EL1)
  participant PB as Partition B (EL1, CPU 2)
  LD->>BOOT: enter at EL2, PA 0x8000_0000
  BOOT->>INIT: haven_init()
  INIT->>INIT: CPU init, banner, check boot CPU is entry 0
  INIT->>HW: build and enable HAVEN's EL2 stage-1 map
  INIT->>INIT: check hardware features
  INIT->>HW: timer, GICv3, SMMUv3 init (panic on failure)
  INIT->>POL: stage-2, IRQ ownership, budget, SMMU, IOMMU, timer, exceptions init
  INIT->>POL: partitions_launch()
  POL->>HW: map partition A: attributes, alignment,<br/>overlap and PA exclusivity checked
  POL->>HW: map partition B (unmap A and panic on failure)
  POL->>HW: DMA grants: SMMU stream table entries
  POL->>HW: assign IRQs: SPI 33 to A on CPU 0, PPI 27 to B on CPU 2
  POL->>POL: budgets A 8 of 10 ms, B 2 of 10 ms
  POL->>HW: enable stage-2 for A (VMID 1)
  POL->>PB: PSCI CPU_ON for CPU 2 (see 08), B starts (VMID 2)
  POL->>PA: ERET to IPA 0x4000_0000 at EL1
  Note over INIT,POL: Any invalid grant stops the boot before a partition runs<br/>(hv_panic): HAVEN fails closed.
```

- Read-back takes no part in this sequence: it runs from the host after
  boot and reads what these steps wrote (diagram 01).
- With a Linux guest, HAVEN also sets up the emulated GIC for partition A
  and passes the device tree's IPA in x0.
