/*
 * ul_gx_skin.s: hand-written assembly, retail (US v1.01) 0x001c3620-0x001c3c14.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). ul_gx first weighted skinning pass with lighting: stores the bone contribution in the indexed vertex.
 * Arguments: $4 = ul_gx context (bone matrix +192, light directions +384, light colours +448, weight
 * scale +1232.w), $5 = 28-byte records (position, normal, index/weight word at +24), $6 = vertex buffer
 * (64-byte entries), $7 = count. Stores weight * position at +16, weight * lit colour at +32 and
 * weight * view-space normal at +48. Returns 0. Called from 0x1c23b8 (opcode 0) and 0x1c2488.
 */
/* 0x001c3620 */
	.set	noreorder
	.text
	.globl	vu0_gx_skin_lit_first
	.ent	vu0_gx_skin_lit_first
vu0_gx_skin_lit_first:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf30,1232($4)
.Lvu0_gx_skin_lit_first_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, $11): pack four words
	pextlw	$8,$9,$8
	pextlw	$10,$11,$10
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	vitof0.w	$vf1,$vf1		# vf1.w = vf1.w integer to float
	# vf1.w = weight * scale
	vmulw.w	$vf1,$vf1,$vf30w		# vf1.w = vf1.w*vf30.w
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf1w	# vf24.xyz = vf24*vf1.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmulw.xyz	$vf26,$vf26,$vf1w	# vf26.xyz = vf26*vf1.w
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_skin_lit_first_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_skin_lit_first

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). ul_gx middle weighted skinning pass with lighting: adds the bone contribution to the indexed vertex.
 * Arguments as 0x1c3000. Adds weight * position to +16, weight * lit colour to +32 and weight *
 * view-space normal.xy to +48. Returns 0. Called from 0x1c23b8 (opcode 0x100) and 0x1c2488.
 */
/* 0x001c370c */
	.set	noreorder
	.text
	.globl	vu0_gx_skin_lit_mid
	.ent	vu0_gx_skin_lit_mid
vu0_gx_skin_lit_mid:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf30,1232($4)
.Lvu0_gx_skin_lit_mid_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, $11): pack four words
	pextlw	$8,$9,$8
	pextlw	$10,$11,$10
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	vitof0.w	$vf1,$vf1		# vf1.w = vf1.w integer to float
	# vf1.w = weight * scale
	vmulw.w	$vf1,$vf1,$vf30w		# vf1.w = vf1.w*vf30.w
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf1w	# vf24.xyz = vf24*vf1.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmulw.xyz	$vf26,$vf26,$vf1w	# vf26.xyz = vf26*vf1.w
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# previous sums: vf2 = position, vf3 = colour, vf4 = normal
	lqc2	$vf2,16($12)
	lqc2	$vf3,32($12)
	lqc2	$vf4,48($12)
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	vadd.xyzw	$vf25,$vf25,$vf3	# vf25 = vf25 + vf3
	vadd.xy	$vf26,$vf26,$vf4		# vf26.xy = vf26 + vf4
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_skin_lit_mid_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_skin_lit_mid

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). ul_gx last weighted skinning pass with lighting and environment map, no clip test: adds the bone contribution and projects.
 * Arguments as 0x1c3000 (also projection +128, map scale/offset +1264/+1280). Entry: +16 screen XYZ
 * (12.4 fixed point), +32 summed lit colour (w = 1), +48 (s, t, Q, -) from the summed normal.
 * Returns 0. Called from 0x1c2488.
 */
/* 0x001c3810 */
	.set	noreorder
	.text
	.globl	vu0_gx_skin_lit_env_last
	.ent	vu0_gx_skin_lit_env_last
vu0_gx_skin_lit_env_last:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1232($4)
.Lvu0_gx_skin_lit_env_last_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, $11): pack four words
	pextlw	$8,$9,$8
	pextlw	$10,$11,$10
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	vitof0.w	$vf1,$vf1		# vf1.w = vf1.w integer to float
	# vf1.w = weight * scale
	vmulw.w	$vf1,$vf1,$vf30w		# vf1.w = vf1.w*vf30.w
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf1w	# vf24.xyz = vf24*vf1.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmulw.xyz	$vf26,$vf26,$vf1w	# vf26.xyz = vf26*vf1.w
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# previous sums: vf2 = position, vf3 = colour, vf4 = normal
	lqc2	$vf2,16($12)
	lqc2	$vf3,32($12)
	lqc2	$vf4,48($12)
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vadd.xyzw	$vf25,$vf25,$vf3	# vf25 = vf25 + vf3
	vmove.w	$vf25,$vf0			# vf25.w = 1
	# (s, t) = (summed normal).xy * scale + offset
	vadd.xy	$vf26,$vf26,$vf4		# vf26.xy = vf26 + vf4
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vadd.xy	$vf26,$vf26,$vf29		# vf26.xy = vf26 + vf29
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_skin_lit_env_last_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_skin_lit_env_last

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). ul_gx last weighted skinning pass with lighting, environment map and a guard-band test.
 * Arguments as 0x1c3000 (also projection +128, map scale/offset +1264/+1280, guard band +1168/+1184).
 * Entry: +0 summed position, +16 screen XYZ (12.4 fixed point), +32 summed lit colour (w = 1),
 * +48 (s, t, Q, clip flags). Adds the number of clipped vertices to $2, which it does not clear.
 * Called from 0x1c23b8 (opcode 0x200).
 */
/* 0x001c395c */
	.set	noreorder
	.text
	.globl	vu0_gx_skin_lit_env_last_clip
	.ent	vu0_gx_skin_lit_env_last_clip
