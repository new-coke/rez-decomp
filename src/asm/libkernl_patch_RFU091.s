/*
 * libkernl_patch_RFU091.s: hand-written assembly, retail (US v1.01) 0x001f6a80-0x001f6a80.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5b (RFU091): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6a80 */
	.set	noreorder
	.text
	.globl	RFU091
	.ent	RFU091
RFU091:
	li	$3,91				# $3 = syscall number 0x5b
	syscall
	jr	$31
	nop
	.end	RFU091
