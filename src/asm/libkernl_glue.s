/*
 * libkernl_glue.s: hand-written assembly, retail (US v1.01) 0x001f6438-0x001f6438.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. Heap break (sbrk): grows the end-of-heap pointer crt_heap_end by n with interrupts disabled; -1 and errno = ENOMEM past EndOfHeap().
 * Argument: $4 = increment. Returns the previous break, or -1 after storing 12 (ENOMEM) through the
 * errno pointer from 0x1ec468 when the new break is beyond EndOfHeap(). Interrupts are disabled with
 * the di / sync.p / re-check loop when Status.EIE was set, and re-enabled before returning.
 * Compiled code with inline assembly (gcc frame and delay-slot use): the mfc0/di/ei/sync.p cannot come
 * from plain C.
 */
/* 0x001f6438 */
	.set	noreorder
	.text
	.globl	_sbrk
	.ent	_sbrk
_sbrk:
	addiu	$sp,$sp,-64
	sd	$31,48($sp)
	sd	$18,32($sp)
	sd	$17,16($sp)
	sd	$16,0($sp)
	# $17 = Status.EIE (bit 16): were interrupts enabled?
	mfc0	$17,$12
	lui	$2,0x1
	and	$17,$17,$2
	beqz	$17,.L_sbrk_noei
	lui	$18,%hi(crt_heap_end)
.L_sbrk_disable:
	# di until Status.EIE reads back clear
	di
	sync.p
	mfc0	$2,$12
	lui	$3,0x1
	and	$2,$2,$3
	bnez	$2,.L_sbrk_disable
	nop
	b	.L_sbrk_check
	lw	$2,%lo(crt_heap_end)($18)
.L_sbrk_noei:
	lw	$2,%lo(crt_heap_end)($18)
.L_sbrk_check:
	jal	EndOfHeap
	addu	$16,$2,$4			# $16 = new break (old + increment)
	sltu	$2,$2,$16
	beqz	$2,.L_sbrk_ok
	lw	$2,%lo(crt_heap_end)($18)
	# too far: errno = ENOMEM (12), return -1
	jal	__errno
	nop
	li	$3,12
	beqz	$17,.L_sbrk_fail
	sw	$3,0($2)
	ei
.L_sbrk_fail:
	lui	$2,0xffff
	b	.L_sbrk_done
	ori	$2,$2,0xffff
.L_sbrk_ok:
	# store the new break, return the old one
	beqz	$17,.L_sbrk_done
	sw	$16,%lo(crt_heap_end)($18)
	ei
.L_sbrk_done:
	ld	$31,48($sp)
	ld	$18,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,64
	.end	_sbrk
