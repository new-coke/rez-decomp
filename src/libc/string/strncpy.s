/*
 * strncpy.s: hand-written assembly, retail (US v1.01) 0x001ee2e8-0x001ee2e8.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ee2e8.
 * strncpy(dst, src, n): copies aligned blocks without a zero byte (16 bytes with lq/sq) while a
 * block of n remains, then bytes, padding with zeros; returns dst.
 */
/* 0x001ee2e8 */
	.set	noreorder
	.text
	.globl	strncpy
	.ent	strncpy
strncpy:
	move	$8,$4
	or	$7,$5,$4
	li	$10,16
	andi	$2,$7,0x7
	li	$9,8
	bnez	$2,.Lstrncpy_4
	andi	$2,$7,0xf
	movz	$9,$10,$2
	bnez	$2,.Lstrncpy_1
	sltu	$2,$6,$9
	bnez	$2,.Lstrncpy_4
	nop
	lui	$7,0x101
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	lq	$3,0($5)
	pcpyld	$9,$7,$7
	pnor	$3,$0,$3
	lui	$7,0x8080
	ori	$7,$7,0x8080
	dsll	$7,$7,0x10
	ori	$7,$7,0x8080
	dsll	$7,$7,0x10
	ori	$7,$7,0x8080
	psubb	$2,$3,$9
	pcpyld	$10,$7,$7
	pand	$2,$2,$3
	pand	$2,$2,$10
	pcpyud	$3,$2,$4
	or	$3,$2,$3
	bnez	$3,.Lstrncpy_3
	move	$7,$8
	lq	$3,0($5)
	addiu	$6,$6,-16
	addiu	$5,$5,16
	sltiu	$2,$6,16
	sq	$3,0($7)
	bnez	$2,.Lstrncpy_3
	addiu	$7,$7,16
	lq	$2,0($5)
	pnor	$3,$0,$2
	psubb	$2,$2,$9
	pand	$2,$2,$3
	pand	$2,$2,$10
	pcpyud	$3,$2,$4
	or	$2,$2,$3
	beqzl	$2,.Lstrncpy_2
	lq	$3,0($5)
	b	.Lstrncpy_4
	move	$4,$7
.Lstrncpy_1:
	bnez	$2,.Lstrncpy_4
	nop
	ld	$3,0($5)
	lui	$9,0x101
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	lui	$10,0x8080
	ori	$10,$10,0x8080
	dsll	$10,$10,0x10
	ori	$10,$10,0x8080
	dsll	$10,$10,0x10
	ori	$10,$10,0x8080
	dsubu	$2,$3,$9
	nor	$3,$0,$3
	and	$2,$2,$3
	and	$2,$2,$10
	bnez	$2,.Lstrncpy_3
	move	$7,$8
	ld	$3,0($5)
	nop
.Lstrncpy_2:
	addiu	$6,$6,-8
	addiu	$5,$5,8
	sltiu	$2,$6,8
	sd	$3,0($7)
	bnez	$2,.Lstrncpy_3
	addiu	$7,$7,8
	ld	$2,0($5)
	nor	$3,$0,$2
	dsubu	$2,$2,$9
	and	$2,$2,$3
	and	$2,$2,$10
	beqzl	$2,.Lstrncpy_2
	ld	$3,0($5)
.Lstrncpy_3:
	move	$4,$7
.Lstrncpy_4:
	beqz	$6,.Lstrncpy_6
	move	$2,$6
	lbu	$2,0($5)
	addiu	$6,$6,-1
	addiu	$5,$5,1
	sb	$2,0($4)
	sll	$2,$2,0x18
	bnez	$2,.Lstrncpy_4
	addiu	$4,$4,1
	move	$2,$6
	beqz	$2,.Lstrncpy_6
	addiu	$6,$6,-1
.Lstrncpy_5:
	sb	$0,0($4)
	move	$2,$6
	addiu	$4,$4,1
	nop
	nop
	bnez	$2,.Lstrncpy_5
	addiu	$6,$6,-1
.Lstrncpy_6:
	jr	$31
	move	$2,$8
	.end	strncpy
