/*
 * memchr.s: hand-written assembly, retail (US v1.01) 0x001f403c-0x001f403c.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001f403c.
 * memchr(s, c, n): when n >= 16 and s is 16-byte aligned, skips 16-byte blocks that cannot hold c
 * (pcpyh/pcpyld/pxor/psubb/pnor/pand/pcpyud), then scans bytes; returns a pointer to the first c, or 0.
 */
/* 0x001f403c */
	.set	noreorder
	.text
	.globl	memchr
	.ent	memchr
memchr:
	sltiu	$2,$6,16
	bnez	$2,.Lmemchr_2
	andi	$5,$5,0xff
	andi	$2,$4,0xf
	bnez	$2,.Lmemchr_2
	move	$7,$4
	dsll	$3,$5,0x8
	lui	$2,0x101
	ori	$2,$2,0x101
	dsll	$2,$2,0x10
	ori	$2,$2,0x101
	dsll	$2,$2,0x10
	ori	$2,$2,0x101
	daddu	$10,$3,$5
	lui	$3,0x8080
	ori	$3,$3,0x8080
	dsll	$3,$3,0x10
	ori	$3,$3,0x8080
	dsll	$3,$3,0x10
	ori	$3,$3,0x8080
	pcpyh	$8,$10
	pcpyld	$9,$8,$8
	move	$4,$2
	pcpyld	$8,$3,$3
.Lmemchr_1:
	lq	$2,0($7)
	pxor	$2,$2,$9
	pcpyld	$10,$4,$4
	pnor	$3,$0,$2
	psubb	$2,$2,$10
	pand	$2,$2,$3
	pand	$2,$2,$8
	pcpyud	$3,$2,$9
	or	$2,$2,$3
	bnezl	$2,.Lmemchr_2
	move	$4,$7
	addiu	$6,$6,-16
	sltiu	$2,$6,16
	beqz	$2,.Lmemchr_1
	addiu	$7,$7,16
	move	$4,$7
.Lmemchr_2:
	lui	$2,0xffff
	addiu	$6,$6,-1
	ori	$2,$2,0xffff
	beq	$6,$2,.Lmemchr_4
	nop
	lui	$3,0xffff
	ori	$3,$3,0xffff
.Lmemchr_3:
	lbu	$2,0($4)
	beq	$2,$5,.Lmemchr_5
	addiu	$6,$6,-1
	bne	$6,$3,.Lmemchr_3
	addiu	$4,$4,1
.Lmemchr_4:
	jr	$31
	move	$2,$0
.Lmemchr_5:
	jr	$31
	move	$2,$4
	.end	memchr
