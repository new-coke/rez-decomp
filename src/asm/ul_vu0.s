/*
 * ul_vu0.s: hand-written assembly, retail (US v1.01) 0x001bfd24-0x001c1c24.
 * Registers are numeric (as 2.9-ee rejects $a0-style names): $2 = v0, $4-$7 = a0-a3,
 * $8-$15 = t0-t7, $24/$25 = t8/t9, $31 = ra.
 */

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Length of a 3-vector with the FPU accumulator: sqrt(x*x + y*y + z*z).
 * Argument: $4 = vector (3 floats). Returns the length in $f0.
 */
/* 0x001bfd24 */
	.set	noreorder
	.text
	.globl	vec3_length
	.ent	vec3_length
vec3_length:
	lwc1	$f4,0($4)
	lwc1	$f5,4($4)
	lwc1	$f6,8($4)
	mula.s	$f4,$f4				# ACC = x * x
	madda.s	$f5,$f5				# ACC += y * y
	madd.s	$f7,$f6,$f6			# $f7 = ACC + z * z
	jr	$31
	sqrt.s	$f0,$f7				# $f0 = sqrt($f7)
	.end	vec3_length

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Normalizes a 3-vector with the FPU: d = s / sqrt(x*x + y*y + z*z).
 * Arguments: $4 = destination, $5 = source (3 floats each; may be the same).
 */
/* 0x001bfd44 */
	.set	noreorder
	.set	noat
	.text
	.globl	vec3_normalize
	.ent	vec3_normalize
vec3_normalize:
	lwc1	$f4,0($5)
	lwc1	$f5,4($5)
	lwc1	$f6,8($5)
	# $f1 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f1
	mula.s	$f4,$f4				# ACC = x * x
	madda.s	$f5,$f5				# ACC += y * y
	madd.s	$f7,$f6,$f6			# $f7 = ACC + z * z
	rsqrt.s	$f0,$f1,$f7			# $f0 = 1 / sqrt($f7)
	mul.s	$f4,$f4,$f0
	mul.s	$f5,$f5,$f0
	mul.s	$f6,$f6,$f0
	swc1	$f4,0($4)
	swc1	$f5,4($4)
	jr	$31
	swc1	$f6,8($4)
	.end	vec3_normalize

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Length of a 3-vector on VU0 (unreferenced).
 * Argument: $4 = vector (qword). Returns sqrt(x*x + y*y + z*z) in $f0. No code in the executable calls it.
 */
/* 0x001bfd84 */
	.set	noreorder
	.text
	.globl	vu0_vec3_length
	.ent	vu0_vec3_length
vu0_vec3_length:
	lqc2	$vf4,0($4)
	vmul.xyz	$vf5,$vf4,$vf4		# vf5.xyz = vf4*vf4
	vaddy.x	$vf5,$vf5,$vf5y			# vf5.x = vf5.x + vf5.y
	vaddz.x	$vf5,$vf5,$vf5z			# vf5.x = vf5.x + vf5.z
	vsqrt	Q,$vf5x				# Q = sqrt(vf5.x)
	vwaitq					# wait for Q
	vaddq.x	$vf4,$vf0,Q			# vf4.x = Q
	qmfc2	$8,$vf4				# $8 = vf4 (128 bits)
	jr	$31
	mtc1	$8,$f0
	.end	vu0_vec3_length

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Component-wise maximum of two vectors: d = max(a, b).
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). Called from 0x1bc730.
 */
/* 0x001bfdac */
	.set	noreorder
	.text
	.globl	vu0_vec_max
	.ent	vu0_vec_max
vu0_vec_max:
	lqc2	$vf4,0($5)
	lqc2	$vf5,0($6)
	vmax.xyzw	$vf6,$vf4,$vf5		# vf6 = max(vf4, vf5)
	jr	$31
	sqc2	$vf6,0($4)
	.end	vu0_vec_max

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Component-wise minimum of two vectors: d = min(a, b).
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). Called from 0x1bc730.
 */
/* 0x001bfdc0 */
	.set	noreorder
	.text
	.globl	vu0_vec_min
	.ent	vu0_vec_min
vu0_vec_min:
	lqc2	$vf4,0($5)
	lqc2	$vf5,0($6)
	vmini.xyzw	$vf6,$vf4,$vf5		# vf6 = min(vf4, vf5)
	jr	$31
	sqc2	$vf6,0($4)
	.end	vu0_vec_min

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Homogeneous divide: d.xyz = s.xyz / s.w; returns 1 / s.w (unreferenced).
 * Arguments: $4 = destination, $5 = source (qwords). Returns 1 / s.w in $f0 (read back from the Q
 * register; the qmfc2 result is overwritten). No code in the executable calls it.
 */
/* 0x001bfdd4 */
	.set	noreorder
	.text
	.globl	vu0_vec_div_w
	.ent	vu0_vec_div_w
vu0_vec_div_w:
	lqc2	$vf4,0($5)
	vdiv	Q,$vf0w,$vf4w			# Q = 1 / vf4.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf4,$vf4,Q		# vf4.xyz = vf4*Q
	qmfc2	$8,$vf4				# $8 = vf4 (128 bits)
	sqc2	$vf4,0($4)
	cfc2	$8,$vi22			# $8 = Q register
	jr	$31
	mtc1	$8,$f0
	.end	vu0_vec_div_w

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Cross product of two 3-vectors: d.xyz = a x b (d.w unchanged from the VU register).
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). No code in the executable calls it.
 */
/* 0x001bfdf8 */
	.set	noreorder
	.text
	.globl	vu0_vec3_cross
	.ent	vu0_vec3_cross
vu0_vec3_cross:
	lqc2	$vf5,0($5)
	lqc2	$vf6,0($6)
	# vf4.xyz = vf5 x vf6 (cross product)
	vopmula.xyz	ACC,$vf5,$vf6
	vopmsub.xyz	$vf4,$vf6,$vf5
	jr	$31
	sqc2	$vf4,0($4)
	.end	vu0_vec3_cross

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Dot product of two 3-vectors: returns a.x*b.x + a.y*b.y + a.z*b.z (unreferenced).
 * Arguments: $4 = a, $5 = b (qwords). Returns the dot product in $f0. No code in the executable calls it.
 */
/* 0x001bfe10 */
	.set	noreorder
	.text
	.globl	vu0_vec3_dot
	.ent	vu0_vec3_dot
vu0_vec3_dot:
	lqc2	$vf4,0($4)
	lqc2	$vf5,0($5)
	vmul.xyz	$vf6,$vf4,$vf5		# vf6.xyz = vf4*vf5
	vaddy.x	$vf6,$vf6,$vf6y			# vf6.x = vf6.x + vf6.y
	vaddz.x	$vf6,$vf6,$vf6z			# vf6.x = vf6.x + vf6.z
	qmfc2	$8,$vf6				# $8 = vf6 (128 bits)
	jr	$31
	mtc1	$8,$f0
	.end	vu0_vec3_dot

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Normalizes a 3-vector on VU0; returns 1 / length.
 * Arguments: $4 = destination, $5 = source (qwords; w is copied). Returns 1 / sqrt(x*x + y*y + z*z)
 * in $f0. Called from gw_mouse_read_pad_move.
 */
/* 0x001bfe30 */
	.set	noreorder
	.text
	.globl	vu0_vec3_normalize
	.ent	vu0_vec3_normalize
vu0_vec3_normalize:
	lqc2	$vf4,0($5)
	vmul.xyz	$vf5,$vf4,$vf4		# vf5.xyz = vf4*vf4
	vaddy.x	$vf5,$vf5,$vf5y			# vf5.x = vf5.x + vf5.y
	vaddz.x	$vf5,$vf5,$vf5z			# vf5.x = vf5.x + vf5.z
	vrsqrt	Q,$vf0w,$vf5x			# Q = 1 / sqrt(vf5.x)
	vwaitq					# wait for Q
	vmulq.xyz	$vf4,$vf4,Q		# vf4.xyz = vf4*Q
	sqc2	$vf4,0($4)
	cfc2	$2,$vi22			# $2 = Q register
	jr	$31
	mtc1	$2,$f0
	.end	vu0_vec3_normalize

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Scales a vector: d = s * f.
 * Arguments: $4 = destination, $5 = source (qword), $f12 = factor. Called from 0x1c1f78 (perspective divide).
 */
/* 0x001bfe5c */
	.set	noreorder
	.text
	.globl	vu0_vec_scale
	.ent	vu0_vec_scale
vu0_vec_scale:
	mfc1	$8,$f12
	qmtc2	$8,$vf6				# vf6 = $8 (128 bits)
	lqc2	$vf4,0($5)
	vmulx.xyzw	$vf4,$vf4,$vf6x		# vf4 = vf4*vf6.x
	jr	$31
	sqc2	$vf4,0($4)
	.end	vu0_vec_scale

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clamps every component of a vector: d = min(max(s, lo), hi).
 * Arguments: $4 = destination, $5 = source (qword), $f12 = lo, $f13 = hi. Called from 0x1a7bf0, 0x1bb370
 * and 0x1c1f18.
 */
/* 0x001bfe74 */
	.set	noreorder
	.text
	.globl	vu0_vec_clamp
	.ent	vu0_vec_clamp
vu0_vec_clamp:
	mfc1	$8,$f12
	mfc1	$9,$f13
	lqc2	$vf6,0($5)
	qmtc2	$8,$vf4				# vf4 = $8 (128 bits)
	qmtc2	$9,$vf5				# vf5 = $9 (128 bits)
	vminix.xyzw	$vf6,$vf6,$vf5x		# vf6 = min(vf6, vf5.x)
	vmaxx.xyzw	$vf6,$vf6,$vf4x		# vf6 = max(vf6, vf4.x)
	jr	$31
	sqc2	$vf6,0($4)
	.end	vu0_vec_clamp

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Component-wise product of two vectors: d = a * b.
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). Called from 0x1bb570.
 */
/* 0x001bfe98 */
	.set	noreorder
	.text
	.globl	vu0_vec_mul
	.ent	vu0_vec_mul
vu0_vec_mul:
	lqc2	$vf5,0($5)
	lqc2	$vf6,0($6)
	vmul.xyzw	$vf4,$vf5,$vf6		# vf4 = vf5*vf6
	jr	$31
	sqc2	$vf4,0($4)
	.end	vu0_vec_mul

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Adds the xyz of two vectors: d.xyz = a.xyz + b.xyz (d.w = a.w) (unreferenced).
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). No code in the executable calls it.
 */
/* 0x001bfeac */
	.set	noreorder
	.text
	.globl	vu0_vec3_add
	.ent	vu0_vec3_add
vu0_vec3_add:
	lqc2	$vf5,0($5)
	lqc2	$vf6,0($6)
	vadd.xyz	$vf5,$vf5,$vf6		# vf5.xyz = vf5 + vf6
	jr	$31
	sqc2	$vf5,0($4)
	.end	vu0_vec3_add

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Adds two vectors: d = a + b.
 * Arguments: $4 = destination, $5 = a, $6 = b (qwords). Called from 0x1a7bf0 and 0x1bb370.
 */
/* 0x001bfec0 */
	.set	noreorder
	.text
	.globl	vu0_vec_add
	.ent	vu0_vec_add
vu0_vec_add:
	lqc2	$vf4,0($5)
	lqc2	$vf5,0($6)
	vadd.xyzw	$vf6,$vf4,$vf5		# vf6 = vf4 + vf5
	jr	$31
	sqc2	$vf6,0($4)
	.end	vu0_vec_add

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Copies one qword: *d = *s (through t0).
 * Arguments: $4 = destination, $5 = source. Called from 0x1a7bf0 and 0x1bb370.
 * Not in the asm target list: the inventory holds a C reconstruction (in the .c of this unit,
 * NON-MATCHING 33%) because gcc picks v0 for the temporary; the routine sits inside this asm object
 * (4-byte aligned at 0x...bc between two VU0 routines), so it is assembly.
 */
/* 0x001bfed4 */
	.set	noreorder
	.text
	.globl	vu0_qword_copy
	.ent	vu0_qword_copy
vu0_qword_copy:
	lq	$8,0($5)
	jr	$31
	sq	$8,0($4)
	.end	vu0_qword_copy

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Transforms a point by a matrix and divides by w: d = (M * (s.xyz, 1)).xyz / w (unreferenced).
 * Arguments: $4 = matrix (4 qword rows), $5 = destination, $6 = source point (qword). Note the order:
 * the destination is the second argument. No code in the executable calls it.
 */
/* 0x001bfee0 */
	.set	noreorder
	.text
	.globl	vu0_matrix_apply_project
	.ent	vu0_matrix_apply_project
vu0_matrix_apply_project:
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	lqc2	$vf8,0($6)
	# vf9 = vf4*vf8.x + vf5*vf8.y + vf6*vf8.z
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmadday.xyzw	ACC,$vf5,$vf8y
	vmaddz.xyzw	$vf9,$vf6,$vf8z
	vadd.xyzw	$vf9,$vf9,$vf7		# vf9 = vf9 + vf7
	vdiv	Q,$vf0w,$vf9w			# Q = 1 / vf9.w
	vwaitq					# wait for Q
	vmulq.xyz	$vf9,$vf9,Q		# vf9.xyz = vf9*Q
	jr	$31
	sqc2	$vf9,0($5)
	.end	vu0_matrix_apply_project

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Transforms a 3-float point by a matrix: d = M * (x, y, z, 1) (unreferenced).
 * Arguments: $4 = matrix (4 qword rows), $5 = point (3 floats, read word by word), $6 = destination
 * qword. No code in the executable calls it.
 */
/* 0x001bff18 */
	.set	noreorder
	.text
	.globl	vu0_matrix_apply_vec3
	.ent	vu0_matrix_apply_vec3
