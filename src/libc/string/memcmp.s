/*
 * memcmp.s: hand-written assembly, retail (US v1.01) 0x001ed4a0-0x001ed4a0.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ed4a0.
 * memcmp(s1, s2, n): when n >= 16 and both pointers are 16-byte aligned, compares 16-byte blocks
 * (lq, pxor, pcpyud) until one differs, then compares bytes; returns the first difference as unsigned chars, or 0.
 */
/* 0x001ed4a0 */
	.set	noreorder
	.text
	.globl	memcmp
	.ent	memcmp
memcmp:
	sltiu	$2,$6,16
	bnez	$2,.Lmemcmp_2
	or	$2,$4,$5
	andi	$2,$2,0xf
	bnez	$2,.Lmemcmp_2
	nop
.Lmemcmp_1:
	lq	$3,0($4)
	sltiu	$7,$6,32
	lq	$2,0($5)
	addiu	$4,$4,16
	pxor	$8,$2,$3
	addiu	$2,$5,16
	pcpyud	$10,$8,$7
	or	$9,$10,$8
	movz	$5,$2,$9
	bnezl	$9,.Lmemcmp_2
	addiu	$4,$4,-16
	beqz	$7,.Lmemcmp_1
	addiu	$6,$6,-16
.Lmemcmp_2:
	lui	$2,0xffff
	addiu	$6,$6,-1
	ori	$2,$2,0xffff
	beq	$6,$2,.Lmemcmp_5
	nop
	lui	$7,0xffff
	ori	$7,$7,0xffff
.Lmemcmp_3:
	lbu	$3,0($4)
	lbu	$2,0($5)
	beq	$3,$2,.Lmemcmp_4
	addiu	$4,$4,1
	jr	$31
	subu	$2,$3,$2
.Lmemcmp_4:
	addiu	$6,$6,-1
	bne	$6,$7,.Lmemcmp_3
	addiu	$5,$5,1
.Lmemcmp_5:
	jr	$31
	move	$2,$0
	.end	memcmp
