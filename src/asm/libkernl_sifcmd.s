/*
 * libkernl_sifcmd.s: hand-written assembly, retail (US v1.01) 0x001f82d0-0x001f84a4.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sony libkernel. SIF command DMA interrupt handler: copies the received packet to the stack, re-arms the receive DMA and dispatches the command to its registered handler.
 * Installed by 0x1f7798 (its address is loaded at 0x1f7908 / 0x1f7914 and passed with channel 5, the
 * SIF0 DMA, to AddDmacHandler). State block libkernl_sif_cmd_ctl: +0 receive buffer pointer, +12/+16 system command
 * table and size, +20/+24 user command table and size (8-byte entries: handler, argument).
 * Packet word 2 is the command id (bit 31 = system command). Returns 0 (or 0 at once when the packet is
 * empty). Compiled code with inline ei / sync and a 128-bit copy.
 */
/* 0x001f82d0 */
	.set	noreorder
	.text
	.globl	libkernl_sif_cmd_dma_handler
	.ent	libkernl_sif_cmd_dma_handler
libkernl_sif_cmd_dma_handler:
	addiu	$sp,$sp,-144
	sd	$16,112($sp)
	sd	$31,128($sp)
	# re-enable interrupts (0x1fd698)
	jal	EIntr
	nop
	lui	$3,%hi(libkernl_sif_cmd_ctl)
	# $7 = receive buffer, $16 = state block; packet size = first byte
	lw	$7,%lo(libkernl_sif_cmd_ctl)($3)
	addiu	$16,$3,%lo(libkernl_sif_cmd_ctl)
	lbu	$2,0($7)
	andi	$5,$2,0xff
	beqz	$5,.Llibkernl_sif_cmd_dma_handler_out
	move	$2,$0
	# qwords = (size + 15) / 16, clear the size byte
	addiu	$2,$5,15
	li	$3,-1
	addiu	$4,$5,30
	slt	$3,$3,$2
	movn	$4,$2,$3
	move	$6,$7
	sra	$5,$4,0x4
	sb	$0,0($7)
	blez	$5,.Llibkernl_sif_cmd_dma_handler_copied
	move	$4,$5
	move	$3,$sp
	nop
.Llibkernl_sif_cmd_dma_handler_copy:
	# copy the packet to the stack, 16 bytes at a time
	lq	$2,0($6)
	addiu	$4,$4,-1
	addiu	$6,$6,16
	sq	$2,0($3)
	addiu	$3,$3,16
	bnez	$4,.Llibkernl_sif_cmd_dma_handler_copy
	nop
.Llibkernl_sif_cmd_dma_handler_copied:
	# re-arm the receive DMA chain
	jal	isceSifSetDChain
	nop
	lw	$3,8($sp)
	bgez	$3,.Llibkernl_sif_cmd_dma_handler_user
	nop
	# system command (id < 0): table at +12, count at +16
	lw	$2,8($sp)
	lui	$3,0x7fff
	ori	$3,$3,0xffff
	lw	$4,16($16)
	and	$5,$2,$3
	slt	$4,$5,$4
	beqz	$4,.Llibkernl_sif_cmd_dma_handler_done
	sll	$2,$5,0x3
	lw	$3,12($16)
	addu	$2,$2,$3
	lw	$6,0($2)
	beqz	$6,.Llibkernl_sif_cmd_dma_handler_done
	nop
	# handler(packet, argument)
	lw	$5,4($2)
	jalr	$6
	move	$4,$sp
	b	.Llibkernl_sif_cmd_dma_handler_done
	nop
.Llibkernl_sif_cmd_dma_handler_user:
	# user command: table at +20, count at +24
	lw	$5,8($sp)
	lw	$2,24($16)
	slt	$2,$5,$2
	beqz	$2,.Llibkernl_sif_cmd_dma_handler_done
	sll	$2,$5,0x3
	lw	$3,20($16)
	addu	$2,$2,$3
	lw	$6,0($2)
	beqz	$6,.Llibkernl_sif_cmd_dma_handler_done
	nop
	lw	$5,4($2)
	jalr	$6
	move	$4,$sp
.Llibkernl_sif_cmd_dma_handler_done:
	sync
	ei
	move	$2,$0
.Llibkernl_sif_cmd_dma_handler_out:
	ld	$31,128($sp)
	ld	$16,112($sp)
	jr	$31
	addiu	$sp,$sp,144
	.end	libkernl_sif_cmd_dma_handler

/*
 * Sony libkernel. Writes back and invalidates the data-cache lines of [p, p + size) (cache 0x18 = DHWBIN), eight lines per loop.
 * Arguments: $4 = address, $5 = size in bytes (nothing happens when size <= 0). Leaf routine using
 * t0-t3 and t9; the odd lines first, then blocks of 8 lines (512 bytes). Called by the SIF RPC code
 * 0x1e1ee0-0x1e2c08 before they hand buffers to the IOP.
 */
/* 0x001f83f8 */
	.set	noreorder
	.text
	.globl	sceSifWriteBackDCache
	.ent	sceSifWriteBackDCache
sceSifWriteBackDCache:
	# $25 = ~63; $8 = first line, $11 = number of lines
	lui	$25,0xffff
	ori	$25,$25,0xffc0
	blez	$5,.LsceSifWriteBackDCache_done
	addu	$10,$4,$5
	and	$8,$4,$25
	addiu	$10,$10,-1
	and	$9,$10,$25
	subu	$10,$9,$8
	srl	$11,$10,0x6
	addiu	$11,$11,1
	# $9 = lines % 8 (done one by one), $10 = lines / 8
	andi	$9,$11,0x7
	beqz	$9,.LsceSifWriteBackDCache_blocks
	srl	$10,$11,0x3
.LsceSifWriteBackDCache_single:
	sync
	cache	0x18,0($8)
	sync
	addiu	$9,$9,-1
	nop
	bgtz	$9,.LsceSifWriteBackDCache_single
	addiu	$8,$8,64
.LsceSifWriteBackDCache_blocks:
	# the delay slot of this beqz is the first instruction of the block loop
	beqz	$10,.LsceSifWriteBackDCache_done
.LsceSifWriteBackDCache_block:
	addiu	$10,$10,-1
	sync
	cache	0x18,0($8)
	sync
	cache	0x18,64($8)
	sync
	cache	0x18,128($8)
	sync
	cache	0x18,192($8)
	sync
	cache	0x18,256($8)
	sync
	cache	0x18,320($8)
	sync
	cache	0x18,384($8)
	sync
	cache	0x18,448($8)
	sync
	bgtz	$10,.LsceSifWriteBackDCache_block
	addiu	$8,$8,512
.LsceSifWriteBackDCache_done:
	jr	$31
	nop
	.end	sceSifWriteBackDCache

/*
 * Sony libkernel. Empty routine: a single jr ra whose delay slot is the first instruction of the next object (0x1f7e68) (unreferenced).
 * BOUNDARY: 4 bytes, the last word of the object that starts at 0x1f7db8 (0x1f7e64 is not 8-aligned,
 * so it cannot start an object of its own). No code in the executable calls it.
 */
/* 0x001f84a4 */
	.set	noreorder
	.text
	.globl	libkernl_sif_nop
	.ent	libkernl_sif_nop
libkernl_sif_nop:
	jr	$31
	.end	libkernl_sif_nop
