/*
 * Ninja lights: eight slots (ambient, directional, point); upload turns ambient plus up to four
 * directional lights into VU1 colour and direction matrices. "Easy" names are the easy-light API.
 */

#include "ninja.h"
#include "libc.h"
#include "nj_light.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_vu0.h"

static void njCnkSetSimpleMultiLightVectorEx(int i, float x, float y, float z);

extern void vu0_matrix_zero(NjMatrix *m);
extern char nj_light_dummy_work[];

/* Slots 0..n, enabled. */
/* 0x001a2e68 */
void nj_lights_clear(int n)
{
    int i;
    NjLight *light;

    for (i = 0; i <= n; i++) {
        light = &nj_lights[i];
        memset(light, 0, 64);
        light->enable = 1;
    }
}

/* 0x001a2ee0 */
void nj_light_enable(int i, int on)
{
    NjLight *light = &nj_lights[i];

    light->enable = on;
}

/* Light 0, colour (x, y, z). */
/* 0x001a2ef8 */
void nj_light_set_ambient(float x, float y, float z)
{
    nj_lights[0].type = 3;
    nj_lights[0].r = x;
    nj_lights[0].g = y;
    nj_lights[0].b = z;
    nj_lights[0].a = 1.0f;
}

/* 0x001a2f28 */
void nj_light_set_color(int i, float x, float y, float z)
{
    NjLight *light = &nj_lights[i];

    light->r = x;
    light->g = y;
    light->b = z;
    light->a = 1.0f;
}

/* Light 1, directional. */
/* 0x001a2f58 */
void nj_light1_set_vector(float x, float y, float z)
{
    njCnkSetSimpleMultiLightVectorEx(1, x, y, z);
}

/* Directional, normalised. Bug: a zero vector warns but is still divided by. */
/* 0x001a2f60 */
static void njCnkSetSimpleMultiLightVectorEx(int i, float x, float y, float z)
{
    NjLight *light = &nj_lights[i];
    float len, inv;

    light->type = 2;
    len = sqrtf(x * x + y * y + z * z);
    if (len < 0.001f * 0.001f)
        printf("njCnkSetSimpleMultiLightVectorEx: direction is zero vector.\n");
    inv = 1.0f / len;
    light->dx = x * inv;
    light->dy = y * inv;
    light->dz = z * inv;
}

/* 0x001a3040 */
void nj_light_set_point(int i, float x, float y, float z)
{
    NjLight *light = &nj_lights[i];

    light->type = 1;
    light->px = x;
    light->py = y;
    light->pz = z;
}

/* Point light range. */
/* 0x001a3068 */
void nj_light_set_range(int i, float near_range, float far_range)
{
    NjLight *light = &nj_lights[i];

    light->type = 1;
    light->range_a = near_range;
    light->range_b = far_range;
}

/* Ambient plus colours and negated directions of the first four directional lights, to VU1. */
/* 0x001a3090 */
void nj_lights_upload(void)
{
    NjSys *sys = &ulGxState;
    NjMatrix col;
    NjMatrix dir;
    NjLight *light;
    int i, n;
    void *dlist, *pkt;

    vu0_matrix_zero(&col);
    vu0_matrix_zero(&dir);
    n = 0;
    light = nj_lights;
    for (i = 0; i < 8; i++) {
        if (!light->enable)
            continue;
        switch (light->type) {
        case 1:
            break;
        case 3:
            sys->ambient_col = *(NjColor *)&light->r;
            break;
        case 2:
            if (n < 4) {
                do {
                    col.m[n * 4 + 0] = light->r;
                } while (0);
                do {
                    col.m[n * 4 + 1] = light->g;
                } while (0);
                col.m[n * 4 + 2] = light->b;
                do {
                    *(dir.m + n) = -light->dx;
                } while (0);
                dir.m[4 + n] = -light->dy;
                do {
                    dir.m[8 + n] = -light->dz;
                } while (0);
                n++;
            }
            break;
        }
        light++;
    }
    vu0_matrix_copy(&sys->light_dir_mtx, &dir);
    vu0_matrix_copy(&sys->light_col_copy, &col);
    vu0_matrix_copy(&sys->light_col_mtx, &col);
    do { } while (0);
    dlist = psGsGetDisplayList();
    pkt = psDisplayListOpenRefPkt(dlist);
    psVifPktAddWord(pkt, 0x01000101);
    psVifPktAddWord(pkt, 0x6c01003c);
    psVifPktAddWords(pkt, &sys->ambient_col, 4);
    psVifPktAddWord(pkt, 0x6c04001c);
    psVifPktAddWords(pkt, &sys->light_col_mtx, 16);
    psDisplayListCloseRefPkt(dlist);
}

/* 0x001a3250 */
void nj_easy_lights_clear(int n)
{
    nj_lights_clear(n);
}

/* 0x001a3258 */
void nj_easy_light_enable(int i, int on)
{
    nj_light_enable(i, on);
}

/* 0x001a3260 */
static void nj_easy_light_set_ambient(float x, float y, float z)
{
    nj_light_set_ambient(x, y, z);
}

/* 0x001a3268 */
void nj_easy_light_set_color(int i, float x, float y, float z)
{
    nj_light_set_color(i, x, y, z);
}

/* 0x001a3270 */
static void nj_easy_light1_set_vector(float x, float y, float z)
{
    nj_light1_set_vector(x, y, z);
}

/* 0x001a3278 */
static void nj_easy_light_set_vector(int i, float x, float y, float z)
{
    njCnkSetSimpleMultiLightVectorEx(i, x, y, z);
}

/* 0x001a3280 */
void nj_easy_light_set_point(int i, float x, float y, float z)
{
    nj_light_set_point(i, x, y, z);
}

/* 0x001a3288 */
void nj_easy_light_set_range(int i, float near_range, float far_range)
{
    nj_light_set_range(i, near_range, far_range);
}

/* 0x001a3290 */
static void nj_easy_lights_upload(void)
{
    nj_lights_upload();
}

/* 0x001a3298 */
static void nj_light_nop(void)
{
}

/* 0x001a32a0 */
void nj_light_exit(void)
{
}

/* 0x001a32a8 */
static void *nj_light_get_dummy_work(void)
{
    return nj_light_dummy_work;
}

/* 0x001a32b8 */
static int nj_light_return_one(void)
{
    return 1;
}

/* 0x001a32c0 */
static int nj_light_return_one_2(void)
{
    return 1;
}

/* 0x001a32c8 */
static int nj_light_return_one_3(void)
{
    return 1;
}

/* 0x001a32d0 */
static void nj_light_nop_2(void)
{
}

/* 0x001a32d8 */
static void nj_light_nop_3(void)
{
}

/* 0x001a32e0 */
static int nj_light_return_one_4(void)
{
    return 1;
}

/* 0x001a32e8 */
static int nj_light_return_one_5(void)
{
    return 1;
}

/* 0x001a32f0 */
static int nj_light_return_one_6(void)
{
    return 1;
}

/* 0x001a32f8 */
static void nj_light_nop_4(void)
{
}
