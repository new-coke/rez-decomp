/*
 * memmove.s: hand-written assembly, retail (US v1.01) 0x001f411c-0x001f411c.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions or lq/sq), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001f411c.
 * memmove(dst, src, len): copies backwards byte by byte when dst overlaps src from above; otherwise
 * copies forwards like memcpy (memcpy): quadword blocks (lq/sq), then words, then bytes; returns dst.
 */
/* 0x001f411c */
	.set	noreorder
	.text
	.globl	memmove
	.ent	memmove
memmove:
	move	$8,$4
	sltu	$2,$5,$8
	beqz	$2,.Lmemmove_2
	move	$3,$8
	addu	$7,$5,$6
	sltu	$2,$8,$7
	beqz	$2,.Lmemmove_2
	li	$2,-1
	addu	$3,$8,$6
	addiu	$6,$6,-1
	beq	$6,$2,.Lmemmove_8
	move	$5,$7
	move	$4,$2
.Lmemmove_1:
	addiu	$5,$5,-1
	addiu	$3,$3,-1
	lbu	$2,0($5)
	addiu	$6,$6,-1
	nop
	bne	$6,$4,.Lmemmove_1
	sb	$2,0($3)
	jr	$31
	move	$2,$8
.Lmemmove_2:
	sltiu	$2,$6,32
	bnezl	$2,.Lmemmove_6
	addiu	$6,$6,-1
	or	$2,$5,$3
	andi	$2,$2,0xf
	bnezl	$2,.Lmemmove_6
	addiu	$6,$6,-1
	move	$7,$3
.Lmemmove_3:
	lq	$3,0($5)
	addiu	$6,$6,-32
	addiu	$5,$5,16
	sltiu	$4,$6,32
	sq	$3,0($7)
	addiu	$7,$7,16
	lq	$2,0($5)
	addiu	$5,$5,16
	sq	$2,0($7)
	beqz	$4,.Lmemmove_3
	addiu	$7,$7,16
	sltiu	$2,$6,8
	bnez	$2,.Lmemmove_5
	move	$3,$7
.Lmemmove_4:
	ld	$3,0($5)
	addiu	$6,$6,-8
	addiu	$5,$5,8
	sltiu	$2,$6,8
	sd	$3,0($7)
	beqz	$2,.Lmemmove_4
	addiu	$7,$7,8
	move	$3,$7
.Lmemmove_5:
	addiu	$6,$6,-1
.Lmemmove_6:
	li	$2,-1
	beq	$6,$2,.Lmemmove_8
	move	$4,$2
.Lmemmove_7:
	lbu	$2,0($5)
	addiu	$6,$6,-1
	addiu	$5,$5,1
	sb	$2,0($3)
	nop
	bne	$6,$4,.Lmemmove_7
	addiu	$3,$3,1
.Lmemmove_8:
	jr	$31
	move	$2,$8
	.end	memmove
