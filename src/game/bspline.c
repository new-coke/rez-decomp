/* B-spline curves through 3D control points: clamped knots, Cox-de Boor and unrolled bases. */

#include "types_game.h"
#include "bspline.h"
#include "yuki_tool.h"

/* Recursive Cox-de Boor N(k, i, t). */
/* 0x00103a40 */
static float bspline_basis(int k, int i, float t, float *kv)
{
    float left;
    float right;

    if (k < 2) {
        if (kv[i] <= t && t < kv[i + 1])
            return 1.0f;
        return 0.0f;
    }
    left = right = 0;
    if (kv[i + k - 1] != kv[i])
        left = (t - kv[i]) * bspline_basis(k - 1, i, t, kv) / (kv[i + k - 1] - kv[i]);
    if (kv[i + k] != kv[i + 1])
        right = (kv[i + k] - t) * bspline_basis(k - 1, i + 1, t, kv) / (kv[i + k] - kv[i + 1]);
    return left + right;
}

/* Hat basis over three knots. */
/* 0x00103ba0 */
static float bspline_basis_linear(float *knots, float t)
{
    float left;
    float right;
    float knot0;
    float knot1;
    float knot2;

    knot0 = knots[0];
    knot1 = knots[1];
    knot2 = knots[2];
    if (knot1 != knot0)
        left = (t - knot0) * (float)(knot0 <= t && t < knot1) / (knot1 - knot0);
    else
        left = 0;
    if (knot2 != knot1)
        right = (knot2 - t) * (float)(knot1 <= t && t < knot2) / (knot2 - knot1);
    else
        right = 0;
    return left + right;
}

/* Four knots, from two linear bases. */
/* 0x00103c60 */
static float bspline_basis_quadratic(float *knots, float t)
{
    float left;
    float right;
    float knot0 = knots[0];
    float knot2 = knots[2];
    float knot1 = knots[1];
    float knot3 = knots[3];

    if (knot2 != knot0)
        left = (t - knot0) * bspline_basis_linear(knots, t) / (knot2 - knot0);
    else
        left = 0;
    if (knot3 != knot1)
        right = (knot3 - t) * bspline_basis_linear(knots + 1, t) / (knot3 - knot1);
    else
        right = 0;
    return left + right;
}

/* Linear bases inlined. Second term added only when knots differ, so -0.0 stays -0.0. */
/* 0x00103d38 */
static float bspline_basis_quadratic_inline(float *knots, float t)
{
    float left;
    float basis_left;
    float basis_right;
    float knot0 = knots[0];
    float knot1 = knots[1];
    float knot2 = knots[2];
    float knot3 = knots[3];
    float first_knot2;
    float second_knot1;
    float second_knot2;
    float second_knot3;

    if (knot2 != knot0) {
        do { } while (0);
        first_knot2 = knots[2];
        do {
            if (knot1 != knot0)
                basis_left = (t - knot0) * (float)(knot0 <= t && t < knot1) / (knot1 - knot0);
            else
                basis_left = 0;
            if (first_knot2 != knot1)
                basis_left += (first_knot2 - t) * (float)(knot1 <= t && t < first_knot2) / (first_knot2 - knot1);
        } while (0);
        left = (t - knot0) * basis_left / (first_knot2 - knot0);
    } else {
        left = 0;
    }
    second_knot1 = knots[1];
    second_knot2 = knots[2];
    second_knot3 = knots[3];
    if (knot3 != knot1) {
        do {
            if (second_knot2 != knot1)
                basis_right = (t - second_knot1) * (float)(second_knot1 <= t && t < knot2) / (knot2 - second_knot1);
            else
                basis_right = 0;
            if (second_knot3 != knot2)
                basis_right += (second_knot3 - t) * (float)(second_knot2 <= t && t < knot3) / (second_knot3 - knot2);
        } while (0);
        left += (knot3 - t) * basis_right / (knot3 - knot1);
    }
    return left;
}

/* Five knots, from two quadratic bases. */
/* 0x00103f08 */
static float bspline_basis_cubic(float *knots, float t)
{
    float left;
    float right;
    float knot0 = knots[0];
    float knot3 = knots[3];
    float knot1 = knots[1];
    float knot4 = knots[4];

    if (knot3 != knot0)
        left = (t - knot0) * bspline_basis_quadratic_inline(knots, t) / (knot3 - knot0);
    else
        left = 0;
    if (knot4 != knot1) {
        right = (knot4 - t) * bspline_basis_quadratic_inline(knots + 1, t) / (knot4 - knot1);
        left = left + right;
    }
    return left;
}

/* Temporary knot vector as bspline_make_knots. */
/* 0x00103fd8 */
static void bspline_eval_alloc(float t, Vec3 *out, Vec3 *pts, int n, int k)
{
    int order = k + 1;
    float *kv;
    int idx = 0;
    int i;
    float weight;

    kv = yuki_alloc((n + order) * 4);
    for (i = 0; i < order; i++)
        kv[i] = 0;
    for (i = 0; i < n - order; i++)
        kv[i + order] = (float)i + (float)order * 0.5f;
    for (i = 0; i < order; i++)
        kv[i + n] = (float)(n - 1);
    t = t * (float)(n - 1);
    for (i = 0; i < n; i++) {
        if (kv[i] <= t && t < kv[i + 1])
            idx = i;
    }
    if (kv[n - 1] <= t) {
        if (!((double)t > (double)kv[n] + 1e-7))
            idx = n - 1;
    }
    out->x = 0;
    out->y = 0;
    out->z = 0;
    for (i = idx - k; i <= idx; i++) {
        weight = bspline_basis(order, i, t, kv);
        out->x += pts[i].x * weight;
        out->y += pts[i].y * weight;
        out->z += pts[i].z * weight;
    }
    do {
        yuki_free(kv);
    } while (0);
}

