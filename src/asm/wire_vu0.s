/*
 * wire_vu0.s: hand-written assembly, retail (US v1.01) 0x0017bb8c-0x0017c124.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Box clip test: transforms 8 points by the product of two 4x4 matrices and returns their combined clip codes.
 * Arguments: $4 = matrix A (16-byte rows), $5 = 8 points (4 floats each), $6 = count (ignored: always 8),
 * $7 = matrix B. Returns ((OR of the 8 clip codes) << 8) | (AND of the 8 clip codes), 6 bits each.
 */
/* 0x0017bb8c */
	.set	noreorder
	.text
	.globl	vu0_box_clip_codes
	.ent	vu0_box_clip_codes
vu0_box_clip_codes:
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	lqc2	$vf8,0($7)
	lqc2	$vf9,16($7)
	lqc2	$vf10,32($7)
	lqc2	$vf11,48($7)
	# vf24..vf27 = the four rows of B, each transformed by A (the combined matrix)
	# vf24 = vf4*vf8.x + vf5*vf8.y + vf6*vf8.z + vf7*vf8.w
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmadday.xyzw	ACC,$vf5,$vf8y
	vmaddaz.xyzw	ACC,$vf6,$vf8z
	vmaddw.xyzw	$vf24,$vf7,$vf8w
	# vf25 = vf4*vf9.x + vf5*vf9.y + vf6*vf9.z + vf7*vf9.w
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmadday.xyzw	ACC,$vf5,$vf9y
	vmaddaz.xyzw	ACC,$vf6,$vf9z
	vmaddw.xyzw	$vf25,$vf7,$vf9w
	# vf26 = vf4*vf10.x + vf5*vf10.y + vf6*vf10.z + vf7*vf10.w
	vmulax.xyzw	ACC,$vf4,$vf10x
	vmadday.xyzw	ACC,$vf5,$vf10y
	vmaddaz.xyzw	ACC,$vf6,$vf10z
	vmaddw.xyzw	$vf26,$vf7,$vf10w
	# vf27 = vf4*vf11.x + vf5*vf11.y + vf6*vf11.z + vf7*vf11.w
	vmulax.xyzw	ACC,$vf4,$vf11x
	vmadday.xyzw	ACC,$vf5,$vf11y
	vmaddaz.xyzw	ACC,$vf6,$vf11z
	vmaddw.xyzw	$vf27,$vf7,$vf11w
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones), $6 = 2 passes of 4 points
	movz	$2,$0,$0
	addi	$3,$0,63
	addi	$6,$0,2
.Lvu0_box_clip_codes_loop:
	lqc2	$vf1,0($5)
	lqc2	$vf2,16($5)
	lqc2	$vf3,32($5)
	lqc2	$vf4,48($5)
	# transform four points by A*B (w taken as 1)
	# vf1 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf1,$vf27,$vf0w
	# vf2 = vf24*vf2.x + vf25*vf2.y + vf26*vf2.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf2x
	vmadday.xyzw	ACC,$vf25,$vf2y
	vmaddaz.xyzw	ACC,$vf26,$vf2z
	vmaddw.xyzw	$vf2,$vf27,$vf0w
	# vf3 = vf24*vf3.x + vf25*vf3.y + vf26*vf3.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf3x
	vmadday.xyzw	ACC,$vf25,$vf3y
	vmaddaz.xyzw	ACC,$vf26,$vf3z
	vmaddw.xyzw	$vf3,$vf27,$vf0w
	# vf4 = vf24*vf4.x + vf25*vf4.y + vf26*vf4.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf4x
	vmadday.xyzw	ACC,$vf25,$vf4y
	vmaddaz.xyzw	ACC,$vf26,$vf4z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	vclipw.xyz	$vf1,$vf1w		# clip judgement of vf1.xyz against +-vf1.w
	vclipw.xyz	$vf2,$vf2w		# clip judgement of vf2.xyz against +-vf2.w
	vclipw.xyz	$vf3,$vf3w		# clip judgement of vf3.xyz against +-vf3.w
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# let the four clip judgements reach the flag register
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$11,$vi18			# $11 = clip flags (6 bits per judgement, newest in bits 0-5)
	# split the four 6-bit codes and accumulate them
	srl	$8,$11,0x12
	srl	$9,$11,0xc
	srl	$10,$11,0x6
	andi	$8,$8,0x3f
	andi	$9,$9,0x3f
	andi	$10,$10,0x3f
	andi	$11,$11,0x3f
	or	$2,$2,$8
	or	$2,$2,$9
	or	$2,$2,$10
	or	$2,$2,$11
	and	$3,$3,$8
	and	$3,$3,$9
	and	$3,$3,$10
	and	$3,$3,$11
	addi	$6,$6,-1
	bnez	$6,.Lvu0_box_clip_codes_loop
	addi	$5,$5,64
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	vu0_box_clip_codes

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Converts n wire vertices for drawing: two scale/offset stages, a 2D clip test and colour scaling.
 * Arguments: $4 = context (scale/offset vectors at +1136..+1184, colour scale at +1248), $5 = source
 * vertices (x, y, z, packed RGBA: 16 bytes each), $6 = 64-byte output entries, $7 = count.
 * Output entry: +0 position, +16 scaled position, +32 colour as floats, +48 w = clip flags.
 * Returns the number of vertices outside the x/y range.
 */
