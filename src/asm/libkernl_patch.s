/*
 * libkernl_patch.s: hand-written assembly, retail (US v1.01) 0x001f6a28-0x001f6a38.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x74 (RFU116_SetSyscall): loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6a28 */
	.set	noreorder
	.text
	.globl	RFU116_SetSyscall
	.ent	RFU116_SetSyscall
RFU116_SetSyscall:
	li	$3,116				# $3 = syscall number 0x74
	syscall
	jr	$31
	nop
	.end	RFU116_SetSyscall

/*
 * Sony libkernel syscall stubs. Kernel call stub for syscall 0x5a: loads the number into $3 (v1), traps with syscall and returns.
 */
/* 0x001f6a38 */
	.set	noreorder
	.text
	.globl	RFU090
	.ent	RFU090
RFU090:
	li	$3,90				# $3 = syscall number 0x5a
	syscall
	jr	$31
	nop
	.end	RFU090
