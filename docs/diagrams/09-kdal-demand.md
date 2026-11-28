# How does KDAL's demand reach the check without growing the TCB?

Linux with KDAL's module in partition A, as the campaign's scenario 2 and
the CI job "Linux in partition A" run it. KDAL never calls HAVEN: its
demand leaves the guest as text on the serial console and is checked on the
host. Sources: `tools/campaign/linux2.py` (M5, M6),
`tools/readback/demand_check.py`, `docs/methodology/stage2/20261108-multi-vcpu/hotplug-init.c`,
KDAL's `src/core/demand.c` and `src/backends/generic/devicetree.c`.

```mermaid
sequenceDiagram
  participant INIT as /init (EL0)
  participant KD as KDAL module (EL1)
  participant HV as HAVEN (EL2)
  participant UART as Serial console
  participant L2 as linux2.py (host)
  participant DC as demand_check.py (host)
  participant RB as readback.py (host)
  INIT->>INIT: CPU hotplug of vCPU 1
  INIT->>KD: insmod /kdal.ko
  KD->>KD: scan the device tree for "kdal,device" nodes
  KD->>KD: demand per device: device-regions, interrupts,<br/>stream-ids (FF-A vocabulary)
  INIT->>KD: read /sys/kernel/debug/kdal/demand
  KD-->>INIT: JSON, "kdal-demand/1"
  INIT->>UART: KDAL-DEMAND-BEGIN, the JSON, KDAL-DEMAND-END
  Note over KD,HV: No hypercall: every step above runs inside what<br/>HAVEN already granted, and HAVEN sees only UART writes.
  UART-->>L2: UART log
  L2->>L2: cut the block out (drop kernel lines another vCPU wrote into it)
  L2->>DC: demand against the grant, partition 1
  DC-->>L2: findings: unmet MMIO, access, interrupt or DMA demand
  L2->>RB: read back the enforced state at KDAL-DEMAND-END
  RB-->>L2: findings: stage-2, GIC, SMMU drift
```

- Observed on every HAVEN pull request that changes code (2 and 3 vCPUs).
- The unchanged setup has a known, unmet demand that the campaign uses as
  its baseline: KDAL's GPIO device at 0x0903_0000 and its INTID 39 are not
  granted to partition 1. A finding is a demand-check result that differs
  from that baseline.