vu0_gx_skin_lit_env_last_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	lqc2	$vf7,1232($4)
.Lvu0_gx_skin_lit_env_last_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, $11): pack four words
	pextlw	$8,$9,$8
	pextlw	$10,$11,$10
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	vitof0.w	$vf1,$vf1		# vf1.w = vf1.w integer to float
	# vf1.w = weight * scale
	vmulw.w	$vf1,$vf1,$vf7w			# vf1.w = vf1.w*vf7.w
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf1w	# vf24.xyz = vf24*vf1.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmulw.xyz	$vf26,$vf26,$vf1w	# vf26.xyz = vf26*vf1.w
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# previous sums: vf2 = position, vf3 = colour, vf4 = normal
	lqc2	$vf2,16($12)
	lqc2	$vf3,32($12)
	lqc2	$vf4,48($12)
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	sqc2	$vf24,0($12)
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vadd.xyzw	$vf25,$vf25,$vf3	# vf25 = vf25 + vf3
	vmove.w	$vf25,$vf0			# vf25.w = 1
	# (s, t) = (summed normal).xy * scale + offset
	vadd.xy	$vf26,$vf26,$vf4		# vf26.xy = vf26 + vf4
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vadd.xy	$vf26,$vf26,$vf29		# vf26.xy = vf26 + vf29
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lvu0_gx_skin_lit_env_last_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_gx_skin_lit_env_last_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_skin_lit_env_last_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	vu0_gx_skin_lit_env_last_clip

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). Strip emitter with back-face marking: GS STQ/RGBAQ/XYZ for an index list until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1c06b4. Arguments: $5 = vertex indices (shorts), $6 = count
 * (negative = opposite winding), $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex).
 * Back-facing triangles get 0x8000 (ADC) in XYZ word 3. Returns the number emitted. Called from 0x1c2cb0.
 */
/* 0x001c3aec */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_tex_strip_cull_clip
	.ent	vu0_gx_emit_tex_strip_cull_clip
vu0_gx_emit_tex_strip_cull_clip:
	movz	$2,$0,$0
	# $13 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$13,$6,$0
	sll	$13,$13,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_tex_strip_cull_clip_loop:
	# $12 = the indexed vertex; stop if it has clip flags
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lw	$11,60($12)
	bnez	$11,.Lvu0_gx_emit_tex_strip_cull_clip_done
	lqc2	$vf24,16($12)
	lqc2	$vf25,32($12)
	lqc2	$vf26,48($12)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y; STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	# winding: z of (prev - prev2) x (prev - this), screen xy as floats
	vitof4.xyzw	$vf7,$vf24		# vf7 = vf24 12.4 fixed point to float
	vsub.xy	$vf1,$vf6,$vf5			# vf1.xy = vf6 - vf5
	vsub.xy	$vf2,$vf6,$vf7			# vf2.xy = vf6 - vf7
	# vf4.xyz = vf1 x vf2 (cross product)
	vopmula.xyz	ACC,$vf1,$vf2
	vopmsub.xyz	$vf4,$vf2,$vf1
	vmove.xyzw	$vf5,$vf6		# vf5 = vf6
	vmove.xyzw	$vf6,$vf7		# vf6 = vf7
	vnop
	vnop
	# back-facing (sign of z, flipped every vertex) -> 0x8000 in XYZ word 3
	cfc2	$11,$vi17			# $11 = MAC flags (bit 5 = sign of the last result z)
	andi	$11,$11,0x20
	xor	$11,$11,$13
	sll	$11,$11,0xa
	sw	$11,44($8)
	xori	$13,$13,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lvu0_gx_emit_tex_strip_cull_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_tex_strip_cull_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_tex_strip_cull_clip

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). Emits GS STQ/RGBAQ/XYZ qwords for an index list until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1c06b4. Arguments: $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex). Returns the number emitted.
 * Called from 0x1c2e60.
 */
/* 0x001c3ba8 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_tex_strip_clip
	.ent	vu0_gx_emit_tex_strip_clip
vu0_gx_emit_tex_strip_clip:
	movz	$2,$0,$0
.Lvu0_gx_emit_tex_strip_clip_loop:
	# $14 = the indexed vertex; stop if it has clip flags
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lw	$11,60($14)
	bnez	$11,.Lvu0_gx_emit_tex_strip_clip_done
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y; STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lvu0_gx_emit_tex_strip_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_tex_strip_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_tex_strip_clip

/*
 * PS2 platform layer ul_gx vertex converters (object at 0x1c3000-0x1c3644). Copies indexed vertices into a 64-byte list for the clipper, turning the 12.4 screen XYZ back into floats.
 * Same code as 0x1c0b50 ($4 = context, vf8/vf9 loaded but not used; $5 = vertex indices, $6 = count,
 * $7 = vertex buffer, $8 = 64-byte output entries). Called from 0x1c2cb0 and 0x1c2e60. Last routine of the
 * object (0x1c3644 is padding before the CRI code at 0x1c3648).
 */
/* 0x001c3c14 */
	.set	noreorder
	.text
	.globl	vu0_gx_clip_copy_b
	.ent	vu0_gx_clip_copy_b
vu0_gx_clip_copy_b:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
.Lvu0_gx_clip_copy_b_loop:
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	lqc2	$vf27,0($14)
	vitof4.xyz	$vf24,$vf24		# vf24.xyz = vf24 12.4 fixed point to float
	sqc2	$vf25,32($8)
	sqc2	$vf26,48($8)
	sqc2	$vf27,0($8)
	sqc2	$vf24,16($8)
	addi	$6,$6,-1
	addi	$8,$8,64
	bnez	$6,.Lvu0_gx_clip_copy_b_loop
	addi	$5,$5,2
	jr	$31
	nop
	.end	vu0_gx_clip_copy_b
