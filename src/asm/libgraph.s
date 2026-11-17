/*
 * libgraph.s: hand-written assembly, retail (US v1.01) 0x001deff0-0x001deff0.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Retail 0x001deff0. * Sony libgraph, sceGsResetPath (proposed name). Resets VIF1 (FBRST.RST,
 * ERR.ME0), VU1 (FBRST bit 9 through VU0 control register vi28: cfc2/ori/ctc2 between sync.l and sync.p),
 * sends the two VIF1 initialisation quadwords at 0x295700 (retail) to the VIF1 FIFO and resets the GIF (GIF_CTRL.RST).
 * The original is C with inline assembly for the VU1 reset, which plain C cannot produce;
 * The C reconstruction kept for the native port is in the .c of this unit.
 */
/* 0x001deff0 */
	.set	noreorder
	.text
	.globl	sceGsResetPath
	.ent	sceGsResetPath
sceGsResetPath:
	lui	$2,0x1000
	li	$7,1
	ori	$2,$2,0x3c10
	lui	$3,0x1000
	sw	$7,0($2)
	ori	$3,$3,0x3c20
	li	$2,2
	move	$4,$0
	sw	$2,0($3)
	sync
	cfc2	$4,$vi28
	ori	$4,$4,0x200
	ctc2	$4,$vi28
	sync.p
	lui	$5,%hi(libgraph_vif1_init_data)
	lui	$6,0x1000
	addiu	$5,$5,%lo(libgraph_vif1_init_data)
	ori	$6,$6,0x5000
	lq	$4,0($5)
	lui	$3,0x1000
	ori	$3,$3,0x3000
	sq	$4,0($6)
	lq	$2,16($5)
	sq	$2,0($6)
	jr	$31
	sw	$7,0($3)
	.end	sceGsResetPath
