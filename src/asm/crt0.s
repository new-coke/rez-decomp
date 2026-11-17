/*
 * crt0.s: hand-written assembly, retail (US v1.01) 0x00100008-0x00100008.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * crt0 (SCE crt0.s startup).
 * _start: clears .bss (0x2c6000 up to _end 0x1d72220) a quadword at a time, sets up the root thread
 * (syscall 0x3c SetupThread: gp, stack -1 = default, stack size 0x80000, the argument block at
 * 0x2c6200 and _root as the return address) and moves sp to the stack it returns, sets up the heap
 * after _end (syscall 0x3d SetupHeap, size -1 = the rest of memory), runs libkernl_startup, flushes the
 * data cache (FlushCache(0)), enables interrupts and calls main(argc, argv) from the argument block,
 * then leaves through _Exit with main's result.
 * $8 = t0, $28 = gp, $29 = sp.
 */
/* 0x00100008 */
	.set	noreorder
	.set	noat
	.text
	.globl	_start
	.ent	_start
_start:
	lui	$2,%hi(iop_rpc_sema)		# .bss start
	lui	$3,%hi(crt0_bss_end)		# _end
	addiu	$2,$2,%lo(iop_rpc_sema)
	addiu	$3,$3,%lo(crt0_bss_end)
1:
	sq	$0,0($2)
	nop
	sltu	$1,$2,$3
	nop
	nop
	bne	$1,$0,1b
	addiu	$2,$2,16

	lui	$4,%hi(_gp)
	lui	$5,0			# _stack = -1 (default): la of a linker symbol
	lui	$6,%hi(0x80000)			# _stack_size
	lui	$7,%hi(crt0_args)		# _args
	lui	$8,%hi(_root)
	addiu	$4,$4,%lo(_gp)
	addiu	$5,$5,-1
	addiu	$6,$6,%lo(0x80000)
	addiu	$7,$7,%lo(crt0_args)
	addiu	$8,$8,%lo(_root)
	move	$28,$4
	li	$3,60				# SetupThread
	syscall
	move	$29,$2

	lui	$4,%hi(crt0_bss_end)		# _end
	lui	$5,0			# _heap_size: -1 = the rest of memory
	addiu	$4,$4,%lo(crt0_bss_end)
	addiu	$5,$5,-1
	li	$3,61				# SetupHeap
	syscall

	jal	libkernl_startup
	nop
	jal	FlushCache
	move	$4,$0
	ei

	lui	$2,%hi(crt0_args)		# _args: argc, then argv
	addiu	$2,$2,%lo(crt0_args)
	lw	$4,0($2)
	jal	main
	addiu	$5,$2,4
	j	_Exit
	move	$4,$2
	.end	_start