vu0_matrix_apply_vec3:
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	# x, y and z go through vf8.x one at a time
	lw	$8,0($5)
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	vmulax.xyzw	ACC,$vf4,$vf8x		# ACC = vf4*vf8.x
	lw	$8,4($5)
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	vmaddax.xyzw	ACC,$vf5,$vf8x		# ACC = ACC + vf5*vf8.x
	lw	$8,8($5)
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	vmaddax.xyzw	ACC,$vf6,$vf8x		# ACC = ACC + vf6*vf8.x
	vmaddw.xyzw	$vf9,$vf7,$vf0w		# vf9 = ACC + vf7
	jr	$31
	sqc2	$vf9,0($6)
	.end	vu0_matrix_apply_vec3

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Transforms a point by a matrix: d = M * (s.xyz, 1).
 * Arguments: $4 = matrix (4 qword rows), $5 = source point (qword), $6 = destination. Called from
 * 0x1a10f0, 0x1a36f8 and the ul_gx transform helpers 0x1c1f18, 0x1c1f78, 0x1c2288 and 0x1c2300.
 */
/* 0x001bff58 */
	.set	noreorder
	.text
	.globl	vu0_matrix_apply
	.ent	vu0_matrix_apply
vu0_matrix_apply:
	lqc2	$vf8,0($5)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	# vf9 = vf4*vf8.x + vf5*vf8.y + vf6*vf8.z + vf7
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmadday.xyzw	ACC,$vf5,$vf8y
	vmaddaz.xyzw	ACC,$vf6,$vf8z
	vmaddw.xyzw	$vf9,$vf7,$vf0w
	jr	$31
	sqc2	$vf9,0($6)
	.end	vu0_matrix_apply

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Normalizes the first three rows of a matrix (the rotation part): d[i].xyz = s[i].xyz / |s[i].xyz|.
 * Arguments: $4 = destination, $5 = source (4 qword rows; row 3 is neither read nor written).
 * Called from 0x1bd040 (ul_gx.c: "normalises the 3x3 part").
 */
/* 0x001bff84 */
	.set	noreorder
	.text
	.globl	vu0_matrix_normalize3
	.ent	vu0_matrix_normalize3
vu0_matrix_normalize3:
	lqc2	$vf4,0($5)
	lqc2	$vf5,16($5)
	lqc2	$vf6,32($5)
	# row 0
	vmul.xyz	$vf8,$vf4,$vf4		# vf8.xyz = vf4*vf4
	vaddy.x	$vf8,$vf8,$vf8y			# vf8.x = vf8.x + vf8.y
	vaddz.x	$vf8,$vf8,$vf8z			# vf8.x = vf8.x + vf8.z
	vrsqrt	Q,$vf0w,$vf8x			# Q = 1 / sqrt(vf8.x)
	vwaitq					# wait for Q
	vmulq.xyz	$vf4,$vf4,Q		# vf4.xyz = vf4*Q
	sqc2	$vf4,0($4)
	# row 1
	vmul.xyz	$vf8,$vf5,$vf5		# vf8.xyz = vf5*vf5
	vaddy.x	$vf8,$vf8,$vf8y			# vf8.x = vf8.x + vf8.y
	vaddz.x	$vf8,$vf8,$vf8z			# vf8.x = vf8.x + vf8.z
	vrsqrt	Q,$vf0w,$vf8x			# Q = 1 / sqrt(vf8.x)
	vwaitq					# wait for Q
	vmulq.xyz	$vf5,$vf5,Q		# vf5.xyz = vf5*Q
	sqc2	$vf5,16($4)
	# row 2
	vmul.xyz	$vf8,$vf6,$vf6		# vf8.xyz = vf6*vf6
	vaddy.x	$vf8,$vf8,$vf8y			# vf8.x = vf8.x + vf8.y
	vaddz.x	$vf8,$vf8,$vf8z			# vf8.x = vf8.x + vf8.z
	vrsqrt	Q,$vf0w,$vf8x			# Q = 1 / sqrt(vf8.x)
	vwaitq					# wait for Q
	vmulq.xyz	$vf6,$vf6,Q		# vf6.xyz = vf6*Q
	jr	$31
	sqc2	$vf6,32($4)
	.end	vu0_matrix_normalize3

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Inverts a rigid-body matrix: transposes the 3x3 rotation (MMI) and sets translation[i] = -dot(t, rotation row i).
 * Arguments: $4 = destination, $5 = source (4 qword rows: rotation rows 0-2, translation row 3).
 * Called from 0x1a1810 (Ninja, extern void vu0_matrix_inverse_rigid(NjMatrix *d, NjMatrix *s)).
 */
/* 0x001bffe8 */
	.set	noreorder
	.text
	.globl	vu0_matrix_inverse_rigid
	.ent	vu0_matrix_inverse_rigid
vu0_matrix_inverse_rigid:
	lq	$8,0($5)
	lq	$9,16($5)
	lq	$10,32($5)
	# vf5 = translation t; vf4 = vf9 = (0, 0, 0, t.w)
	lqc2	$vf4,48($5)
	vmove.xyzw	$vf5,$vf4		# vf5 = vf4
	vsub.xyz	$vf4,$vf4,$vf4		# vf4.xyz = 0
	vmove.xyzw	$vf9,$vf4		# vf9 = vf4
	# transpose rows 0-2 (row 3 = (0, 0, 0, t.w)) with MMI word shuffles
	qmfc2	$11,$vf4			# $11 = vf4 (128 bits)
	pextlw	$12,$9,$8
	pextuw	$13,$9,$8
	pextlw	$14,$11,$10
	pextuw	$15,$11,$10
	pcpyld	$8,$14,$12
	pcpyud	$9,$12,$14
	pcpyld	$10,$15,$13
	qmtc2	$8,$vf6				# vf6 = $8 (128 bits)
	qmtc2	$9,$vf7				# vf7 = $9 (128 bits)
	qmtc2	$10,$vf8			# vf8 = $10 (128 bits)
	# vf4.xyz = -(t.x * column 0 + t.y * column 1 + t.z * column 2): translation[i] = -dot(t, row i)
	# vf4.xyz = vf6*vf5.x + vf7*vf5.y + vf8*vf5.z
	vmulax.xyz	ACC,$vf6,$vf5x
	vmadday.xyz	ACC,$vf7,$vf5y
	vmaddz.xyz	$vf4,$vf8,$vf5z
	vsub.xyz	$vf4,$vf9,$vf4		# vf4.xyz = vf9 - vf4
	sq	$8,0($4)
	sq	$9,16($4)
	sq	$10,32($4)
	jr	$31
	sqc2	$vf4,48($4)
	.end	vu0_matrix_inverse_rigid

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 0 and 1 of a matrix (about z) by an angle given as sine and cosine.
 * Arguments: $4 = matrix (in place), $f12 = sin, $f13 = cos. row0' = row0 * c + row1 * s,
 * row1' = row1 * c - row0 * s. Tail-called by 0x1b4e40 (matrix module) with two trig helpers' results.
 */
/* 0x001c0054 */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_z_sc
	.ent	vu0_matrix_rotate_z_sc
vu0_matrix_rotate_z_sc:
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	# vf8.x = sin, vf9.x = cos
	mfc1	$8,$f12
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	mfc1	$9,$f13
	qmtc2	$9,$vf9				# vf9 = $9 (128 bits)
	# vf6 = vf4*vf9.x + vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmaddx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf5*vf9.x - vf4*vf8.x
	vmulax.xyzw	ACC,$vf5,$vf9x
	vmsubx.xyzw	$vf7,$vf4,$vf8x
	sqc2	$vf6,0($4)
	jr	$31
	sqc2	$vf7,16($4)
	.end	vu0_matrix_rotate_z_sc

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 0 and 2 of a matrix (about y) by an angle given as sine and cosine.
 * Arguments: $4 = matrix (in place), $f12 = sin, $f13 = cos. row0' = row0 * c - row2 * s,
 * row2' = row0 * s + row2 * c. Tail-called by 0x1b4e98 (matrix module).
 */
/* 0x001c0088 */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_y_sc
	.ent	vu0_matrix_rotate_y_sc
vu0_matrix_rotate_y_sc:
	lqc2	$vf4,0($4)
	lqc2	$vf5,32($4)
	# vf8.x = sin, vf9.x = cos
	mfc1	$8,$f12
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	mfc1	$9,$f13
	qmtc2	$9,$vf9				# vf9 = $9 (128 bits)
	# vf6 = vf4*vf9.x - vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmsubx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf4*vf8.x + vf5*vf9.x
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmaddx.xyzw	$vf7,$vf5,$vf9x
	sqc2	$vf6,0($4)
	jr	$31
	sqc2	$vf7,32($4)
	.end	vu0_matrix_rotate_y_sc

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 1 and 2 of a matrix (about x) by an angle given as sine and cosine.
 * Arguments: $4 = matrix (in place), $f12 = sin, $f13 = cos. row1' = row1 * c + row2 * s,
 * row2' = row2 * c - row1 * s. Tail-called by 0x1b4ef0 (matrix module).
 */
/* 0x001c00bc */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_x_sc
	.ent	vu0_matrix_rotate_x_sc
vu0_matrix_rotate_x_sc:
	lqc2	$vf4,16($4)
	lqc2	$vf5,32($4)
	# vf8.x = sin, vf9.x = cos
	mfc1	$8,$f12
	qmtc2	$8,$vf8				# vf8 = $8 (128 bits)
	mfc1	$9,$f13
	qmtc2	$9,$vf9				# vf9 = $9 (128 bits)
	# vf6 = vf4*vf9.x + vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmaddx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf5*vf9.x - vf4*vf8.x
	vmulax.xyzw	ACC,$vf5,$vf9x
	vmsubx.xyzw	$vf7,$vf4,$vf8x
	sqc2	$vf6,16($4)
	jr	$31
	sqc2	$vf7,32($4)
	.end	vu0_matrix_rotate_x_sc

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 0 and 1 of a matrix (about z) by a 16-bit angle, using the sine table ulSinTable.
 * Arguments: $4 = matrix (in place), $5 = angle (0x10000 = full turn). Called by enemy_task_rotate_yxz,
 * continue_rotate_yxz, camera_free_fly and others (after 0x1bfb30 and 0x1bfb88: y, x, then z).
 */
/* 0x001c00f0 */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_z
	.ent	vu0_matrix_rotate_z
vu0_matrix_rotate_z:
	# $5 = &sin[a & 0xffff], $6 = &sin[(a + 0x4000) & 0xffff] (the cosine)
	lui	$8,%hi(ulSinTable)
	addiu	$8,$8,%lo(ulSinTable)
	addiu	$6,$5,16384
	andi	$5,$5,0xffff
	andi	$6,$6,0xffff
	sll	$5,$5,0x2
	sll	$6,$6,0x2
	add	$5,$5,$8
	add	$6,$6,$8
	lw	$5,0($5)
	lw	$6,0($6)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	# vf8.x = sin, vf9.x = cos
	qmtc2	$5,$vf8				# vf8 = $5 (128 bits)
	qmtc2	$6,$vf9				# vf9 = $6 (128 bits)
	# vf6 = vf4*vf9.x + vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmaddx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf5*vf9.x - vf4*vf8.x
	vmulax.xyzw	ACC,$vf5,$vf9x
	vmsubx.xyzw	$vf7,$vf4,$vf8x
	sqc2	$vf6,0($4)
	jr	$31
	sqc2	$vf7,16($4)
	.end	vu0_matrix_rotate_z

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 0 and 2 of a matrix (about y) by a 16-bit angle, using the sine table ulSinTable.
 * Arguments: $4 = matrix (in place), $5 = angle (0x10000 = full turn). Called by enemy_task_rotate_yxz,
 * continue_rotate_yxz, enemy_pass_get_matrix_angles, item_draw_model and others.
 */
/* 0x001c0148 */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_y
	.ent	vu0_matrix_rotate_y
vu0_matrix_rotate_y:
	# $5 = &sin[a & 0xffff], $6 = &sin[(a + 0x4000) & 0xffff] (the cosine)
	lui	$8,%hi(ulSinTable)
	addiu	$8,$8,%lo(ulSinTable)
	addiu	$6,$5,16384
	andi	$5,$5,0xffff
	andi	$6,$6,0xffff
	sll	$5,$5,0x2
	sll	$6,$6,0x2
	add	$5,$5,$8
	add	$6,$6,$8
	lw	$5,0($5)
	lw	$6,0($6)
	lqc2	$vf4,0($4)
	lqc2	$vf5,32($4)
	# vf8.x = sin, vf9.x = cos
	qmtc2	$5,$vf8				# vf8 = $5 (128 bits)
	qmtc2	$6,$vf9				# vf9 = $6 (128 bits)
	# vf6 = vf4*vf9.x - vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmsubx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf4*vf8.x + vf5*vf9.x
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmaddx.xyzw	$vf7,$vf5,$vf9x
	sqc2	$vf6,0($4)
	jr	$31
	sqc2	$vf7,32($4)
	.end	vu0_matrix_rotate_y

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Rotates rows 1 and 2 of a matrix (about x) by a 16-bit angle, using the sine table ulSinTable.
 * Arguments: $4 = matrix (in place), $5 = angle (0x10000 = full turn). Called by enemy_task_rotate_yxz,
 * continue_rotate_yxz, enemy_pass_get_matrix_angles and others.
 */
/* 0x001c01a0 */
	.set	noreorder
	.text
	.globl	vu0_matrix_rotate_x
	.ent	vu0_matrix_rotate_x
