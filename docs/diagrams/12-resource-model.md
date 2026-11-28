# What data describes a resource, and what is compared?

HAVEN's own types (C, in the trusted code) and the files the host-side
checks compare (JSON). Sources: `include/haven/stage2.h`,
`include/haven/irq_ownership.h`, `include/haven/budget_sched.h`,
`include/haven/smmu.h`, `tools/readback/grant.py`,
`tools/readback/readback.py`, `tools/readback/demand_check.py`,
`tools/readback/dt_check.py`, KDAL's `src/core/demand.c`.

```mermaid
classDiagram
  namespace HAVEN_C_types {
    class hv_partition_mem {
      partition_id : u32
      regions : hv_mem_region[]
      region_count : u32
    }
    class hv_mem_region {
      ipa_base : u64
      pa_base : u64
      size : u64
      attrs : u64, HV_MEM bits
    }
    class hv_irq_route {
      irq_id : u32
      owner_partition_id : u32
      target_cpu : u32
    }
    class hv_budget {
      partition_id : u32
      period_ns : u64
      budget_ns : u64
    }
    class hv_device_dma_t {
      streamid : u16
      partition : u32
      access : RO, WO or RW
      dma_base : u64
      dma_size : u64
    }
  }
  namespace Host_side_files {
    class Grant {
      G, format 2 in FF-A vocabulary
      memory-regions, device-regions
      interrupts, stream-ids
      haven,pa  haven,cpus  haven,ppis
    }
    class Demand {
      D, "kdal-demand/1"
      device-regions, interrupts
      smmu-id, stream-ids
    }
    class GuestDeviceTree {
      T, the dts or dtb the guest boots with
    }
    class EnforcedState {
      H, read back from the hardware
      stage-2 tables, EL2 map
      GIC routing, enables, priorities
      SMMU stream table, HAVEN's ownership arrays
    }
  }
  hv_partition_mem "1" *-- "1..*" hv_mem_region
  Grant ..> hv_partition_mem : written to match
  Grant ..> hv_irq_route : written to match
  Grant ..> hv_device_dma_t : written to match
  Demand ..> Grant : demand_check.py, D within G
  GuestDeviceTree ..> Grant : dt_check.py, T within G
  EnforcedState ..> Grant : readback.py, H = G

```

- Every type and file shown is observed: in the code, and used in the CI
  runs and the campaign.
- One vocabulary for D and G: the property names of Arm's FF-A partition
  manifest binding. HAVEN's own additions carry a `haven,` prefix.
- A memory region with `stream-ids` is a DMA window only: it must lie
  inside a CPU-mapped region of the same partition, and read-back checks it
  on the SMMU side (a deliberate deviation from FF-A).
- Budgets are in the C types but not in the grant: read-back checks that
  the EL2 timer that enforces them is on (PPI 26, HAVEN's priority), not
  the budget values.
