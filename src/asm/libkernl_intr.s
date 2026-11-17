/*
 * libkernl_intr.s: hand-written assembly, retail (US v1.01) 0x001fdc90-0x001fdcd8.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. Disables interrupts: returns whether Status.EIE was set, looping on di / sync.p until it reads back clear.
 * Returns 1 if interrupts were enabled (and are now disabled), else 0. Called before kernel calls that
 * must not be interrupted (0x1f5ff0, 0x1f61c8 ... and CRI 0x1e1980-0x1e1e78).
 */
/* 0x001fdc90 */
	.set	noreorder
	.text
	.globl	DIntr
	.ent	DIntr
DIntr:
	# $3 = Status.EIE (bit 16), $4 = was it set
	mfc0	$3,$12
	lui	$2,0x1
	and	$3,$3,$2
	beqz	$3,.LDIntr_off
	sltu	$4,$0,$3
	nop
.LDIntr_disable:
	di
	sync.p
	mfc0	$2,$12
	lui	$3,0x1
	and	$2,$2,$3
	bnez	$2,.LDIntr_disable
	nop
	jr	$31
	move	$2,$4
.LDIntr_off:
	move	$4,$0
	jr	$31
	move	$2,$4
	.end	DIntr

/*
 * Sony libkernel. Enables interrupts (ei); returns whether Status.EIE was set before.
 * Returns 1 if interrupts were already enabled, else 0. Pairs with 0x1fd650.
 */
/* 0x001fdcd8 */
	.set	noreorder
	.text
	.globl	EIntr
	.ent	EIntr
EIntr:
	# $2 = Status.EIE (bit 16) before the ei
	mfc0	$2,$12
	lui	$3,0x1
	and	$2,$2,$3
	ei
	jr	$31
	sltu	$2,$0,$2
	.end	EIntr
