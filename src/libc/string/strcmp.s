/*
 * strcmp.s: hand-written assembly, retail (US v1.01) 0x001edbf0-0x001edbf0.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001edbf0.
 * strcmp(s1, s2): steps over equal aligned blocks (16 bytes with psubw/pcpyud), returns 0 at the
 * terminator, else compares bytes and returns the first difference as unsigned chars.
 */
/* 0x001edbf0 */
	.set	noreorder
	.text
	.globl	strcmp
	.ent	strcmp
strcmp:
	or	$8,$4,$5
	andi	$2,$8,0x7
	bnezl	$2,.Lstrcmp_7
	lb	$2,0($4)
	andi	$9,$8,0xf
	lui	$7,0x101
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	lui	$6,0x8080
	ori	$6,$6,0x8080
	dsll	$6,$6,0x10
	ori	$6,$6,0x8080
	dsll	$6,$6,0x10
	ori	$6,$6,0x8080
	bnez	$9,.Lstrcmp_3
	ld	$2,0($5)
	lq	$3,0($4)
	pcpyld	$8,$7,$7
	lq	$2,0($5)
	pcpyld	$10,$6,$6
	psubw	$7,$2,$3
	pcpyud	$6,$7,$4
	or	$3,$6,$7
	bnezl	$3,.Lstrcmp_7
	lb	$2,0($4)
	lq	$2,0($4)
	pnor	$3,$0,$2
.Lstrcmp_1:
	psubb	$2,$2,$8
	pand	$2,$2,$3
	pand	$2,$2,$10
	pcpyud	$3,$2,$4
	or	$6,$3,$2
	beqz	$6,.Lstrcmp_2
	addiu	$4,$4,16
	jr	$31
	move	$2,$0
.Lstrcmp_2:
	addiu	$5,$5,16
	lq	$2,0($4)
	lq	$3,0($5)
	psubw	$7,$2,$3
	pcpyud	$6,$7,$4
	or	$9,$6,$7
	beqzl	$9,.Lstrcmp_1
	pnor	$3,$0,$2
	b	.Lstrcmp_7
	lb	$2,0($4)
.Lstrcmp_3:
	ld	$3,0($4)
	bnel	$3,$2,.Lstrcmp_7
	lb	$2,0($4)
	ld	$2,0($4)
	nor	$8,$0,$2
.Lstrcmp_4:
	dsubu	$2,$2,$7
	and	$2,$2,$8
	and	$2,$2,$6
	beqz	$2,.Lstrcmp_5
	addiu	$4,$4,8
	jr	$31
	move	$2,$0
.Lstrcmp_5:
	addiu	$5,$5,8
	ld	$2,0($4)
	ld	$3,0($5)
	beql	$3,$2,.Lstrcmp_4
	nor	$8,$0,$2
	b	.Lstrcmp_7
	lb	$2,0($4)
.Lstrcmp_6:
	sll	$2,$3,0x18
	lb	$3,0($5)
	sra	$2,$2,0x18
	bnel	$2,$3,.Lstrcmp_8
	lbu	$3,0($4)
	addiu	$4,$4,1
	addiu	$5,$5,1
	lb	$2,0($4)
.Lstrcmp_7:
	bnez	$2,.Lstrcmp_6
	lbu	$3,0($4)
.Lstrcmp_8:
	lbu	$2,0($5)
	jr	$31
	subu	$2,$3,$2
	.end	strcmp
