/*
 * strcpy.s: hand-written assembly, retail (US v1.01) 0x001edd34-0x001edd34.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001edd34.
 * strcpy(dst, src): copies aligned blocks without a zero byte (16 bytes with lq/sq), then bytes up
 * to and including the terminator; returns dst.
 */
/* 0x001edd34 */
	.set	noreorder
	.text
	.globl	strcpy
	.ent	strcpy
strcpy:
	move	$7,$4
	or	$8,$5,$7
	andi	$2,$8,0x7
	bnez	$2,.Lstrcpy_5
	move	$3,$7
	andi	$2,$8,0xf
	lui	$9,0x101
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	dsll	$9,$9,0x10
	ori	$9,$9,0x101
	lui	$4,0x8080
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	bnezl	$2,.Lstrcpy_2
	ld	$10,0($5)
	pcpyld	$10,$9,$9
	lq	$9,0($5)
	pcpyld	$8,$4,$4
	psubb	$2,$9,$10
	pnor	$3,$0,$9
	pand	$2,$2,$3
	pand	$2,$2,$8
	pcpyud	$4,$2,$9
	or	$3,$2,$4
	bnez	$3,.Lstrcpy_4
	move	$6,$7
.Lstrcpy_1:
	sq	$9,0($6)
	addiu	$5,$5,16
	lq	$9,0($5)
	psubb	$2,$9,$10
	pnor	$3,$0,$9
	pand	$2,$2,$3
	pand	$2,$2,$8
	pcpyud	$4,$2,$9
	or	$3,$2,$4
	beqz	$3,.Lstrcpy_1
	addiu	$6,$6,16
	b	.Lstrcpy_5
	move	$3,$6
.Lstrcpy_2:
	dsubu	$2,$10,$9
	nor	$3,$0,$10
	and	$2,$2,$3
	and	$2,$2,$4
	bnez	$2,.Lstrcpy_4
	move	$6,$7
.Lstrcpy_3:
	sd	$10,0($6)
	addiu	$5,$5,8
	ld	$10,0($5)
	nor	$2,$0,$10
	dsubu	$3,$10,$9
	and	$3,$3,$2
	and	$3,$3,$4
	beqz	$3,.Lstrcpy_3
	addiu	$6,$6,8
.Lstrcpy_4:
	move	$3,$6
.Lstrcpy_5:
	lbu	$2,0($5)
	addiu	$5,$5,1
	sb	$2,0($3)
	sll	$2,$2,0x18
	nop
	bnez	$2,.Lstrcpy_5
	addiu	$3,$3,1
	jr	$31
	move	$2,$7
	.end	strcpy
