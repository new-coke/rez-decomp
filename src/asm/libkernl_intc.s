/*
 * libkernl_intc.s: hand-written assembly, retail (US v1.01) 0x001f6808-0x001f6a08.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. DisableIntc wrapper: calls the _DisableIntc syscall with interrupts disabled and a sync afterwards.
 * Argument: $4 = interrupt cause number. Returns _DisableIntc's result. Called from CRI code (0x1de830,
 * 0x1df1c8).
 */
/* 0x001f6808 */
	.set	noreorder
	.text
	.globl	DisableIntc
	.ent	DisableIntc
DisableIntc:
	addiu	$sp,$sp,-48
	sd	$17,16($sp)
	sd	$31,32($sp)
	move	$17,$4
	sd	$16,0($sp)
	# $16 = Status.EIE (bit 16)
	mfc0	$16,$12
	lui	$2,0x1
	and	$16,$16,$2
	beqz	$16,.LDisableIntc_call
	nop
	jal	DIntr
	nop
.LDisableIntc_call:
	jal	_DisableIntc
	move	$4,$17
	move	$17,$2
	sync
	beqz	$16,.LDisableIntc_done
	move	$2,$17
	jal	EIntr
	nop
	move	$2,$17
.LDisableIntc_done:
	ld	$31,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,48
	.end	DisableIntc

/*
 * Sony libkernel. EnableIntc wrapper: calls the _EnableIntc syscall with interrupts disabled and a sync afterwards.
 * Argument: $4 = interrupt cause number. Returns _EnableIntc's result. Called by render_init_all, render_init
 * and 0x1df1c8.
 */
/* 0x001f6870 */
	.set	noreorder
	.text
	.globl	EnableIntc
	.ent	EnableIntc
EnableIntc:
	addiu	$sp,$sp,-48
	sd	$17,16($sp)
	sd	$31,32($sp)
	move	$17,$4
	sd	$16,0($sp)
	# $16 = Status.EIE (bit 16)
	mfc0	$16,$12
	lui	$2,0x1
	and	$16,$16,$2
	beqz	$16,.LEnableIntc_call
	nop
	jal	DIntr
	nop
.LEnableIntc_call:
	jal	_EnableIntc
	move	$4,$17
	move	$17,$2
	sync
	beqz	$16,.LEnableIntc_done
	move	$2,$17
	jal	EIntr
	nop
	move	$2,$17
.LEnableIntc_done:
	ld	$31,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,48
	.end	EnableIntc

/*
 * Sony libkernel. DisableDmac wrapper: calls the _DisableDmac syscall with interrupts disabled and a sync afterwards.
 * Argument: $4 = DMA channel. Returns _DisableDmac's result. Called from 0x1f7a18.
 */
/* 0x001f68d8 */
	.set	noreorder
	.text
	.globl	DisableDmac
	.ent	DisableDmac
DisableDmac:
	addiu	$sp,$sp,-48
	sd	$17,16($sp)
	sd	$31,32($sp)
	move	$17,$4
	sd	$16,0($sp)
	# $16 = Status.EIE (bit 16)
	mfc0	$16,$12
	lui	$2,0x1
	and	$16,$16,$2
	beqz	$16,.LDisableDmac_call
	nop
	jal	DIntr
	nop
.LDisableDmac_call:
	jal	_DisableDmac
	move	$4,$17
	move	$17,$2
	sync
	beqz	$16,.LDisableDmac_done
	move	$2,$17
	jal	EIntr
	nop
	move	$2,$17
.LDisableDmac_done:
	ld	$31,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,48
	.end	DisableDmac

/*
 * Sony libkernel. EnableDmac wrapper: calls the _EnableDmac syscall with interrupts disabled and a sync afterwards.
 * Argument: $4 = DMA channel. Returns _EnableDmac's result. Called from 0x1f7798 (the SIF command setup
 * that installs 0x1f7c90).
 */
/* 0x001f6940 */
	.set	noreorder
	.text
	.globl	EnableDmac
	.ent	EnableDmac
EnableDmac:
	addiu	$sp,$sp,-48
	sd	$17,16($sp)
	sd	$31,32($sp)
	move	$17,$4
	sd	$16,0($sp)
	# $16 = Status.EIE (bit 16)
	mfc0	$16,$12
	lui	$2,0x1
	and	$16,$16,$2
	beqz	$16,.LEnableDmac_call
	nop
	jal	DIntr
	nop
.LEnableDmac_call:
	jal	_EnableDmac
	move	$4,$17
	move	$17,$2
	sync
	beqz	$16,.LEnableDmac_done
	move	$2,$17
	jal	EIntr
	nop
	move	$2,$17
.LEnableDmac_done:
	ld	$31,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,48
	.end	EnableDmac

/*
 * Sony libkernel. iEnableIntc wrapper: calls the _iEnableIntc syscall and syncs (unreferenced).
 * Argument: $4 = interrupt cause number. Returns the syscall's result. No code in the executable calls it.
 */
/* 0x001f69a8 */
	.set	noreorder
	.text
	.globl	iEnableIntc
	.ent	iEnableIntc
iEnableIntc:
	addiu	$sp,$sp,-16
	sd	$31,0($sp)
	jal	_iEnableIntc
	nop
	sync
	ld	$31,0($sp)
	jr	$31
	addiu	$sp,$sp,16
	.end	iEnableIntc

/*
 * Sony libkernel. iDisableIntc wrapper: calls the _iDisableIntc syscall and syncs (unreferenced).
 * Argument: $4 = interrupt cause number. Returns the syscall's result. No code in the executable calls it.
 */
/* 0x001f69c8 */
	.set	noreorder
	.text
	.globl	iDisableIntc
	.ent	iDisableIntc
iDisableIntc:
	addiu	$sp,$sp,-16
	sd	$31,0($sp)
	jal	_iDisableIntc
	nop
	sync
	ld	$31,0($sp)
	jr	$31
	addiu	$sp,$sp,16
	.end	iDisableIntc

/*
 * Sony libkernel. iEnableDmac wrapper: calls the _iEnableDmac syscall and syncs (unreferenced).
 * Argument: $4 = DMA channel. Returns the syscall's result. No code in the executable calls it.
 */
/* 0x001f69e8 */
	.set	noreorder
	.text
	.globl	iEnableDmac
	.ent	iEnableDmac
iEnableDmac:
	addiu	$sp,$sp,-16
	sd	$31,0($sp)
	jal	_iEnableDmac
	nop
	sync
	ld	$31,0($sp)
	jr	$31
	addiu	$sp,$sp,16
	.end	iEnableDmac

/*
 * Sony libkernel. iDisableDmac wrapper: calls the _iDisableDmac syscall and syncs (unreferenced).
 * Argument: $4 = DMA channel. Returns the syscall's result. No code in the executable calls it.
 */
/* 0x001f6a08 */
	.set	noreorder
	.text
	.globl	iDisableDmac
	.ent	iDisableDmac
iDisableDmac:
	addiu	$sp,$sp,-16
	sd	$31,0($sp)
	jal	_iDisableDmac
	nop
	sync
	ld	$31,0($sp)
	jr	$31
	addiu	$sp,$sp,16
	.end	iDisableDmac
