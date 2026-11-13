/* SPDX-License-Identifier: Apache-2.0 */
/* haven_handoff(entry, uart): the loader's last step (L1b step 8), after
 * ExitBootServices and the cache clean to the point of coherency.
 *
 * Masks DAIF, turns the EL2 MMU and data cache off (SCTLR_EL2.M and C), as
 * Linux v5.15 arch/arm64/kernel/efi-entry.S does, prints the jump line on the
 * PL011 at `uart` from the cleaned image, then branches to `entry` at EL2
 * with x0 to x3 = 0.
 *
 * Test-only builds (never for a board) show that HAVEN's entry checks refuse:
 *   SEED_MMU  leaves the MMU and data cache on;
 *   SEED_E2H  sets HCR_EL2.E2H (after the MMU is off) before the jump. */

	.text
	.global	haven_handoff
	.type	haven_handoff, %function
	.align	3
haven_handoff:
	msr	daifset, #0xf
	mov	x19, x0
	mov	x20, x1

#ifndef SEED_MMU
	mrs	x4, sctlr_el2
	bic	x4, x4, #(1 << 0)	/* M */
	bic	x4, x4, #(1 << 2)	/* C */
	msr	sctlr_el2, x4
	isb
	tlbi	alle2
	dsb	nsh
	isb
#endif

#ifdef SEED_E2H
	mrs	x4, hcr_el2
	orr	x4, x4, #(1 << 34)	/* E2H */
	msr	hcr_el2, x4
	isb
#endif

	adr	x5, jump_msg
1:	ldrb	w6, [x5], #1
	cbz	w6, 3f
2:	ldr	w7, [x20, #0x18]	/* FR */
	tbnz	w7, #5, 2b		/* TXFF */
	str	w6, [x20]		/* DR */
	b	1b

3:	mov	x0, xzr
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	br	x19
	.size	haven_handoff, . - haven_handoff

jump_msg:
	.asciz	"haven-boot: jump\r\n"
