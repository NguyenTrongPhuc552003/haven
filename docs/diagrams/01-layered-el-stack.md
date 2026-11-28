# What runs where, and what is trusted?

Armv8/v9 exception levels in the QEMU setup. Sources: `src/core/`,
`arch/arm64/`, `src/core/partition.c`, `src/platform/qemu-virt/memory.h`,
`tools/readback/README.md`, `tools/el3-readback/README.md`,
`.github/workflows/ci.yml` (jobs "Linux in partition A" and "Partition B
under TF-A").

```mermaid
flowchart TB
  subgraph EL0["EL0: guest user space (untrusted)"]
    INIT["/init of the CI runs<br/>hotplug, insmod kdal.ko, print the demand"]
    APP["KDAL client application<br/>(proposed)"]
  end
  subgraph EL1["EL1: guest kernels (untrusted)"]
    LNX["Partition A: Linux, 2 or 3 vCPUs<br/>or the bare-metal test guest"]
    KDAL["KDAL module in Linux<br/>reports its demand"]
    PB["Partition B: bare-metal guest on CPU 2"]
  end
  subgraph EL2["EL2: HAVEN (the trusted computing base)"]
    ADM["Admission<br/>mm/stage2.c, irq/ownership.c,<br/>dma/smmu.c, sched/budget.c"]
    ARCH["Arch binding<br/>arch/arm64, drivers/irqchip, drivers/iommu"]
    EXC["Exceptions<br/>stage-2 aborts, MMIO emulation (vGIC),<br/>PSCI for guests, system register traps"]
    IRQ["Interrupt delivery<br/>list registers, maintenance interrupt"]
  end
  subgraph EL3["EL3: firmware (qemu-tfa profile only)"]
    TFA["TF-A BL31<br/>PSCI; EL3 read-back dump (patch)"]
  end
  subgraph HW["Hardware HAVEN programs"]
    S2["Stage-2 MMU"]
    GIC["GICv3"]
    SMMU["SMMUv3"]
    TMR["Generic timer"]
  end
  RB["read-back, outside HAVEN<br/>QEMU monitor or the EL3 dump"]

  APP -.->|ioctl| KDAL
  EL1 -->|stage-2 abort, HVC, trapped SMC| EXC
  ADM --> ARCH
  ARCH -->|programs| HW
  ARCH -->|SMC PSCI CPU_ON| TFA
  HW -.->|enforced state| RB
  TFA -.->|dump on UART1| RB

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class INIT,LNX,KDAL,PB,ADM,ARCH,EXC,IRQ,TFA,RB observed
  class APP proposed
```

- Everything at EL1 and EL0 is untrusted, including what KDAL says about
  its own needs. HAVEN at EL2 is the only trusted part; read-back is a host
  tool and never runs inside HAVEN.
- Without TF-A (profiles qemu-direct and qemu-psci) HAVEN is entered at EL2
  directly and there is no EL3 software.
- No KDAL client application runs in HAVEN's partitions yet.
