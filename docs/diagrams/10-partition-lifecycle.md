# What states can a partition be in?

Partitions are fixed at build time; there is no run-time creation. Sources:
`src/core/partition.c`, `src/core/exc/el2_exceptions.c`,
`src/core/exc/psci.c`.

```mermaid
stateDiagram-v2
  [*] --> Configured
  Configured: Configured<br/>regions, IRQs, budget in partition.c (build time)
  Mapped: Mapped<br/>stage-2 regions checked and written
  Halted: Boot halted<br/>hv_panic: HAVEN fails closed
  Inactive: Not started<br/>mapped, but no CPU (qemu-direct)
  Enabled: Stage-2 enabled (VMID)
  Running: Running at EL1
  Stopped: Stopped<br/>CPU parked at EL2
  Off: Off<br/>guest PSCI SYSTEM_OFF or CPU_OFF
  Torn: Torn down<br/>(proposed)

  Configured --> Mapped: map regions
  Configured --> Halted: invalid grant, overlap
  Mapped --> Halted: IRQ or budget setup fails
  Mapped --> Enabled: partition A (CPU 0)
  Mapped --> Enabled: partition B, CPU_ON succeeds
  Mapped --> Inactive: partition B, no PSCI provider
  Enabled --> Running: ERET
  Running --> Running: denied data access<br/>logged, skipped
  Running --> Stopped: stage-2 instruction abort
  Running --> Off: PSCI from the guest
  Running --> Torn: operator teardown
  Stopped --> Torn: operator teardown
  Torn --> [*]

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class Configured,Mapped,Halted,Inactive,Enabled,Running,Stopped,Off observed
  class Torn proposed
```

- Observed: partition A keeps running after a denied read of partition B's
  memory; a stage-2 instruction abort stops only its own CPU; a Linux guest
  powers off its partition with PSCI SYSTEM_OFF at the end of each CI run.
- Teardown and restart of a partition are not built
  (`hv_stage2_unmap_partition` is used only to undo a failed boot).
