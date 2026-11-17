/*
 * strchr.s: hand-written assembly, retail (US v1.01) 0x001eda60-0x001eda60.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001eda60.
 * strchr(s, c): skips aligned blocks holding neither a zero byte nor c (16-byte blocks with MMI),
 * then scans bytes; returns a pointer to the first c, or 0.
 */
/* 0x001eda60 */
	.set	noreorder
	.text
	.globl	strchr
	.ent	strchr
strchr:
	andi	$2,$4,0x7
	bnez	$2,.Lstrchr_5
	andi	$5,$5,0xff
	dsll	$3,$5,0x8
	lui	$6,0x101
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	daddu	$10,$3,$5
	andi	$3,$4,0xf
	dsll	$2,$10,0x10
	lui	$8,0x8080
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	daddu	$2,$2,$10
	dsll32	$10,$2,0x0
	bnez	$3,.Lstrchr_2
	daddu	$7,$2,$10
.Lstrchr_1:
	lq	$9,0($4)
	pcpyld	$10,$6,$6
	pnor	$3,$0,$9
	psubb	$2,$9,$10
	pcpyld	$6,$8,$8
	pand	$2,$2,$3
	pcpyld	$8,$7,$7
	pand	$2,$2,$6
	pcpyud	$3,$2,$7
	or	$3,$2,$3
	bnezl	$3,.Lstrchr_6
	lbu	$2,0($4)
	pxor	$2,$9,$8
	psubb	$3,$2,$10
	pnor	$2,$0,$2
	lui	$8,0x8080
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	dsll	$8,$8,0x10
	ori	$8,$8,0x8080
	pand	$3,$3,$2
	pand	$3,$3,$6
	lui	$6,0x101
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	pcpyud	$2,$3,$5
	or	$3,$2,$3
	beqzl	$3,.Lstrchr_1
	addiu	$4,$4,16
	b	.Lstrchr_6
	lbu	$2,0($4)
.Lstrchr_2:
	ld	$9,0($4)
	nor	$3,$0,$9
	dsubu	$2,$9,$6
	and	$2,$2,$3
	and	$2,$2,$8
	bnezl	$2,.Lstrchr_6
	lbu	$2,0($4)
	xor	$2,$9,$7
	dsubu	$3,$2,$6
	nor	$2,$0,$2
	and	$3,$3,$2
	and	$3,$3,$8
	bnezl	$3,.Lstrchr_6
	lbu	$2,0($4)
	move	$9,$6
	addiu	$4,$4,8
.Lstrchr_3:
	ld	$6,0($4)
	dsubu	$2,$6,$9
	nor	$3,$0,$6
	and	$2,$2,$3
	and	$2,$2,$8
	bnez	$2,.Lstrchr_5
	xor	$2,$6,$7
	nor	$3,$0,$2
	dsubu	$2,$2,$9
	and	$2,$2,$3
	and	$2,$2,$8
	beqzl	$2,.Lstrchr_3
	addiu	$4,$4,8
	b	.Lstrchr_6
	lbu	$2,0($4)
.Lstrchr_4:
	beql	$2,$5,.Lstrchr_7
	lbu	$3,0($4)
	addiu	$4,$4,1
.Lstrchr_5:
	lbu	$2,0($4)
.Lstrchr_6:
	bnez	$2,.Lstrchr_4
	nop
	lbu	$3,0($4)
.Lstrchr_7:
	move	$2,$0
	xor	$3,$3,$5
	jr	$31
	movz	$2,$4,$3
	.end	strchr
