/* Stage 0 diagnostic only. Not a production HAVEN initialization path. */
#include <asm/sysregs.h>
#include <haven/types.h>
#include <haven/el2_exceptions.h>
#include <haven/irq_ownership.h>
#include "drivers/irqchip/gic_v3.h"
#ifndef PROBE_MODE
#define PROBE_MODE 0
#endif
extern void hv_printk(const char *, ...);
extern void hv_arch_cpu_init(void), hv_arch_gic_el2_setup(void);
extern void hv_arch_timer_init(void), hv_arch_timer_cancel(void);
extern hv_status_t hv_arch_inject_virtual_irq(uint32_t,uint32_t);
static volatile unsigned count, last_irq, bad_owner;
static unsigned lr_count;
static uint64_t hz;
static uint32_t rd32(uintptr_t p) { return *(volatile uint32_t *)p; }
static void wr32(uintptr_t p,uint32_t v) { *(volatile uint32_t *)p=v; __asm__ volatile("dsb sy" ::: "memory"); }
static void timer_arm(void) {
 uint64_t now=read_sysreg(cntpct_el0);
 write_sysreg(now+(PROBE_MODE==6?hz*2:hz/100),cnthp_cval_el2);
 hv_printk("PROBE ARM now=%lu cval=%lu hz=%lu\n",now,read_sysreg(cnthp_cval_el2),hz);
 write_sysreg(PROBE_MODE==4?3:1,cnthp_ctl_el2); isb();
}
static hv_status_t irq_handler(hv_exc_context_t *ctx) {
 (void)ctx;
 uint32_t iar=gic_v3_ack_irq(), id=iar&0xffffff;
 if(id>=1020) return HV_OK;
 last_irq=id; count++;
 if(id==40) {
  if(hv_irq_is_owned_by(40,2)!=HV_OK || hv_irq_is_owned_by(40,1)!=HV_EPERM) bad_owner++;
  gic_v3_disable_irq(40);
 }
 if(id==26) hv_arch_timer_cancel();
 hv_printk("PROBE IRQ cpu=%lu id=%u count=%u ctl=%lx iar=%x\n",read_sysreg(mpidr_el1)&255,id,count,read_sysreg(cnthp_ctl_el2),iar);
 gic_v3_eoi_irq(iar);
 /* Explicit EOImode=0; EOI both priority-drops and deactivates. */
 if(id==26 && count==1 && PROBE_MODE==3) timer_arm();
 return HV_OK;
}
void stage0_prepare(void) {
 /* Wait for A's UART output; no claim about timer IRQ at this point. */
 uint64_t start=read_sysreg(cntpct_el0); hz=read_sysreg(cntfrq_el0);
 while(read_sysreg(cntpct_el0)-start<hz/20) __asm__ volatile("yield");
 hv_printk("PROBE ENTRY mode=%u cpu=%lu el=%lu hz=%lu\n",PROBE_MODE,read_sysreg(mpidr_el1)&255,read_sysreg(CurrentEL)>>2,hz);
 hv_arch_cpu_init();
 hv_arch_gic_el2_setup();
 hv_arch_timer_init();
 /* Enable EL1 physical-counter reads for the bounded diagnostic guest. */
 write_sysreg(3,cnthctl_el2); write_sysreg(0,cntvoff_el2);
 lr_count=(read_sysreg(ich_vtr_el2)&31)+1;
 /* LR0 alone is used, no scan beyond implemented registers. */
 write_sysreg(0,ich_lr0_el2);
 hv_printk("PROBE INIT hcr=%lx vtcr=%lx vbar=%lx sre=%lx ich=%lx vmcr=%lx lrs=%u pmr=%lx grp1=%lx\n",read_sysreg(hcr_el2),read_sysreg(vtcr_el2),read_sysreg(vbar_el2),read_sysreg(icc_sre_el2),read_sysreg(ich_hcr_el2),read_sysreg(ich_vmcr_el2),lr_count,read_sysreg(icc_pmr_el1),read_sysreg(icc_igrpen1_el1));
 /* Diagnostic physical CPU interface, absent from HAVEN's init path. */
 write_sysreg(0xff,icc_pmr_el1); write_sysreg(0,icc_bpr1_el1);
 write_sysreg(0,icc_ctlr_el1); write_sysreg(1,icc_igrpen1_el1); isb();
 hv_el2_register_handler(HV_EXC_IRQ,irq_handler);
 if(PROBE_MODE==0 || PROBE_MODE==1) {
  int rc=hv_arch_inject_virtual_irq(60,0x80);
  if(PROBE_MODE==1) write_sysreg(0,ich_hcr_el2); /* real disabled-interface control */
  hv_printk("PROBE VIRQ rc=%d lr0=%lx ich=%lx\n",rc,read_sysreg(ich_lr0_el2),read_sysreg(ich_hcr_el2));
 }
 if(PROBE_MODE==2) {
  volatile struct hv_irq_route good; good.irq_id=40; good.owner_partition_id=2; good.target_cpu=2;
  volatile struct hv_irq_route denied; denied.irq_id=40; denied.owner_partition_id=1; denied.target_cpu=0;
  int cfg=gic_v3_configure_irq(40,0x80,1), ok=hv_irq_assign((const struct hv_irq_route *)&good);
  uint64_t before=*(volatile uint64_t *)(0x08000000UL+0x6000+40*8);
  int reject=hv_irq_assign((const struct hv_irq_route *)&denied);
  uint64_t after=*(volatile uint64_t *)(0x08000000UL+0x6000+40*8);
  hv_printk("PROBE OWNER cfg=%d assign=%d denied=%d before=%lx after=%lx\n",cfg,ok,reject,before,after);
  if(cfg || ok || reject!=HV_EPERM || before!=after || after!=2) bad_owner++;
  wr32(0x08000000UL+0x204,1U<<8); /* software-pend synthetic physical SPI 40 */
 }
 if(PROBE_MODE>=3) {
  if(PROBE_MODE==5) gic_v3_enable_irq(26); /* unchanged API selects CPU0 */
  else wr32(0x080a0000UL+2*0x20000+0x10100,1U<<26);
  hv_printk("PROBE PPI cpu0=%x cpu2=%x\n",rd32(0x080a0000UL+0x10100),rd32(0x080a0000UL+2*0x20000+0x10100));
  timer_arm();
 }
 isb();
}
/* Called only for the diagnostic guest's HVC; preserve normal S2 handling. */
void stage0_report(uint64_t sp) {
 volatile uint64_t *f=(volatile uint64_t *)sp;
 uint64_t guest_count=f[1], guest_id=f[2];
 unsigned want=PROBE_MODE==2?1:PROBE_MODE==3?2:0;
 int good=(count==want && bad_owner==0);
 if(PROBE_MODE==0) good=good && guest_count==1 && guest_id==60;
 else good=good && guest_count==0;
 if(PROBE_MODE==2) good=good && last_irq==40;
 if(PROBE_MODE==3) good=good && last_irq==26;
 uint64_t ctl=read_sysreg(cnthp_ctl_el2), lr=read_sysreg(ich_lr0_el2);
 if(PROBE_MODE==0) good=good && (lr>>62)==0;
 if(PROBE_MODE==1) good=good && (lr>>62)==1;
 if(PROBE_MODE==4 || PROBE_MODE==5) good=good && (ctl&4);
 if(PROBE_MODE==6) good=good && !(ctl&4);
 hv_arch_timer_cancel();
 hv_printk("PROBE RESULT mode=%u expected_phys=%u phys=%u last=%u guest=%lu vintid=%lu owner_errors=%u ctl=%lx lr0=%lx active_spi=%x active_ppi=%x %s\n",PROBE_MODE,want,count,last_irq,guest_count,guest_id,bad_owner,ctl,lr,rd32(0x08000304),rd32(0x080a0000UL+2*0x20000+0x10300),good?"PASS":"FAIL");
 for(;;) __asm__ volatile("wfi");
}