vu0_matrix_rotate_x:
	# $5 = &sin[a & 0xffff], $6 = &sin[(a + 0x4000) & 0xffff] (the cosine)
	lui	$8,%hi(ulSinTable)
	addiu	$8,$8,%lo(ulSinTable)
	addiu	$6,$5,16384
	andi	$5,$5,0xffff
	andi	$6,$6,0xffff
	sll	$5,$5,0x2
	sll	$6,$6,0x2
	add	$5,$5,$8
	add	$6,$6,$8
	lw	$5,0($5)
	lw	$6,0($6)
	lqc2	$vf4,16($4)
	lqc2	$vf5,32($4)
	# vf8.x = sin, vf9.x = cos
	qmtc2	$5,$vf8				# vf8 = $5 (128 bits)
	qmtc2	$6,$vf9				# vf9 = $6 (128 bits)
	# vf6 = vf4*vf9.x + vf5*vf8.x
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmaddx.xyzw	$vf6,$vf5,$vf8x
	# vf7 = vf5*vf9.x - vf4*vf8.x
	vmulax.xyzw	ACC,$vf5,$vf9x
	vmsubx.xyzw	$vf7,$vf4,$vf8x
	sqc2	$vf6,16($4)
	jr	$31
	sqc2	$vf7,32($4)
	.end	vu0_matrix_rotate_x

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Scales rows 0-2 of a matrix by x, y and z.
 * Arguments: $4 = matrix (in place), $f12 = x, $f13 = y, $f14 = z. Called by enemy_task_draw_fade,
 * char_task_disp, fade_shape_draw and others.
 */
/* 0x001c01f8 */
	.set	noreorder
	.text
	.globl	vu0_matrix_scale
	.ent	vu0_matrix_scale
vu0_matrix_scale:
	mfc1	$8,$f12
	mfc1	$9,$f13
	mfc1	$10,$f14
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	vmulx.xyzw	$vf4,$vf4,$vf1x		# vf4 = vf4*vf1.x
	vmuly.xyzw	$vf5,$vf5,$vf1y		# vf5 = vf5*vf1.y
	vmulz.xyzw	$vf6,$vf6,$vf1z		# vf6 = vf6*vf1.z
	sqc2	$vf4,0($4)
	sqc2	$vf5,16($4)
	jr	$31
	sqc2	$vf6,32($4)
	.end	vu0_matrix_scale

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Translates a matrix in its own frame by a qword vector: row3 = M * (v.xyz, 1) (unreferenced).
 * Arguments: $4 = matrix (in place), $5 = vector (qword). No code in the executable calls it.
 */
/* 0x001c0238 */
	.set	noreorder
	.text
	.globl	vu0_matrix_translate_qv
	.ent	vu0_matrix_translate_qv
vu0_matrix_translate_qv:
	lqc2	$vf1,0($5)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	# vf8 = vf4*vf1.x + vf5*vf1.y + vf6*vf1.z + vf7
	vmulax.xyzw	ACC,$vf4,$vf1x
	vmadday.xyzw	ACC,$vf5,$vf1y
	vmaddaz.xyzw	ACC,$vf6,$vf1z
	vmaddw.xyzw	$vf8,$vf7,$vf0w
	jr	$31
	sqc2	$vf8,48($4)
	.end	vu0_matrix_translate_qv

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Translates a matrix in its own frame by a 3-float vector: row3 = M * (x, y, z, 1) (unreferenced).
 * Arguments: $4 = matrix (in place), $5 = vector (3 floats). No code in the executable calls it.
 */
/* 0x001c0264 */
	.set	noreorder
	.text
	.globl	vu0_matrix_translate_v
	.ent	vu0_matrix_translate_v
vu0_matrix_translate_v:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	# vf8 = vf4*vf1.x + vf5*vf1.y + vf6*vf1.z + vf7
	vmulax.xyzw	ACC,$vf4,$vf1x
	vmadday.xyzw	ACC,$vf5,$vf1y
	vmaddaz.xyzw	ACC,$vf6,$vf1z
	vmaddw.xyzw	$vf8,$vf7,$vf0w
	jr	$31
	sqc2	$vf8,48($4)
	.end	vu0_matrix_translate_v

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Translates a matrix in its own frame: row3 = M * (x, y, z, 1).
 * Arguments: $4 = matrix (in place), $f12 = x, $f13 = y, $f14 = z. Called by camera_free_fly,
 * enemy_task_draw_fade, obj_draw_at, fade_shape_draw, flow_editor_draw_gizmo and others.
 */
/* 0x001c02a4 */
	.set	noreorder
	.text
	.globl	vu0_matrix_translate
	.ent	vu0_matrix_translate
vu0_matrix_translate:
	mfc1	$8,$f12
	mfc1	$9,$f13
	mfc1	$10,$f14
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	lqc2	$vf4,0($4)
	lqc2	$vf5,16($4)
	lqc2	$vf6,32($4)
	lqc2	$vf7,48($4)
	# vf8 = vf4*vf1.x + vf5*vf1.y + vf6*vf1.z + vf7
	vmulax.xyzw	ACC,$vf4,$vf1x
	vmadday.xyzw	ACC,$vf5,$vf1y
	vmaddaz.xyzw	ACC,$vf6,$vf1z
	vmaddw.xyzw	$vf8,$vf7,$vf0w
	jr	$31
	sqc2	$vf8,48($4)
	.end	vu0_matrix_translate

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Multiplies two matrices: each row of B is transformed by A (d = rows of B times A).
 * Arguments: $4 = destination, $5 = A, $6 = B (4 qword rows each; d may alias B). Called by
 * camera_update, enemy_pass_draw_tree, player_wire_break_start, ul_gx 0x1bd040 ("4x4 multiply") and others.
 */
/* 0x001c02e4 */
	.set	noreorder
	.text
	.globl	vu0_matrix_mul
	.ent	vu0_matrix_mul
vu0_matrix_mul:
	lqc2	$vf4,0($5)
	lqc2	$vf8,0($6)
	lqc2	$vf5,16($5)
	lqc2	$vf6,32($5)
	lqc2	$vf7,48($5)
	# vf8 = vf4*vf8.x + vf5*vf8.y + vf6*vf8.z + vf7*vf8.w
	vmulax.xyzw	ACC,$vf4,$vf8x
	vmadday.xyzw	ACC,$vf5,$vf8y
	vmaddaz.xyzw	ACC,$vf6,$vf8z
	vmaddw.xyzw	$vf8,$vf7,$vf8w
	lqc2	$vf9,16($6)
	lqc2	$vf10,32($6)
	lqc2	$vf11,48($6)
	# vf9 = vf4*vf9.x + vf5*vf9.y + vf6*vf9.z + vf7*vf9.w
	vmulax.xyzw	ACC,$vf4,$vf9x
	vmadday.xyzw	ACC,$vf5,$vf9y
	vmaddaz.xyzw	ACC,$vf6,$vf9z
	vmaddw.xyzw	$vf9,$vf7,$vf9w
	sqc2	$vf8,0($4)
	# vf10 = vf4*vf10.x + vf5*vf10.y + vf6*vf10.z + vf7*vf10.w
	vmulax.xyzw	ACC,$vf4,$vf10x
	vmadday.xyzw	ACC,$vf5,$vf10y
	vmaddaz.xyzw	ACC,$vf6,$vf10z
	vmaddw.xyzw	$vf10,$vf7,$vf10w
	sqc2	$vf9,16($4)
	# vf11 = vf4*vf11.x + vf5*vf11.y + vf6*vf11.z + vf7*vf11.w
	vmulax.xyzw	ACC,$vf4,$vf11x
	vmadday.xyzw	ACC,$vf5,$vf11y
	vmaddaz.xyzw	ACC,$vf6,$vf11z
	vmaddw.xyzw	$vf11,$vf7,$vf11w
	sqc2	$vf10,32($4)
	jr	$31
	sqc2	$vf11,48($4)
	.end	vu0_matrix_mul

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Transposes a 4x4 matrix with MMI word shuffles (unreferenced).
 * Arguments: $4 = destination, $5 = source (4 qword rows). No code in the executable calls it.
 */
/* 0x001c0358 */
	.set	noreorder
	.text
	.globl	vu0_matrix_transpose
	.ent	vu0_matrix_transpose
vu0_matrix_transpose:
	lq	$8,0($5)
	lq	$9,16($5)
	lq	$10,32($5)
	lq	$11,48($5)
	pextlw	$12,$9,$8
	pextuw	$13,$9,$8
	pextlw	$14,$11,$10
	pextuw	$15,$11,$10
	pcpyld	$8,$14,$12
	pcpyud	$9,$12,$14
	pcpyld	$10,$15,$13
	pcpyud	$11,$13,$15
	sq	$8,0($4)
	sq	$9,16($4)
	sq	$10,32($4)
	jr	$31
	sq	$11,48($4)
	.end	vu0_matrix_transpose

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Sets a matrix to the identity, built from vf0 = (0, 0, 0, 1) with MMI shifts.
 * Argument: $4 = matrix. Called by the matrix stack init 0x1b4d58 ("set to identity by vu0_matrix_identity")
 * and others.
 */
/* 0x001c039c */
	.set	noreorder
	.text
	.globl	vu0_matrix_identity
	.ent	vu0_matrix_identity
vu0_matrix_identity:
	# $8 = (0, 0, 0, 1)
	qmfc2	$8,$vf0				# $8 = vf0 (128 bits)
	pcpyud	$10,$8,$0			# $10 = (0, 1, 0, 0)
	pextlw	$11,$0,$10			# $11 = (0, 0, 1, 0)
	pcpyud	$9,$11,$0			# $9 = (1, 0, 0, 0)
	sq	$8,48($4)
	sq	$10,16($4)
	sq	$11,32($4)
	jr	$31
	sq	$9,0($4)
	.end	vu0_matrix_identity

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Copies a 4x4 matrix with quadword loads and stores.
 * Arguments: $4 = destination, $5 = source. Called by model_camera_angles, the matrix module 0x1b4c30
 * ("copy via vu0_matrix_copy(dst, src)"), ul_gx 0x1bcf80 and others.
 */
/* 0x001c03c0 */
	.set	noreorder
	.text
	.globl	vu0_matrix_copy
	.ent	vu0_matrix_copy
vu0_matrix_copy:
	lq	$8,0($5)
	lq	$9,16($5)
	lq	$10,32($5)
	lq	$11,48($5)
	sq	$8,0($4)
	sq	$9,16($4)
	sq	$10,32($4)
	jr	$31
	sq	$11,48($4)
	.end	vu0_matrix_copy

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clears a 4x4 matrix to zero.
 * Argument: $4 = matrix. vf4 = vf0 - vf0 = 0 is stored to all four rows. (The C comment of the caller
 * 0x1bd940 calls the result an identity; the code writes zeros.)
 */
/* 0x001c03e4 */
	.set	noreorder
	.text
	.globl	vu0_matrix_zero
	.ent	vu0_matrix_zero
vu0_matrix_zero:
	vsub.xyzw	$vf4,$vf0,$vf0		# vf4 = 0
	sqc2	$vf4,0($4)
	sqc2	$vf4,16($4)
	sqc2	$vf4,32($4)
	jr	$31
	sqc2	$vf4,48($4)
	.end	vu0_matrix_zero

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clip codes of a point list: transforms n points by a matrix and returns their combined clip codes (unreferenced).
 * Arguments: $4 = matrix, $5 = points (qwords; one qword past the end is read), $6 = count.
 * Returns ((OR of the codes) << 8) | (AND of the codes). No code in the executable calls it.
 */
/* 0x001c03fc */
	.set	noreorder
	.text
	.globl	vu0_points_clip_codes
	.ent	vu0_points_clip_codes
vu0_points_clip_codes:
	lqc2	$vf24,0($4)
	lqc2	$vf25,16($4)
	lqc2	$vf26,32($4)
	lqc2	$vf27,48($4)
	# $2 = OR of the codes, $3 = AND of the codes (starts all ones)
	movz	$2,$0,$0
	addi	$3,$0,63
	lqc2	$vf1,0($5)
.Lvu0_points_clip_codes_loop:
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	addi	$5,$5,16
	addi	$6,$6,-1
	lqc2	$vf1,0($5)
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	or	$2,$2,$8
	bnez	$6,.Lvu0_points_clip_codes_loop
	and	$3,$3,$8
	# return (or << 8) | and
	sll	$2,$2,0x8
	jr	$31
	or	$2,$2,$3
	.end	vu0_points_clip_codes

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Visibility test of a point list: returns 1 as soon as one point is outside the clip volume, else 0.
 * Arguments: $4 = matrix, $5 = points (qwords; one qword past the end is read), $6 = count. Called by
 * 0x179e18 and 0x179ed8 (8 box corners) and 0x1a3390.
 */
/* 0x001c046c */
	.set	noreorder
	.text
	.globl	vu0_points_any_clipped
	.ent	vu0_points_any_clipped
vu0_points_any_clipped:
	lqc2	$vf24,0($4)
	lqc2	$vf25,16($4)
	lqc2	$vf26,32($4)
	lqc2	$vf27,48($4)
	lqc2	$vf1,0($5)
.Lvu0_points_any_clipped_loop:
	# vf4 = vf24*vf1.x + vf25*vf1.y + vf26*vf1.z + vf27
	vmulax.xyzw	ACC,$vf24,$vf1x
	vmadday.xyzw	ACC,$vf25,$vf1y
	vmaddaz.xyzw	ACC,$vf26,$vf1z
	vmaddw.xyzw	$vf4,$vf27,$vf0w
	addi	$5,$5,16
	addi	$6,$6,-1
	lqc2	$vf1,0($5)
	vclipw.xyz	$vf4,$vf4w		# clip judgement of vf4.xyz against +-vf4.w
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2	$8,$vi18			# $8 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$8,$8,0x3f
	bnez	$8,.Lvu0_points_any_clipped_clipped
	nop
	bnez	$6,.Lvu0_points_any_clipped_loop
	nop
	jr	$31
	movz	$2,$0,$0
.Lvu0_points_any_clipped_clipped:
	jr	$31
	addi	$2,$0,1
	.end	vu0_points_any_clipped

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Reciprocal square root: returns 1 / sqrt(x) (rsqrt.s).
 * Argument: $f12 = x. Returns the result in $f0. Called by vec3_normalize_into, quat_nlerp and others.
 */
