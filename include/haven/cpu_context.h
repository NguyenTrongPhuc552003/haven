#ifndef HAVEN_CPU_CONTEXT_H
#define HAVEN_CPU_CONTEXT_H

#include <haven/types.h>

struct hv_cpu_context {
  hv_u64 x[30];
  hv_u64 x30;
  hv_u64 sp_el0;
  hv_u64 sp_el1;
  hv_u64 elr_el1;
  hv_u64 spsr_el1;
  hv_u64 sctlr_el1;
  hv_u64 ttbr0_el1;
  hv_u64 ttbr1_el1;
  hv_u64 tcr_el1;
  hv_u64 mair_el1;
  hv_u64 amair_el1;
  hv_u64 vbar_el1;
  hv_u64 esr_el1;
  hv_u64 far_el1;
  hv_u64 afsr0_el1;
  hv_u64 afsr1_el1;
  hv_u64 tpidr_el0;
  hv_u64 tpidr_el1;
  hv_u64 tpidrro_el0;
  hv_u32 vmid;
  hv_u32 reserved;
};

#endif
