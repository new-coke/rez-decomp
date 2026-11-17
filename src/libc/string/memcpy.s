/*
 * memcpy.s: hand-written assembly, retail (US v1.01) 0x001ed534-0x001ed534.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions or lq/sq), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ed534.
 * memcpy(dst, src, len): when len >= 16 and both pointers are 16-byte aligned, copies 32-byte and
 * then 16-byte blocks as quadwords (lq/sq), then 4-byte words, then bytes; returns dst.
 */
/* 0x001ed534 */
	.set	noreorder
	.text
	.globl	memcpy
	.ent	memcpy
memcpy:
	move	$8,$4
	sltiu	$2,$6,32
	bnez	$2,.Lmemcpy_3
	move	$3,$8
	or	$2,$5,$8
	andi	$2,$2,0xf
	bnezl	$2,.Lmemcpy_4
	addiu	$6,$6,-1
	move	$7,$8
.Lmemcpy_1:
	lq	$3,0($5)
	addiu	$6,$6,-32
	addiu	$5,$5,16
	sltiu	$4,$6,32
	sq	$3,0($7)
	addiu	$7,$7,16
	lq	$2,0($5)
	addiu	$5,$5,16
	sq	$2,0($7)
	beqz	$4,.Lmemcpy_1
	addiu	$7,$7,16
	sltiu	$2,$6,8
	bnez	$2,.Lmemcpy_3
	move	$3,$7
.Lmemcpy_2:
	ld	$3,0($5)
	addiu	$6,$6,-8
	addiu	$5,$5,8
	sltiu	$2,$6,8
	sd	$3,0($7)
	beqz	$2,.Lmemcpy_2
	addiu	$7,$7,8
	move	$3,$7
.Lmemcpy_3:
	addiu	$6,$6,-1
.Lmemcpy_4:
	li	$2,-1
	beq	$6,$2,.Lmemcpy_6
	move	$4,$2
.Lmemcpy_5:
	lbu	$2,0($5)
	addiu	$6,$6,-1
	addiu	$5,$5,1
	sb	$2,0($3)
	nop
	bne	$6,$4,.Lmemcpy_5
	addiu	$3,$3,1
.Lmemcpy_6:
	jr	$31
	move	$2,$8
	.end	memcpy