/* 0x001c04dc */
	.set	noreorder
	.set	noat
	.text
	.globl	rsqrtf
	.ent	rsqrtf
rsqrtf:
	# $f4 = 1.0
	lui	$1,0x3f80
	mtc1	$1,$f4
	jr	$31
	rsqrt.s	$f0,$f4,$f12
	.end	rsqrtf

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: lights and projects n vertices with normals, with an environment map (no clip test).
 * Arguments: $4 = ul_gx context (view matrix +192, screen matrix +320, light directions +384, light
 * colours +448, map scale/offset +1264/+1280), $5 = vertices (position, normal: 24 bytes), $6 = 64-byte
 * output entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed point), +32 lit colour (w = 1),
 * +48 (s, t, Q, -) with (s, t) = view-space normal.xy * scale + offset. Returns 0. Called from 0x1bc5d8
 * and 0x1c2598.
 */
/* 0x001c04ec */
	.set	noreorder
	.text
	.globl	vu0_gx_project_lit_env
	.ent	vu0_gx_project_lit_env
vu0_gx_project_lit_env:
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
.Lvu0_gx_project_lit_env_loop:
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
	# (s, t) = view-space normal.xy * scale + offset
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vadd.xy	$vf26,$vf26,$vf29		# vf26.xy = vf26 + vf29
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
	bnez	$7,.Lvu0_gx_project_lit_env_loop
	addi	$5,$5,24
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_project_lit_env

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: projects n vertices with packed colours and an environment map (no clip test).
 * Arguments: $4 = ul_gx context (view matrix +192, screen matrix +320, colour scale +1248.x, map
 * scale/offset +1264/+1280), $5 = vertices (position, normal, packed BGRA: 28 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed point), +32 colour (w = 1), +48 (s, t, Q, -).
 * Returns 0. Called from 0x1bc5d8.
 */
/* 0x001c05e0 */
	.set	noreorder
	.text
	.globl	vu0_gx_project_col_env
	.ent	vu0_gx_project_col_env
vu0_gx_project_col_env:
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
.Lvu0_gx_project_col_env_loop:
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
	# (s, t) = view-space normal.xy * scale + offset
	# vf26 = vf8*vf2.x + vf9*vf2.y + vf10*vf2.z
	vmulax.xyzw	ACC,$vf8,$vf2x
	vmadday.xyzw	ACC,$vf9,$vf2y
	vmaddz.xyzw	$vf26,$vf10,$vf2z
	vmul.xy	$vf26,$vf26,$vf28		# vf26.xy = vf26*vf28
	vadd.xy	$vf26,$vf26,$vf29		# vf26.xy = vf26 + vf29
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
	bnez	$7,.Lvu0_gx_project_col_env_loop
	addi	$5,$5,28
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_project_col_env

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: projects n vertices with packed colours (no clip test).
 * Arguments: $4 = ul_gx context (screen matrix +320, colour scale +1248.x), $5 = vertices (x, y, z, packed
 * BGRA: 16 bytes), $6 = 64-byte output entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed point),
 * +32 colour (w = 1), +48 z = Q. Returns 0. Same code as the Ninja routine 0x1af0fc. Called from 0x1bc5d8.
 */
/* 0x001c06ac */
	.set	noreorder
	.text
	.globl	vu0_gx_project_col
	.ent	vu0_gx_project_col
vu0_gx_project_col:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
	lqc2	$vf23,1248($4)
.Lvu0_gx_project_col_loop:
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
	bnez	$7,.Lvu0_gx_project_col_loop
	addi	$5,$5,16
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_project_col

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: projects n bare positions with white colour (no clip test).
 * Arguments: $4 = ul_gx context (screen matrix +320), $5 = positions (12 bytes), $6 = 64-byte output
 * entries, $7 = count. Entry: +16 screen XYZ (12.4 fixed point), +32 (1, 1, 1, 1), +48 z = Q. Returns 0.
 * Same code as the Ninja routine 0x1af338. Called from 0x1bc5d8.
 */
/* 0x001c0748 */
	.set	noreorder
	.text
	.globl	vu0_gx_project_pos
	.ent	vu0_gx_project_pos
vu0_gx_project_pos:
	lqc2	$vf8,192($4)
	lqc2	$vf9,208($4)
	lqc2	$vf10,224($4)
	lqc2	$vf12,320($4)
	lqc2	$vf13,336($4)
	lqc2	$vf14,352($4)
	lqc2	$vf15,368($4)
.Lvu0_gx_project_pos_loop:
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
	bnez	$7,.Lvu0_gx_project_pos_loop
	addi	$5,$5,12
	jr	$31
	movz	$2,$0,$0
	.end	vu0_gx_project_pos

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: transforms, lights and projects n vertices with an environment map and a guard-band test.
 * Arguments: $4 = ul_gx context (view matrix +192, projection +128, light directions +384, light colours
 * +448, map scale/offset +1264/+1280, guard band +1168/+1184), $5 = vertices (position, normal: 24 bytes),
 * $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16 screen XYZ (12.4 fixed
 * point), +32 lit colour (w = 1), +48 (s, t, Q, clip flags). Returns the number of vertices with any
 * clip flag set. Called from the display-list interpreter 0x1bc1d8 and from 0x1c2560.
 */
/* 0x001c07cc */
	.set	noreorder
	.text
	.globl	vu0_gx_project_lit_env_clip
	.ent	vu0_gx_project_lit_env_clip
vu0_gx_project_lit_env_clip:
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
	lqc2	$vf28,1264($4)
	lqc2	$vf29,1280($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	movz	$2,$0,$0
.Lvu0_gx_project_lit_env_clip_loop:
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
	# (s, t) = view-space normal.xy * scale + offset
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
	beqz	$8,.Lvu0_gx_project_lit_env_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_gx_project_lit_env_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_project_lit_env_clip_loop
	addi	$5,$5,24
	jr	$31
	nop
	.end	vu0_gx_project_lit_env_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: transforms and projects n coloured vertices with an environment map and a guard-band test.
 * Same code as the Ninja routine 0x1ae9e4: $4 = ul_gx context, $5 = vertices (position, normal, packed
 * BGRA: 28 bytes), $6 = 64-byte output entries, $7 = count; returns the clipped count. Called from the
 * display-list interpreter 0x1bc1d8.
 */
/* 0x001c0910 */
	.set	noreorder
	.text
	.globl	vu0_gx_project_col_env_clip
	.ent	vu0_gx_project_col_env_clip
vu0_gx_project_col_env_clip:
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
.Lvu0_gx_project_col_env_clip_loop:
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
	# (s, t) = view-space normal.xy * scale + offset
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
	beqz	$8,.Lvu0_gx_project_col_env_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_gx_project_col_env_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_project_col_env_clip_loop
	addi	$5,$5,28
	jr	$31
	nop
	.end	vu0_gx_project_col_env_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: transforms and projects n vertices with packed colours and a guard-band test.
 * Same code as the Ninja routine 0x1aeb00: $4 = ul_gx context, $5 = vertices (x, y, z, packed BGRA:
 * 16 bytes), $6 = 64-byte output entries, $7 = count; returns the clipped count. Called from the
 * display-list interpreter 0x1bc1d8.
 */
/* 0x001c0a2c */
	.set	noreorder
	.text
	.globl	vu0_gx_project_col_clip
	.ent	vu0_gx_project_col_clip
vu0_gx_project_col_clip:
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
.Lvu0_gx_project_col_clip_loop:
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
	beqz	$8,.Lvu0_gx_project_col_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_gx_project_col_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_project_col_clip_loop
	addi	$5,$5,16
	jr	$31
	nop
	.end	vu0_gx_project_col_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). ul_gx converter: transforms and projects n bare positions (white colour) with a guard-band test.
 * Arguments: $4 = ul_gx context (view matrix +192, projection +128, guard band +1168/+1184), $5 = positions
 * (12 bytes), $6 = 64-byte output entries, $7 = count. Entry: +0 view-space position, +16 screen XYZ
 * (12.4 fixed point), +32 (1, 1, 1, 1), +48 (-, -, Q, clip flags). Returns the clipped count. (vf23,
 * vf28 and vf29 are loaded but not used.) Called from the display-list interpreter 0x1bc1d8.
 */
/* 0x001c0b30 */
	.set	noreorder
	.text
	.globl	vu0_gx_project_pos_clip
	.ent	vu0_gx_project_pos_clip
vu0_gx_project_pos_clip:
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
.Lvu0_gx_project_pos_clip_loop:
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
	beqz	$8,.Lvu0_gx_project_pos_clip_inside
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	addi	$2,$2,1
.Lvu0_gx_project_pos_clip_inside:
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	vaddq.z	$vf26,$vf0,Q			# vf26.z = Q
	sqc2	$vf24,16($6)
	sqc2	$vf25,32($6)
	sqc2	$vf26,48($6)
	addi	$6,$6,64
	addi	$7,$7,-1
	bnez	$7,.Lvu0_gx_project_pos_clip_loop
	addi	$5,$5,12
	jr	$31
	nop
	.end	vu0_gx_project_pos_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Strip emitter with back-face marking: GS RGBAQ/XYZ pairs for an index list, setting the XYZ ADC bit on back-facing triangles.
 * Arguments: $4 = ul_gx context (colour multiplier +976, offset +992, scales +1232), $5 = vertex indices
 * (shorts), $6 = count (negative = start with the opposite winding), $7 = vertex buffer (64-byte
 * entries, 12.4 screen XYZ at +16, colour at +32), $8 = output (32 bytes per vertex). The sign of the
 * 2D cross product of the last two edges, XORed with the alternating strip parity, becomes bit 15 of
 * XYZ word 3 (0x8000: ADC, no drawing kick). Returns the output end pointer. Called from 0x1bbd18.
 */
/* 0x001c0c1c */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_strip_cull
	.ent	vu0_gx_emit_strip_cull
vu0_gx_emit_strip_cull:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	# $10 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$10,$6,$0
	sll	$10,$10,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_strip_cull_loop:
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
	cfc2	$11,$vi17			# $11 = MAC flags (bit 5 = sign of the last result z)
	# back-facing (sign of z, flipped every vertex) -> 0x8000 in XYZ word 3
	andi	$11,$11,0x20
	xor	$11,$11,$10
	sll	$11,$11,0xa
	sw	$11,28($8)
	xori	$10,$10,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf25,0($8)
	bne	$0,$6,.Lvu0_gx_emit_strip_cull_loop
	addi	$8,$8,32
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_strip_cull

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Loads the strip emitters' VU0 constants (vf8 = +976, vf9 = +992, vf10 = +960, vf11 = +1232) and returns a word of the attached block.
 * Argument: $4 = strip context. Returns *(*(int *)($4 + 1536) + 20). Called before the emitters by
 * 0x1c25f0, 0x1c27a8, 0x1c2948, 0x1c2b00, 0x1c2cb0 and 0x1c2e60.
 */
/* 0x001c0ccc */
	.set	noreorder
	.text
	.globl	vu0_gx_load_strip_consts
	.ent	vu0_gx_load_strip_consts
vu0_gx_load_strip_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	lw	$8,1536($4)
	jr	$31
	lw	$2,20($8)
	.end	vu0_gx_load_strip_consts

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Writes a GIF tag with NLOOP = n and then GS RGBAQ/XYZ pairs for an index list (unreferenced).
 * Expects vf8, vf9 and vf11 loaded (0x1c06b4). Arguments: $4 = context (GIF tag template +1200, block
 * pointer +1536 whose +40 receives the tag address), $5 = vertex indices (shorts), $6 = count,
 * $7 = vertex buffer (64-byte entries), $8 = output. Returns the output end pointer.
 * No code in the executable calls it.
 */
/* 0x001c0ce8 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_strip_tag
	.ent	vu0_gx_emit_strip_tag
vu0_gx_emit_strip_tag:
	# GIF tag = template | (count & 0x7fff) (NLOOP); remember where it is
	lq	$12,1200($4)
	andi	$6,$6,0x7fff
	or	$12,$12,$6
	sq	$12,0($8)
	lw	$9,1536($4)
	sw	$8,40($9)
	addi	$8,$8,16
.Lvu0_gx_emit_strip_tag_loop:
	lh	$15,0($5)
	sll	$15,$15,0x6
	add	$15,$15,$7
	lqc2	$vf24,16($15)
	lqc2	$vf25,32($15)
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
	bne	$0,$6,.Lvu0_gx_emit_strip_tag_loop
	addi	$8,$8,32
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_strip_tag

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS RGBAQ/XYZ pairs for an index list, loading its own VU0 constants (unreferenced).
 * Arguments: $4 = context (colour multiplier +976, offset +992, scales +1232), $5 = vertex indices
 * (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (32 bytes per vertex).
 * Returns the output end pointer. ($15 = +1520 and $13 = count copy are not used.) No code in the
 * executable calls it.
 */
/* 0x001c0d54 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_strip
	.ent	vu0_gx_emit_strip
vu0_gx_emit_strip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	lw	$15,1520($4)
	movz	$13,$6,$0
.Lvu0_gx_emit_strip_loop:
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
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
	bne	$0,$6,.Lvu0_gx_emit_strip_loop
	addi	$8,$8,32
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_strip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Strip emitter with back-face marking and per-corner texture coordinates: GS STQ/RGBAQ/XYZ for 6-byte index records.
 * Arguments: $4 = ul_gx context (colour multiplier +976, offset +992, scales +1232: y = colour,
 * z = texture coordinate), $5 = 6-byte records (vertex index, s, t as shorts), $6 = count (negative =
 * opposite winding), $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex).
 * Back-facing triangles get 0x8000 (ADC) in XYZ word 3. Returns the output end pointer.
 * Called from 0x1bba58.
 */
