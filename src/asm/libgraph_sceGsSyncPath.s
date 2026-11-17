/*
 * libgraph_sceGsSyncPath.s: hand-written assembly, retail (US v1.01) 0x001df4d0-0x001df4d0.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Retail 0x001df4d0. * Sony libgraph, sceGsSyncPath(mode, timeout). mode 0 waits for DMA
 * channels 1 and 2, VIF1, VU1 (VPU-STAT VBS1, read with cfc2 $vi29) and the GIF paths, sharing one timeout
 * counter; on a timeout prints which unit hung, dumps the DMA/VIF1/GIF registers and returns -1, else 0.
 * Other modes return a busy mask (1 D1, 2 D2, 4 VIF1, 8 VU1, 16 GIF). The original is C with inline
 * assembly for the cfc2; The C reconstruction kept for the native port is in the .c of this unit.
 */
/* 0x001df4d0 */
	.set	noreorder
	.text
	.globl	sceGsSyncPath
	.ent	sceGsSyncPath
sceGsSyncPath:
	addiu	$29,$29,-16
	move	$5,$0
	bnez	$4,.LsceGsSyncPath_17
	sd	$31,0($29)
	lui	$2,0x1000
	ori	$2,$2,0x9000
	lw	$3,0($2)
	andi	$3,$3,0x100
	beqz	$3,.LsceGsSyncPath_2
	lui	$3,0x1000
	lui	$4,0x100
	ori	$3,$3,0x9000
	move	$2,$5
	nop
.LsceGsSyncPath_1:
	sltu	$2,$4,$2
	bnez	$2,.LsceGsSyncPath_11
	addiu	$5,$5,1
	lw	$2,0($3)
	andi	$2,$2,0x100
	bnez	$2,.LsceGsSyncPath_1
	move	$2,$5
.LsceGsSyncPath_2:
	lui	$2,0x1000
	ori	$2,$2,0xa000
	lw	$3,0($2)
	andi	$3,$3,0x100
	beqz	$3,.LsceGsSyncPath_4
	lui	$3,0x1000
	lui	$4,0x100
	ori	$3,$3,0xa000
	move	$2,$5
.LsceGsSyncPath_3:
	sltu	$2,$4,$2
	bnez	$2,.LsceGsSyncPath_12
	addiu	$5,$5,1
	lw	$2,0($3)
	andi	$2,$2,0x100
	bnez	$2,.LsceGsSyncPath_3
	move	$2,$5
.LsceGsSyncPath_4:
	lui	$2,0x1000
	lui	$4,0x1f00
	ori	$2,$2,0x3c00
	ori	$4,$4,0x3
	lw	$3,0($2)
	and	$3,$3,$4
	beqz	$3,.LsceGsSyncPath_6
	lui	$3,0x1f00
	lui	$4,0x1000
	lui	$6,0x100
	ori	$4,$4,0x3c00
	ori	$3,$3,0x3
	move	$2,$5
.LsceGsSyncPath_5:
	sltu	$2,$6,$2
	bnez	$2,.LsceGsSyncPath_13
	addiu	$5,$5,1
	lw	$2,0($4)
	and	$2,$2,$3
	bnez	$2,.LsceGsSyncPath_5
	move	$2,$5
.LsceGsSyncPath_6:
	cfc2	$6,$vi29
	andi	$2,$6,0x100
	beqz	$2,.LsceGsSyncPath_8
	lui	$3,0x100
	move	$2,$5
.LsceGsSyncPath_7:
	sltu	$2,$3,$2
	bnez	$2,.LsceGsSyncPath_14
	addiu	$5,$5,1
	cfc2	$6,$vi29
	andi	$2,$6,0x100
	bnez	$2,.LsceGsSyncPath_7
	move	$2,$5
.LsceGsSyncPath_8:
	lui	$2,0x1000
	ori	$2,$2,0x3020
	lw	$3,0($2)
	andi	$3,$3,0xc00
	beqz	$3,.LsceGsSyncPath_10
	lui	$3,0x1000
	lui	$4,0x100
	ori	$3,$3,0x3020
	move	$2,$5