/* 0x0017bcc4 */
	.set	noreorder
	.text
	.globl	vu0_wire_vertex_convert
	.ent	vu0_wire_vertex_convert
vu0_wire_vertex_convert:
	lqc2	$vf8,1136($4)
	lqc2	$vf9,1152($4)
	lqc2	$vf10,1168($4)
	lqc2	$vf11,1184($4)
	lqc2	$vf28,1248($4)
	movz	$2,$0,$0
.Lvu0_wire_vertex_convert_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,12($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf4				# vf4 = $8 (128 bits)
	# vf5 = (0,0,0,1) + vf9 + vf4*vf8
	vadda.xyzw	ACC,$vf0,$vf9
	vmadd.xyzw	$vf5,$vf4,$vf8
	# vf5 = (0,0,0,1) + vf11 + vf5*vf10
	vadda.xyzw	ACC,$vf0,$vf11
	vmadd.xyzw	$vf5,$vf5,$vf10
	vclipw.xyz	$vf5,$vf0w		# clip judgement of vf5.xyz against +-1
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	qmtc2	$11,$vf2			# vf2 = $11 (128 bits)
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	vmul.xyzw	$vf2,$vf2,$vf28		# vf2 = vf2*vf28
	vnop
	vnop
	# only the x and y planes count (andi 0xf)
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0xf
	beqz	$8,.Lvu0_wire_vertex_convert_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_wire_vertex_convert_inside:
	# entry +48 w = this vertex's clip flags
	vmr32.w	$vf3,$vf1			# vf3.w = vf1.x
	addi	$7,$7,-1
	addi	$5,$5,16
	sqc2	$vf4,0($6)
	sqc2	$vf5,16($6)
	sqc2	$vf3,48($6)
	sqc2	$vf2,32($6)
	bnez	$7,.Lvu0_wire_vertex_convert_loop
	addi	$6,$6,64
	jr	$31
	nop
	.end	vu0_wire_vertex_convert

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Transforms n vertices by two matrices, clip-tests them and converts their colour.
 * Arguments: $4 = context (matrix at +192, clip matrix at +768, colour scale at +1248), $5 = source
 * vertices (x, y, z, packed RGBA), $6 = 64-byte output entries, $7 = count.
 * Output entry: +0 position * matrix +192, +16 position * matrix +768 (clip space), +32 colour as
 * floats, +60 clip flags. Returns the number of vertices with any clip flag set.
 */
/* 0x0017bd68 */
	.set	noreorder
	.text
	.globl	vu0_vertex_transform_clip_col
	.ent	vu0_vertex_transform_clip_col
vu0_vertex_transform_clip_col:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	lqc2	$vf28,1248($4)
	movz	$2,$0,$0
.Lvu0_vertex_transform_clip_col_loop:
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
	vitof0.xyzw	$vf2,$vf2		# vf2 = vf2 integer to float
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	vmul.xyzw	$vf2,$vf2,$vf28		# vf2 = vf2*vf28
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lvu0_vertex_transform_clip_col_inside
	sw	$8,60($6)
	addi	$2,$2,1
.Lvu0_vertex_transform_clip_col_inside:
	addi	$7,$7,-1
	addi	$5,$5,16
	sqc2	$vf5,0($6)
	sqc2	$vf4,16($6)
	sqc2	$vf2,32($6)
	bnez	$7,.Lvu0_vertex_transform_clip_col_loop
	addi	$6,$6,64
	jr	$31
	nop
	.end	vu0_vertex_transform_clip_col

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Weighted vertex blend: adds weight * (matrix * position) to the indexed output vertex.
 * Arguments: $4 = context (matrix at +192, weight scale in +1232.w), $5 = 28-byte source records
 * (x, y, z at +0, vertex index in the low half of +24, integer weight in the high half), $6 = vertex
 * buffer (64-byte entries), $7 = count. The sum is kept in xyz of the entry's first qword. Returns 0.
 */
/* 0x0017be24 */
	.set	noreorder
	.text
	.globl	vu0_weight_blend_add
	.ent	vu0_weight_blend_add
vu0_weight_blend_add:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
.Lvu0_weight_blend_add_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer)
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
	# vf6.w = weight * scale
	vmulw.w	$vf6,$vf1,$vf30w		# vf6.w = vf1.w*vf30.w
	# vf7 = the vertex's running sum
	lqc2	$vf7,0($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vmulw.xyz	$vf1,$vf1,$vf6w		# vf1.xyz = vf1*vf6.w
	vadd.xyz	$vf5,$vf1,$vf7		# vf5.xyz = vf1 + vf7
	addi	$7,$7,-1
	addi	$5,$5,28
	# store the new sum (delay slot)
	bnez	$7,.Lvu0_weight_blend_add_loop
	sqc2	$vf5,0($12)
	jr	$31
	movz	$2,$0,$0
	.end	vu0_weight_blend_add

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Final weighted blend pass: adds weight * (matrix * position) to the indexed vertex, then clip-tests the sum.
 * Arguments as 0x17bd2c ($4 context: matrix +192, clip matrix +704, weight scale +1232.w; $5 = 28-byte
 * records; $6 = vertex buffer; $7 = count). Writes the sum to +0, its clip-space position to +16 and
 * its clip flags to +48 w. Returns the number of vertices with any clip flag set.
 */
/* 0x0017bea4 */
	.set	noreorder
	.text
	.globl	vu0_weight_blend_last
	.ent	vu0_weight_blend_last
vu0_weight_blend_last:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,704($4)
	lqc2	$vf25,720($4)
	lqc2	$vf26,736($4)
	lqc2	$vf27,752($4)
	lqc2	$vf30,1232($4)
	movz	$2,$0,$0
.Lvu0_weight_blend_last_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer)
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
	# vf6.w = weight * scale
	vmulw.w	$vf6,$vf1,$vf30w		# vf6.w = vf1.w*vf30.w
	# vf7 = the vertex's running sum
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
	# let the clip judgement reach the flag register
	vnop
	vnop
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	beqz	$8,.Lvu0_weight_blend_last_inside
	qmtc2	$8,$vf7				# vf7 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_weight_blend_last_inside:
	vmr32.w	$vf3,$vf7			# vf3.w = vf7.x
	sqc2	$vf5,0($12)
	sqc2	$vf4,16($12)
	sqc2	$vf3,48($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_weight_blend_last_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	vu0_weight_blend_last

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Weighted vertex blend: adds weight * (matrix * position) to the indexed vertex; returns the count.
 * Same as 0x17bd2c (context $4: matrix +192, weight scale +1232.w; $5 = 28-byte records with the
 * index/weight word at +24; $6 = vertex buffer; $7 = count) but returns the count instead of 0.
 */
/* 0x0017bf88 */
	.set	noreorder
	.text
	.globl	vu0_weight_blend_add_count
	.ent	vu0_weight_blend_add_count
vu0_weight_blend_add_count:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
	movz	$2,$7,$0
.Lvu0_weight_blend_add_count_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	lw	$11,24($5)
	# $12 = address of the target vertex (index * 64 + buffer)
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
	# vf6.w = weight * scale
	vmulw.w	$vf6,$vf1,$vf30w		# vf6.w = vf1.w*vf30.w
	# vf7 = the vertex's running sum
	lqc2	$vf7,0($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vmulw.xyz	$vf1,$vf1,$vf6w		# vf1.xyz = vf1*vf6.w
	vadd.xyz	$vf1,$vf1,$vf7		# vf1.xyz = vf1 + vf7
	sqc2	$vf1,0($12)
	addi	$7,$7,-1
	bnez	$7,.Lvu0_weight_blend_add_count_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	vu0_weight_blend_add_count

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). First weighted blend pass: stores weight * (matrix * position) into the indexed vertex.
 * Arguments: $4 = context (matrix +192, weight scale +1232.w), $5 = 28-byte records (x, y, z;
 * unsigned vertex index at +24, unsigned weight at +26), $6 = vertex buffer (64-byte entries),
 * $7 = count. Overwrites the entry's first qword (xyz scaled by the weight). Returns the count.
 */
/* 0x0017c00c */
	.set	noreorder
	.text
	.globl	vu0_weight_blend_first
	.ent	vu0_weight_blend_first
vu0_weight_blend_first:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf30,1232($4)
	movz	$2,$7,$0
.Lvu0_weight_blend_first_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $12 = address of the target vertex (index * 64 + buffer), $11 = weight
	lhu	$12,24($5)
	lhu	$11,26($5)
	sll	$12,$12,0x6
	add	$12,$12,$6
	qmtc2	$11,$vf6			# vf6 = $11 (128 bits)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	vitof0.x	$vf6,$vf6		# vf6.x = vf6.x integer to float
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf1 = matrix * position, with the weight scaling interleaved
	vmulax.xyzw	ACC,$vf8,$vf1x		# ACC = vf8*vf1.x
	vmadday.xyzw	ACC,$vf9,$vf1y		# ACC = ACC + vf9*vf1.y
	vmaddaz.xyzw	ACC,$vf10,$vf1z		# ACC = ACC + vf10*vf1.z
	vmulw.x	$vf6,$vf6,$vf30w		# vf6.x = vf6.x*vf30.w
	vmaddw.xyzw	$vf1,$vf11,$vf0w	# vf1 = ACC + vf11
	addi	$7,$7,-1
	vmulx.xyz	$vf1,$vf1,$vf6x		# vf1.xyz = vf1*vf6.x
	sqc2	$vf1,0($12)
	bnez	$7,.Lvu0_weight_blend_first_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	vu0_weight_blend_first

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Transforms a strided point list by two matrices and clip-tests it.
 * Arguments: $4 = context (matrix +192, clip matrix +768), $5 = points (x, y, z), $6 = 64-byte output
 * entries, $7 = count, $8 = point stride in words. Output: +0 point * matrix +192, +16 point * clip
 * matrix, +60 clip flags. Returns the number of points with any clip flag set.
 */
/* 0x0017c084 */
	.set	noreorder
	.text
	.globl	vu0_points_transform_clip
	.ent	vu0_points_transform_clip
vu0_points_transform_clip:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	lqc2	$vf24,768($4)
	lqc2	$vf25,784($4)
	lqc2	$vf26,800($4)
	lqc2	$vf27,816($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
	movz	$2,$0,$0
.Lvu0_points_transform_clip_loop:
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
	addi	$7,$7,-1
	add	$5,$5,$12
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	sqc2	$vf4,16($6)
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	sqc2	$vf5,0($6)
	beqz	$8,.Lvu0_points_transform_clip_inside
	sw	$8,60($6)
	addi	$2,$2,1
.Lvu0_points_transform_clip_inside:
	bnez	$7,.Lvu0_points_transform_clip_loop
	addi	$6,$6,64
	jr	$31
	nop
	.end	vu0_points_transform_clip

/*
 * game VU0 helpers (object at 0x17ba94-0x17c084, after rez_wire_compile.c). Transforms a strided point list by a matrix into 64-byte entries; returns 0.
 * Arguments: $4 = context (matrix +192), $5 = points (x, y, z), $6 = 64-byte output entries,
 * $7 = count, $8 = point stride in words.
 */
/* 0x0017c124 */
	.set	noreorder
	.text
	.globl	vu0_points_transform
	.ent	vu0_points_transform
vu0_points_transform:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf11,240($4)
	# $12 = stride in bytes
	sll	$12,$8,0x2
.Lvu0_points_transform_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf5 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf5,$vf11,$vf0w
	add	$5,$5,$12
	addi	$7,$7,-1
	sqc2	$vf5,0($6)
	bnez	$7,.Lvu0_points_transform_loop
	addi	$6,$6,64
	jr	$31
	movz	$2,$0,$0
	.end	vu0_points_transform