/* 0x001c0dbc */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_uv_strip_cull
	.ent	vu0_gx_emit_uv_strip_cull
vu0_gx_emit_uv_strip_cull:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	# $10 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$10,$6,$0
	sll	$10,$10,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_uv_strip_cull_loop:
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
	# STQ = ((s, t) * scale * Q, Q) with Q from the vertex (+48 z)
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmulx.w	$vf24,$vf0,$vf0x		# vf24.w = 0
	sqc2	$vf24,32($8)
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf25,16($8)
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
	xor	$11,$11,$10
	sll	$11,$11,0xa
	sw	$11,44($8)
	xori	$10,$10,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,6
	addi	$6,$6,-1
	bne	$0,$6,.Lvu0_gx_emit_uv_strip_cull_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_uv_strip_cull

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS STQ/RGBAQ/XYZ qwords for 6-byte index records with per-corner texture coordinates, loading its own VU0 constants.
 * Arguments: $4 = context (colour multiplier +976, offset +992, scales +1232), $5 = 6-byte records
 * (vertex index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes
 * per vertex). Returns the output end pointer. ($15 = +1520 and $13 = count copy are not used.)
 * Called from the Ninja routine 0x1a59f8.
 */
/* 0x001c0e94 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_uv_strip
	.ent	vu0_gx_emit_uv_strip
vu0_gx_emit_uv_strip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	lw	$15,1520($4)
	movz	$13,$6,$0
.Lvu0_gx_emit_uv_strip_loop:
	lh	$14,0($5)
	lh	$2,2($5)
	lh	$3,4($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	pextlw	$2,$3,$2			# $2 = qword ($2, $3, -, -)
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	# STQ = ((s, t) * scale * Q, Q) with Q from the vertex (+48 z)
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmulx.w	$vf24,$vf0,$vf0x		# vf24.w = 0
	sqc2	$vf24,32($8)
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf25,16($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	bne	$0,$6,.Lvu0_gx_emit_uv_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_uv_strip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Strip emitter with back-face marking: GS STQ/RGBAQ/XYZ for an index list (texture coordinates from the vertices).
 * Arguments: $4 = ul_gx context (colour multiplier +976, offset +992, scales +1232), $5 = vertex indices
 * (shorts), $6 = count (negative = opposite winding), $7 = vertex buffer (64-byte entries), $8 = output
 * (48 bytes per vertex). Back-facing triangles get 0x8000 (ADC) in XYZ word 3. Returns the output end
 * pointer. Called from 0x1bbbc0.
 */
/* 0x001c0f24 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_tex_strip_cull
	.ent	vu0_gx_emit_tex_strip_cull
vu0_gx_emit_tex_strip_cull:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	# $10 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$10,$6,$0
	sll	$10,$10,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_tex_strip_cull_loop:
	lh	$9,0($5)
	sll	$9,$9,0x6
	add	$9,$9,$7
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y; STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	sqc2	$vf24,32($8)
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
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
	xor	$11,$11,$10
	sll	$11,$11,0xa
	sw	$11,44($8)
	xori	$10,$10,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf25,16($8)
	bne	$0,$6,.Lvu0_gx_emit_tex_strip_cull_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_tex_strip_cull

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS STQ/RGBAQ/XYZ qwords for an index list (texture coordinates from the vertices), loading its own VU0 constants (unreferenced).
 * Arguments: $4 = context (colour multiplier +976, offset +992, scales +1232), $5 = vertex indices
 * (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per vertex).
 * Returns the output end pointer. No code in the executable calls it.
 */
/* 0x001c0fe0 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_tex_strip
	.ent	vu0_gx_emit_tex_strip
vu0_gx_emit_tex_strip:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf10,960($4)
	lqc2	$vf11,1232($4)
	lw	$15,1520($4)
	movz	$13,$6,$0
.Lvu0_gx_emit_tex_strip_loop:
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y; STQ = (s * q, t * q, q)
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	sqc2	$vf24,32($8)
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf26,0($8)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	addi	$5,$5,2
	addi	$6,$6,-1
	sqc2	$vf25,16($8)
	bne	$0,$6,.Lvu0_gx_emit_tex_strip_loop
	addi	$8,$8,48
	jr	$31
	movz	$2,$8,$0
	.end	vu0_gx_emit_tex_strip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Strip emitter with back-face marking: GS RGBAQ/XYZ pairs for an index list until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1c06b4. Arguments: $5 = vertex indices (shorts), $6 = count
 * (negative = opposite winding), $7 = vertex buffer (64-byte entries, clip flags at +60), $8 = output
 * (32 bytes per vertex). Back-facing triangles get 0x8000 (ADC) in XYZ word 3. Returns the number
 * emitted. Called from 0x1c25f0.
 */
/* 0x001c1054 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_strip_cull_clip
	.ent	vu0_gx_emit_strip_cull_clip
vu0_gx_emit_strip_cull_clip:
	movz	$2,$0,$0
	# $13 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$13,$6,$0
	sll	$13,$13,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_strip_cull_clip_loop:
	# $12 = the indexed vertex; stop if it has clip flags
	lh	$12,0($5)
	sll	$12,$12,0x6
	add	$12,$12,$7
	lw	$11,60($12)
	bnez	$11,.Lvu0_gx_emit_strip_cull_clip_done
	lqc2	$vf24,16($12)
	lqc2	$vf25,32($12)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf24,16($8)
	sqc2	$vf25,0($8)
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
	sw	$11,28($8)
	xori	$13,$13,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,32
	bnez	$6,.Lvu0_gx_emit_strip_cull_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_strip_cull_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_strip_cull_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS RGBAQ/XYZ qword pairs for an index list of converted vertices, stopping at the first clipped one.
 * Same code as the Ninja routine 0x1af3bc. Expects vf8, vf9 and vf11 loaded by 0x1c06b4. Arguments:
 * $5 = vertex indices (shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (32 bytes
 * per vertex). Returns the number emitted. Called from 0x1c27a8.
 */
/* 0x001c1104 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_strip_clip
	.ent	vu0_gx_emit_strip_clip
vu0_gx_emit_strip_clip:
	movz	$2,$0,$0
	movz	$13,$6,$0
.Lvu0_gx_emit_strip_clip_loop:
	# $14 = the indexed vertex; stop if it has clip flags
	lh	$14,0($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lw	$11,60($14)
	bnez	$11,.Lvu0_gx_emit_strip_clip_done
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
	sqc2	$vf24,16($8)
	sqc2	$vf25,0($8)
	addi	$5,$5,2
	addi	$6,$6,-1
	addi	$8,$8,32
	bnez	$6,.Lvu0_gx_emit_strip_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_strip_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_strip_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Copies indexed vertices into a 64-byte list for the clipper, turning the 12.4 screen XYZ back into floats.
 * Arguments: $4 = context (vf8/vf9 loaded from +976/+992 but not used), $5 = vertex indices (shorts),
 * $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (64-byte entries). Same code as 0x1c35f4.
 * Called from 0x1a57e0, 0x1c25f0 and 0x1c27a8.
 */
/* 0x001c1168 */
	.set	noreorder
	.text
	.globl	vu0_gx_clip_copy
	.ent	vu0_gx_clip_copy
vu0_gx_clip_copy:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
.Lvu0_gx_clip_copy_loop:
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
	bnez	$6,.Lvu0_gx_clip_copy_loop
	addi	$5,$5,2
	jr	$31
	nop
	.end	vu0_gx_clip_copy

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Strip emitter with back-face marking and per-corner texture coordinates, until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded by 0x1c06b4. Arguments: $5 = 6-byte records (vertex index, s, t as
 * shorts), $6 = count (negative = opposite winding), $7 = vertex buffer (64-byte entries), $8 = output
 * (48 bytes per vertex: STQ, RGBAQ, XYZ). Back-facing triangles get 0x8000 (ADC) in XYZ word 3.
 * Returns the number emitted. Called from 0x1c2948.
 */
/* 0x001c11b8 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_uv_strip_cull_clip
	.ent	vu0_gx_emit_uv_strip_cull_clip
vu0_gx_emit_uv_strip_cull_clip:
	movz	$2,$0,$0
	# $10 = 0x20 when the count is negative (winding parity), $6 = |count|
	slt	$10,$6,$0
	sll	$10,$10,0x5
	pabsw	$6,$6
	# vf5, vf6 = the two previous screen positions
	vmove.xyzw	$vf5,$vf0		# vf5 = (0,0,0,1)
	vmove.xyzw	$vf6,$vf0		# vf6 = (0,0,0,1)
.Lvu0_gx_emit_uv_strip_cull_clip_loop:
	lh	$9,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	# $9 = the indexed vertex; stop if it has clip flags
	sll	$9,$9,0x6
	add	$9,$9,$7
	lw	$11,60($9)
	bnez	$11,.Lvu0_gx_emit_uv_strip_cull_clip_done
	lqc2	$vf24,16($9)
	lqc2	$vf25,32($9)
	lqc2	$vf26,48($9)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	# STQ = ((s, t) * scale * Q, Q) with Q from the vertex (+48 z)
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf1			# vf1 = $12 (128 bits)
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
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
	xor	$11,$11,$10
	sll	$11,$11,0xa
	sw	$11,44($8)
	xori	$10,$10,0x20			# flip the parity for the next triangle of the strip
	addi	$5,$5,6
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lvu0_gx_emit_uv_strip_cull_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_uv_strip_cull_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_uv_strip_cull_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS STQ/RGBAQ/XYZ qwords for 6-byte index records with per-corner texture coordinates, until the first clipped vertex.
 * Expects vf8, vf9 and vf11 loaded (0x1c06b4 / the Ninja loader). Arguments: $5 = 6-byte records (vertex
 * index, s, t as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (48 bytes per
 * vertex). Returns the number emitted. Called from 0x1a55a8 and 0x1c2b00.
 */
/* 0x001c1290 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_uv_strip_clip
	.ent	vu0_gx_emit_uv_strip_clip
vu0_gx_emit_uv_strip_clip:
	movz	$2,$0,$0
.Lvu0_gx_emit_uv_strip_clip_loop:
	lh	$14,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	# $14 = the indexed vertex; stop if it has clip flags
	sll	$14,$14,0x6
	add	$14,$14,$7
	lw	$11,60($14)
	bnez	$11,.Lvu0_gx_emit_uv_strip_clip_done
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	# XYZ with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y as integers
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyz	$vf25,$vf25,$vf9	# vf25.xyz = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	# STQ = ((s, t) * scale * Q, Q) with Q from the vertex (+48 z)
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf1			# vf1 = $12 (128 bits)
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf1,$vf1,$vf11z	# vf1.xy = vf1*vf11.z
	vmove.xy	$vf26,$vf1		# vf26.xy = vf1
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf24,32($8)
	sqc2	$vf25,16($8)
	sqc2	$vf26,0($8)
	addi	$5,$5,6
	addi	$6,$6,-1
	addi	$8,$8,48
	bnez	$6,.Lvu0_gx_emit_uv_strip_clip_loop
	addi	$2,$2,1
.Lvu0_gx_emit_uv_strip_clip_done:
	jr	$31
	nop
	.end	vu0_gx_emit_uv_strip_clip

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Copies indexed vertices into a 64-byte list for the clipper with per-corner texture coordinates, XYZ back to floats.
 * Expects vf11 loaded (z = texture coordinate scale). Arguments: $5 = 6-byte records (vertex index, s, t
 * as shorts), $6 = count, $7 = vertex buffer (64-byte entries), $8 = output (64-byte entries: +16 float
 * XYZ, +48 = ((s, t) * scale, the vertex's z and w)). Called from 0x1a55a8, 0x1c2948 and 0x1c2b00.
 */
/* 0x001c1318 */
	.set	noreorder
	.text
	.globl	vu0_gx_clip_copy_uv
	.ent	vu0_gx_clip_copy_uv
vu0_gx_clip_copy_uv:
.Lvu0_gx_clip_copy_uv_loop:
	lh	$14,0($5)
	lh	$12,2($5)
	lh	$13,4($5)
	sll	$14,$14,0x6
	add	$14,$14,$7
	lqc2	$vf24,16($14)
	lqc2	$vf25,32($14)
	lqc2	$vf26,48($14)
	lqc2	$vf27,0($14)
	vitof4.xyz	$vf24,$vf24		# vf24.xyz = vf24 12.4 fixed point to float
	# (s, t) as floats times the scale
	pextlw	$12,$13,$12			# $12 = qword ($12, $13, -, -)
	qmtc2	$12,$vf1			# vf1 = $12 (128 bits)
	vitof0.xy	$vf1,$vf1		# vf1.xy = vf1 integer to float
	vmulz.xy	$vf26,$vf1,$vf11z	# vf26.xy = vf1*vf11.z
	sqc2	$vf24,16($8)
	sqc2	$vf25,32($8)
	sqc2	$vf26,48($8)
	sqc2	$vf27,0($8)
	addi	$6,$6,-1
	addi	$8,$8,64
	bnez	$6,.Lvu0_gx_clip_copy_uv_loop
	addi	$5,$5,6
	jr	$31
	nop
	.end	vu0_gx_clip_copy_uv

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Back-face test of a screen triangle: returns 0x20 if its 2D winding (XOR the sign of cull) is negative, else 0.
 * Arguments: $4, $5, $6 = clip vertices (screen x, y at +16), $7 = cull mode (negative flips the
 * result). Returns (sign of z of (b - a) x (b - c)) << 5 XOR (cull < 0) << 5. Called from 0x1b95e0 and
 * 0x1b9e70 ("if (cull != 0 && ul_tri_backfacing(...) != 0)").
 */
/* 0x001c1378 */
	.set	noreorder
	.text
	.globl	ul_tri_backfacing
	.ent	ul_tri_backfacing
