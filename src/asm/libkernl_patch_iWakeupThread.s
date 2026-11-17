/*
 * libkernl_patch_iWakeupThread.s: hand-written assembly, retail (US v1.01) 0x001f6d18-0x001f6d18.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. Interrupt-context wake-up of a thread: other threads via _iWakeupThread; the current thread is queued for a helper through a ring and iSignalSema.
 * Argument: $4 = thread id. Gets the current thread id with an inline syscall -0x2f; if it is another
 * thread, returns _iWakeupThread(id). Otherwise (id < 256 and the helper flag libkernl_defer_tid set) it appends
 * (0, id) to the 512-entry ring libkernl_defer_queue and signals semaphore libkernl_defer_sema; returns id, or -1.
 * Called by the CRI ADXM thread waker 0x1c6100 and others.
 */
/* 0x001f6d18 */
	.set	noreorder
	.text
	.globl	iWakeupThread
	.ent	iWakeupThread
iWakeupThread:
	addiu	$sp,$sp,-32
	sd	$31,16($sp)
	sd	$16,0($sp)
	li	$3,-47				# $3 = syscall number -0x2f (current thread id)
	syscall
	move	$16,$2
	beq	$16,$4,.LiWakeupThread_self
	sltiu	$2,$16,256
	# another thread: wake it directly
	jal	_iWakeupThread
	nop
	b	.LiWakeupThread_out
	ld	$31,16($sp)
.LiWakeupThread_self:
	# the current thread: needs id < 256 and the helper enabled
	beqz	$2,.LiWakeupThread_fail
	lui	$2,%hi(libkernl_defer_tid)
	lw	$3,%lo(libkernl_defer_tid)($2)
	bnez	$3,.LiWakeupThread_queue
	lui	$3,%hi(libkernl_defer_queue)
.LiWakeupThread_fail:
	b	.LiWakeupThread_done
	li	$2,-1
.LiWakeupThread_queue:
	# ring[(count & 0x1ff)] = (0, id); count++; wake the helper
	lui	$5,%hi(libkernl_defer_sema)
	addiu	$3,$3,%lo(libkernl_defer_queue)
	lw	$4,%lo(libkernl_defer_sema)($5)
	lw	$2,4($3)
	andi	$2,$2,0x1ff
	sll	$6,$2,0x1
	addiu	$2,$2,1
	addu	$5,$3,$6
	sw	$2,4($3)
	move	$3,$5
	sb	$0,8($5)
	jal	iSignalSema
	sb	$16,9($3)
	move	$2,$16
.LiWakeupThread_done:
	ld	$31,16($sp)
.LiWakeupThread_out:
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,32
	.end	iWakeupThread
