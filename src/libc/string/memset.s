/*
 * memset.s: hand-written assembly, retail (US v1.01) 0x001ed5e4-0x001ed5e4.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * newlib libc, EE-tuned string routine. The original is C with inline assembly for the 16-byte
 * path (one quadword per block, MMI instructions), which the pinned compiler cannot produce from C;
 * The C reconstruction kept for the native port is in the .c of this unit.
 * Also used, unchanged, by PS2 US v1.01 retail (ps2-us-1.01) at 0x001ed5e4.
 * memset(m, c, n): when n >= 8 and m is 16-byte aligned, replicates c with pcpyh/pcpyld and fills
 * 32-byte blocks with sq, then 8-byte words, then bytes; returns m.
 */
/* 0x001ed5e4 */
	.set	noreorder
	.text
	.globl	memset
	.ent	memset
memset:
	sltiu	$2,$6,8
	bnez	$2,.Lmemset_4
	move	$3,$4
	andi	$2,$4,0xf
	bnez	$2,.Lmemset_4
	move	$7,$4
	andi	$9,$5,0xff
	sltiu	$10,$6,32
	move	$8,$9
	dsll	$3,$8,0x8
	or	$8,$3,$9
	pcpyh	$3,$8
	bnez	$10,.Lmemset_3
	sltiu	$2,$6,8
	pcpyld	$8,$3,$3
.Lmemset_1:
	sq	$8,0($7)
	addiu	$6,$6,-32
	addiu	$7,$7,16
	sltiu	$2,$6,32
	sq	$8,0($7)
	beqz	$2,.Lmemset_1
	addiu	$7,$7,16
	b	.Lmemset_3
	sltiu	$2,$6,8
.Lmemset_2:
	addiu	$6,$6,-8
	addiu	$7,$7,8
	sltiu	$2,$6,8
.Lmemset_3:
	nop
	nop
	beqzl	$2,.Lmemset_2
	sd	$3,0($7)
	move	$3,$7
.Lmemset_4:
	lui	$2,0xffff
	addiu	$6,$6,-1
	ori	$2,$2,0xffff
	beq	$6,$2,.Lmemset_6
	nop
	lui	$2,0xffff
	ori	$2,$2,0xffff
.Lmemset_5:
	sb	$5,0($3)
	addiu	$6,$6,-1
	nop
	nop
	nop
	bne	$6,$2,.Lmemset_5
	addiu	$3,$3,1
.Lmemset_6:
	jr	$31
	move	$2,$4
	.end	memset