ul_tri_backfacing:
	lqc2	$vf5,16($4)
	lqc2	$vf6,16($5)
	lqc2	$vf7,16($6)
	# winding: z of (b - a) x (b - c)
	vsub.xy	$vf1,$vf6,$vf5			# vf1.xy = vf6 - vf5
	vsub.xy	$vf2,$vf6,$vf7			# vf2.xy = vf6 - vf7
	# vf4.xyz = vf1 x vf2 (cross product)
	vopmula.xyz	ACC,$vf1,$vf2
	vopmsub.xyz	$vf4,$vf2,$vf1
	# $13 = 0x20 when cull < 0
	slt	$13,$7,$0
	sll	$13,$13,0x5
	vnop
	vnop
	vnop
	vnop
	cfc2	$11,$vi17			# $11 = MAC flags (bit 5 = sign of the last result z)
	andi	$11,$11,0x20
	xor	$11,$11,$13
	jr	$31
	movz	$2,$11,$0
	.end	ul_tri_backfacing

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS STQ/RGBAQ/XYZ qwords for a list of clipped-vertex pointers (float positions converted to 12.4).
 * Arguments: $4 = ul_gx context (colour multiplier +976, offset +992, scales +1232), $5 = vertex pointers,
 * $6 = count, $7 = output (48 bytes per vertex). Returns the output end pointer. Called from 0x1b95e0.
 */
/* 0x001c13c0 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_clipped_tex
	.ent	vu0_gx_emit_clipped_tex
vu0_gx_emit_clipped_tex:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf11,1232($4)
.Lvu0_gx_emit_clipped_tex_loop:
	lw	$12,0($5)
	lqc2	$vf24,16($12)
	lqc2	$vf25,32($12)
	lqc2	$vf26,48($12)
	# XYZ = 12.4 fixed point with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.y; STQ = (s * q, t * q, q)
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyzw	$vf25,$vf25,$vf9	# vf25 = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmuly.xyzw	$vf25,$vf25,$vf11y	# vf25 = vf25*vf11.y
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	vmulz.xy	$vf26,$vf26,$vf26z	# vf26.xy = vf26*vf26.z
	sqc2	$vf24,32($7)
	sqc2	$vf25,16($7)
	sqc2	$vf26,0($7)
	addi	$7,$7,48
	addi	$6,$6,-1
	bnez	$6,.Lvu0_gx_emit_clipped_tex_loop
	addi	$5,$5,4
	jr	$31
	movz	$2,$7,$0
	.end	vu0_gx_emit_clipped_tex

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper edge split: interpolates two clip vertices at the plane y = Y and re-tests the new vertex against the guard band (unreferenced).
 * Arguments: $5 = vertex a, $6 = vertex b (screen position +16, colour +32, texture +48), $7 = output
 * vertex, $f12 = Y. Expects vf19 (z.z = depth numerator) and vf30/vf31 (guard band) already loaded.
 * Output: +16 position lerp (y = Y, z = vf19.z / w), +32 colour lerp, +48 texture lerp with w = x/y clip
 * flags; returns those flags. No code in the executable calls it.
 */
/* 0x001c1424 */
	.set	noreorder
	.text
	.globl	vu0_clip_split_tex_y
	.ent	vu0_clip_split_tex_y
vu0_clip_split_tex_y:
	lqc2	$vf10,16($5)
	lqc2	$vf11,16($6)
	mfc1	$12,$f12
	qmtc2	$12,$vf21			# vf21 = $12 (128 bits)
	# t = (Y - a.y) / (b.y - a.y)
	vaddx.y	$vf21,$vf0,$vf21x		# vf21.y = vf21.x
	vsub.y	$vf2,$vf11,$vf10		# vf2.y = vf11.y - vf10.y
	vsub.y	$vf3,$vf21,$vf10		# vf3.y = vf21.y - vf10.y
	vdiv	Q,$vf3y,$vf2y			# Q = vf3.y / vf2.y
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf12,32($5)
	lqc2	$vf13,32($6)
	lqc2	$vf14,48($5)
	lqc2	$vf15,48($6)
	# vf20 = (t, 1 - t)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	# position = a * (1 - t) + b * t, y = Y; z = vf19.z / w
	# vf24.xzw = vf10*vf20.y + vf11*vf20.x
	vmulay.xzw	ACC,$vf10,$vf20y
	vmaddx.xzw	$vf24,$vf11,$vf20x
	vmove.y	$vf24,$vf21			# vf24.y = vf21.y
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.z	$vf24,$vf19,Q			# vf24.z = vf19.z*Q
	# colour and texture coordinates interpolated the same way
	# vf25 = vf12*vf20.y + vf13*vf20.x
	vmulay.xyzw	ACC,$vf12,$vf20y
	vmaddx.xyzw	$vf25,$vf13,$vf20x
	# vf26.xyz = vf14*vf20.y + vf15*vf20.x
	vmulay.xyz	ACC,$vf14,$vf20y
	vmaddx.xyz	$vf26,$vf15,$vf20x
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($7)
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_tex_y

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper edge split: interpolates two clip vertices at the plane x = X and re-tests the new vertex against the guard band (unreferenced).
 * Arguments: $5 = vertex a, $6 = vertex b (screen position +16, colour +32, texture +48), $7 = output
 * vertex, $f12 = X. Expects vf19 (z = depth numerator) and vf30/vf31 (guard band) already loaded.
 * Output: +16 position lerp (x = X, z = vf19.z / w), +32 colour lerp, +48 texture lerp with w = x/y clip
 * flags; returns those flags. No code in the executable calls it. (The x twin of 0x1c0e0c.)
 */
/* 0x001c14d0 */
	.set	noreorder
	.text
	.globl	vu0_clip_split_tex_x
	.ent	vu0_clip_split_tex_x
vu0_clip_split_tex_x:
	lqc2	$vf10,16($5)
	lqc2	$vf11,16($6)
	mfc1	$12,$f12
	qmtc2	$12,$vf21			# vf21 = $12 (128 bits)
	# t = (X - a.x) / (b.x - a.x)
	vsub.x	$vf2,$vf11,$vf10		# vf2.x = vf11.x - vf10.x
	vsub.x	$vf3,$vf21,$vf10		# vf3.x = vf21.x - vf10.x
	vdiv	Q,$vf3x,$vf2x			# Q = vf3.x / vf2.x
	# vf20 = (t, 1 - t)
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf12,32($5)
	lqc2	$vf13,32($6)
	lqc2	$vf14,48($5)
	lqc2	$vf15,48($6)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	# position = a * (1 - t) + b * t, x = X; z = vf19.z / w
	# vf24.yzw = vf10*vf20.y + vf11*vf20.x
	vmulay.yzw	ACC,$vf10,$vf20y
	vmaddx.yzw	$vf24,$vf11,$vf20x
	vmove.x	$vf24,$vf21			# vf24.x = vf21.x
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.z	$vf24,$vf19,Q			# vf24.z = vf19.z*Q
	# colour and texture coordinates interpolated the same way
	# vf25 = vf12*vf20.y + vf13*vf20.x
	vmulay.xyzw	ACC,$vf12,$vf20y
	vmaddx.xyzw	$vf25,$vf13,$vf20x
	# vf26.xyz = vf14*vf20.y + vf15*vf20.x
	vmulay.xyz	ACC,$vf14,$vf20y
	vmaddx.xyz	$vf26,$vf15,$vf20x
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($7)
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_tex_x

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper near-plane split: interpolates two view-space vertices at z = plane.z, projects the result and guard-band tests it.
 * Arguments: $5 = vertex a, $6 = vertex b (view position +0, colour +32, texture +48), $7 = output
 * vertex, $8 = plane (z = plane depth, w = its 1/depth). Expects vf16-vf19 (projection) and vf30/vf31
 * (guard band) loaded (0x1c1234). Output: +0 view position, +16 projected position, +32 colour,
 * +48 (s, t, 1/depth, x/y clip flags). Returns the flags. Called from 0x1b95e0.
 */
/* 0x001c1578 */
	.set	noreorder
	.text
	.globl	vu0_clip_split_tex_z
	.ent	vu0_clip_split_tex_z
vu0_clip_split_tex_z:
	lqc2	$vf10,0($5)
	lqc2	$vf11,0($6)
	lqc2	$vf21,0($8)
	# t = (plane.z - a.z) / (b.z - a.z)
	vsub.z	$vf2,$vf11,$vf10		# vf2.z = vf11.z - vf10.z
	vsub.z	$vf3,$vf21,$vf10		# vf3.z = vf21.z - vf10.z
	vdiv	Q,$vf3z,$vf2z			# Q = vf3.z / vf2.z
	# vf20 = (t, 1 - t)
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf12,32($5)
	lqc2	$vf13,32($6)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	lqc2	$vf14,48($5)
	lqc2	$vf15,48($6)
	# view position = a * (1 - t) + b * t with z = plane.z; colour and texture the same way
	# vf27.xy = vf10*vf20.y + vf11*vf20.x
	vmulay.xy	ACC,$vf10,$vf20y
	vmaddx.xy	$vf27,$vf11,$vf20x
	vmove.z	$vf27,$vf21			# vf27.z = vf21.z
	# vf25 = vf12*vf20.y + vf13*vf20.x
	vmulay.xyzw	ACC,$vf12,$vf20y
	vmaddx.xyzw	$vf25,$vf13,$vf20x
	sqc2	$vf27,0($7)
	# vf26.xy = vf14*vf20.y + vf15*vf20.x
	vmulay.xy	ACC,$vf14,$vf20y
	vmaddx.xy	$vf26,$vf15,$vf20x
	# project (the w divide is the plane's 1/depth)
	# vf24 = vf16*vf27.x + vf17*vf27.y + vf18*vf27.z + vf19
	vmulax.xyzw	ACC,$vf16,$vf27x
	vmadday.xyzw	ACC,$vf17,$vf27y
	vmaddaz.xyzw	ACC,$vf18,$vf27z
	vmaddw.xyzw	$vf24,$vf19,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf21w	# vf24.xyz = vf24*vf21.w
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vaddw.z	$vf26,$vf0,$vf21w		# vf26.z = vf21.w
	sqc2	$vf24,16($7)
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_tex_z

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Emits GS RGBAQ/XYZ pairs for a list of clipped-vertex pointers (float positions converted to 12.4).
 * Arguments: $4 = ul_gx context (colour multiplier +976, offset +992, scales +1232), $5 = vertex
 * pointers, $6 = count, $7 = output (32 bytes per vertex). Returns the output end pointer.
 * Called from 0x1b9e70.
 */
/* 0x001c1624 */
	.set	noreorder
	.text
	.globl	vu0_gx_emit_clipped
	.ent	vu0_gx_emit_clipped
vu0_gx_emit_clipped:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf11,1232($4)
.Lvu0_gx_emit_clipped_loop:
	lw	$12,0($5)
	lqc2	$vf24,16($12)
	lqc2	$vf25,32($12)
	# XYZ = 12.4 fixed point with w = 0; colour = clamp(colour * vf8 + vf9, 0, 1) * vf11.x as integers
	vftoi4.xyz	$vf24,$vf24		# vf24.xyz = vf24 as 12.4 fixed point
	vmulx.w	$vf24,$vf24,$vf0x		# vf24.w = 0
	vmul.xyzw	$vf25,$vf25,$vf8	# vf25 = vf25*vf8
	vadd.xyzw	$vf25,$vf25,$vf9	# vf25 = vf25 + vf9
	vminiw.xyzw	$vf25,$vf25,$vf0w	# vf25 = min(vf25, 1)
	vmaxx.xyzw	$vf25,$vf25,$vf0x	# vf25 = max(vf25, 0)
	vmulx.xyzw	$vf25,$vf25,$vf11x	# vf25 = vf25*vf11.x
	vftoi0.xyzw	$vf25,$vf25		# vf25 = vf25 as integer
	sqc2	$vf24,16($7)
	sqc2	$vf25,0($7)
	addi	$7,$7,32
	addi	$6,$6,-1
	bnez	$6,.Lvu0_gx_emit_clipped_loop
	addi	$5,$5,4
	jr	$31
	movz	$2,$7,$0
	.end	vu0_gx_emit_clipped

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper edge split without texture: interpolates two clip vertices at y = Y and re-tests the new vertex.
 * Arguments: $5 = vertex a, $6 = vertex b, $7 = output vertex, $f12 = Y. Expects vf19, vf30 and vf31
 * loaded. Output: +16 position lerp (y = Y, z = vf19.z / w), +32 colour lerp, +48 w = x/y clip flags.
 * Returns the flags. Called from 0x1b9e70.
 */
/* 0x001c167c */
	.set	noreorder
	.text
	.globl	vu0_clip_split_y
	.ent	vu0_clip_split_y
vu0_clip_split_y:
	lqc2	$vf10,16($5)
	lqc2	$vf11,16($6)
	mfc1	$12,$f12
	qmtc2	$12,$vf21			# vf21 = $12 (128 bits)
	# t = (Y - a.y) / (b.y - a.y)
	vaddx.y	$vf21,$vf0,$vf21x		# vf21.y = vf21.x
	vsub.y	$vf2,$vf11,$vf10		# vf2.y = vf11.y - vf10.y
	vsub.y	$vf3,$vf21,$vf10		# vf3.y = vf21.y - vf10.y
	vdiv	Q,$vf3y,$vf2y			# Q = vf3.y / vf2.y
	# vf20 = (t, 1 - t)
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf12,32($5)
	lqc2	$vf13,32($6)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	# position = a * (1 - t) + b * t, y = Y; z = vf19.z / w
	# vf24.xzw = vf10*vf20.y + vf11*vf20.x
	vmulay.xzw	ACC,$vf10,$vf20y
	vmaddx.xzw	$vf24,$vf11,$vf20x
	vmove.y	$vf24,$vf21			# vf24.y = vf21.y
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.z	$vf24,$vf19,Q			# vf24.z = vf19.z*Q
	# vf25 = vf12*vf20.y + vf13*vf20.x
	vmulay.xyzw	ACC,$vf12,$vf20y
	vmaddx.xyzw	$vf25,$vf13,$vf20x
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($7)
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_y

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper edge split without texture: interpolates two clip vertices at x = X and re-tests the new vertex.
 * Arguments: $5 = vertex a, $6 = vertex b, $7 = output vertex, $f12 = X. Expects vf19, vf30 and vf31
 * loaded. Output: +16 position lerp (x = X, z = vf19.z / w), +32 colour lerp, +48 w = x/y clip flags.
 * Returns the flags. Called from 0x1b9e70.
 */
