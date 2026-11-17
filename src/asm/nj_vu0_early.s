/*
 * nj_vu0_early.s: hand-written assembly, retail (US v1.01) none (routines only the earlier build has).
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n vertices with an environment map (texture coordinates from the view-space normal).
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768, map scale/offset +1264/+1280,
 * colour scale +1248.x), $5 = vertices (position, normal, packed BGRA: 28 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +0 view-space position, +16 clip-space position, +32 colour as floats,
 * +48 (s, t, -, clip flags). Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
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
	addi	$3,$0,63
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
	andi	$8,$8,0x3f
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
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n vertices with packed colours.
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768, colour scale +1248.x),
 * $5 = vertices (x, y, z, packed BGRA: 16 bytes), $6 = 64-byte output entries, $7 = count. Entry:
 * +0 view-space position, +16 clip-space position, +32 colour as floats, +60 clip flags (+48 xyz
 * receive whatever vf3 holds). Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
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
	addi	$3,$0,63
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
	andi	$8,$8,0x3f
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
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms and clip-tests n bare positions (white colour).
 * Arguments: $4 = Ninja system block (view matrix +192, clip matrix +768), $5 = positions (12 bytes),
 * $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16 clip-space position,
 * +32 (1, 1, 1, 1), +60 clip flags. Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8f30.
 */
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
	addi	$3,$0,63
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
	andi	$8,$8,0x3f
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
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Transforms, lights (first light only) and clip-tests n vertices with normals, with an environment map.
 * Arguments: $4 = Ninja system block (view matrix +192, light directions +384, light colours +448,
 * clip matrix +768, map scale/offset +1264/+1280), $5 = vertices (position, normal: 24 bytes), $6 = 64-byte
 * output entries, $7 = count. Entry: +0 view-space position, +16 clip-space position, +32 colour =
 * light colour 0 * clamp(intensity 0, 0, 1) with w = 1, +48 (s, t, -, clip flags).
 * Returns ((OR of the clip codes) << 8) | (AND of the clip codes). Called from 0x1a8f30.
 */
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
	addi	$3,$0,63
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
	andi	$8,$8,0x3f
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
 * Sega Ninja VU0 unit (object at 0x1ae534-0x1b1514). Last weighted skinning pass with lighting and environment map: adds the bone contribution, then clip-tests the sum.
 * Arguments: $4 = Ninja system block (bone matrix +192, light directions +384, light colours +448, clip
 * matrix +704, map scale/offset +1264/+1280, weight scale +1232.w), $5 = 28-byte records (position,
 * normal, index/weight word at +24), $6 = vertex buffer (64-byte entries), $7 = count. Entry: +0 summed
 * position, +16 its clip-space position, +32 summed lit colour (w = 1), +48 (s, t) from the summed
 * normal, +60 clip flags. Returns ((OR of the clip codes) << 8) | (AND of the clip codes).
 * Called from 0x1a8e70 (after 0x1b069c and 0x1b059c).
 */
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
	addi	$3,$0,63
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
	andi	$8,$8,0x3f
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