/* Clamped uniform. */
/* 0x00104268 */
void bspline_make_knots(int n, int k, float **out)
{
    int order = k + 1;
    float *kv;
    int i;

    kv = yuki_alloc((n + order) * 4);
    *out = kv;
    for (i = 0; i < order; i++)
        kv[i] = 0;
    for (i = 0; i < n - order; i++)
        kv[i + order] = (float)i + (float)order * 0.5f;
    for (i = 0; i < order; i++)
        kv[i + n] = (float)(n - 1);
}

/* Degree k, t in 0..1. */
/* 0x00104368 */
void bspline_eval(int k, int n, Vec3 *pts, float *kv, Vec3 *out, float t)
{
    int idx = 0;
    int i;
    int order = k + 1;
    float weight;
    int start;
    Vec3 *p;

    t = t * (float)(n - 1);
    for (i = 0; i < n; i++) {
        if (kv[i] <= t && t < kv[i + 1])
            idx = i;
    }
    if (kv[n - 1] <= t) {
        if (!((double)t > (double)kv[n] + 1e-7))
            idx = n - 1;
    }
    start = idx - k;
    p = &pts[start];
    out->x = 0;
    out->y = 0;
    out->z = 0;
    for (i = start; i <= idx; i++, p++) {
        weight = bspline_basis(order, i, t, kv);
        out->x += p->x * weight;
        out->y += p->y * weight;
        out->z += p->z * weight;
    }
}

/* t at the very end (within epsilon) falls in the last span. */
/* 0x00104528 */
static int bspline_find_span(float t, int k, int n, float *kv)
{
    int idx = 0;
    int i;

    t = t * (float)(n - 1);
    for (i = 0; i < n; i++) {
        if (kv[i] <= t && t < kv[i + 1])
            idx = i;
    }
    if (kv[n - 1] <= t) {
        if (!((double)t > (double)kv[n] + 1e-7))
            idx = n - 1;
    }
    return idx;
}

/* 0x00104618 */
static void bspline_eval_span(float t, int k, int hi, int n, Vec3 *pts, float *kv, Vec3 *out)
{
    int i;
    int order;
    float weight;

    order = k + 1;
    out->x = 0;
    out->y = 0;
    out->z = 0;
    for (i = hi - k; i <= hi; i++) {
        weight = bspline_basis(order, i, t, kv);
        out->x += pts[i].x * weight;
        out->y += pts[i].y * weight;
        out->z += pts[i].z * weight;
    }
}

/* Searches the span from the end. */
/* 0x00104700 */
static void bspline_eval_cubic(float t, int n, Vec3 *pts, float *kv, Vec3 *out)
{
    int idx = 0;
    int i;
    float first_weight;
    float x;
    float y;
    float z;
    Vec3 *p;
    float *knot;

    t = t * (float)(n - 1);
    knot = kv + n - 1;
    if (*knot <= t && !((double)t > (double)knot[1] + 1e-7)) {
        idx = n - 1;
    } else {
        int j;

        for (j = n - 1; j >= 0; j--, knot--) {
            if (*knot <= t && t < knot[1]) {
                idx = j;
                break;
            }
        }
    }
    p = &pts[idx - 3];
    first_weight = bspline_basis_cubic(kv + idx - 3, t);
    x = p->x * first_weight;
    y = p->y * first_weight;
    z = p->z * first_weight;
    p++;
    for (i = idx - 3 + 1; i <= idx; i++) {
        float weight = bspline_basis_cubic(&kv[i], t);

        x += p->x * weight;
        y += p->y * weight;
        z += p->z * weight;
        p++;
    }
    out->x = x;
    out->y = y;
    out->z = z;
}

/* 0x00104900 */
static void bspline_eval_quadratic(float t, int n, Vec3 *pts, float *kv, Vec3 *out)
{
    int idx = 0;
    int i;
    float first_weight;
    float x;
    float y;
    float z;
    Vec3 *p;
    float *knot;

    t = t * (float)(n - 1);
    knot = kv + n - 1;
    if (*knot <= t && !((double)t > (double)knot[1] + 1e-7)) {
        idx = n - 1;
    } else {
        int j;

        for (j = n - 1; j >= 0; j--, knot--) {
            if (*knot <= t && t < knot[1]) {
                idx = j;
                break;
            }
        }
    }
    p = &pts[idx - 2];
    first_weight = bspline_basis_quadratic(kv + idx - 2, t);
    x = p->x * first_weight;
    y = p->y * first_weight;
    z = p->z * first_weight;
    p++;
    for (i = idx - 2 + 1; i <= idx; i++) {
        float weight = bspline_basis_quadratic(&kv[i], t);

        x += p->x * weight;
        y += p->y * weight;
        z += p->z * weight;
        p++;
    }
    out->x = x;
    out->y = y;
    out->z = z;
}