.LsceGsSyncPath_9:
	sltu	$2,$4,$2
	bnez	$2,.LsceGsSyncPath_15
	addiu	$5,$5,1
	lw	$2,0($3)
	andi	$2,$2,0xc00
	bnez	$2,.LsceGsSyncPath_9
	move	$2,$5
.LsceGsSyncPath_10:
	b	.LsceGsSyncPath_18
	move	$2,$0
.LsceGsSyncPath_11:
	lui	$4,%hi(str_gs_sync_path_ch1)
	b	.LsceGsSyncPath_16
	addiu	$4,$4,%lo(str_gs_sync_path_ch1)
.LsceGsSyncPath_12:
	lui	$4,%hi(str_gs_sync_path_ch2)
	b	.LsceGsSyncPath_16
	addiu	$4,$4,%lo(str_gs_sync_path_ch2)
.LsceGsSyncPath_13:
	lui	$4,%hi(str_gs_sync_path_vif1)
	b	.LsceGsSyncPath_16
	addiu	$4,$4,%lo(str_gs_sync_path_vif1)
.LsceGsSyncPath_14:
	lui	$4,%hi(str_gs_sync_path_vu1)
	b	.LsceGsSyncPath_16
	addiu	$4,$4,%lo(str_gs_sync_path_vu1)
.LsceGsSyncPath_15:
	lui	$4,%hi(str_gs_sync_path_gif)
	addiu	$4,$4,%lo(str_gs_sync_path_gif)
.LsceGsSyncPath_16:
	jal	printf
	nop
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d1_chcr)
	ori	$3,$3,0x9000
	addiu	$4,$4,%lo(str_gs_sync_path_d1_chcr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d1_tadr)
	ori	$3,$3,0x9030
	addiu	$4,$4,%lo(str_gs_sync_path_d1_tadr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d1_madr)
	ori	$3,$3,0x9010
	addiu	$4,$4,%lo(str_gs_sync_path_d1_madr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d1_qwc)
	ori	$3,$3,0x9020
	addiu	$4,$4,%lo(str_gs_sync_path_d1_qwc)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d2_chcr)
	ori	$3,$3,0xa000
	addiu	$4,$4,%lo(str_gs_sync_path_d2_chcr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d2_tadr)
	ori	$3,$3,0xa030
	addiu	$4,$4,%lo(str_gs_sync_path_d2_tadr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d2_madr)
	ori	$3,$3,0xa010
	addiu	$4,$4,%lo(str_gs_sync_path_d2_madr)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_d2_qwc)
	ori	$3,$3,0xa020
	addiu	$4,$4,%lo(str_gs_sync_path_d2_qwc)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_vif1_stat)
	ori	$3,$3,0x3c00
	addiu	$4,$4,%lo(str_gs_sync_path_vif1_stat)
	jal	printf
	lw	$5,0($3)
	lui	$3,0x1000
	lui	$4,%hi(str_gs_sync_path_gif_stat)
	ori	$3,$3,0x3020
	addiu	$4,$4,%lo(str_gs_sync_path_gif_stat)
	jal	printf
	lw	$5,0($3)
	b	.LsceGsSyncPath_18
	li	$2,-1
.LsceGsSyncPath_17:
	lui	$3,0x1000
	lui	$5,0x1000
	ori	$3,$3,0x9000
	ori	$5,$5,0xa000
	lw	$2,0($3)
	lui	$4,0x1000
	lw	$6,0($5)
	ori	$4,$4,0x3c00
	andi	$2,$2,0x100
	lw	$5,0($4)
	sltu	$2,$0,$2
	andi	$6,$6,0x100
	ori	$4,$2,0x2
	lui	$3,0x1f00
	movn	$2,$4,$6
	ori	$3,$3,0x3
	and	$5,$5,$3
	ori	$4,$2,0x4
	movn	$2,$4,$5
	cfc2	$6,$vi29
	lui	$3,0x1000
	andi	$6,$6,0x100
	ori	$3,$3,0x3020
	ori	$5,$2,0x8
	lw	$4,0($3)
	movn	$2,$5,$6
	ori	$3,$2,0x10
	andi	$4,$4,0xc00
	movn	$2,$3,$4
.LsceGsSyncPath_18:
	ld	$31,0($29)
	jr	$31
	addiu	$29,$29,16
	.end	sceGsSyncPath
