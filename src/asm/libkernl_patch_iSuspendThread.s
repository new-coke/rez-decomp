/*
 * libkernl_patch_iSuspendThread.s: hand-written assembly, retail (US v1.01) 0x001f6e30-0x001f6e30.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. Interrupt-context suspend of a thread: other threads via _iSuspendThread; the current thread is queued for a helper through a ring and iSignalSema (unreferenced).
 * Argument: $4 = thread id. Same shape as 0x1f66d8 with request code 2 in the ring entry and
 * _iSuspendThread for other threads. Returns id, -1, or the syscall's result. No code in the
 * executable calls it.
 */
/* 0x001f6e30 */
	.set	noreorder
	.text
	.globl	iSuspendThread
	.ent	iSuspendThread
iSuspendThread:
	addiu	$sp,$sp,-32
	sd	$31,16($sp)
	sd	$16,0($sp)
	li	$3,-47				# $3 = syscall number -0x2f (current thread id)
	syscall
	move	$16,$2
	beq	$16,$4,.LiSuspendThread_self
	sltiu	$2,$16,256
	# another thread: suspend it directly
	jal	_iSuspendThread
	nop
	b	.LiSuspendThread_out
	ld	$31,16($sp)
.LiSuspendThread_self:
	# the current thread: needs id < 256 and the helper enabled
	beqz	$2,.LiSuspendThread_fail
	lui	$2,%hi(libkernl_defer_tid)
	lw	$3,%lo(libkernl_defer_tid)($2)
	bnez	$3,.LiSuspendThread_queue
	lui	$3,%hi(libkernl_defer_queue)
.LiSuspendThread_fail:
	b	.LiSuspendThread_done
	li	$2,-1
.LiSuspendThread_queue:
	# ring[(count & 0x1ff)] = (2, id); count++; wake the helper
	lui	$5,%hi(libkernl_defer_sema)
	addiu	$3,$3,%lo(libkernl_defer_queue)
	lw	$4,%lo(libkernl_defer_sema)($5)
	lw	$2,4($3)
	li	$7,2
	andi	$2,$2,0x1ff
	sll	$6,$2,0x1
	addiu	$2,$2,1
	addu	$5,$3,$6
	sw	$2,4($3)
	sb	$7,8($5)
	move	$3,$5
	jal	iSignalSema
	sb	$16,9($3)
	move	$2,$16
.LiSuspendThread_done:
	ld	$31,16($sp)
.LiSuspendThread_out:
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,32
	.end	iSuspendThread
