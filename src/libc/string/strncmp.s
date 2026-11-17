/*
 * strncmp.s: hand-written assembly, retail (US v1.01) 0x001ee130-0x001ee130.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ee130.
 * strncmp(s1, s2, n): steps over equal aligned blocks (16 bytes with MMI) while n allows, then
 * compares bytes; returns the first difference as unsigned chars, or 0.
 */
/* 0x001ee130 */
	.set	noreorder
	.text
	.globl	strncmp
	.ent	strncmp
strncmp:
	bnez	$6,.Lstrncmp_2
	or	$3,$4,$5
.Lstrncmp_1:
	jr	$31
	move	$2,$0
.Lstrncmp_2:
	andi	$2,$3,0x7
	bnez	$2,.Lstrncmp_8
	move	$2,$6
	andi	$2,$3,0xf
	sltiu	$7,$6,16
	lui	$9,0x101
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	or	$2,$2,$7
	bnez	$2,.Lstrncmp_4
	move	$7,$4
	lq	$3,0($4)
	pcpyld	$10,$9,$9
	lq	$2,0($5)
	lui	$8,0x8080
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	psubw	$3,$3,$2
	pcpyld	$9,$8,$8
	pcpyud	$2,$3,$4
	move	$8,$5
	or	$3,$2,$3
	bnez	$3,.Lstrncmp_8
	move	$2,$6
	addiu	$6,$6,-16
.Lstrncmp_3:
	beqz	$6,.Lstrncmp_1
	nop
	lq	$2,0($7)
	pnor	$3,$0,$2
	psubb	$2,$2,$10
	pand	$2,$2,$3
	pand	$3,$2,$9
	pcpyud	$2,$3,$4
	or	$2,$2,$3
	bnez	$2,.Lstrncmp_1
	addiu	$7,$7,16
	sltiu	$2,$6,16
	lq	$3,0($7)
	bnez	$2,.Lstrncmp_6
	addiu	$8,$8,16
	lq	$2,0($8)
	psubw	$3,$3,$2
	pcpyud	$2,$3,$4
	or	$2,$2,$3
	beqzl	$2,.Lstrncmp_3
	addiu	$6,$6,-16
	b	.Lstrncmp_7
	move	$4,$7
.Lstrncmp_4:
	sltiu	$2,$6,8
	bnez	$2,.Lstrncmp_6
	move	$8,$5
	ld	$3,0($4)
	ld	$2,0($5)
	bne	$3,$2,.Lstrncmp_8
	move	$2,$6
	addiu	$6,$6,-8
	lui	$10,0x8080
	ori	$10,$10,0x8080
	dsll	$10,$10,0x10
	ori	$10,$10,0x8080
	dsll	$10,$10,0x10
	ori	$10,$10,0x8080
.Lstrncmp_5:
	beqz	$6,.Lstrncmp_1
	nop
	ld	$2,0($7)
	nor	$3,$0,$2
	dsubu	$2,$2,$9
	and	$2,$2,$3
	and	$2,$2,$10
	bnez	$2,.Lstrncmp_1
	addiu	$7,$7,8
	sltiu	$2,$6,8
	bnez	$2,.Lstrncmp_6
	addiu	$8,$8,8
	ld	$3,0($7)
	ld	$2,0($8)
	beql	$3,$2,.Lstrncmp_5
	addiu	$6,$6,-8
.Lstrncmp_6:
	move	$4,$7
.Lstrncmp_7:
	move	$5,$8
	move	$2,$6
.Lstrncmp_8:
	beqz	$2,.Lstrncmp_11
	addiu	$6,$6,-1
	b	.Lstrncmp_10
	lb	$3,0($4)
	nop
.Lstrncmp_9:
	beqz	$6,.Lstrncmp_1
	nop
	beqz	$7,.Lstrncmp_1
	addiu	$4,$4,1
	addiu	$5,$5,1
	lb	$3,0($4)
	addiu	$6,$6,-1
.Lstrncmp_10:
	lb	$2,0($5)
	beq	$3,$2,.Lstrncmp_9
	lbu	$7,0($4)
.Lstrncmp_11:
	lbu	$2,0($5)
	lbu	$3,0($4)
	jr	$31
	subu	$2,$3,$2
	.end	strncmp
