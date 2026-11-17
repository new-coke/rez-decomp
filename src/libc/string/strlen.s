/*
 * strlen.s: hand-written assembly, retail (US v1.01) 0x001ede48-0x001ede48.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ede48.
 * strlen(s): skips aligned blocks without a zero byte (16 bytes with MMI), then counts bytes.
 */
/* 0x001ede48 */
	.set	noreorder
	.text
	.globl	strlen
	.ent	strlen
strlen:
	andi	$2,$4,0x7
	bnez	$2,.Lstrlen_4
	move	$7,$4
	andi	$3,$4,0xf
	lui	$2,0x101
	ori	$2,$2,0x101
	dsll	$2,$2,0x10
	ori	$2,$2,0x101
	dsll	$2,$2,0x10
	ori	$2,$2,0x101
	bnez	$3,.Lstrlen_2
	move	$5,$4
	lq	$3,0($5)
	pcpyld	$8,$2,$2
	lui	$4,0x8080
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	psubb	$2,$3,$8
	pnor	$3,$0,$3
	pcpyld	$9,$4,$4
	pand	$2,$2,$3
	pand	$2,$2,$9
	pcpyud	$3,$2,$8
	or	$6,$3,$2
	bnezl	$6,.Lstrlen_4
	move	$4,$5
	addiu	$5,$5,16
.Lstrlen_1:
	lq	$2,0($5)
	pnor	$3,$0,$2
	psubb	$2,$2,$8
	pand	$2,$2,$3
	pand	$4,$2,$9
	pcpyud	$3,$4,$6
	or	$3,$3,$4
	beqzl	$3,.Lstrlen_1
	addiu	$5,$5,16
	b	.Lstrlen_4
	move	$4,$5
.Lstrlen_2:
	ld	$3,0($5)
	lui	$4,0x8080
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsubu	$2,$3,$2
	nor	$3,$0,$3
	and	$2,$2,$3
	and	$2,$2,$4
	bnezl	$2,.Lstrlen_4
	move	$4,$5
	lui	$6,0x101
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	dsll	$6,$6,0x10
	ori	$6,$6,0x101
	addiu	$5,$5,8
.Lstrlen_3:
	ld	$2,0($5)
	nor	$3,$0,$2
	dsubu	$2,$2,$6
	and	$2,$2,$3
	and	$2,$2,$4
	beqzl	$2,.Lstrlen_3
	addiu	$5,$5,8
	move	$4,$5
.Lstrlen_4:
	lb	$2,0($4)
	nop
	nop
	nop
	nop
	bnezl	$2,.Lstrlen_4
	addiu	$4,$4,1
	jr	$31
	subu	$2,$4,$7
	.end	strlen
