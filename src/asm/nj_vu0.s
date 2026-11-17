/*
 * nj_vu0.s: hand-written assembly, retail (US v1.01) 0x001ae9b4-0x001b19dc.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Converts n textured vertices for the clipper: transform, clip test, colour and texture coordinates.
 * Arguments: $4 = Ninja system block (matrix +192, clip matrix +768, colour scale +1248.w), $5 = vertices
 * (x, y, z, u, v, packed BGRA: 24 bytes), $6 = count, $7 = 64-byte output entries: +0 position * matrix
 * +192, +16 clip-space position, +32 colour as floats, +48 (u, v, -, clip flags).
 */
/* 0x001ae9b4 */
	.set	noreorder
	.text
	.globl	nj_vu0_clip_tex_vertices
	.ent	nj_vu0_clip_tex_vertices
nj_vu0_clip_tex_vertices:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf28,768($4)
	lqc2	$vf29,784($4)
	lqc2	$vf30,800($4)
	lqc2	$vf31,816($4)
	lqc2	$vf7,1248($4)
.Lnj_vu0_clip_tex_vertices_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,20($5)
	lw	$12,12($5)
	lw	$13,16($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf28*vf1.x + vf29*vf1.y + vf30*vf1.z + vf31
	vmulax.xyzw	ACC,$vf28,$vf1x
	vmadday.xyzw	ACC,$vf29,$vf1y
	vmaddaz.xyzw	ACC,$vf30,$vf1z
	vmaddw.xyzw	$vf4,$vf31,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	# vf3 = (u, v, -, -)
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf3			# vf3 = $12 (128 bits)
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmulw.xyzw	$vf2,$vf2,$vf7w		# vf2 = vf2*vf7.w
	# keep the clip flags of this vertex
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	sqc2	$vf1,0($7)
	sqc2	$vf2,32($7)
	sqc2	$vf3,48($7)
	sqc2	$vf4,16($7)
	sw	$8,60($7)
	addi	$6,$6,-1
	addi	$5,$5,24
	bnez	$6,.Lnj_vu0_clip_tex_vertices_loop
	addi	$7,$7,64
	jr	$31
	nop
	.end	nj_vu0_clip_tex_vertices

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Converts the leading run of unclipped textured vertices straight to GS STQ/RGBAQ/XYZF qwords.
 * Arguments: $4 = Ninja system block (screen matrix +320, clip matrix +768, fog parameters +1376),
 * $5 = vertices (x, y, z, u, v, packed BGRA: 24 bytes), $6 = count, $7 = output (48 bytes per vertex).
 * Stops at the first vertex that fails the clip test. Returns the number of vertices converted.
 */
/* 0x001aea70 */
	.set	noreorder
	.text
	.globl	nj_vu0_emit_tex_vertices
	.ent	nj_vu0_emit_tex_vertices
nj_vu0_emit_tex_vertices:
	lqc2	$vf8,320($4)
	lqc2	$vf9,336($4)
	lqc2	$vf10,352($4)
	lqc2	$vf11,368($4)
	lqc2	$vf28,768($4)
	lqc2	$vf29,784($4)
	lqc2	$vf30,800($4)
	lqc2	$vf31,816($4)
	lqc2	$vf19,1376($4)
	movz	$2,$0,$0
.Lnj_vu0_emit_tex_vertices_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf28*vf1.x + vf29*vf1.y + vf30*vf1.z + vf31
	vmulax.xyzw	ACC,$vf28,$vf1x
	vmadday.xyzw	ACC,$vf29,$vf1y
	vmaddaz.xyzw	ACC,$vf30,$vf1z
	vmaddw.xyzw	$vf4,$vf31,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	lw	$8,12($5)
	lw	$9,16($5)
	vnop
	vnop
	# stop at the first clipped vertex
	cfc2	$12,$vi18			# $12 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$12,$12,0x3f
	bnez	$12,.Lnj_vu0_emit_tex_vertices_done
	nop
	# project to the screen and divide by w
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vdiv	Q,$vf0w,$vf1w			# Q = 1 / vf1.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf1,$vf1,Q		# vf1.xyz = vf1*Q
	# fog: vf4.w = clamp(vf19.w + w * vf19.x, 0, vf19.z)
	vmulaw.w	ACC,$vf0,$vf19w
	vmaddx.w	$vf4,$vf1,$vf19x
	vminiz.w	$vf4,$vf4,$vf19z	# vf4.w = min(vf4.w, vf19.z)
	vmaxx.w	$vf4,$vf4,$vf0x			# vf4.w = max(vf4.w, 0)
	vftoi4.w	$vf1,$vf4		# vf1.w = vf4.w as 12.4 fixed point
	vftoi4.xyz	$vf1,$vf1		# vf1.xyz = vf1 as 12.4 fixed point
	# STQ: (u * Q, v * Q, Q)
	pextlw	$8,$9,$8			# $8 = qword ($8, $9, -, -)
	qmtc2	$8,$vf3				# vf3 = $8 (128 bits)
	vmulq.xy	$vf3,$vf3,Q		# vf3.xy = vf3*Q
	vaddq.z	$vf3,$vf0,Q			# vf3.z = Q
	# write STQ, RGBAQ (four colour words) and XYZF (12.4 fixed point, fog in w)
	sq	$11,16($7)
	sqc2	$vf1,32($7)
	sqc2	$vf3,0($7)
	addi	$6,$6,-1
	addi	$2,$2,1
	addi	$5,$5,24
	bnez	$6,.Lnj_vu0_emit_tex_vertices_loop
	addi	$7,$7,48
.Lnj_vu0_emit_tex_vertices_done:
	jr	$31
	nop
	.end	nj_vu0_emit_tex_vertices

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Converts n untextured vertices for the clipper: transform, clip test and colour.
 * Arguments: $4 = Ninja system block (matrix +192, clip matrix +768, colour scale +1248), $5 = vertices
 * (x, y, z, packed BGRA: 16 bytes), $6 = count, $7 = 64-byte output entries: +0 position * matrix +192,
 * +16 clip-space position, +32 colour as floats, +60 clip flags.
 */
/* 0x001aeb5c */
	.set	noreorder
	.text
	.globl	nj_vu0_clip_color_vertices
	.ent	nj_vu0_clip_color_vertices
nj_vu0_clip_color_vertices:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf28,768($4)
	lqc2	$vf29,784($4)
	lqc2	$vf30,800($4)
	lqc2	$vf31,816($4)
	lqc2	$vf7,1248($4)
.Lnj_vu0_clip_color_vertices_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf28*vf1.x + vf29*vf1.y + vf30*vf1.z + vf31
	vmulax.xyzw	ACC,$vf28,$vf1x
	vmadday.xyzw	ACC,$vf29,$vf1y
	vmaddaz.xyzw	ACC,$vf30,$vf1z
	vmaddw.xyzw	$vf4,$vf31,$vf0w
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmul.xyzw	$vf2,$vf2,$vf7		# vf2 = vf2*vf7
	sqc2	$vf1,0($7)
	sqc2	$vf2,32($7)
	sqc2	$vf4,16($7)
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	sw	$8,60($7)
	addi	$6,$6,-1
	addi	$5,$5,16
	bnez	$6,.Lnj_vu0_clip_color_vertices_loop
	addi	$7,$7,64
	jr	$31
	nop
	.end	nj_vu0_clip_color_vertices

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Converts the leading run of unclipped untextured vertices straight to GS STQ/RGBAQ/XYZF qwords.
 * Arguments: $4 = Ninja system block (screen matrix +320, clip matrix +768, fog parameters +1376),
 * $5 = vertices (x, y, z, packed BGRA: 16 bytes), $6 = count, $7 = output (48 bytes per vertex).
 * Stops at the first vertex that fails the clip test. Returns the number of vertices converted.
 */
/* 0x001aec04 */
	.set	noreorder
	.text
	.globl	nj_vu0_emit_color_vertices
	.ent	nj_vu0_emit_color_vertices
nj_vu0_emit_color_vertices:
	lqc2	$vf8,320($4)
	lqc2	$vf9,336($4)
	lqc2	$vf10,352($4)
	lqc2	$vf11,368($4)
	lqc2	$vf28,768($4)
	lqc2	$vf29,784($4)
	lqc2	$vf30,800($4)
	lqc2	$vf31,816($4)
	lqc2	$vf7,1040($4)
	lqc2	$vf19,1376($4)
	movz	$2,$0,$0
.Lnj_vu0_emit_color_vertices_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf28*vf1.x + vf29*vf1.y + vf30*vf1.z + vf31
	vmulax.xyzw	ACC,$vf28,$vf1x
	vmadday.xyzw	ACC,$vf29,$vf1y
	vmaddaz.xyzw	ACC,$vf30,$vf1z
	vmaddw.xyzw	$vf4,$vf31,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	vnop
	vnop
	vnop
	# stop at the first clipped vertex
	cfc2	$12,$vi18			# $12 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$12,$12,0x3f
	bnez	$12,.Lnj_vu0_emit_color_vertices_done
	nop
	# project to the screen and divide by w
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vdiv	Q,$vf0w,$vf1w			# Q = 1 / vf1.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf1,$vf1,Q		# vf1.xyz = vf1*Q
	lqc2	$vf19,1376($4)
	# fog: vf4.w = clamp(vf19.w + w * vf19.x, 0, vf19.z)
	vmulaw.w	ACC,$vf0,$vf19w
	vmaddx.w	$vf4,$vf1,$vf19x
	vminiz.w	$vf4,$vf4,$vf19z	# vf4.w = min(vf4.w, vf19.z)
	vmaxx.w	$vf4,$vf4,$vf0x			# vf4.w = max(vf4.w, 0)
	vftoi4.w	$vf1,$vf4		# vf1.w = vf4.w as 12.4 fixed point
	vftoi4.xyz	$vf1,$vf1		# vf1.xyz = vf1 as 12.4 fixed point
	# write STQ (only Q is meaningful), RGBAQ and XYZF (12.4 fixed point, fog in w)
	vaddq.z	$vf3,$vf0,Q			# vf3.z = Q
	sqc2	$vf3,0($7)
	sq	$11,16($7)
	sqc2	$vf1,32($7)
	addi	$6,$6,-1
	addi	$2,$2,1
	addi	$5,$5,16
	bnez	$6,.Lnj_vu0_emit_color_vertices_loop
	addi	$7,$7,48
.Lnj_vu0_emit_color_vertices_done:
	jr	$31
	nop
	.end	nj_vu0_emit_color_vertices

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with lighting: adds the weighted bone contribution, projects and clip-tests.
 * Arguments: $4 = Ninja system block, $5 = 28-byte records (position, normal, index/weight word at +24),
 * $6 = vertex buffer (64-byte entries), $7 = count. Uses the bone matrix +192, projection +128, light
 * directions +384, light colours +448, guard-band scale/offset +1168/+1184 and weight scale +1232.w.
 * Entry: +0 summed position, +16 screen XYZ (12.4 fixed point), +32 lit colour (w = 1), +48 weighted
 * normal with z = Q and w = clip flags. Returns the number of vertices with any clip flag set.
 */
/* 0x001aece8 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_skin_lit_last_clip
	.ent	nj_vu0_gs_skin_lit_last_clip
nj_vu0_gs_skin_lit_last_clip:
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
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	lqc2	$vf7,1232($4)
.Lnj_vu0_gs_skin_lit_last_clip_loop:
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
	# previous sums: vf2 = position, vf3 = colour
	lqc2	$vf2,16($12)
	lqc2	$vf3,32($12)
	lqc2	$vf4,48($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	sqc2	$vf24,0($12)
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	vadd.xyzw	$vf25,$vf25,$vf3	# vf25 = vf25 + vf3
	vmove.w	$vf25,$vf0			# vf25.w = 1
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
	beqz	$8,.Lnj_vu0_gs_skin_lit_last_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_gs_skin_lit_last_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_skin_lit_last_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_gs_skin_lit_last_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and projects n vertices with an environment map: texture coordinates from the view-space normal.
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, colour scale +1248, map
 * scale/offset +1264/+1280, guard-band scale/offset +1168/+1184), $5 = vertices (position, normal,
 * packed BGRA: 28 bytes), $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16
 * screen XYZ (12.4 fixed point), +32 colour, +48 (s, t, Q, clip flags). Returns the clipped count.
 */
/* 0x001aee64 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_env_clip
	.ent	nj_vu0_gs_env_clip
nj_vu0_gs_env_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf23,1248($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	movz	$2,$0,$0
.Lnj_vu0_gs_env_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	# s, t = view-space normal.xy * scale + offset
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vadd.xy	$vf26,$vf26,$vf29		# vf26.xy = vf26 + vf29
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	lw	$8,24($5)
	# $8 = its four bytes zero-extended to four words
	pextlb	$8,$0,$8
	pextlh	$8,$0,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmulx.xyzw	$vf25,$vf2,$vf23x	# vf25 = vf2*vf23.x
	vmove.w	$vf25,$vf0			# vf25.w = 1
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_gs_env_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_gs_env_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_env_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_gs_env_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and projects n vertices with packed colours, with a guard-band clip test.
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, colour scale +1248, guard-band
 * scale/offset +1168/+1184), $5 = vertices (x, y, z, packed BGRA: 16 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +0 view-space position, +16 screen XYZ (12.4 fixed point), +32 colour,
 * +48 (-, -, Q, clip flags). Returns the number of vertices with any clip flag set.
 */
/* 0x001aef80 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_col_clip
	.ent	nj_vu0_gs_col_clip
nj_vu0_gs_col_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf23,1248($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	movz	$2,$0,$0
.Lnj_vu0_gs_col_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	lw	$8,12($5)
	# $8 = its four bytes zero-extended to four words
	pextlb	$8,$0,$8
	pextlh	$8,$0,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmulx.xyzw	$vf25,$vf2,$vf23x	# vf25 = vf2*vf23.x
	vmove.w	$vf25,$vf0			# vf25.w = 1
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_gs_col_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_gs_col_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_col_clip_loop
	addi	$5,$5,16
	jr	$31
	nop
	.end	nj_vu0_gs_col_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms, lights and projects n vertices with normals, with a guard-band clip test.
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, light directions +384, light
 * colours +448, guard-band scale/offset +1168/+1184), $5 = vertices (position, normal: 24 bytes),
 * $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16 screen XYZ (12.4 fixed
 * point), +32 lit colour (w = 1), +48 (-, -, Q, clip flags). Returns the clipped count.
 */
/* 0x001af084 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_lit_clip
	.ent	nj_vu0_gs_lit_clip
nj_vu0_gs_lit_clip:
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
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	movz	$2,$0,$0
.Lnj_vu0_gs_lit_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
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
	vmove.w	$vf25,$vf0			# vf25.w = 1
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_gs_lit_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_gs_lit_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_lit_clip_loop
	addi	$5,$5,24
	jr	$31
	nop
	.end	nj_vu0_gs_lit_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and projects n bare positions (white colour), with a guard-band clip test.
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, guard-band scale/offset
 * +1168/+1184), $5 = positions (12 bytes), $6 = 64-byte output entries, $7 = count. Entry: +0 view-space
 * position, +16 screen XYZ (12.4 fixed point), +32 (1, 1, 1, 1), +48 (-, -, Q, clip flags).
 * Returns the number of vertices with any clip flag set.
 */
/* 0x001af1b0 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_pos_clip
	.ent	nj_vu0_gs_pos_clip
nj_vu0_gs_pos_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf23,1248($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	movz	$2,$0,$0
.Lnj_vu0_gs_pos_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	# vf25 = (1, 1, 1, 1)
	vmove.xyzw	$vf25,$vf0		# vf25 = (0,0,0,1)
	vaddw.xyz	$vf25,$vf25,$vf0w	# vf25.xyz = vf25 + 1
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_gs_pos_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_gs_pos_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_pos_clip_loop
	addi	$5,$5,12
	jr	$31
	nop
	.end	nj_vu0_gs_pos_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). First weighted skinning pass with lighting: stores the weighted bone contribution in the indexed vertex.
 * Arguments: $4 = Ninja system block (bone matrix +192, light directions +384, light colours +448,
 * weight scale +1232.w), $5 = 28-byte records (position, normal, index/weight word at +24), $6 = vertex
 * buffer (64-byte entries), $7 = count. Writes weight * position to +16 and weight * lit colour to +32.
 * Returns 0.
 */
/* 0x001af294 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_skin_lit_first
	.ent	nj_vu0_gs_skin_lit_first
nj_vu0_gs_skin_lit_first:
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
.Lnj_vu0_gs_skin_lit_first_loop:
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
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_skin_lit_first_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_skin_lit_first

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Middle weighted skinning pass with lighting: adds the weighted bone contribution to the indexed vertex.
 * Arguments as 0x1aee14 ($4 = Ninja system block, $5 = 28-byte records, $6 = vertex buffer,
 * $7 = count). Adds weight * position to +16 and weight * lit colour to +32. Returns 0.
 */
/* 0x001af36c */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_skin_lit_mid
	.ent	nj_vu0_gs_skin_lit_mid
nj_vu0_gs_skin_lit_mid:
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
.Lnj_vu0_gs_skin_lit_mid_loop:
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
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# previous sums: vf2 = position, vf3 = colour
	lqc2	$vf2,16($12)
	lqc2	$vf3,32($12)
	# vf25 = vf20*vf25.x + vf21*vf25.y + vf22*vf25.z + vf23*vf25.w
	vmulax.xyzw	ACC,$vf20,$vf25x
	vmadday.xyzw	ACC,$vf21,$vf25y
	vmaddaz.xyzw	ACC,$vf22,$vf25z
	vmaddw.xyzw	$vf25,$vf23,$vf25w
	vmulw.xyzw	$vf25,$vf25,$vf1w	# vf25 = vf25*vf1.w
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	vadd.xyzw	$vf25,$vf25,$vf3	# vf25 = vf25 + vf3
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_skin_lit_mid_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_skin_lit_mid

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with lighting, no clip test: adds the bone contribution and projects.
 * Arguments as 0x1aee14 ($4 = Ninja system block, also projection +128; $5 = 28-byte records; $6 = vertex
 * buffer; $7 = count). Entry: +16 screen XYZ (12.4 fixed point), +32 summed lit colour (w = 1), +48 z = Q.
 * Returns 0.
 */
/* 0x001af454 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_skin_lit_last
	.ent	nj_vu0_gs_skin_lit_last
nj_vu0_gs_skin_lit_last:
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
	lqc2	$vf30,1232($4)
.Lnj_vu0_gs_skin_lit_last_loop:
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
	# light intensities, clamped to [0, 1]
	# vf25 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf25,$vf18,$vf2z
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	# previous sums: vf2 = position, vf3 = colour
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
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_skin_lit_last_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_skin_lit_last

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Projects n vertices with packed colours to screen coordinates (no clip test).
 * Arguments: $4 = Ninja system block (screen matrix +320, colour scale +1248), $5 = vertices (x, y, z,
 * packed BGRA: 16 bytes), $6 = 64-byte output entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed
 * point), +32 colour (w = 1), +48 z = Q. Returns 0. (vf8-vf10 are loaded but not used.)
 */
/* 0x001af57c */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_col
	.ent	nj_vu0_gs_col
nj_vu0_gs_col:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	lqc2	$vf23,1248($4)
.Lnj_vu0_gs_col_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	lw	$8,12($5)
	# $8 = its four bytes zero-extended to four words
	pextlb	$8,$0,$8
	pextlh	$8,$0,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmulx.xyzw	$vf25,$vf2,$vf23x	# vf25 = vf2*vf23.x
	vmove.w	$vf25,$vf0			# vf25.w = 1
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_col_loop
	addi	$5,$5,16
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_col

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Projects n vertices with normals and packed colours to screen coordinates (no clip test, normal unused).
 * Arguments: $4 = Ninja system block (screen matrix +320, colour scale +1248), $5 = vertices (position,
 * normal, packed BGRA: 28 bytes), $6 = 64-byte output entries, $7 = count. Entry: +16 screen XYZ (12.4
 * fixed point), +32 colour (w = 1), +48 z = Q. Returns 0. (The normal, vf8-vf10, vf28 and vf29 are
 * loaded but not used.)
 */
/* 0x001af618 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_nrm_col
	.ent	nj_vu0_gs_nrm_col
nj_vu0_gs_nrm_col:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	lqc2	$vf23,1248($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
.Lnj_vu0_gs_nrm_col_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	lw	$8,24($5)
	# $8 = its four bytes zero-extended to four words
	pextlb	$8,$0,$8
	pextlh	$8,$0,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmulx.xyzw	$vf25,$vf2,$vf23x	# vf25 = vf2*vf23.x
	vmove.w	$vf25,$vf0			# vf25.w = 1
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_nrm_col_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_nrm_col

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Lights and projects n vertices with normals to screen coordinates (no clip test).
 * Arguments: $4 = Ninja system block (screen matrix +320, light directions +384, light colours +448),
 * $5 = vertices (position, normal: 24 bytes), $6 = 64-byte output entries, $7 = count. Entry: +16 screen
 * XYZ (12.4 fixed point), +32 lit colour (w = 1), +48 z = Q. Returns 0.
 */
/* 0x001af6d4 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_lit
	.ent	nj_vu0_gs_lit
nj_vu0_gs_lit:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
.Lnj_vu0_gs_lit_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
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
	vmove.w	$vf25,$vf0			# vf25.w = 1
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_lit_loop
	addi	$5,$5,24
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_lit

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Projects n bare positions to screen coordinates with white colour (no clip test).
 * Arguments: $4 = Ninja system block (screen matrix +320), $5 = positions (12 bytes), $6 = 64-byte
 * output entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed point), +32 (1, 1, 1, 1), +48 z = Q.
 * Returns 0. (vf8-vf10 are loaded but not used.) Called from 0x1a5318.
 */
/* 0x001af7b8 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_pos
	.ent	nj_vu0_gs_pos
nj_vu0_gs_pos:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
.Lnj_vu0_gs_pos_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	# vf25 = (1, 1, 1, 1)
	vmove.xyzw	$vf25,$vf0		# vf25 = (0,0,0,1)
	vaddw.xyz	$vf25,$vf25,$vf0w	# vf25.xyz = vf25 + 1
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_gs_pos_loop
	addi	$5,$5,12
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_gs_pos

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS RGBAQ/XYZ qword pairs for an index list of converted vertices, stopping at the first clipped one.
 * Expects vf8 (colour multiplier), vf9 (colour offset) and vf11 (scales) loaded by 0x1af4e8.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries, screen XYZ
 * at +16, colour at +32, clip flags at +60), $8 = output (32 bytes per vertex). Returns the number of
 * vertices emitted. ($4 is not used; $13 keeps a copy of the count that is not read.)
 */
/* 0x001af83c */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_emit_strip_clip
	.ent	nj_vu0_gs_emit_strip_clip
nj_vu0_gs_emit_strip_clip:
	movz	$2,$0,$0
	movz	$13,$6,$0
.Lnj_vu0_gs_emit_strip_clip_loop:
	# $14 = the indexed vertex; stop if it has clip flags
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lw	$11,60($14)
	bnez	$11,.Lnj_vu0_gs_emit_strip_clip_done
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	# write XYZ (w = 0) and RGBAQ
	sqc2	$vf24,16($8)
	sqc2	$vf25,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,32
	bnez	$6,.Lnj_vu0_gs_emit_strip_clip_loop
	addi	$2,$2,1
.Lnj_vu0_gs_emit_strip_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gs_emit_strip_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZ qwords for an index list with per-corner texture coordinates (unreferenced).
 * Expects vf8, vf9 and vf11 loaded by 0x1af4e8. Arguments: $5 = 6-byte records (vertex index, s, t as
 * shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex).
 * Returns the output end pointer. No code in the executable calls it.
 */
/* 0x001af8a0 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_emit_uv_strip
	.ent	nj_vu0_gs_emit_uv_strip
nj_vu0_gs_emit_uv_strip:
.Lnj_vu0_gs_emit_uv_strip_loop:
	lh	$9,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf1			# vf1 = $12 (128 bits)
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# vf1.xy = (s, t) * vf11.z
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	# STQ = (s * Q, t * Q, Q) with Q from the vertex (+48 z)
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	# XYZ with w = 0
	vmulx.w	$vf24,$vf0,$vf0x		# vf24.w = 0
	sqc2	$vf24,32($8)
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf25,16($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	bne	$0,$6,.Lnj_vu0_gs_emit_uv_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_gs_emit_uv_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS RGBAQ/XYZ qword pairs for an index list of converted vertices (no clip check).
 * Expects vf8, vf9 and vf11 loaded by 0x1af4e8. Arguments: $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex buffer (64-byte entries), $8 = output (32 bytes per vertex). Returns the output end pointer.
 */
/* 0x001af918 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_emit_strip
	.ent	nj_vu0_gs_emit_strip
nj_vu0_gs_emit_strip:
.Lnj_vu0_gs_emit_strip_loop:
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	sqc2	$vf24,16($8)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf25,0($8)
	bne	$0,$6,.Lnj_vu0_gs_emit_strip_loop
	addi	$8,$8,32
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_gs_emit_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Loads the strip emitters' VU0 constants: vf8 = sys+976, vf9 = sys+992, vf10 = sys+960, vf11 = sys+1232.
 * Argument: $4 = Ninja system block. Called by 0x1a55a8, 0x1a57e0 (before 0x1af3bc), 0x1a59f8 and
 * 0x1a5b48 (before 0x1af498). Byte-identical twins: 0x1afa94 and 0x1aff88.
 */
/* 0x001af968 */
	.set	noreorder
	.text
	.globl	nj_vu0_gs_load_consts
	.ent	nj_vu0_gs_load_consts
nj_vu0_gs_load_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	jr	$31
	lqc2	$vf11,1232($4)
	.end	nj_vu0_gs_load_consts

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms a strided point list by two matrices and clip-tests it (unreferenced).
 * Arguments: $4 = Ninja system block (matrix +192, clip matrix +768), $5 = points (x, y, z), $6 = 64-byte
 * output entries, $7 = count, $8 = point stride in words. Entry: +0 point * matrix +192, +16 clip-space
 * point, +48 w = clip flags. Returns the number of points with any clip flag set.
 * Note: vf25 (the clip matrix's second row) is overwritten by the load from +976 before use, and
 * vf28-vf31 are loaded but not used. No code in the executable calls it.
 */
/* 0x001af97c */
	.set	noreorder
	.text
	.globl	nj_vu0_clip_points
	.ent	nj_vu0_clip_points
nj_vu0_clip_points:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	lqc2	$vf25,976($4)
	lqc2	$vf28,1296($4)
	lqc2	$vf29,1312($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
	movz	$2,$0,$0
.Lnj_vu0_clip_points_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_clip_points_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_clip_points_inside:
	vmr32.w	$vf3,$vf1			# vf3.w = vf1.x
	sqc2	$vf5,0($6)
	sqc2	$vf4,16($6)
	sqc2	$vf3,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_clip_points_loop
	add	$5,$5,$12
	jr	$31
	nop
	.end	nj_vu0_clip_points

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and projects a strided point list, with screen-mapped texture coordinates and a guard-band test.
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, colour +976, texture mapping
 * scale/offset +1296/+1312, guard-band scale/offset +1168/+1184), $5 = points (x, y, z), $6 = 64-byte
 * output entries, $7 = count, $8 = point stride in words. Entry: +0 view-space point, +16 screen XYZ
 * (12.4 fixed point, z = +1296.w), +32 the colour at +976, +48 (s, t, +1296.z, clip flags) with
 * (s, t) = (screen xy + offset) * scale. Returns the number of points with any clip flag set.
 */
/* 0x001afa30 */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_points_map_clip
	.ent	nj_vu0_direct_points_map_clip
nj_vu0_direct_points_map_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf25,976($4)
	lqc2	$vf28,1296($4)
	lqc2	$vf29,1312($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
	movz	$2,$0,$0
.Lnj_vu0_direct_points_map_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	# (s, t) = (screen xy + offset) * scale, q = vf28.z; screen z = vf28.w
	vadd.xy	$vf26,$vf24,$vf29		# vf26.xy = vf24 + vf29
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vaddz.z	$vf26,$vf0,$vf28z		# vf26.z = vf28.z
	vaddw.z	$vf24,$vf0,$vf28w		# vf24.z = vf28.w
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_direct_points_map_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_direct_points_map_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_direct_points_map_clip_loop
	add	$5,$5,$12
	jr	$31
	nop
	.end	nj_vu0_direct_points_map_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with screen-mapped texture coordinates and a guard-band test (unreferenced).
 * Arguments: $4 = Ninja system block (bone matrix +192, projection +128, colour +976, mapping
 * scale/offset +1296/+1312, guard band +1168/+1184, weight scale +1232.w), $5 = 28-byte records
 * (position at +0, index/weight word at +24), $6 = vertex buffer (64-byte entries), $7 = count.
 * Entry: +0 summed position, +16 screen XYZ (12.4 fixed point, z = +1296.w), +32 the colour at +976,
 * +48 (s, t, +1296.z, clip flags). Adds the clipped count to $2, which it does not clear first.
 * No code in the executable calls it.
 */
/* 0x001afb1c */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_skin_map_last_clip
	.ent	nj_vu0_direct_skin_map_last_clip
nj_vu0_direct_skin_map_last_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf25,976($4)
	lqc2	$vf28,1296($4)
	lqc2	$vf29,1312($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	lqc2	$vf7,1232($4)
.Lnj_vu0_direct_skin_map_last_clip_loop:
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
	# add the previous sum (+16)
	lqc2	$vf2,16($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	sqc2	$vf24,0($12)
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	# (s, t) = (screen xy + offset) * scale, q = vf28.z; screen z = vf28.w
	vadd.xy	$vf26,$vf24,$vf29		# vf26.xy = vf24 + vf29
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vaddz.z	$vf26,$vf0,$vf28z		# vf26.z = vf28.z
	vaddw.z	$vf24,$vf0,$vf28w		# vf24.z = vf28.w
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_direct_skin_map_last_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_direct_skin_map_last_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_direct_skin_map_last_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_direct_skin_map_last_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). First weighted skinning pass, position only: stores weight * (bone matrix * position) in the indexed vertex (unreferenced).
 * Arguments: $4 = Ninja system block (bone matrix +192, weight scale +1232.w), $5 = 28-byte records
 * (position, index/weight word at +24), $6 = vertex buffer (64-byte entries, sum at +16), $7 = count.
 * Returns 0. No code in the executable calls it.
 */
/* 0x001afc24 */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_skin_first
	.ent	nj_vu0_direct_skin_first
nj_vu0_direct_skin_first:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
.Lnj_vu0_direct_skin_first_loop:
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
	addi	$7,$7,-1
	addi	$5,$5,28
	bnez	$7,.Lnj_vu0_direct_skin_first_loop
	sqc2	$vf24,16($12)
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_direct_skin_first

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Middle weighted skinning pass, position only: adds weight * (bone matrix * position) to the indexed vertex (unreferenced).
 * Arguments as 0x1af7a4 ($4 = Ninja system block, $5 = 28-byte records, $6 = vertex buffer with the
 * sum at +16, $7 = count). Returns 0. No code in the executable calls it.
 */
/* 0x001afc9c */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_skin_mid
	.ent	nj_vu0_direct_skin_mid
nj_vu0_direct_skin_mid:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
.Lnj_vu0_direct_skin_mid_loop:
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
	lqc2	$vf2,16($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	addi	$7,$7,-1
	addi	$5,$5,28
	bnez	$7,.Lnj_vu0_direct_skin_mid_loop
	sqc2	$vf24,16($12)
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_direct_skin_mid

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with screen-mapped texture coordinates, no clip test (unreferenced).
 * Arguments as 0x1af69c ($4 = Ninja system block, $5 = 28-byte records, $6 = vertex buffer,
 * $7 = count). Entry: +16 screen XYZ (12.4 fixed point, z = +1296.w), +32 the colour at +976, +48
 * (s, t, +1296.z, -). Returns 0. No code in the executable calls it.
 */
/* 0x001afd1c */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_skin_map_last
	.ent	nj_vu0_direct_skin_map_last
nj_vu0_direct_skin_map_last:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf28,1296($4)
	lqc2	$vf29,1312($4)
	lqc2	$vf25,976($4)
	lqc2	$vf30,1232($4)
.Lnj_vu0_direct_skin_map_last_loop:
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
	lqc2	$vf2,16($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# (s, t) = (screen xy + offset) * scale; screen z = vf28.w, q = vf28.z
	vadd.xy	$vf26,$vf24,$vf29		# vf26.xy = vf24 + vf29
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vaddw.z	$vf24,$vf0,$vf28w		# vf24.z = vf28.w
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddz.z	$vf26,$vf0,$vf28z		# vf26.z = vf28.z
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_direct_skin_map_last_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_direct_skin_map_last

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Projects a strided point list with screen-mapped texture coordinates (no clip test, unreferenced).
 * Arguments: $4 = Ninja system block (screen matrix +320, colour +976, mapping scale/offset
 * +1296/+1312), $5 = points (x, y, z), $6 = 64-byte output entries, $7 = count, $8 = point stride
 * in words. Entry: +16 screen XYZ (12.4 fixed point, z = +1296.w), +32 the colour at +976, +48
 * (s, t, +1296.z, -). Returns 0. No code in the executable calls it.
 */
/* 0x001afdf0 */
	.set	noreorder
	.text
	.globl	nj_vu0_direct_points_map
	.ent	nj_vu0_direct_points_map
nj_vu0_direct_points_map:
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	lqc2	$vf28,1296($4)
	lqc2	$vf29,1312($4)
	lqc2	$vf25,976($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
.Lnj_vu0_direct_points_map_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# (s, t) = (screen xy + offset) * scale; screen z = vf28.w, q = vf28.z
	vadd.xy	$vf26,$vf24,$vf29		# vf26.xy = vf24 + vf29
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vaddw.z	$vf24,$vf0,$vf28w		# vf24.z = vf28.w
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vaddz.z	$vf26,$vf0,$vf28z		# vf26.z = vf28.z
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_direct_points_map_loop
	add	$5,$5,$12
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_direct_points_map

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZ qwords for a 6-byte-record index list of converted vertices.
 * Expects vf11 loaded by 0x1afa94. Arguments: $5 = 6-byte records (vertex index first), $6 = count,
 * $7 = vertex buffer (64-byte entries: XYZ +16, colour +32, (s, t, q) +48), $8 = output (48 bytes per
 * vertex). Returns the output end pointer. Called from 0x1a6d88.
 */
/* 0x001afe7c */
	.set	noreorder
	.text
	.globl	nj_vu0_env_emit_uv_strip
	.ent	nj_vu0_env_emit_uv_strip
nj_vu0_env_emit_uv_strip:
.Lnj_vu0_env_emit_uv_strip_loop:
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# XYZ with w = 0, colour * vf11.y as integers, STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	addi	$5,$5,6
	addi	$6,$6,-1
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	bne	$0,$6,.Lnj_vu0_env_emit_uv_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_env_emit_uv_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZ qwords for a short index list of converted vertices.
 * Same as 0x1af9fc with 2-byte records (vertex indices). Expects vf11 loaded by 0x1afa94.
 * Returns the output end pointer. Called from 0x1a6ed8.
 */
/* 0x001afec8 */
	.set	noreorder
	.text
	.globl	nj_vu0_env_emit_strip
	.ent	nj_vu0_env_emit_strip
nj_vu0_env_emit_strip:
.Lnj_vu0_env_emit_strip_loop:
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# XYZ with w = 0, colour * vf11.y as integers, STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	bne	$0,$6,.Lnj_vu0_env_emit_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_env_emit_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Loads the strip emitters' VU0 constants: vf8 = sys+976, vf9 = sys+992, vf10 = sys+960, vf11 = sys+1232.
 * Argument: $4 = Ninja system block. Byte-identical to 0x1af4e8 and 0x1aff88; called by 0x1a6d88 and
 * 0x1a6ed8 before 0x1af9fc / 0x1afa48.
 */
/* 0x001aff14 */
	.set	noreorder
	.text
	.globl	nj_vu0_env_load_consts
	.ent	nj_vu0_env_load_consts
nj_vu0_env_load_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	jr	$31
	lqc2	$vf11,1232($4)
	.end	nj_vu0_env_load_consts

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass, position only, with projection and a guard-band test (screen position kept as floats).
 * Arguments: $4 = Ninja system block (bone matrix +192, projection +128, guard band +1168/+1184, weight
 * scale +1232.w), $5 = 28-byte records (position, index/weight word at +24), $6 = vertex buffer (64-byte
 * entries), $7 = count. Entry: +0 summed position, +16 screen position (floats), +48 (-, -, Q, clip
 * flags); +32 receives whatever vf25 holds. Returns the number of vertices with any clip flag set.
 */
/* 0x001aff28 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_skin_last_clip
	.ent	nj_vu0_wire_skin_last_clip
nj_vu0_wire_skin_last_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	lqc2	$vf7,1232($4)
	movz	$2,$0,$0
.Lnj_vu0_wire_skin_last_clip_loop:
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
	# add the previous sum (+16)
	lqc2	$vf2,16($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	sqc2	$vf24,0($12)
	# project the summed position
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_wire_skin_last_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_wire_skin_last_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($12)
	sqc2	$vf25,32($12)
	sqc2	$vf26,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_wire_skin_last_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_wire_skin_last_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and projects a strided point list with a guard-band test (screen position kept as floats).
 * Arguments: $4 = Ninja system block (view matrix +192, projection +128, guard band +1168/+1184),
 * $5 = points (x, y, z), $6 = 64-byte output entries, $7 = count, $8 = point stride in words. Entry:
 * +0 view-space point, +16 screen position (floats), +48 (-, -, Q, clip flags). Returns the number of
 * points with any clip flag set.
 */
/* 0x001b002c */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_points_clip
	.ent	nj_vu0_wire_points_clip
nj_vu0_wire_points_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf12,128($4)
	lqc2	$vf13,144($4)
	lqc2	$vf14,160($4)
	lqc2	$vf15,176($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
	movz	$2,$0,$0
.Lnj_vu0_wire_points_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf24,$vf11,$vf0w
	sqc2	$vf24,0($6)
	# vf24 = vf12*vf24.x + vf13*vf24.y + vf14*vf24.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf24x
	vmadday.xyzw	ACC,$vf13,$vf24y
	vmaddaz.xyzw	ACC,$vf14,$vf24z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	# guard-band test: (screen * scale + offset) against +-1
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lnj_vu0_wire_points_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lnj_vu0_wire_points_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_wire_points_clip_loop
	add	$5,$5,$12
	jr	$31
	nop
	.end	nj_vu0_wire_points_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). First weighted skinning pass, position only: stores weight * (bone matrix * position) at +16 of the indexed vertex.
 * Arguments: $4 = Ninja system block (bone matrix +192, weight scale +1232.w), $5 = 28-byte records
 * (position, index/weight word at +24), $6 = vertex buffer (64-byte entries), $7 = count. Returns 0.
 * Same computation as 0x1af7a4 (the store is outside the delay slot). Called from 0x1a7218.
 */
/* 0x001b0104 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_skin_first
	.ent	nj_vu0_wire_skin_first
nj_vu0_wire_skin_first:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
.Lnj_vu0_wire_skin_first_loop:
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
	sqc2	$vf24,16($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_wire_skin_first_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_wire_skin_first

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Middle weighted skinning pass, position only: adds weight * (bone matrix * position) to +16 of the indexed vertex.
 * Arguments as 0x1afc84. Returns 0. Same computation as 0x1af81c. Called from
 * 0x1a7218.
 */
/* 0x001b017c */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_skin_mid
	.ent	nj_vu0_wire_skin_mid
nj_vu0_wire_skin_mid:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
.Lnj_vu0_wire_skin_mid_loop:
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
	lqc2	$vf2,16($12)
	vadd.xyz	$vf24,$vf24,$vf2	# vf24.xyz = vf24 + vf2
	sqc2	$vf24,16($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_wire_skin_mid_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_wire_skin_mid

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Projects a strided point list to screen positions kept as floats (unreferenced).
 * Arguments: $4 = Ninja system block (screen matrix +320), $5 = points (x, y, z), $6 = 64-byte output
 * entries (+16 = projected xyz, w), $7 = count, $8 = point stride in words. Returns 0.
 * No code in the executable calls it.
 */
/* 0x001b01fc */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_points
	.ent	nj_vu0_wire_points
nj_vu0_wire_points:
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
.Lnj_vu0_wire_points_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf24 = vf12*vf1.x + vf13*vf1.y + vf14*vf1.z + vf15
	vmulax.xyzw	ACC,$vf12,$vf1x
	vmadday.xyzw	ACC,$vf13,$vf1y
	vmaddaz.xyzw	ACC,$vf14,$vf1z
	vmaddw.xyzw	$vf24,$vf15,$vf0w
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf24,$vf24,Q		# vf24.xyz = vf24*Q
	sqc2	$vf24,16($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_wire_points_loop
	add	$5,$5,$12
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_wire_points

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS RGBAQ/XYZ qword pairs for 6-byte index records, converting float positions, until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1aff88. Arguments: $5 = 6-byte records (vertex index first),
 * $6 = count (its absolute value is used), $7 = vertex buffer (64-byte entries: float screen position
 * +16, colour +32, clip flags +60), $8 = output (32 bytes per vertex). Returns the number emitted.
 */
/* 0x001b0260 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_emit_uv_strip_clip
	.ent	nj_vu0_wire_emit_uv_strip_clip
nj_vu0_wire_emit_uv_strip_clip:
	movz	$2,$0,$0
	# $6 = |count|
	pabsw	$6,$6
.Lnj_vu0_wire_emit_uv_strip_clip_loop:
	lh	$9,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	# stop at the first vertex with clip flags
	lw	$11,60($9)
	bnez	$11,.Lnj_vu0_wire_emit_uv_strip_clip_done
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	# XYZ = 12.4 fixed point with w = 0
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	# colour = clamp(colour * vf8 + vf9, 0, 1), rgb * vf11.x, alpha * vf11.y, as integers
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmulx.xyz	$vf25,$vf25,$vf11x	# vf25.xyz = vf25*vf11.x
	vmuly.w	$vf25,$vf25,$vf11y		# vf25.w = vf25.w*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf24,16($8)
	sqc2	$vf25,0($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	addi	$8,$8,32
	bnez	$6,.Lnj_vu0_wire_emit_uv_strip_clip_loop
	addi	$2,$2,1
.Lnj_vu0_wire_emit_uv_strip_clip_done:
	jr	$31
	nop
	.end	nj_vu0_wire_emit_uv_strip_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS RGBAQ/XYZ qword pairs for a short index list, converting float positions, until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1aff88. Arguments: $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex buffer (64-byte entries: float screen position +16, colour +32, clip flags +60),
 * $8 = output (32 bytes per vertex). Returns the number emitted.
 */
/* 0x001b02d4 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_emit_strip_clip
	.ent	nj_vu0_wire_emit_strip_clip
nj_vu0_wire_emit_strip_clip:
	movz	$2,$0,$0
.Lnj_vu0_wire_emit_strip_clip_loop:
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	# stop at the first vertex with clip flags
	lw	$11,60($14)
	bnez	$11,.Lnj_vu0_wire_emit_strip_clip_done
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	# XYZ = 12.4 fixed point with w = 0
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf24,16($8)
	sqc2	$vf25,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,32
	bnez	$6,.Lnj_vu0_wire_emit_strip_clip_loop
	addi	$2,$2,1
.Lnj_vu0_wire_emit_strip_clip_done:
	jr	$31
	nop
	.end	nj_vu0_wire_emit_strip_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZ qwords for 6-byte index records with per-corner texture coordinates, converting float positions.
 * Expects vf8, vf9 and vf11 loaded by 0x1aff88. Arguments: $5 = 6-byte records (vertex index, s, t as
 * shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex).
 * Returns the output end pointer. Called from 0x1a7920.
 */
/* 0x001b0338 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_emit_uv_strip
	.ent	nj_vu0_wire_emit_uv_strip
nj_vu0_wire_emit_uv_strip:
.Lnj_vu0_wire_emit_uv_strip_loop:
	lh	$9,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf1			# vf1 = $12 (128 bits)
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# vf1.xy = (s, t) * vf11.z
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	# STQ = (s * Q, t * Q, Q) with Q from the vertex (+48 z)
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	# XYZ = 12.4 fixed point with w = 0
	vftoi4.xyzw	$vf24,$vf24		# vf24 = vf24 as 12.4 fixed point
	vmulx.w	$vf24,$vf0,$vf0x		# vf24.w = 0
	sqc2	$vf24,32($8)
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf25,16($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	bne	$0,$6,.Lnj_vu0_wire_emit_uv_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_wire_emit_uv_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS RGBAQ/XYZ qword pairs for a short index list, converting float positions (no clip check).
 * Expects vf8, vf9 and vf11 loaded by 0x1aff88. Arguments: $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex buffer (64-byte entries), $8 = output (32 bytes per vertex). Returns the output end
 * pointer. Called from 0x1a7a70.
 */
/* 0x001b03b4 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_emit_strip
	.ent	nj_vu0_wire_emit_strip
nj_vu0_wire_emit_strip:
.Lnj_vu0_wire_emit_strip_loop:
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	# XYZ = 12.4 fixed point with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vftoi4.xyzw	$vf24,$vf24		# vf24 = vf24 as 12.4 fixed point
	sqc2	$vf24,16($8)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf25,0($8)
	bne	$0,$6,.Lnj_vu0_wire_emit_strip_loop
	addi	$8,$8,32
	jr	$31
	movz	$2,$8,$0
	.end	nj_vu0_wire_emit_strip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Loads the strip emitters' VU0 constants: vf8 = sys+976, vf9 = sys+992, vf10 = sys+960, vf11 = sys+1232.
 * Argument: $4 = Ninja system block. Byte-identical to 0x1af4e8 and 0x1afa94; called by 0x1a7538,
 * 0x1a7738, 0x1a7920 and 0x1a7a70 before the emitters 0x1afde0-0x1aff34.
 */
/* 0x001b0408 */
	.set	noreorder
	.text
	.globl	nj_vu0_wire_load_consts
	.ent	nj_vu0_wire_load_consts
nj_vu0_wire_load_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	jr	$31
	lqc2	$vf11,1232($4)
	.end	nj_vu0_wire_load_consts

/*
 * Retail 0x001b041c. The clip-code mask is 47 (0b101111) where the earlier build's is 63 (bit 4 is not kept).
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n vertices with an environment map (texture coordinates from the view-space normal).
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768, map scale/offset +1264/+1280,
 * colour scale +1248.x), $5 = vertices (position, normal, packed BGRA: 28 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +0 view-space position, +16 clip-space position, +32 colour as floats,
 * +48 (s, t, -, clip flags). Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
/* 0x001b041c */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_env_clip
	.ent	nj_vu0_vu1_env_clip
nj_vu0_vu1_env_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1248($4)
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,47
.Lnj_vu0_vu1_env_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	lw	$12,16($5)
	lw	$13,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	# $11 = qword ($11, $12, $13, -): pack three words
	pextlw	$11,$12,$11
	pcpyld	$11,$13,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	lw	$14,24($5)
	# view-space normal; (s, t) = normal.xy * scale + offset
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	# $14 = its four bytes zero-extended to four words
	pextlb	$14,$0,$14
	pextlh	$14,$0,$14
	qmtc2	$14,$vf2			# vf2 = $14 (128 bits)
	vmul.xy	$vf3,$vf3,$vf28			# vf3.xy = vf3*vf28
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	sqc2	$vf4,16($6)
	vadd.xy	$vf3,$vf3,$vf29			# vf3.xy = vf3 + vf29
	vmulx.xyzw	$vf2,$vf2,$vf30x	# vf2 = vf2*vf30.x
	# accumulate the codes; entry +60 = this vertex's flags
	andi	$8,$8,0x2f
	or	$2,$2,$8
	and	$3,$3,$8
	addi	$5,$5,28
	sqc2	$vf5,0($6)
	sqc2	$vf2,32($6)
	addi	$7,$7,-1
	sqc2	$vf3,48($6)
	sw	$8,60($6)
	bnez	$7,.Lnj_vu0_vu1_env_clip_loop
	addi	$6,$6,64
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	nj_vu0_vu1_env_clip

/*
 * Retail 0x001b0510. The clip-code mask is 47 (0b101111) where the earlier build's is 63 (bit 4 is not kept).
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n vertices with packed colours.
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768, colour scale +1248.x),
 * $5 = vertices (x, y, z, packed BGRA: 16 bytes), $6 = 64-byte output entries, $7 = count. Entry:
 * +0 view-space position, +16 clip-space position, +32 colour as floats, +60 clip flags (+48 xyz
 * receive whatever vf3 holds). Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
/* 0x001b0510 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_col_clip
	.ent	nj_vu0_vu1_col_clip
nj_vu0_vu1_col_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	lqc2	$vf30,1248($4)
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,47
	nop
.Lnj_vu0_vu1_col_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# view-space position, with the colour conversion interleaved
	vmulax.xyzw	ACC,$vf8,$vf1x		# ACC = vf8*vf1.x
	vmadday.xyzw	ACC,$vf9,$vf1y		# ACC = ACC + vf9*vf1.y
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmaddaz.xyzw	ACC,$vf10,$vf1z		# ACC = ACC + vf10*vf1.z
	vmaddw.xyzw	$vf5,$vf11,$vf0w	# vf5 = ACC + vf11
	vmulx.xyzw	$vf2,$vf2,$vf30x	# vf2 = vf2*vf30.x
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	# accumulate the codes; entry +60 = this vertex's flags
	andi	$8,$8,0x2f
	or	$2,$2,$8
	and	$3,$3,$8
	sqc2	$vf5,0($6)
	sqc2	$vf4,16($6)
	sqc2	$vf2,32($6)
	sqc2	$vf3,48($6)
	sw	$8,60($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_col_clip_loop
	addi	$5,$5,16
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	nj_vu0_vu1_col_clip

/*
 * Retail 0x001b05d4. The clip-code mask is 47 (0b101111) where the earlier build's is 63 (bit 4 is not kept).
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n bare positions (white colour).
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768), $5 = positions (12 bytes),
 * $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16 clip-space position,
 * +32 (1, 1, 1, 1), +60 clip flags. Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
/* 0x001b05d4 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_pos_clip
	.ent	nj_vu0_vu1_pos_clip
nj_vu0_vu1_pos_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	# vf2 = (1, 1, 1, 1)
	vaddw.xyz	$vf2,$vf0,$vf0w		# vf2.xyz = 1
	vmove.w	$vf2,$vf0			# vf2.w = 1
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,47
.Lnj_vu0_vu1_pos_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	# accumulate the codes; entry +60 = this vertex's flags
	andi	$8,$8,0x2f
	or	$2,$2,$8
	and	$3,$3,$8
	sqc2	$vf5,0($6)
	sqc2	$vf4,16($6)
	sqc2	$vf2,32($6)
	sqc2	$vf3,48($6)
	sw	$8,60($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_pos_clip_loop
	addi	$5,$5,12
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	nj_vu0_vu1_pos_clip

/*
 * Retail 0x001b0684. The clip-code mask is 47 (0b101111) where the earlier build's is 63 (bit 4 is not kept).
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms, lights (first light only) and clip-tests n vertices with normals, with an environment map.
 * Arguments: $4 = Ninja system block (view matrix +192, light directions +384, light colours +448,
 * clip matrix +768, map scale/offset +1264/+1280), $5 = vertices (position, normal: 24 bytes), $6 = 64-byte
 * output entries, $7 = count. Entry: +0 view-space position, +16 clip-space position, +32 colour =
 * light colour 0 * clamp(intensity 0, 0, 1) with w = 1, +48 (s, t, -, clip flags).
 * Returns ((OR of the clip codes) << 8) | (AND of the clip codes). Called from 0x1a8f30.
 */
/* 0x001b0684 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_lit_env_clip
	.ent	nj_vu0_vu1_lit_env_clip
nj_vu0_vu1_lit_env_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,47
.Lnj_vu0_vu1_lit_env_clip_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	lw	$12,16($5)
	lw	$13,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	# $11 = qword ($11, $12, $13, -): pack three words
	pextlw	$11,$12,$11
	pcpyld	$11,$13,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	# vf3 = view-space normal, vf2 = light intensities
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	# vf2 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf2,$vf18,$vf2z
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x2f
	# (s, t) = normal.xy * scale + offset; colour = vf20 * clamp(intensity.x, 0, 1)
	vadda.xy	ACC,$vf0,$vf29		# ACC.xy = vf29
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vmadd.xy	$vf3,$vf3,$vf28		# vf3.xy = ACC + vf3*vf28
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	vmulx.xyzw	$vf2,$vf20,$vf2x	# vf2 = vf20*vf2.x
	# accumulate the codes; entry +60 = this vertex's flags
	or	$2,$2,$8
	and	$3,$3,$8
	vmove.w	$vf2,$vf0			# vf2.w = 1
	sqc2	$vf5,0($6)
	sqc2	$vf4,16($6)
	sqc2	$vf2,32($6)
	sqc2	$vf3,48($6)
	sw	$8,60($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_lit_env_clip_loop
	addi	$5,$5,24
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	nj_vu0_vu1_lit_env_clip

/*
 * Retail 0x001b0798. The clip-code mask is 47 (0b101111) where the earlier build's is 63 (bit 4 is not kept).
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with lighting and environment map: adds the bone contribution, then clip-tests the sum.
 * Arguments: $4 = Ninja system block (bone matrix +192, light directions +384, light colours +448, clip
 * matrix +704, map scale/offset +1264/+1280, weight scale +1232.w), $5 = 28-byte records (position,
 * normal, index/weight word at +24), $6 = vertex buffer (64-byte entries), $7 = count. Entry: +0 summed
 * position, +16 its clip-space position, +32 summed lit colour (w = 1), +48 (s, t) from the summed
 * normal, +60 clip flags. Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8e70 (after 0x1b069c and 0x1b059c).
 */
/* 0x001b0798 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_skin_lit_env_last_clip
	.ent	nj_vu0_vu1_skin_lit_env_last_clip
nj_vu0_vu1_skin_lit_env_last_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf24,704($4)
	lqc2	$vf25,720($4)
	lqc2	$vf26,736($4)
	lqc2	$vf27,752($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1232($4)
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,47
.Lnj_vu0_vu1_skin_lit_env_last_clip_loop:
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
	# vf6.w = weight * scale; vf7 = the vertex's position sum
	vmulw.w	$vf6,$vf1,$vf30w		# vf6.w = vf1.w*vf30.w
	lqc2	$vf7,0($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vmulw.xyz	$vf1,$vf1,$vf6w		# vf1.xyz = vf1*vf6.w
	vadd.xyz	$vf5,$vf1,$vf7		# vf5.xyz = vf1 + vf7
	# vf4 = vf24*vf5.x + vf25*vf5.y + vf26*vf5.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf5x
	vmadday.xyzw	ACC,$vf25,$vf5y
	vmaddaz.xyzw	ACC,$vf26,$vf5z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal; vf7 = the vertex's normal sum (+48)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	lqc2	$vf7,48($12)
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	vmulw.xyz	$vf3,$vf3,$vf6w		# vf3.xyz = vf3*vf6.w
	# (s, t) = (summed normal).xy * scale + offset
	vadd.xy	$vf3,$vf3,$vf7			# vf3.xy = vf3 + vf7
	vmul.xy	$vf3,$vf3,$vf28			# vf3.xy = vf3*vf28
	vadd.xy	$vf3,$vf3,$vf29			# vf3.xy = vf3 + vf29
	# light intensities, clamped to [0, 1]
	# vf2 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf2,$vf18,$vf2z
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	# vf7 = the vertex's colour sum (+32)
	lqc2	$vf7,32($12)
	# vf2 = vf20*vf2.x + vf21*vf2.y + vf22*vf2.z + vf23*vf2.w
	vmulax.xyzw	ACC,$vf20,$vf2x
	vmadday.xyzw	ACC,$vf21,$vf2y
	vmaddaz.xyzw	ACC,$vf22,$vf2z
	vmaddw.xyzw	$vf2,$vf23,$vf2w
	vmulw.xyzw	$vf2,$vf2,$vf6w		# vf2 = vf2*vf6.w
	vadd.xyzw	$vf2,$vf2,$vf7		# vf2 = vf2 + vf7
	vmove.w	$vf2,$vf0			# vf2.w = 1
	# accumulate the codes
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x2f
	or	$2,$2,$8
	and	$3,$3,$8
	sqc2	$vf4,16($12)
	sqc2	$vf5,0($12)
	sqc2	$vf2,32($12)
	sqc2	$vf3,48($12)
	sw	$8,60($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_skin_lit_env_last_clip_loop
	addi	$5,$5,28
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	nj_vu0_vu1_skin_lit_env_last_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with lighting and environment map, no clip test: adds the bone contribution to the indexed vertex.
 * Arguments: $4 = Ninja system block (bone matrix +192, light directions +384, light colours +448, map
 * scale/offset +1264/+1280, weight scale +1232.w), $5 = 28-byte records (position, normal, index/weight
 * word at +24), $6 = vertex buffer (64-byte entries), $7 = count. Adds to the position sum (+0) and the
 * colour sum (+32); +48 gets (s, t) from the summed normal. Returns 0. Called from 0x1a9318 (after
 * 0x1b069c and 0x1b059c).
 */
/* 0x001b08fc */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_skin_lit_env_last
	.ent	nj_vu0_vu1_skin_lit_env_last
nj_vu0_vu1_skin_lit_env_last:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1232($4)
	movz	$2,$0,$0
.Lnj_vu0_vu1_skin_lit_env_last_loop:
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
	# vf6.w = weight * scale; vf7 = the vertex's position sum
	vmulw.w	$vf6,$vf1,$vf30w		# vf6.w = vf1.w*vf30.w
	lqc2	$vf7,0($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vmulw.xyz	$vf1,$vf1,$vf6w		# vf1.xyz = vf1*vf6.w
	vadd.xyz	$vf1,$vf1,$vf7		# vf1.xyz = vf1 + vf7
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	# vf2 = normal; vf7 = the vertex's normal sum (+48)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	lqc2	$vf7,48($12)
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	vmulw.xyz	$vf3,$vf3,$vf6w		# vf3.xyz = vf3*vf6.w
	# (s, t) = (summed normal).xy * scale + offset
	vadd.xy	$vf3,$vf3,$vf7			# vf3.xy = vf3 + vf7
	vmul.xy	$vf3,$vf3,$vf28			# vf3.xy = vf3*vf28
	vadd.xy	$vf3,$vf3,$vf29			# vf3.xy = vf3 + vf29
	# light intensities, clamped to [0, 1]
	# vf2 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf2,$vf18,$vf2z
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	# vf7 = the vertex's colour sum (+32)
	lqc2	$vf7,32($12)
	# vf2 = vf20*vf2.x + vf21*vf2.y + vf22*vf2.z + vf23*vf2.w
	vmulax.xyzw	ACC,$vf20,$vf2x
	vmadday.xyzw	ACC,$vf21,$vf2y
	vmaddaz.xyzw	ACC,$vf22,$vf2z
	vmaddw.xyzw	$vf2,$vf23,$vf2w
	vmulw.xyzw	$vf2,$vf2,$vf6w		# vf2 = vf2*vf6.w
	vadd.xyzw	$vf2,$vf2,$vf7		# vf2 = vf2 + vf7
	vmove.w	$vf2,$vf0			# vf2.w = 1
	sqc2	$vf1,0($12)
	sqc2	$vf2,32($12)
	sqc2	$vf3,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_skin_lit_env_last_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_vu1_skin_lit_env_last

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Middle weighted skinning pass with one light: adds the bone contribution (position, normal, colour) to the indexed vertex.
 * Arguments: $4 = Ninja system block (bone matrix +192, light direction +384, light colour +448, weight
 * scale +1232.w), $5 = 28-byte records (position, normal, index/weight word at +24), $6 = vertex buffer
 * (64-byte entries), $7 = count. Adds weight * position to +0, weight * normal.xy to +48 and weight *
 * light colour 0 (+448) * clamp(intensity 0, 0, 1) to +32. Returns the count. Called from 0x1a8e70 and 0x1a9318.
 */
/* 0x001b0a1c */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_skin_lit_mid
	.ent	nj_vu0_vu1_skin_lit_mid
nj_vu0_vu1_skin_lit_mid:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf30,1232($4)
	movz	$2,$7,$0
.Lnj_vu0_vu1_skin_lit_mid_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$11,$vf6			# vf6 = $11 (128 bits)
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf6.x = weight * scale; vf7 = the vertex's position sum
	vitof0.x	$vf6,$vf6		# vf6.x = vf6.x integer to float
	lqc2	$vf7,0($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vmulw.x	$vf6,$vf6,$vf30w		# vf6.x = vf6.x*vf30.w
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	vmulx.xyz	$vf1,$vf1,$vf6x		# vf1.xyz = vf1*vf6.x
	# vf2 = normal
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	vadd.xyz	$vf1,$vf1,$vf7		# vf1.xyz = vf1 + vf7
	# vf7 = the vertex's normal sum (+48)
	lqc2	$vf7,48($12)
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	# vf2 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf2,$vf18,$vf2z
	vmulx.xyz	$vf3,$vf3,$vf6x		# vf3.xyz = vf3*vf6.x
	vadd.xy	$vf3,$vf3,$vf7			# vf3.xy = vf3 + vf7
	# light intensity 0, clamped to [0, 1]
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	# vf7 = the vertex's colour sum (+32)
	lqc2	$vf7,32($12)
	vmulx.xyzw	$vf2,$vf20,$vf2x	# vf2 = vf20*vf2.x
	vmulx.xyzw	$vf2,$vf2,$vf6x		# vf2 = vf2*vf6.x
	vadd.xyzw	$vf2,$vf2,$vf7		# vf2 = vf2 + vf7
	sqc2	$vf1,0($12)
	sqc2	$vf2,32($12)
	sqc2	$vf3,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lnj_vu0_vu1_skin_lit_mid_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_vu1_skin_lit_mid

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). First weighted skinning pass with one light: stores the bone contribution (position, normal, colour) in the indexed vertex.
 * Arguments as 0x1b059c ($4 = Ninja system block, $5 = 28-byte records, $6 = vertex buffer, $7 = count).
 * Stores weight * position at +0, weight * view-space normal at +48 and weight * light colour 0 *
 * clamp(intensity 0, 0, 1) at +32. Returns the count. Called from 0x1a8e70 and 0x1a9318.
 */
/* 0x001b0b1c */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_skin_lit_first
	.ent	nj_vu0_vu1_skin_lit_first
nj_vu0_vu1_skin_lit_first:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf30,1232($4)
	movz	$2,$7,$0
.Lnj_vu0_vu1_skin_lit_first_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	andi	$12,$11,0xffff
	sll	$12,$12,0x6
	add	$12,$12,$6
	srl	$11,$11,0x10
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$11,$vf6			# vf6 = $11 (128 bits)
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf6.x = weight * scale
	vitof0.x	$vf6,$vf6		# vf6.x = vf6.x integer to float
	lw	$8,12($5)
	lw	$9,16($5)
	lw	$10,20($5)
	vmulw.x	$vf6,$vf6,$vf30w		# vf6.x = vf6.x*vf30.w
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf2				# vf2 = $8 (128 bits)
	# vf4 = light intensities, vf3 = view-space normal
	# vf4 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf4,$vf18,$vf2z
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	vmaxx.xyzw	$vf4,$vf4,$vf0x		# vf4 = max(vf4, 0)
	# vf1 = bone matrix * position; colour = vf20 * clamp(intensity.x, 0, 1); all scaled by the weight
	vmulax.xyzw	ACC,$vf8,$vf1x		# ACC = vf8*vf1.x
	vmadday.xyzw	ACC,$vf9,$vf1y		# ACC = ACC + vf9*vf1.y
	vmaddaz.xyzw	ACC,$vf10,$vf1z		# ACC = ACC + vf10*vf1.z
	vminiw.xyzw	$vf4,$vf4,$vf0w		# vf4 = min(vf4, 1)
	vmaddw.xyzw	$vf1,$vf11,$vf0w	# vf1 = ACC + vf11
	vmulx.xyzw	$vf4,$vf20,$vf4x	# vf4 = vf20*vf4.x
	vmulx.xyz	$vf1,$vf1,$vf6x		# vf1.xyz = vf1*vf6.x
	vmulx.xyz	$vf3,$vf3,$vf6x		# vf3.xyz = vf3*vf6.x
	vmulx.xyzw	$vf4,$vf4,$vf6x		# vf4 = vf4*vf6.x
	sqc2	$vf1,0($12)
	sqc2	$vf3,48($12)
	addi	$7,$7,-1
	sqc2	$vf4,32($12)
	bnez	$7,.Lnj_vu0_vu1_skin_lit_first_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	nj_vu0_vu1_skin_lit_first

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms n vertices with an environment map and packed colours (no clip test).
 * Arguments: $4 = Ninja system block (view matrix +192, map scale/offset +1264/+1280, colour scale
 * +1248.x), $5 = vertices (position, normal, packed BGRA: 28 bytes), $6 = 64-byte output entries,
 * $7 = count. Entry: +0 view-space position, +32 colour as floats, +48 (s, t) from the view-space
 * normal. Returns 0. Called from 0x1a93d8.
 */
/* 0x001b0c04 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_env
	.ent	nj_vu0_vu1_env
nj_vu0_vu1_env:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1248($4)
.Lnj_vu0_vu1_env_loop:
	lw	$11,12($5)
	lw	$12,16($5)
	lw	$13,20($5)
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $11 = qword ($11, $12, $13, -): pack three words
	pextlw	$11,$12,$11
	pcpyld	$11,$13,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	# view-space normal; (s, t) = normal.xy * scale + offset
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	lw	$14,24($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	vmul.xy	$vf3,$vf3,$vf28			# vf3.xy = vf3*vf28
	# $14 = its four bytes zero-extended to four words
	pextlb	$14,$0,$14
	pextlh	$14,$0,$14
	qmtc2	$14,$vf2			# vf2 = $14 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vadd.xy	$vf3,$vf3,$vf29			# vf3.xy = vf3 + vf29
	vmulx.xyzw	$vf2,$vf2,$vf30x	# vf2 = vf2*vf30.x
	addi	$5,$5,28
	addi	$7,$7,-1
	sqc2	$vf3,48($6)
	sqc2	$vf2,32($6)
	sqc2	$vf1,0($6)
	bnez	$7,.Lnj_vu0_vu1_env_loop
	addi	$6,$6,64
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_vu1_env

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms n vertices with packed colours (no clip test).
 * Arguments: $4 = Ninja system block (view matrix +192, colour scale +1248.x), $5 = vertices (x, y, z,
 * packed BGRA: 16 bytes), $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position,
 * +32 colour as floats. Returns 0. Called from 0x1a93d8.
 */
/* 0x001b0cb0 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_col
	.ent	nj_vu0_vu1_col
nj_vu0_vu1_col:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1248($4)
.Lnj_vu0_vu1_col_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	addi	$5,$5,16
	addi	$7,$7,-1
	vmulx.xyzw	$vf2,$vf2,$vf30x	# vf2 = vf2*vf30.x
	sqc2	$vf1,0($6)
	sqc2	$vf2,32($6)
	bnez	$7,.Lnj_vu0_vu1_col_loop
	addi	$6,$6,64
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_vu1_col

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms n bare positions with white colour (no clip test).
 * Arguments: $4 = Ninja system block (view matrix +192), $5 = positions (12 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +0 view-space position, +32 (1, 1, 1, 1). Returns 0. Called from 0x1a93d8.
 */
/* 0x001b0d24 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_pos
	.ent	nj_vu0_vu1_pos
nj_vu0_vu1_pos:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	# vf2 = (1, 1, 1, 1)
	vaddw.xyz	$vf2,$vf0,$vf0w		# vf2.xyz = 1
	vmove.w	$vf2,$vf0			# vf2.w = 1
.Lnj_vu0_vu1_pos_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	addi	$5,$5,12
	addi	$7,$7,-1
	sqc2	$vf2,32($6)
	sqc2	$vf1,0($6)
	bnez	$7,.Lnj_vu0_vu1_pos_loop
	addi	$6,$6,64
	jr	$31
	movz	$2,$0,$0
	.end	nj_vu0_vu1_pos

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and lights (first light only) n vertices with normals, with an environment map (no clip test).
 * Arguments: $4 = Ninja system block (view matrix +192, light directions +384, light colours +448, map
 * scale/offset +1264/+1280), $5 = vertices (position, normal: 24 bytes), $6 = 64-byte output entries,
 * $7 = count. Entry: +0 view-space position, +32 light colour 0 * clamp(intensity 0, 0, 1) with w = 1,
 * +48 (s, t) from the view-space normal. Leaves $2 unset. Called from 0x1a93d8.
 */
/* 0x001b0d84 */
	.set	noreorder
	.text
	.globl	nj_vu0_vu1_lit_env
	.ent	nj_vu0_vu1_lit_env
nj_vu0_vu1_lit_env:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf16,384($4)
	lqc2	$vf17,400($4)
	lqc2	$vf18,416($4)
	lqc2	$vf19,432($4)
	lqc2	$vf20,448($4)
	lqc2	$vf21,464($4)
	lqc2	$vf22,480($4)
	lqc2	$vf23,496($4)
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
.Lnj_vu0_vu1_lit_env_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	lw	$12,16($5)
	lw	$13,20($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# view-space position
	vmulax.xyzw	ACC,$vf8,$vf1x		# ACC = vf8*vf1.x
	vmadday.xyzw	ACC,$vf9,$vf1y		# ACC = ACC + vf9*vf1.y
	vmaddaz.xyzw	ACC,$vf10,$vf1z		# ACC = ACC + vf10*vf1.z
	# $11 = qword ($11, $12, $13, -): pack three words
	pextlw	$11,$12,$11
	pcpyld	$11,$13,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vmaddw.xyzw	$vf1,$vf11,$vf0w	# vf1 = ACC + vf11
	# vf3 = view-space normal, vf2 = light intensities
	# vf3 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf3,$vf10,$vf2z
	# vf2 = vf16*vf2.x + vf17*vf2.y + vf18*vf2.z
	vmulax.xyzw	ACC,$vf16,$vf2x
	vmadday.xyzw	ACC,$vf17,$vf2y
	vmaddz.xyzw	$vf2,$vf18,$vf2z
	# (s, t) = normal.xy * scale + offset; colour = vf20 * clamp(intensity.x, 0, 1)
	vmul.xy	$vf3,$vf3,$vf28			# vf3.xy = vf3*vf28
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vadd.xy	$vf3,$vf3,$vf29			# vf3.xy = vf3 + vf29
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	vmulx.xyzw	$vf2,$vf20,$vf2x	# vf2 = vf20*vf2.x
	addi	$5,$5,24
	addi	$7,$7,-1
	sqc2	$vf1,0($6)
	sqc2	$vf3,48($6)
	vmove.w	$vf2,$vf0			# vf2.w = 1
	sqc2	$vf2,32($6)
	bnez	$7,.Lnj_vu0_vu1_lit_env_loop
	addi	$6,$6,64
	jr	$31
	nop
	.end	nj_vu0_vu1_lit_env

/*
 * Sega Ninja VU0 unit; retail only.
 * Loads the quadwords at $4 and $5 into VU0 registers vf1 and vf2 (inputs for a following VU0 routine).
 */
/* 0x001b0e50 */
	.set	noreorder
	.text
	.globl	nj_vu0_load_vf1_vf2
	.ent	nj_vu0_load_vf1_vf2
nj_vu0_load_vf1_vf2:
	lqc2	$vf1,0($4)
	lqc2	$vf2,0($5)
	jr	$31
	nop
	.end	nj_vu0_load_vf1_vf2

/*
 * Sega Ninja VU0 unit; retail only.
 * Colour unpack: $4 = Ninja system block (colour scale +1248.x), $5 = packed 4-byte colour, $6 = output
 * quadword, $7 = optional colour to add (or 0). Expands the four bytes to integers (pextlb/pextlh/pexew),
 * converts them to floats, scales them, adds *$7 and clamps to [0, 1] when $7 is given, and stores the
 * result (sqc2). Called from 0x1a8170.
 */
/* 0x001b0e60 */
	.set	noreorder
	.text
	.globl	nj_vu0_unpack_color
	.ent	nj_vu0_unpack_color
nj_vu0_unpack_color:
	lqc2	$vf28,1248($4)
	lh	$8,0($5)
	lh	$9,2($5)
	andi	$8,$8,0xffff
	sll	$9,$9,0x10
	or	$8,$8,$9
	pextlb	$8,$0,$8
	pextlh	$8,$0,$8
	pexew	$8,$8
	qmtc2	$8,$vf1
	vitof0.xyzw	$vf1xyzw,$vf1xyzw
	beqz	$7,.Lnj_vu0_unpack_color_1
	vmulx.xyzw	$vf1xyzw,$vf1xyzw,$vf28x
	lqc2	$vf2,0($7)
	vadd.xyzw	$vf1xyzw,$vf1xyzw,$vf2xyzw
	vminiw.xyzw	$vf1xyzw,$vf1xyzw,$vf0w
	vmaxx.xyzw	$vf1xyzw,$vf1xyzw,$vf0x
.Lnj_vu0_unpack_color_1:
	jr	$31
	sqc2	$vf1,0($6)
	.end	nj_vu0_unpack_color

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Unpacks indexed 28-byte vertex records into separate position, normal and float colour arrays.
 * Arguments: $4 = Ninja system block (colour scale +1248.x), $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex records (position, normal, packed BGRA: 28 bytes), $8 = positions out (12 bytes each),
 * $9 = normals out (12 bytes each), $10 = colours out (4 floats each). Its address is taken at 0x1aa0c4
 * (0x1aa008).
 */
/* 0x001b0eac */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_nrm_col
	.ent	nj_vu0_copy_nrm_col
nj_vu0_copy_nrm_col:
	lqc2	$vf30,1248($4)
	# $3 = record size
	li	$3,28
.Lnj_vu0_copy_nrm_col_loop:
	# $24 = record of the next index
	lh	$24,0($5)
	mult	$24,$24,$3
	addi	$5,$5,2
	addi	$6,$6,-1
	add	$24,$24,$7
	lw	$15,24($24)
	# $15 = its four bytes zero-extended to four words
	pextlb	$15,$0,$15
	pextlh	$15,$0,$15
	qmtc2	$15,$vf2			# vf2 = $15 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	lw	$12,0($24)
	lw	$13,4($24)
	lw	$14,8($24)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	vmulx.xyzw	$vf2,$vf2,$vf30x	# vf2 = vf2*vf30.x
	lw	$12,12($24)
	lw	$13,16($24)
	lw	$14,20($24)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	# store the four colour floats: pcpyud/prot3w bring each word to the low end
	qmfc2	$15,$vf2			# $15 = vf2 (128 bits)
	pcpyud	$14,$15,$8			# $14 = (vf2.z, vf2.w, ...)
	sw	$15,0($10)
	sw	$14,8($10)
	prot3w	$15,$15				# $15.w0 = vf2.y
	prot3w	$14,$14				# $14.w0 = vf2.w
	sw	$15,4($10)
	sw	$14,12($10)
	addi	$8,$8,12
	addi	$9,$9,12
	bnez	$6,.Lnj_vu0_copy_nrm_col_loop
	addi	$10,$10,16
	jr	$31
	nop
	.end	nj_vu0_copy_nrm_col

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed converted vertices into position, colour and texture-coordinate arrays until the first clipped one (unreferenced).
 * Arguments: $4 = Ninja system block, $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer
 * (64-byte entries, clip flags at +60), $8 = qword at +0 out (16 bytes each), $9 = colour (+32) out
 * (16 bytes each), $10 = (s, t) (+48) out (8 bytes each). Returns the number gathered. Loads sys+1240
 * into $f4 without using it. No code in the executable calls it.
 */
/* 0x001b0f48 */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_q_st_clip
	.ent	nj_vu0_gather_q_st_clip
nj_vu0_gather_q_st_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
.Lnj_vu0_gather_q_st_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_q_st_clip_done
	lq	$12,0($11)
	lq	$13,32($11)
	ld	$14,48($11)
	sq	$12,0($8)
	sq	$13,0($9)
	sd	$14,0($10)
	addi	$2,$2,1
	addi	$5,$5,2
	addi	$8,$8,16
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_q_st_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_q_st_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_q_st_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed converted vertices into position and colour arrays with scaled per-corner texture coordinates until the first clipped one (unreferenced).
 * Arguments: $4 = Ninja system block (texture coordinate scale +1240), $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = qword at +0 out,
 * $9 = colour (+32) out, $10 = (s, t) * scale out (8 bytes each). Returns the number gathered.
 * No code in the executable calls it.
 */
/* 0x001b0fa0 */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_q_uv_clip
	.ent	nj_vu0_gather_q_uv_clip
nj_vu0_gather_q_uv_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
.Lnj_vu0_gather_q_uv_clip_loop:
	lh	$11,0($5)
	lh	$24,2($5)
	lh	$25,4($5)
	# $11 = the indexed vertex; stop if it has clip flags
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_q_uv_clip_done
	lq	$12,0($11)
	lq	$13,32($11)
	# (s, t) as floats times the scale
	mtc1	$24,$f8
	mtc1	$25,$f9
	cvt.s.w	$f8,$f8
	cvt.s.w	$f9,$f9
	mul.s	$f8,$f8,$f4
	mul.s	$f9,$f9,$f4
	swc1	$f8,0($10)
	swc1	$f9,4($10)
	sq	$12,0($8)
	sq	$13,0($9)
	addi	$2,$2,1
	addi	$5,$5,6
	addi	$8,$8,16
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_q_uv_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_q_uv_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_q_uv_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed converted vertices into position and colour arrays until the first clipped one (unreferenced).
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = qword
 * at +0 out (16 bytes each), $9 = colour (+32) out (16 bytes each). Returns the number gathered.
 * No code in the executable calls it.
 */
/* 0x001b1018 */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_q_clip
	.ent	nj_vu0_gather_q_clip
nj_vu0_gather_q_clip:
	movz	$2,$0,$0
	nop
.Lnj_vu0_gather_q_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_q_clip_done
	lq	$12,0($11)
	lq	$13,32($11)
	addi	$2,$2,1
	addi	$5,$5,2
	sq	$12,0($8)
	sq	$13,0($9)
	addi	$8,$8,16
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_q_clip_loop
	addi	$9,$9,16
.Lnj_vu0_gather_q_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_q_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, white colour and (s, t) arrays until the first clipped one.
 * Arguments: $4 = Ninja system block, $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer
 * (64-byte entries, clip flags at +60), $8 = xyz out (12 bytes), $9 = colour out (1, 1, 1, 1),
 * $10 = (s, t) from +48 out (8 bytes). Returns the number gathered. Called from 0x1aa500.
 */
/* 0x001b1064 */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white_st_clip
	.ent	nj_vu0_gather_white_st_clip
nj_vu0_gather_white_st_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
	# $f5 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f5
.Lnj_vu0_gather_white_st_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_white_st_clip_done
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f5,0($9)
	swc1	$f5,4($9)
	swc1	$f5,8($9)
	swc1	$f5,12($9)
	lw	$12,48($11)
	lw	$13,52($11)
	sw	$12,0($10)
	sw	$13,4($10)
	addi	$2,$2,1
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_white_st_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_white_st_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_white_st_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, white colour and scaled per-corner (s, t) arrays until the first clipped one.
 * Arguments: $4 = Ninja system block (texture coordinate scale +1240), $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out (12 bytes),
 * $9 = colour out (1, 1, 1, 1), $10 = (s, t) * scale out. Returns the number gathered.
 * Called from 0x1aa8d8.
 */
/* 0x001b10e4 */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white_uv_clip
	.ent	nj_vu0_gather_white_uv_clip
nj_vu0_gather_white_uv_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
	# $f5 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f5
.Lnj_vu0_gather_white_uv_clip_loop:
	lh	$11,0($5)
	lh	$24,2($5)
	lh	$25,4($5)
	# $11 = the indexed vertex; stop if it has clip flags
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_white_uv_clip_done
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f5,0($9)
	swc1	$f5,4($9)
	swc1	$f5,8($9)
	swc1	$f5,12($9)
	# (s, t) as floats times the scale
	mtc1	$24,$f8
	mtc1	$25,$f9
	cvt.s.w	$f8,$f8
	cvt.s.w	$f9,$f9
	mul.s	$f8,$f8,$f4
	mul.s	$f9,$f9,$f4
	swc1	$f8,0($10)
	swc1	$f9,4($10)
	addi	$2,$2,1
	addi	$5,$5,6
	addi	$8,$8,12
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_white_uv_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_white_uv_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_white_uv_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz and white colour arrays until the first clipped one.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries, clip flags
 * at +60), $8 = xyz out (12 bytes), $9 = colour out (1, 1, 1, 1). Returns the number gathered.
 * Called from 0x1aac80.
 */
/* 0x001b117c */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white_clip
	.ent	nj_vu0_gather_white_clip
nj_vu0_gather_white_clip:
	# $f5 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f5
	movz	$2,$0,$0
.Lnj_vu0_gather_white_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_white_clip_done
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f5,0($9)
	swc1	$f5,4($9)
	swc1	$f5,8($9)
	swc1	$f5,12($9)
	addi	$2,$2,1
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_white_clip_loop
	addi	$9,$9,16
.Lnj_vu0_gather_white_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_white_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, colour and (s, t) arrays until the first clipped one.
 * Arguments: $4 = Ninja system block, $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer
 * (64-byte entries, clip flags at +60), $8 = xyz out (12 bytes), $9 = colour (+32) out (16 bytes),
 * $10 = (s, t) from +48 out (8 bytes). Returns the number gathered. Called from 0x1ab010 and 0x1ad0d0.
 */
/* 0x001b11e4 */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_st_clip
	.ent	nj_vu0_gather_st_clip
nj_vu0_gather_st_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
.Lnj_vu0_gather_st_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_st_clip_done
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	lw	$12,48($11)
	lw	$13,52($11)
	sw	$12,0($10)
	sw	$13,4($10)
	addi	$2,$2,1
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_st_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_st_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_st_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz and colour arrays with scaled per-corner (s, t) until the first clipped one.
 * Arguments: $4 = Ninja system block (texture coordinate scale +1240), $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out (12 bytes),
 * $9 = colour (+32) out (16 bytes), $10 = (s, t) * scale out. Returns the number gathered.
 * Called from 0x1ab3e0.
 */
/* 0x001b126c */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_uv_clip
	.ent	nj_vu0_gather_uv_clip
nj_vu0_gather_uv_clip:
	movz	$2,$0,$0
	lwc1	$f4,1240($4)
.Lnj_vu0_gather_uv_clip_loop:
	lh	$11,0($5)
	lh	$24,2($5)
	lh	$25,4($5)
	# $11 = the indexed vertex; stop if it has clip flags
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_uv_clip_done
	# (s, t) as floats times the scale
	mtc1	$24,$f8
	mtc1	$25,$f9
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	cvt.s.w	$f8,$f8
	cvt.s.w	$f9,$f9
	mul.s	$f8,$f8,$f4
	mul.s	$f9,$f9,$f4
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	swc1	$f8,0($10)
	swc1	$f9,4($10)
	addi	$2,$2,1
	addi	$5,$5,6
	addi	$8,$8,12
	addi	$6,$6,-1
	addi	$9,$9,16
	bnez	$6,.Lnj_vu0_gather_uv_clip_loop
	addi	$10,$10,8
.Lnj_vu0_gather_uv_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_uv_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz and colour arrays until the first clipped one.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries, clip flags
 * at +60), $8 = xyz out (12 bytes), $9 = colour (+32) out (16 bytes). Returns the number gathered.
 * Called from 0x1ab788.
 */
/* 0x001b130c */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_clip
	.ent	nj_vu0_gather_clip
nj_vu0_gather_clip:
	movz	$2,$0,$0
	nop
.Lnj_vu0_gather_clip_loop:
	# $11 = the indexed vertex; stop if it has clip flags
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,60($11)
	bnez	$12,.Lnj_vu0_gather_clip_done
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	addi	$2,$2,1
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_clip_loop
	addi	$9,$9,16
.Lnj_vu0_gather_clip_done:
	jr	$31
	nop
	.end	nj_vu0_gather_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list with the colour set to white.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output
 * (64-byte entries: +0, +16 and +48 copied, +32 = (1, 1, 1, 1)). Called from 0x1aa500. Same code as
 * 0x1b0f58.
 */
/* 0x001b1380 */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_white
	.ent	nj_vu0_copy_white
nj_vu0_copy_white:
	# vf2 = (1, 1, 1, 1)
	vmulw.w	$vf2,$vf0,$vf0w			# vf2.w = 1
	vaddw.xyz	$vf2,$vf0,$vf0w		# vf2.xyz = 1
.Lnj_vu0_copy_white_loop:
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lqc2	$vf1,0($12)
	lqc2	$vf3,48($12)
	lqc2	$vf4,16($12)
	sqc2	$vf1,0($8)
	sqc2	$vf2,32($8)
	sqc2	$vf3,48($8)
	sqc2	$vf4,16($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_copy_white_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy_white

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list with white colour and scaled per-corner texture coordinates.
 * Arguments: $4 = Ninja system block (scales +1232; z = texture coordinate scale), $5 = 6-byte records
 * (vertex index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (64-byte
 * entries: +0 and +16 copied, +32 = (1, 1, 1, 1), +48 = (s, t) * scale with w from the vertex).
 * Called from 0x1aa8d8.
 */
/* 0x001b13c8 */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_white_uv
	.ent	nj_vu0_copy_white_uv
nj_vu0_copy_white_uv:
	lqc2	$vf11,1232($4)
	# vf2 = (1, 1, 1, 1)
	vmulw.w	$vf2,$vf0,$vf0w			# vf2.w = 1
	vaddw.xyz	$vf2,$vf0,$vf0w		# vf2.xyz = 1
	nop
.Lnj_vu0_copy_white_uv_loop:
	lh	$12,0($5)
	lh	$13,2($5)
	lh	$14,4($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	# vf3.xy = (s, t) as floats
	pextlw	$13,$14,$13			# $13 = qword ($13, $14, -, -)
	qmtc2	$13,$vf3			# vf3 = $13 (128 bits)
	vitof0.xy	$vf3,$vf3		# vf3.xy = vf3 integer to float
	lqc2	$vf1,0($12)
	lqc2	$vf4,16($12)
	lqc2	$vf5,48($12)
	vmulz.xy	$vf3,$vf3,$vf11z	# vf3.xy = vf3*vf11.z
	sqc2	$vf1,0($8)
	sqc2	$vf2,32($8)
	sqc2	$vf4,16($8)
	# keep the vertex's +48 w (its clip flags)
	vmove.w	$vf3,$vf5			# vf3.w = vf5.w
	addi	$5,$5,6
	addi	$6,$6,-1
	sqc2	$vf3,48($8)
	bnez	$6,.Lnj_vu0_copy_white_uv_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy_white_uv

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list with the colour set to white.
 * Same code as 0x1b0ea4 ($5 = vertex indices, $6 = count, $7 = vertex buffer, $8 = 64-byte output
 * entries). Called from 0x1aac80.
 */
/* 0x001b1434 */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_white_b
	.ent	nj_vu0_copy_white_b
nj_vu0_copy_white_b:
	# vf2 = (1, 1, 1, 1)
	vmulw.w	$vf2,$vf0,$vf0w			# vf2.w = 1
	vaddw.xyz	$vf2,$vf0,$vf0w		# vf2.xyz = 1
.Lnj_vu0_copy_white_b_loop:
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lqc2	$vf1,0($12)
	lqc2	$vf3,48($12)
	lqc2	$vf4,16($12)
	sqc2	$vf1,0($8)
	sqc2	$vf2,32($8)
	sqc2	$vf3,48($8)
	sqc2	$vf4,16($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_copy_white_b_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy_white_b

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list (unreferenced).
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output
 * (64-byte entries). No code in the executable calls it.
 */
/* 0x001b147c */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_b
	.ent	nj_vu0_copy_b
nj_vu0_copy_b:
.Lnj_vu0_copy_b_loop:
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lqc2	$vf1,0($12)
	lqc2	$vf2,32($12)
	lqc2	$vf3,48($12)
	lqc2	$vf4,16($12)
	sqc2	$vf1,0($8)
	sqc2	$vf2,32($8)
	sqc2	$vf3,48($8)
	sqc2	$vf4,16($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_copy_b_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy_b

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list with scaled per-corner texture coordinates.
 * Arguments: $4 = Ninja system block (scales +1232; z = texture coordinate scale), $5 = 6-byte records
 * (vertex index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (64-byte
 * entries: +0, +16, +32 copied, +48 = (s, t) * scale with w from the vertex). Called from 0x1ab3e0
 * and 0x1abde8.
 */
/* 0x001b14c0 */
	.set	noreorder
	.text
	.globl	nj_vu0_copy_uv
	.ent	nj_vu0_copy_uv
nj_vu0_copy_uv:
	lqc2	$vf11,1232($4)
.Lnj_vu0_copy_uv_loop:
	lh	$12,0($5)
	lh	$13,2($5)
	lh	$14,4($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	# vf3.xy = (s, t) as floats
	pextlw	$13,$14,$13			# $13 = qword ($13, $14, -, -)
	qmtc2	$13,$vf3			# vf3 = $13 (128 bits)
	vitof0.xy	$vf3,$vf3		# vf3.xy = vf3 integer to float
	lqc2	$vf1,0($12)
	lqc2	$vf2,32($12)
	lqc2	$vf4,16($12)
	lqc2	$vf5,48($12)
	vmulz.xy	$vf3,$vf3,$vf11z	# vf3.xy = vf3*vf11.z
	addi	$5,$5,6
	addi	$6,$6,-1
	sqc2	$vf1,0($8)
	# keep the vertex's +48 w (its clip flags)
	vmove.w	$vf3,$vf5			# vf3.w = vf5.w
	sqc2	$vf2,32($8)
	sqc2	$vf4,16($8)
	sqc2	$vf3,48($8)
	bnez	$6,.Lnj_vu0_copy_uv_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy_uv

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Copies indexed 64-byte vertices into a contiguous list.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output
 * (64-byte entries). Called from 0x1ab788 and 0x1ac0d0.
 */
/* 0x001b1524 */
	.set	noreorder
	.text
	.globl	nj_vu0_copy
	.ent	nj_vu0_copy
nj_vu0_copy:
.Lnj_vu0_copy_loop:
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lqc2	$vf1,0($12)
	lqc2	$vf2,16($12)
	lqc2	$vf4,32($12)
	lqc2	$vf3,48($12)
	sqc2	$vf1,0($8)
	sqc2	$vf2,16($8)
	sqc2	$vf4,32($8)
	sqc2	$vf3,48($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_copy_loop
	addi	$8,$8,64
	jr	$31
	nop
	.end	nj_vu0_copy

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZF qwords with fog for an index list of converted vertices, stopping at the first clipped one.
 * Arguments: $4 = Ninja system block (colour multiplier +976, offset +992, scale +1040.w, fog
 * parameters +1376), $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries:
 * XYZ +16, colour +32, (s, t, q) +48, clip flags +60), $8 = output (48 bytes per vertex). Returns the
 * number emitted. Called from 0x1abb18.
 */
/* 0x001b1568 */
	.set	noreorder
	.text
	.globl	nj_vu0_emit_fog_clip
	.ent	nj_vu0_emit_fog_clip
nj_vu0_emit_fog_clip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf11,1040($4)
	lqc2	$vf13,1376($4)
	movz	$2,$0,$0
.Lnj_vu0_emit_fog_clip_loop:
	# $12 = the indexed vertex; stop if it has clip flags
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lw	$11,60($12)
	bnez	$11,.Lnj_vu0_emit_fog_clip_done
	lqc2	$vf1,16($12)
	lqc2	$vf2,32($12)
	lqc2	$vf3,48($12)
	# fog = clamp(vf13.w * q, 0, vf13.z) as 12.4 fixed point in XYZF w (the first write is overwritten)
	vmulx.w	$vf1,$vf0,$vf13x		# vf1.w = vf13.x
	vmulz.w	$vf1,$vf13,$vf3z		# vf1.w = vf13.w*vf3.z
	vminiz.w	$vf1,$vf1,$vf13z	# vf1.w = min(vf1.w, vf13.z)
	vmaxx.w	$vf1,$vf1,$vf0x			# vf1.w = max(vf1.w, 0)
	vftoi4.w	$vf1,$vf1		# vf1.w = vf1.w as 12.4 fixed point
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.w as integers
	vmul.xyzw	$vf2,$vf2,$vf8		# vf2 = vf2*vf8
	vadd.xyz	$vf2,$vf2,$vf9		# vf2.xyz = vf2 + vf9
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vmulw.xyzw	$vf2,$vf2,$vf11w	# vf2 = vf2*vf11.w
	vftoi0.xyzw	$vf2,$vf2		# vf2 = vf2 as integer
	# STQ = (s * q, t * q, q)
	vmulz.xy	$vf3,$vf3,$vf3z		# vf3.xy = vf3*vf3.z
	sqc2	$vf1,32($8)
	sqc2	$vf2,16($8)
	sqc2	$vf3,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lnj_vu0_emit_fog_clip_loop
	addi	$2,$2,1
.Lnj_vu0_emit_fog_clip_done:
	jr	$31
	nop
	.end	nj_vu0_emit_fog_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZF qwords with fog and per-corner texture coordinates, stopping at the first clipped vertex.
 * Arguments: $4 = Ninja system block (colour multiplier +976, offset +992, scale +1040.w, texture
 * coordinate scale +1232.z, fog parameters +1376), $5 = 6-byte records (vertex index, s, t as shorts),
 * $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex). Returns the
 * number emitted. Called from 0x1abde8.
 */
/* 0x001b15f4 */
	.set	noreorder
	.text
	.globl	nj_vu0_emit_fog_uv_clip
	.ent	nj_vu0_emit_fog_uv_clip
nj_vu0_emit_fog_uv_clip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf11,1040($4)
	lqc2	$vf12,1232($4)
	lqc2	$vf13,1376($4)
	movz	$2,$0,$0
.Lnj_vu0_emit_fog_uv_clip_loop:
	lh	$12,0($5)
	lh	$13,2($5)
	lh	$14,4($5)
	# $12 = the indexed vertex; stop if it has clip flags
	sll	$12,$12,0x6
	add	$12,$12,$7
	lw	$11,60($12)
	bnez	$11,.Lnj_vu0_emit_fog_uv_clip_done
	lqc2	$vf1,16($12)
	lqc2	$vf2,32($12)
	lqc2	$vf3,48($12)
	# STQ = ((s, t) * scale * q, q)
	pextlw	$13,$14,$13			# $13 = qword ($13, $14, -, -)
	qmtc2	$13,$vf4			# vf4 = $13 (128 bits)
	vitof0.xy	$vf4,$vf4		# vf4.xy = vf4 integer to float
	vmulz.xy	$vf3,$vf4,$vf12z	# vf3.xy = vf4*vf12.z
	vmulz.xy	$vf3,$vf3,$vf3z		# vf3.xy = vf3*vf3.z
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.w as integers
	vmul.xyzw	$vf2,$vf2,$vf8		# vf2 = vf2*vf8
	vadd.xyz	$vf2,$vf2,$vf9		# vf2.xyz = vf2 + vf9
	vminiw.xyzw	$vf2,$vf2,$vf0w		# vf2 = min(vf2, 1)
	vmaxx.xyzw	$vf2,$vf2,$vf0x		# vf2 = max(vf2, 0)
	vmulw.xyzw	$vf2,$vf2,$vf11w	# vf2 = vf2*vf11.w
	vftoi0.xyzw	$vf2,$vf2		# vf2 = vf2 as integer
	# fog = clamp(vf13.w * q, 0, vf13.z) as 12.4 fixed point in XYZF w (the first write is overwritten)
	vmulx.w	$vf1,$vf0,$vf13x		# vf1.w = vf13.x
	vmulz.w	$vf1,$vf13,$vf3z		# vf1.w = vf13.w*vf3.z
	vminiz.w	$vf1,$vf1,$vf13z	# vf1.w = min(vf1.w, vf13.z)
	vmaxx.w	$vf1,$vf1,$vf0x			# vf1.w = max(vf1.w, 0)
	vftoi4.w	$vf1,$vf1		# vf1.w = vf1.w as 12.4 fixed point
	sqc2	$vf1,32($8)
	sqc2	$vf2,16($8)
	sqc2	$vf3,0($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lnj_vu0_emit_fog_uv_clip_loop
	addi	$2,$2,1
.Lnj_vu0_emit_fog_uv_clip_done:
	jr	$31
	nop
	.end	nj_vu0_emit_fog_uv_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Emits GS STQ/RGBAQ/XYZF qwords with fog for an index list, the colour scaled per component, stopping at the first clipped vertex.
 * Arguments: $4 = Ninja system block (colour multiplier +976, offset +992, per-component scale +1040, fog
 * parameters +1376), $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries),
 * $8 = output (48 bytes per vertex; STQ is the vertex's +48 unchanged). Returns the number emitted.
 * Called from 0x1ac0d0.
 */
/* 0x001b169c */
	.set	noreorder
	.text
	.globl	nj_vu0_emit_fog_col_clip
	.ent	nj_vu0_emit_fog_col_clip
nj_vu0_emit_fog_col_clip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf11,1040($4)
	lqc2	$vf12,1376($4)
	movz	$2,$0,$0
.Lnj_vu0_emit_fog_col_clip_loop:
	# $9 = the indexed vertex; stop if it has clip flags
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lw	$11,60($9)
	bnez	$11,.Lnj_vu0_emit_fog_col_clip_done
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# fog = clamp(vf12.w * q, 0, vf12.z) as 12.4 fixed point in XYZF w (the first write is overwritten)
	vmulx.w	$vf24,$vf0,$vf12x		# vf24.w = vf12.x
	vmulz.w	$vf24,$vf12,$vf26z		# vf24.w = vf12.w*vf26.z
	vminiz.w	$vf24,$vf24,$vf12z	# vf24.w = min(vf24.w, vf12.z)
	vmaxx.w	$vf24,$vf24,$vf0x		# vf24.w = max(vf24.w, 0)
	vftoi4.w	$vf24,$vf24		# vf24.w = vf24.w as 12.4 fixed point
	# colour = clamp(colour * vf8 + vf9, 0, 1) * vf11 as integers
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmul.xyzw	$vf25,$vf25,$vf11	# vf25 = vf25*vf11
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lnj_vu0_emit_fog_col_clip_loop
	addi	$2,$2,1
.Lnj_vu0_emit_fog_col_clip_done:
	jr	$31
	nop
	.end	nj_vu0_emit_fog_col_clip

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, white colour and (s, t) arrays.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out
 * (12 bytes), $9 = colour out (1, 1, 1, 1), $10 = (s, t) from +48 out (8 bytes). Its address is taken
 * at 0x1acaf8 (0x1acab0).
 */
/* 0x001b1724 */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white_st
	.ent	nj_vu0_gather_white_st
nj_vu0_gather_white_st:
	# $f5 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f5
	nop
.Lnj_vu0_gather_white_st_loop:
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f5,0($9)
	swc1	$f5,4($9)
	swc1	$f5,8($9)
	swc1	$f5,12($9)
	lw	$12,48($11)
	lw	$13,52($11)
	sw	$12,0($10)
	sw	$13,4($10)
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$9,$9,16
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_white_st_loop
	addi	$10,$10,8
	jr	$31
	nop
	.end	nj_vu0_gather_white_st

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, white colour and scaled per-corner (s, t) arrays.
 * Arguments: $4 = Ninja system block (texture coordinate scale +1240), $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out (12 bytes),
 * $9 = colour out (1, 1, 1, 1), $10 = (s, t) * scale out. Clobbers $2 and $3. Its address is taken at
 * 0x1accf8 (0x1accb0).
 */
/* 0x001b1794 */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white_uv
	.ent	nj_vu0_gather_white_uv
nj_vu0_gather_white_uv:
	lwc1	$f4,1240($4)
	# $f5 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f5
.Lnj_vu0_gather_white_uv_loop:
	lh	$11,0($5)
	lh	$2,2($5)
	lh	$3,4($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f5,0($9)
	swc1	$f5,4($9)
	swc1	$f5,8($9)
	swc1	$f5,12($9)
	# (s, t) as floats times the scale
	mtc1	$2,$f8
	mtc1	$3,$f9
	cvt.s.w	$f8,$f8
	cvt.s.w	$f9,$f9
	mul.s	$f8,$f8,$f4
	mul.s	$f9,$f9,$f4
	swc1	$f8,0($10)
	swc1	$f9,4($10)
	addi	$5,$5,6
	addi	$8,$8,12
	addi	$9,$9,16
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_white_uv_loop
	addi	$10,$10,8
	jr	$31
	nop
	.end	nj_vu0_gather_white_uv

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz and white colour arrays.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out
 * (12 bytes), $9 = colour out (1, 1, 1, 1). Its address is taken at 0x1acf08 (0x1acec0).
 */
/* 0x001b181c */
	.set	noreorder
	.set	noat
	.text
	.globl	nj_vu0_gather_white
	.ent	nj_vu0_gather_white
nj_vu0_gather_white:
	# $f4 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f4
	nop
.Lnj_vu0_gather_white_loop:
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	swc1	$f4,0($9)
	swc1	$f4,4($9)
	swc1	$f4,8($9)
	swc1	$f4,12($9)
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_white_loop
	addi	$9,$9,16
	jr	$31
	nop
	.end	nj_vu0_gather_white

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, colour and (s, t) arrays.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out
 * (12 bytes), $9 = colour (+32) out (16 bytes), $10 = (s, t) from +48 out (8 bytes). Called from 0x1ac3a8;
 * its address is taken in 0x1acab0.
 */
/* 0x001b1878 */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_st
	.ent	nj_vu0_gather_st
	.align	2	# steer(frag): a new frag, so gas sees the whole loop and adds no short-loop nops
nj_vu0_gather_st:
.Lnj_vu0_gather_st_loop:
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	lw	$12,48($11)
	lw	$13,52($11)
	sw	$12,0($10)
	sw	$13,4($10)
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$9,$9,16
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_st_loop
	addi	$10,$10,8
	jr	$31
	nop
	.end	nj_vu0_gather_st

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz, colour and scaled per-corner (s, t) arrays.
 * Arguments: $4 = Ninja system block (texture coordinate scale +1240), $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out (12 bytes),
 * $9 = colour (+32) out (16 bytes), $10 = (s, t) * scale out. Clobbers $2 and $3. Called from 0x1ac620;
 * its address is taken in 0x1accb0.
 */
/* 0x001b18ec */
	.set	noreorder
	.text
	.globl	nj_vu0_gather_uv
	.ent	nj_vu0_gather_uv
nj_vu0_gather_uv:
	lwc1	$f4,1240($4)
.Lnj_vu0_gather_uv_loop:
	lh	$11,0($5)
	lh	$2,2($5)
	lh	$3,4($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	# (s, t) as floats times the scale
	mtc1	$2,$f8
	mtc1	$3,$f9
	cvt.s.w	$f8,$f8
	cvt.s.w	$f9,$f9
	mul.s	$f8,$f8,$f4
	mul.s	$f9,$f9,$f4
	swc1	$f8,0($10)
	swc1	$f9,4($10)
	addi	$5,$5,6
	addi	$8,$8,12
	addi	$9,$9,16
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_uv_loop
	addi	$10,$10,8
	jr	$31
	nop
	.end	nj_vu0_gather_uv

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Gathers indexed vertices into xyz and colour arrays.
 * Arguments: $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = xyz out
 * (12 bytes), $9 = colour (+32) out (16 bytes). Called from 0x1ac8a0; its address is taken in 0x1acec0.
 */
/* 0x001b197c */
	.set	noreorder
	.text
	.globl	nj_vu0_gather
	.ent	nj_vu0_gather
nj_vu0_gather:
.Lnj_vu0_gather_loop:
	lh	$11,0($5)
	sll	$11,$11,0x6
	add	$11,$11,$7
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($9)
	sw	$13,4($9)
	sw	$14,8($9)
	sw	$15,12($9)
	addi	$5,$5,2
	addi	$8,$8,12
	addi	$6,$6,-1
	bnez	$6,.Lnj_vu0_gather_loop
	addi	$9,$9,16
	jr	$31
	nop
	.end	nj_vu0_gather

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Loads emitter VU0 constants: vf8 = sys+976, vf9 = sys+992, vf10 = sys+960, vf11 = sys+1040 (unreferenced).
 * Argument: $4 = Ninja system block. Like 0x1af4e8 but vf11 comes from +1040. Last routine of the
 * unit; the nop at 0x1b1514 is padding before the next object. No code in the executable calls it.
 */
/* 0x001b19dc */
	.set	noreorder
	.text
	.globl	nj_vu0_load_emit_consts
	.ent	nj_vu0_load_emit_consts
nj_vu0_load_emit_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	jr	$31
	lqc2	$vf11,1040($4)
	.end	nj_vu0_load_emit_consts
