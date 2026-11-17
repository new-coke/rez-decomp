/*
 * strcat.s: hand-written assembly, retail (US v1.01) 0x001ed934-0x001ed934.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ed934.
 * strcat(s1, s2): finds the end of s1 (16-byte blocks tested with lq/psubb/pnor/pand/pcpyud when
 * aligned), copies s2 there with strcpy (strcpy) and returns s1.
 */
/* 0x001ed934 */
	.set	noreorder
	.text
	.globl	strcat
	.ent	strcat
strcat:
	addiu	$29,$29,-32
	sq	$16,0($29)
	move	$16,$4
	andi	$2,$16,0x7
	bnez	$2,.Lstrcat_4
	sq	$31,16($29)
	andi	$2,$16,0xf
	lui	$3,0x101
	ori	$3,$3,0x101
	dsll	$3,$3,0x10
	ori	$3,$3,0x101
	dsll	$3,$3,0x10
	ori	$3,$3,0x101
	lui	$4,0x8080
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	bnez	$2,.Lstrcat_2
	ld	$6,0($16)
	lq	$2,0($16)
	pcpyld	$7,$3,$3
	pcpyld	$8,$4,$4
	psubb	$3,$2,$7
	pnor	$2,$0,$2
	pand	$3,$3,$2
	pand	$3,$3,$8
	pcpyud	$2,$3,$3
	or	$3,$2,$3
	bnez	$3,.Lstrcat_4
	move	$4,$16
	addiu	$6,$4,16
.Lstrcat_1:
	lq	$2,0($6)
	pnor	$3,$0,$2
	psubb	$2,$2,$7
	pand	$2,$2,$3
	pand	$2,$2,$8
	pcpyud	$3,$2,$2
	or	$2,$2,$3
	beqzl	$2,.Lstrcat_1
	addiu	$6,$6,16
	b	.Lstrcat_4
	move	$4,$6
.Lstrcat_2:
	move	$7,$3
	move	$8,$4
	dsubu	$3,$6,$3
	nor	$2,$0,$6
	and	$3,$3,$2
	and	$3,$3,$4
	bnez	$3,.Lstrcat_4
	move	$4,$16
	addiu	$6,$16,8
.Lstrcat_3:
	ld	$2,0($6)
	nor	$3,$0,$2
	dsubu	$2,$2,$7
	and	$2,$2,$3
	and	$2,$2,$8
	beqzl	$2,.Lstrcat_3
	addiu	$6,$6,8
	move	$4,$6
.Lstrcat_4:
	lb	$2,0($4)
	nop
	nop
	nop
	nop
	bnezl	$2,.Lstrcat_4
	addiu	$4,$4,1
	jal	strcpy
	nop
	move	$2,$16
	lq	$31,16($29)
	lq	$16,0($29)
	jr	$31
	addiu	$29,$29,32
	.end	strcat
