/*
 * strncat.s: hand-written assembly, retail (US v1.01) 0x001edf80-0x001edf80.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001edf80.
 * strncat(s1, s2, n): finds the end of s1 (16-byte blocks with MMI when aligned), appends at most
 * n characters of s2 and terminates; returns s1.
 */
/* 0x001edf80 */
	.set	noreorder
	.text
	.globl	strncat
	.ent	strncat
strncat:
	move	$8,$4
	andi	$2,$8,0x7
	bnezl	$2,.Lstrncat_5
	lb	$2,0($4)
	andi	$3,$8,0xf
	bnezl	$3,.Lstrncat_2
	ld	$2,0($8)
	lui	$7,0x101
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	dsll	$7,$7,0x10
	ori	$7,$7,0x101
	lq	$2,0($8)
	pcpyld	$9,$7,$7
	lui	$4,0x8080
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	dsll	$4,$4,0x10
	ori	$4,$4,0x8080
	psubb	$3,$2,$9
	pnor	$2,$0,$2
	pcpyld	$10,$4,$4
	pand	$3,$3,$2
	pand	$3,$3,$10
	pcpyud	$2,$3,$4
	or	$3,$3,$2
	bnez	$3,.Lstrncat_4
	move	$7,$8
	addiu	$7,$7,0x10
	nop
.Lstrncat_1:
	lq	$2,0($7)
	pnor	$3,$0,$2
	psubb	$2,$2,$9
	pand	$2,$2,$3
	pand	$2,$2,$10
	pcpyud	$3,$2,$4
	or	$2,$2,$3
	beqz	$2,.Lstrncat_1
	addiu	$7,$7,16
	b	.Lstrncat_4
	addiu	$7,$7,-16
.Lstrncat_2:
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
	dsubu	$3,$2,$9
	nor	$2,$0,$2
	and	$3,$3,$2
	and	$3,$3,$4
	bnez	$3,.Lstrncat_4
	move	$7,$8
	addiu	$7,$7,8
.Lstrncat_3:
	ld	$2,0($7)
	nor	$3,$0,$2
	dsubu	$2,$2,$9
	and	$2,$2,$3
	and	$2,$2,$4
	beqzl	$2,.Lstrncat_3
	addiu	$7,$7,8
.Lstrncat_4:
	move	$4,$7
	lb	$2,0($4)
.Lstrncat_5:
	beqz	$2,.Lstrncat_7
	addiu	$6,$6,-1
	addiu	$4,$4,1
.Lstrncat_6:
	lb	$2,0($4)
	nop
	nop
	nop
	nop
	bnezl	$2,.Lstrncat_6
	addiu	$4,$4,1
.Lstrncat_7:
	lui	$2,0xffff
	ori	$2,$2,0xffff
	beq	$6,$2,.Lstrncat_10
	nop
	lbu	$2,0($5)
	addiu	$5,$5,1
	sb	$2,0($4)
	sll	$2,$2,0x18
	beqz	$2,.Lstrncat_10
	addiu	$4,$4,1
	lui	$3,0xffff
	ori	$3,$3,0xffff
	nop
.Lstrncat_8:
	beqzl	$6,.Lstrncat_9
	sb	$0,0($4)
.Lstrncat_9:
	addiu	$6,$6,-1
	beq	$6,$3,.Lstrncat_10
	nop
	lbu	$2,0($5)
	addiu	$5,$5,1
	sb	$2,0($4)
	sll	$2,$2,0x18
	bnez	$2,.Lstrncat_8
	addiu	$4,$4,1
.Lstrncat_10:
	jr	$31
	move	$2,$8
	.end	strncat