/* 0x001c1718 */
	.set	noreorder
	.text
	.globl	vu0_clip_split_x
	.ent	vu0_clip_split_x
vu0_clip_split_x:
	lqc2	$vf10,16($5)
	lqc2	$vf11,16($6)
	mfc1	$12,$f12
	qmtc2	$12,$vf21			# vf21 = $12 (128 bits)
	# t = (X - a.x) / (b.x - a.x)
	vsub.x	$vf2,$vf11,$vf10		# vf2.x = vf11.x - vf10.x
	vsub.x	$vf3,$vf21,$vf10		# vf3.x = vf21.x - vf10.x
	vdiv	Q,$vf3x,$vf2x			# Q = vf3.x / vf2.x
	# vf20 = (t, 1 - t)
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf12,32($5)
	lqc2	$vf13,32($6)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	# position = a * (1 - t) + b * t, x = X; z = vf19.z / w
	# vf24.yzw = vf10*vf20.y + vf11*vf20.x
	vmulay.yzw	ACC,$vf10,$vf20y
	vmaddx.yzw	$vf24,$vf11,$vf20x
	vmove.x	$vf24,$vf21			# vf24.x = vf21.x
	vdiv	Q,$vf0w,$vf24w			# Q = 1 / vf24.w
	vwaitq					# wait for Q
	vmulq.z	$vf24,$vf19,Q			# vf24.z = vf19.z*Q
	# vf25 = vf12*vf20.y + vf13*vf20.x
	vmulay.xyzw	ACC,$vf12,$vf20y
	vmaddx.xyzw	$vf25,$vf13,$vf20x
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vnop
	vnop
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf24,16($7)
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_x

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clipper near-plane split without texture: interpolates two view-space vertices at z = plane.z and projects the result.
 * Arguments: $5 = vertex a, $6 = vertex b (view position +0, colour +32), $7 = output vertex, $8 = plane
 * (z = depth, w = 1/depth). Expects vf16-vf19, vf30 and vf31 loaded (0x1c1234). Output: +0 view
 * position, +16 projected position, +32 colour, +48 (-, -, 1/depth, x/y clip flags). Returns the flags.
 * Called from 0x1b9e70.
 */
/* 0x001c17b0 */
	.set	noreorder
	.text
	.globl	vu0_clip_split_z
	.ent	vu0_clip_split_z
vu0_clip_split_z:
	lqc2	$vf12,0($5)
	lqc2	$vf13,0($6)
	lqc2	$vf21,0($8)
	# t = (plane.z - a.z) / (b.z - a.z)
	vsub.z	$vf2,$vf13,$vf12		# vf2.z = vf13.z - vf12.z
	vsub.z	$vf3,$vf21,$vf12		# vf3.z = vf21.z - vf12.z
	vdiv	Q,$vf3z,$vf2z			# Q = vf3.z / vf2.z
	# vf20 = (t, 1 - t)
	vaddw.y	$vf20,$vf0,$vf0w		# vf20.y = 1
	lqc2	$vf14,32($5)
	lqc2	$vf15,32($6)
	vwaitq					# wait for Q
	vaddq.x	$vf20,$vf0,Q			# vf20.x = Q
	vsubq.y	$vf20,$vf20,Q			# vf20.y = vf20.y - Q
	# view position = a * (1 - t) + b * t with z = plane.z; colour the same way
	# vf27.xy = vf12*vf20.y + vf13*vf20.x
	vmulay.xy	ACC,$vf12,$vf20y
	vmaddx.xy	$vf27,$vf13,$vf20x
	vmove.z	$vf27,$vf21			# vf27.z = vf21.z
	# vf25 = vf14*vf20.y + vf15*vf20.x
	vmulay.xyzw	ACC,$vf14,$vf20y
	vmaddx.xyzw	$vf25,$vf15,$vf20x
	sqc2	$vf27,0($7)
	# project (the w divide is the plane's 1/depth)
	# vf24 = vf16*vf27.x + vf17*vf27.y + vf18*vf27.z + vf19
	vmulax.xyzw	ACC,$vf16,$vf27x
	vmadday.xyzw	ACC,$vf17,$vf27y
	vmaddaz.xyzw	ACC,$vf18,$vf27z
	vmaddw.xyzw	$vf24,$vf19,$vf0w
	vmulw.xyz	$vf24,$vf24,$vf21w	# vf24.xyz = vf24*vf21.w
	# guard-band test: (screen * scale + offset) against +-1, x and y planes only
	vmul.xyzw	$vf27,$vf24,$vf30	# vf27 = vf24*vf30
	vadd.xyzw	$vf27,$vf27,$vf31	# vf27 = vf27 + vf31
	vaddw.z	$vf27,$vf0,$vf27w		# vf27.z = vf27.w
	vclipw.xyz	$vf27,$vf0w		# clip judgement of vf27.xyz against +-1
	vaddw.z	$vf26,$vf0,$vf21w		# vf26.z = vf21.w
	sqc2	$vf24,16($7)
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	andi	$2,$2,0xf
	qmtc2	$2,$vf1				# vf1 = $2 (128 bits)
	vmr32.w	$vf26,$vf1			# vf26.w = vf1.x
	sqc2	$vf25,32($7)
	sqc2	$vf26,48($7)
	jr	$31
	nop
	.end	vu0_clip_split_z

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Loads the clipper's VU0 constants: vf8/vf9 = colour multiplier/offset (+976/+992), vf16-vf19 = projection (+128), vf30/vf31 = guard band (+1168/+1184).
 * Argument: $4 = ul_gx context (or the Ninja system block, same layout). Called from the Ninja
 * routines 0x1a55a8 / 0x1a57e0 and the ul_gx strip drawers 0x1c25f0, 0x1c27a8, 0x1c2948, 0x1c2b00 and
 * 0x1c2cb0 before they run the clipper.
 */
/* 0x001c184c */
	.set	noreorder
	.text
	.globl	vu0_clip_load_consts
	.ent	vu0_clip_load_consts
vu0_clip_load_consts:
	lqc2	$vf8,976($4)
	lqc2	$vf9,992($4)
	lqc2	$vf16,128($4)
	lqc2	$vf17,144($4)
	lqc2	$vf18,160($4)
	lqc2	$vf19,176($4)
	lqc2	$vf30,1168($4)
	lqc2	$vf31,1184($4)
	jr	$31
	nop
	.end	vu0_clip_load_consts

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Gathers a list of clip-vertex pointers into xyz, colour and (s, t) arrays.
 * Arguments: $5 = vertex pointers, $6 = count, $7 = xyz out (12 bytes), $8 = colour (+32) out (16 bytes),
 * $9 = (s, t) (+48) out (8 bytes). Called from the Ninja routines 0x1aa500, 0x1aa8d8, 0x1ab010, 0x1ab3e0
 * and 0x1ad0d0.
 */
/* 0x001c1874 */
	.set	noreorder
	.text
	.globl	vu0_clip_gather_st
	.ent	vu0_clip_gather_st
vu0_clip_gather_st:
.Lvu0_clip_gather_st_loop:
	lw	$11,0($5)
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($7)
	sw	$13,4($7)
	sw	$14,8($7)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	sw	$15,12($8)
	lw	$12,48($11)
	lw	$13,52($11)
	sw	$12,0($9)
	sw	$13,4($9)
	addi	$5,$5,4
	addi	$6,$6,-1
	addi	$7,$7,12
	addi	$8,$8,16
	bnez	$6,.Lvu0_clip_gather_st_loop
	addi	$9,$9,8
	jr	$31
	nop
	.end	vu0_clip_gather_st

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Gathers a list of clip-vertex pointers into xyz and colour arrays.
 * Arguments: $5 = vertex pointers, $6 = count, $7 = xyz out (12 bytes), $8 = colour (+32) out (16 bytes).
 * Called from the Ninja routines 0x1aac80 and 0x1ab788.
 */
/* 0x001c18e0 */
	.set	noreorder
	.text
	.globl	vu0_clip_gather
	.ent	vu0_clip_gather
