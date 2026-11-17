/*
 * crt0__root.s: hand-written assembly, retail (US v1.01) 0x001000c0-0x001000c0.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * crt0 (SCE crt0.s startup). Root-thread exit stub: calls ExitThread (EE syscall 0x23); _start passes its address to SetupThread.
 * _start (0x100008) loads 0x1000c0 into t0 (lui/addiu) before syscall 0x3c (SetupThread), so this is
 * the root-thread return address; SCE crt0.s names the same two instructions _root.
 */
/* 0x001000c0 */
	.set	noreorder
	.text
	.globl	_root
	.ent	_root
_root:
	li	$3,35				# $3 = syscall number 0x23 (ExitThread)
	syscall
	.end	_root
