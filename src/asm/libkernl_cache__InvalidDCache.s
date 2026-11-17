/*
 * libkernl_cache__InvalidDCache.s: hand-written assembly, retail (US v1.01) 0x001f66c8-0x001f6770.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. Invalidates data-cache lines whose tags fall in [start, end): index walk over both ways of all 64 lines.
 * Arguments: $4 = start, $5 = end (64-byte aligned by the caller 0x1f6130). As 0x1f5f48 but the
 * matching lines are invalidated without write-back (cache 0x16 = DXIN). Called by 0x1f6130 (and 0x1f61b0).
 */
/* 0x001f66c8 */
	.set	noreorder
	.text
	.globl	_InvalidDCache
	.ent	_InvalidDCache
_InvalidDCache:
	# $7 = TagLo address mask 0xfffff000, $6 = line index
	lui	$7,0xffff
	move	$6,$0
	ori	$7,$7,0xf000
	nop
.L_InvalidDCache_loop:
	# way 0: load the tag, rebuild the line address
	sync
	cache	0x10,0($6)
	sync
	mfc0	$2,$28
	and	$2,$2,$7
	addu	$2,$2,$6
	sltu	$3,$5,$2
	sltu	$2,$2,$4
	bnez	$2,.L_InvalidDCache_way1
	nop
	bnez	$3,.L_InvalidDCache_way1
	nop
	sync
	cache	0x16,0($6)
	sync
.L_InvalidDCache_way1:
	# way 1 (index | 1)
	sync
	cache	0x10,1($6)
	sync
	mfc0	$2,$28
	and	$2,$2,$7
	addu	$2,$2,$6
	sltu	$3,$5,$2
	sltu	$2,$2,$4
	bnez	$2,.L_InvalidDCache_next
	nop
	bnez	$3,.L_InvalidDCache_next
	nop
	sync
	cache	0x16,1($6)
	sync
.L_InvalidDCache_next:
	sync
	addiu	$6,$6,64
	slti	$2,$6,4096
	bnez	$2,.L_InvalidDCache_loop
	nop
	jr	$31
	nop
	.end	_InvalidDCache

	.set	noreorder
	.text
	nop

/*
 * Sony libkernel. Data-cache invalidate of a range with interrupts disabled: rounds start/end down to 64 bytes and runs 0x1f6088.
 * Arguments: $4 = start, $5 = end. Disables interrupts with 0x1fd650 when Status.EIE is set and
 * re-enables them with 0x1fd698 (tail call). Called by DTX_Create and other CRI code.
 */
/* 0x001f6770 */
	.set	noreorder
	.text
	.globl	InvalidDCache
	.ent	InvalidDCache
InvalidDCache:
	addiu	$sp,$sp,-64
	sd	$18,32($sp)
	sd	$17,16($sp)
	move	$18,$4
	sd	$31,48($sp)
	move	$17,$5
	sd	$16,0($sp)
	# $16 = Status.EIE (bit 16)
	mfc0	$16,$12
	lui	$2,0x1
	and	$16,$16,$2
	beqz	$16,.LInvalidDCache_go
	nop
	jal	DIntr
	nop
.LInvalidDCache_go:
	# round start and end down to a 64-byte line
	lui	$4,0xffff
	ori	$4,$4,0xffc0
	and	$5,$17,$4
	jal	_InvalidDCache
	and	$4,$18,$4
	beqz	$16,.LInvalidDCache_noei
	ld	$31,48($sp)
	ld	$18,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	j	EIntr
	addiu	$sp,$sp,64
.LInvalidDCache_noei:
	ld	$18,32($sp)
	ld	$17,16($sp)
	ld	$16,0($sp)
	jr	$31
	addiu	$sp,$sp,64
	.end	InvalidDCache