vu0_clip_gather:
.Lvu0_clip_gather_loop:
	lw	$11,0($5)
	lw	$12,0($11)
	lw	$13,4($11)
	lw	$14,8($11)
	sw	$12,0($7)
	sw	$13,4($7)
	sw	$14,8($7)
	lw	$12,32($11)
	lw	$13,36($11)
	lw	$14,40($11)
	lw	$15,44($11)
	sw	$12,0($8)
	sw	$13,4($8)
	sw	$14,8($8)
	sw	$15,12($8)
	addi	$5,$5,4
	addi	$6,$6,-1
	addi	$7,$7,12
	bnez	$6,.Lvu0_clip_gather_loop
	addi	$8,$8,16
	jr	$31
	nop
	.end	vu0_clip_gather

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Projects a list of clipped textured vertices to GS STQ/RGBAQ/XYZF qwords with fog.
 * Arguments: $4 = context (projection +128, colour scale +1040.w, fog parameters +1376), $5 = vertex
 * pointers (view position +0, colour +32, (s, t) +48), $6 = count, $7 = output (48 bytes per vertex).
 * Returns the output end pointer. Called from the Ninja routines 0x1a4570 ("sent as a fan through
 * 0x1c1320"), 0x1abb18 and 0x1abde8.
 */
/* 0x001c1938 */
	.set	noreorder
	.text
	.globl	vu0_clip_emit_tex_fog
	.ent	vu0_clip_emit_tex_fog
vu0_clip_emit_tex_fog:
	lqc2	$vf8,128($4)
	lqc2	$vf9,144($4)
	lqc2	$vf10,160($4)
	lqc2	$vf11,176($4)
	lqc2	$vf7,1040($4)
	lqc2	$vf19,1376($4)
.Lvu0_clip_emit_tex_fog_loop:
	lw	$12,0($5)
	lqc2	$vf1,0($12)
	lqc2	$vf2,32($12)
	lqc2	$vf3,48($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vdiv	Q,$vf0w,$vf1w			# Q = 1 / vf1.w
	# colour * scale as integers
	vmulw.xyzw	$vf2,$vf2,$vf7w		# vf2 = vf2*vf7.w
	vftoi0.xyzw	$vf2,$vf2		# vf2 = vf2 as integer
	vwaitq					# wait for Q
	vmulq.xyz	$vf1,$vf1,Q		# vf1.xyz = vf1*Q
	# fog: vf4.w = clamp(vf19.w + w * vf19.x, 0, vf19.z)
	vmulaw.w	ACC,$vf0,$vf19w
	vmaddx.w	$vf4,$vf1,$vf19x
	vminiz.w	$vf4,$vf4,$vf19z	# vf4.w = min(vf4.w, vf19.z)
	vmaxx.w	$vf4,$vf4,$vf0x			# vf4.w = max(vf4.w, 0)
	vftoi4.w	$vf1,$vf4		# vf1.w = vf4.w as 12.4 fixed point
	vftoi4.xyz	$vf1,$vf1		# vf1.xyz = vf1 as 12.4 fixed point
	# STQ = (s * Q, t * Q, Q)
	vaddw.z	$vf3,$vf0,$vf0w			# vf3.z = 1
	vmulq.xyz	$vf3,$vf3,Q		# vf3.xyz = vf3*Q
	sqc2	$vf3,0($7)
	sqc2	$vf2,16($7)
	sqc2	$vf1,32($7)
	addi	$6,$6,-1
	addi	$5,$5,4
	bnez	$6,.Lvu0_clip_emit_tex_fog_loop
	addi	$7,$7,48
	jr	$31
	movz	$2,$7,$0
	.end	vu0_clip_emit_tex_fog

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Projects a list of clipped untextured vertices to GS STQ/RGBAQ/XYZF qwords with fog.
 * Arguments: $4 = context (projection +128, per-component colour scale +1040, fog parameters +1376),
 * $5 = vertex pointers (view position +0, colour +32), $6 = count, $7 = output (48 bytes per vertex; only
 * Q of STQ is set). Returns the output end pointer. Called from the Ninja routines 0x1a4890 and 0x1ac0d0.
 */
/* 0x001c19c8 */
	.set	noreorder
	.text
	.globl	vu0_clip_emit_fog
	.ent	vu0_clip_emit_fog
vu0_clip_emit_fog:
	lqc2	$vf8,128($4)
	lqc2	$vf9,144($4)
	lqc2	$vf10,160($4)
	lqc2	$vf11,176($4)
	lqc2	$vf7,1040($4)
	lqc2	$vf19,1376($4)
.Lvu0_clip_emit_fog_loop:
	lw	$12,0($5)
	lqc2	$vf1,0($12)
	lqc2	$vf2,32($12)
	# vf1 = vf8*vf1.x + vf9*vf1.y + vf10*vf1.z + vf11
	vmulax.xyzw	ACC,$vf8,$vf1x
	vmadday.xyzw	ACC,$vf9,$vf1y
	vmaddaz.xyzw	ACC,$vf10,$vf1z
	vmaddw.xyzw	$vf1,$vf11,$vf0w
	vdiv	Q,$vf0w,$vf1w			# Q = 1 / vf1.w
	# colour * scale as integers
	vmul.xyzw	$vf2,$vf2,$vf7		# vf2 = vf2*vf7
	vftoi0.xyzw	$vf2,$vf2		# vf2 = vf2 as integer
	vwaitq					# wait for Q
	vmulq.xyz	$vf1,$vf1,Q		# vf1.xyz = vf1*Q
	# fog: vf4.w = clamp(vf19.w + w * vf19.x, 0, vf19.z)
	vmulaw.w	ACC,$vf0,$vf19w
	vmaddx.w	$vf4,$vf1,$vf19x
	vminiz.w	$vf4,$vf4,$vf19z	# vf4.w = min(vf4.w, vf19.z)
	vmaxx.w	$vf4,$vf4,$vf0x			# vf4.w = max(vf4.w, 0)
	vftoi4.w	$vf1,$vf4		# vf1.w = vf4.w as 12.4 fixed point
	vftoi4.xyz	$vf1,$vf1		# vf1.xyz = vf1 as 12.4 fixed point
	vaddq.z	$vf3,$vf0,Q			# vf3.z = Q
	sqc2	$vf2,16($7)
	sqc2	$vf1,32($7)
	sqc2	$vf3,0($7)
	addi	$6,$6,-1
	addi	$5,$5,4
	bnez	$6,.Lvu0_clip_emit_fog_loop
	addi	$7,$7,48
	jr	$31
	movz	$2,$7,$0
	.end	vu0_clip_emit_fog

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clip codes of a triangle: returns the 18 flag bits of three clip-space positions (unreferenced).
 * Arguments: $4, $5, $6 = vertices (clip-space position at +16). Returns the clip flag register
 * (a in bits 12-17, b in 6-11, c in 0-5). No code in the executable calls it.
 */
/* 0x001c1a50 */
	.set	noreorder
	.text
	.globl	ul_tri_clip_codes
	.ent	ul_tri_clip_codes
ul_tri_clip_codes:
	lqc2	$vf1,16($4)
	lqc2	$vf2,16($5)
	lqc2	$vf3,16($6)
	vclipw.xyz	$vf1,$vf1w		# clip judgement of vf1.xyz against +-vf1.w
	vclipw.xyz	$vf2,$vf2w		# clip judgement of vf2.xyz against +-vf2.w
	vclipw.xyz	$vf3,$vf3w		# clip judgement of vf3.xyz against +-vf3.w
	# let the judgements reach the flag register
	vnop
	vnop
	vnop
	vnop
	vnop
	jr	$31
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	.end	ul_tri_clip_codes

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Clip codes of a line: returns the 12 flag bits of two clip-space positions (a in bits 6-11, b in 0-5).
 * Arguments: $4, $5 = vertices (clip-space position at +16). Called from the line clipper 0x1ba6f8
 * ("the combined outcode word from 0x1c146c(a, b)").
 */
/* 0x001c1a84 */
	.set	noreorder
	.text
	.globl	ul_line_clip_codes
	.ent	ul_line_clip_codes
ul_line_clip_codes:
	lqc2	$vf10,16($4)
	lqc2	$vf11,16($5)
	vclipw.xyz	$vf10,$vf10w		# clip judgement of vf10.xyz against +-vf10.w
	vclipw.xyz	$vf11,$vf11w		# clip judgement of vf11.xyz against +-vf11.w
	# let the judgements reach the flag register
	vnop
	vnop
	vnop
	vnop
	vnop
	jr	$31
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	.end	ul_line_clip_codes

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Line clipper near-plane split: out = a + (b - a) * t where the edge meets z = s * w; returns the new vertex's clip flags.
 * Arguments: $4 = output vertex, $5 = a, $6 = b (64-byte clip vertices: +0, clip position +16, +32, +48),
 * $f12 = s (+1 or -1). t = |da / (db - da)| with d = z - s * w. All four qwords are interpolated; the
 * clip flags are stored at +60 and returned. Called from the line clipper 0x1ba6f8 with s = -1.
 */
/* 0x001c1ab0 */
	.set	noreorder
	.text
	.globl	ul_clip_split_z
	.ent	ul_clip_split_z
ul_clip_split_z:
	lqc2	$vf19,16($5)
	lqc2	$vf23,16($6)
	mfc1	$8,$f12
	qmtc2	$8,$vf9				# vf9 = $8 (128 bits)
	# vf24 = a - s * a.w, vf25 = b - s * b.w (signed distances in z)
	vmulx.w	$vf24,$vf19,$vf9x		# vf24.w = vf19.w*vf9.x
	vmulx.w	$vf25,$vf23,$vf9x		# vf25.w = vf23.w*vf9.x
	lqc2	$vf16,0($5)
	lqc2	$vf17,32($5)
	lqc2	$vf18,48($5)
	lqc2	$vf20,0($6)
	lqc2	$vf21,32($6)
	lqc2	$vf22,48($6)
	vsubw.xyzw	$vf24,$vf19,$vf24w	# vf24 = vf19 - vf24.w
	vsubw.xyzw	$vf25,$vf23,$vf25w	# vf25 = vf23 - vf25.w
	vsub.xyz	$vf26,$vf25,$vf24	# vf26.xyz = vf25 - vf24
	# t = da.z / (db.z - da.z)
	vdiv	Q,$vf24z,$vf26z			# Q = vf24.z / vf26.z
	# differences b - a of the four qwords
	vsub.xyzw	$vf27,$vf23,$vf19	# vf27 = vf23 - vf19
	vsub.xyzw	$vf24,$vf20,$vf16	# vf24 = vf20 - vf16
	vsub.xyzw	$vf25,$vf21,$vf17	# vf25 = vf21 - vf17
	vsub.xyzw	$vf26,$vf22,$vf18	# vf26 = vf22 - vf18
	vwaitq					# wait for Q
	vaddq.x	$vf29,$vf0,Q			# vf29.x = Q
	# t = |t|; out = a + (b - a) * t
	vabs.x	$vf29,$vf29			# vf29.x = |vf29.x|
	vmulx.xyzw	$vf27,$vf27,$vf29x	# vf27 = vf27*vf29.x
	vmulx.xyzw	$vf24,$vf24,$vf29x	# vf24 = vf24*vf29.x
	vmulx.xyzw	$vf25,$vf25,$vf29x	# vf25 = vf25*vf29.x
	vmulx.xyzw	$vf26,$vf26,$vf29x	# vf26 = vf26*vf29.x
	vadd.xyzw	$vf27,$vf27,$vf19	# vf27 = vf27 + vf19
	vadd.xyzw	$vf24,$vf24,$vf16	# vf24 = vf24 + vf16
	vadd.xyzw	$vf25,$vf25,$vf17	# vf25 = vf25 + vf17
	vadd.xyzw	$vf26,$vf26,$vf18	# vf26 = vf26 + vf18
	# clip judgement of the new position, stored at +60 and returned
	vclipw.xyz	$vf27,$vf27w		# clip judgement of vf27.xyz against +-vf27.w
	sqc2	$vf24,0($4)
	sqc2	$vf25,32($4)
	sqc2	$vf26,48($4)
	sqc2	$vf27,16($4)
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	jr	$31
	sw	$2,60($4)
	.end	ul_clip_split_z

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Line clipper split on any axis: out = a + (b - a) * t where the edge meets axis = s * w; returns the new vertex's clip flags.
 * Arguments: $4 = output vertex, $5 = a, $6 = b (64-byte clip vertices), $f12 = s (+1 or -1), $7 = axis
 * (0 = x, 1 = y, 2 = z: the distance words are rotated with prot3w until the axis is in x). Called from
 * the line clipper 0x1ba6f8 for the x and y planes.
 */
/* 0x001c1b54 */
	.set	noreorder
	.text
	.globl	ul_clip_split_axis
	.ent	ul_clip_split_axis
ul_clip_split_axis:
	lqc2	$vf19,16($5)
	lqc2	$vf23,16($6)
	mfc1	$8,$f12
	qmtc2	$8,$vf9				# vf9 = $8 (128 bits)
	# vf24 = a - s * a.w, vf25 = b - s * b.w (signed distances)
	vmulx.w	$vf24,$vf19,$vf9x		# vf24.w = vf19.w*vf9.x
	vmulx.w	$vf25,$vf23,$vf9x		# vf25.w = vf23.w*vf9.x
	lqc2	$vf16,0($5)
	lqc2	$vf17,32($5)
	lqc2	$vf18,48($5)
	lqc2	$vf20,0($6)
	lqc2	$vf21,32($6)
	lqc2	$vf22,48($6)
	vsubw.xyzw	$vf24,$vf19,$vf24w	# vf24 = vf19 - vf24.w
	vsubw.xyzw	$vf25,$vf23,$vf25w	# vf25 = vf23 - vf25.w
	qmfc2	$9,$vf24			# $9 = vf24 (128 bits)
	qmfc2	$10,$vf25			# $10 = vf25 (128 bits)
.Lul_clip_split_axis_rotate:
	# rotate the distances $7 times so the chosen axis lands in x
	beqz	$7,.Lul_clip_split_axis_rotated
	nop
	prot3w	$9,$9
	prot3w	$10,$10
	addi	$7,$7,-1
	j	.Lul_clip_split_axis_rotate
	nop
.Lul_clip_split_axis_rotated:
	qmtc2	$9,$vf24			# vf24 = $9 (128 bits)
	qmtc2	$10,$vf25			# vf25 = $10 (128 bits)
	vsub.xyz	$vf26,$vf25,$vf24	# vf26.xyz = vf25 - vf24
	# t = da.x / (db.x - da.x)
	vdiv	Q,$vf24x,$vf26x			# Q = vf24.x / vf26.x
	# differences b - a of the four qwords
	vsub.xyzw	$vf27,$vf23,$vf19	# vf27 = vf23 - vf19
	vsub.xyzw	$vf24,$vf20,$vf16	# vf24 = vf20 - vf16
	vsub.xyzw	$vf25,$vf21,$vf17	# vf25 = vf21 - vf17
	vsub.xyzw	$vf26,$vf22,$vf18	# vf26 = vf22 - vf18
	vwaitq					# wait for Q
	vaddq.x	$vf29,$vf0,Q			# vf29.x = Q
	# t = |t|; out = a + (b - a) * t
	vabs.x	$vf29,$vf29			# vf29.x = |vf29.x|
	vmulx.xyzw	$vf27,$vf27,$vf29x	# vf27 = vf27*vf29.x
	vmulx.xyzw	$vf24,$vf24,$vf29x	# vf24 = vf24*vf29.x
	vmulx.xyzw	$vf25,$vf25,$vf29x	# vf25 = vf25*vf29.x
	vmulx.xyzw	$vf26,$vf26,$vf29x	# vf26 = vf26*vf29.x
	vadd.xyzw	$vf27,$vf27,$vf19	# vf27 = vf27 + vf19
	vadd.xyzw	$vf24,$vf24,$vf16	# vf24 = vf24 + vf16
	vadd.xyzw	$vf25,$vf25,$vf17	# vf25 = vf25 + vf17
	vadd.xyzw	$vf26,$vf26,$vf18	# vf26 = vf26 + vf18
	# clip judgement of the new position, stored at +60 and returned
	vclipw.xyz	$vf27,$vf27w		# clip judgement of vf27.xyz against +-vf27.w
	sqc2	$vf24,0($4)
	sqc2	$vf25,32($4)
	sqc2	$vf26,48($4)
	sqc2	$vf27,16($4)
	vnop
	vnop
	cfc2	$2,$vi18			# $2 = clip flags (6 bits per judgement, newest in bits 0-5)
	jr	$31
	sw	$2,60($4)
	.end	ul_clip_split_axis

/*
 * PS2 platform layer VU0 unit (object at 0x1bf70c-0x1c167c: vector/matrix maths, ul_gx vertex converters). Converts n screen-space vertices to GS ST/RGBAQ/XYZ qwords: position * scale + offset in 12.4, bytes colour, (s, t, 1.0).
 * Arguments: $4 = context (scale +1136, offset +1152), $5 = 24-byte records (x, y, z, s, t, packed RGBA),
 * $6 = count, $7 = output (48 bytes per vertex). Returns the output end pointer. Called from 0x1bec90
 * ("converts them with vu0_gx_sprite_vertices"). Last routine of the object (0x1c167c is padding).
 */
/* 0x001c1c24 */
	.set	noreorder
	.text
	.globl	vu0_gx_sprite_vertices
	.ent	vu0_gx_sprite_vertices
vu0_gx_sprite_vertices:
	lqc2	$vf28,1136($4)
	lqc2	$vf29,1152($4)
	# $12 = 1.0f
	lui	$12,0x3f80
.Lvu0_gx_sprite_vertices_loop:
	lw	$8,0($5)
	lw	$9,4($5)
	lw	$10,8($5)
	# $8 = qword ($8, $9, $10, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$10,$8
	qmtc2	$8,$vf1				# vf1 = $8 (128 bits)
	# vf1.xyz = vf29 + vf1*vf28
	vadda.xyz	ACC,$vf0,$vf29
	vmadd.xyz	$vf1,$vf1,$vf28
	vftoi4.xyzw	$vf1,$vf1		# vf1 = vf1 as 12.4 fixed point
	sqc2	$vf1,32($7)
	lw	$11,20($5)
	# $11 = its four bytes zero-extended to four words
	pextlb	$11,$0,$11
	pextlh	$11,$0,$11
	sq	$11,16($7)
	# ST qword = (s, t, 1.0, -)
	lw	$8,12($5)
	lw	$9,16($5)
	# $8 = qword ($8, $9, $12, -): pack three words
	pextlw	$8,$9,$8
	pcpyld	$8,$12,$8
	sq	$8,0($7)
	addi	$5,$5,24
	addi	$6,$6,-1
	bnez	$6,.Lvu0_gx_sprite_vertices_loop
	addi	$7,$7,48
	jr	$31
	movz	$2,$7,$0
	.end	vu0_gx_sprite_vertices
