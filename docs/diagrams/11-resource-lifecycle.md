# How does a resource move from demand to checked enforcement?

One resource (a memory or device region, an interrupt, a DMA stream) from
the driver's demand to enforcement that read-back has checked. The checks
on the way are the relations the research question tests. Sources:
`tools/readback/demand_check.py`, `dt_check.py`, `grant.py`,
`readback.py`, `src/core/partition.c`.

```mermaid
stateDiagram-v2
  [*] --> Demanded
  Demanded: Demanded (D)<br/>KDAL reports what a device needs (untrusted)
  Granted: Granted (G)<br/>in the grant the owner approves
  Built: Built in<br/>HAVEN's tables for the platform
  Described: Described (T)<br/>in the guest's device tree
  Enforced: Enforced (H)<br/>stage-2, GIC, SMMU written at boot
  Checked: Checked<br/>read-back finds H = G
  Finding: Finding<br/>reported with its class
  InUse: In use<br/>no EL2 call per operation
  Revoked: Revoked and quiesced<br/>(proposed)

  Demanded --> Granted: demand check, D within G
  Demanded --> Finding: unmet demand
  Granted --> Built: build
  Granted --> Described: guest device tree
  Described --> Finding: dt_check, T outside G
  Built --> Enforced: boot, admission checks
  Enforced --> Checked: read-back
  Enforced --> Finding: read-back, drift from G
  Checked --> InUse
  InUse --> Revoked: operator, no guest cooperation
  Revoked --> [*]

  classDef observed stroke:#0ca30c,stroke-width:2px
  classDef proposed stroke:#8e8e93,stroke-width:2px,stroke-dasharray:6 4
  class Demanded,Granted,Built,Described,Enforced,Checked,Finding,InUse observed
  class Revoked proposed
```

- The grant is written by hand and HAVEN's tables in `partition.c` and
  `memory.h` are written to match it; nothing generates one from the other
  (`tools/config-gen/` is reserved). That gap is one reason to read back
  what the hardware enforces.
- Revocation (unmap, retarget interrupts, abort the stream, invalidate
  TLBs, prove nothing is left) is not built.
