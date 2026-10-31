/* Diagnostic EL1 guest: one virtual IRQ, then bounded 100ms counter polling. */
.section ".text.entry", "ax"
.global _guest_b_start
_guest_b_start:
 adr x0, guest_vectors
 msr vbar_el1,x0
 mov x0,#1
 msr icc_sre_el1,x0
 isb
 mov x20,#0
 mov x21,#0
 mrs x23,cntfrq_el0
 mov x0,#10
 udiv x23,x23,x0
 mrs x22,cntpct_el0
 msr daifclr,#2
1:
 mrs x0,cntpct_el0
 sub x0,x0,x22
 cmp x0,x23
 b.lo 1b
 msr daifset,#2
 mov x1,x20
 mov x2,x21
 hvc #0
2: wfi
 b 2b
.balign 2048
guest_vectors:
.rept 4
 .balign 128
 b guest_bad
.endr
.balign 128
 b guest_bad
.balign 128
 b guest_irq
.rept 10
 .balign 128
 b guest_bad
.endr
guest_irq:
 mrs x21,icc_iar1_el1
 add x20,x20,#1
 msr icc_eoir1_el1,x21
 isb
 eret
guest_bad:
 mov x1,#0xffff
 mov x2,#0xffff
 hvc #0
 b guest_bad
