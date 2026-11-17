/* Ninja render state: fog, blending, texture filter/clamp/shading mapped onto the renderer. */

#include "nj_state.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "nj_cnk_vu1.h"

extern int nj_alpha_blend_mode;
extern int nj_blend_src_table[];
extern int nj_blend_dst_table[];

/* 0x001a26a8 */
static void njFogEnable(void)
{
    ulGxEnableFog(1);
}

/* 0x001a26b0 */
static void njFogDisable(void)
{
    ulGxEnableFog(0);
}

/* col is 0xRRGGBB; the GS wants red in the low byte. */
/* 0x001a26b8 */
static void njSetFogColor(unsigned int col)
{
    psGsWriteReg(61, (unsigned long)((col >> 16) & 0xff)
                 | ((unsigned long)((col >> 8) & 0xff) << 8)
                 | ((unsigned long)(col & 0xff) << 16));
}

/* 0x001a26e8 */
void nj_state_set0(void)
{
}

/* 0x001a26f0 */
void nj_state_set1(void)
{
}

/* 0x001a26f8 */
void nj_state_set2(void)
{
}

/* 0x001a2700 */
void nj_state_set3(void)
{
}

/* 0x001a2708 */
void nj_state_set4(void)
{
}

/* 0x001a2710 */
void nj_state_set5(void)
{
}

/* 0x001a2718 */
void nj_state_set6(void)
{
}

/* 0x001a2720 */
void nj_state_set7(void)
{
}

/* 0x001a2728 */
void nj_texture_flip_mode(void)
{
}

/* 0x001a2730 */
void nj_state_set_colors(void)
{
}

/* blend 1, 0: fixed modes; others replace the source (dst 0) or destination (dst 1) factor. */
/* 0x001a2738 */
static void njColorBlendingMode(int dst, int blend)
{
    unsigned long alpha;

    if (blend == 1)
        nj_alpha_blend_mode = 0x2400;
    else if (blend == 0)
        nj_alpha_blend_mode = 0x2d00;
    else {
        if (dst == 0)
            nj_alpha_blend_mode = (nj_alpha_blend_mode & ~0x3800) | nj_blend_src_table[blend];
        else
            nj_alpha_blend_mode = (nj_alpha_blend_mode & ~0x700) | nj_blend_dst_table[blend];
    }
    alpha = ulGxAlphaTable[nj_alpha_blend_mode >> 8];
    psDisplayListUpdateGsReg(psGsGetDisplayList(), 66, alpha);
}

/* Renderer filter 0, 1 or 2. */
/* 0x001a27e0 */
static void njTextureFilterMode(int mode)
{
    int filter;

    switch (mode) {
    case 0:
        filter = 0;
        break;
    case 0x2000:
        filter = 1;
        break;
    default:
        filter = 1;
        break;
    case 0x4000:
    case 0x6000:
        filter = 2;
        break;
    }
    ulGxSetTexFilter(filter);
}

/* Renderer u and v clamp flags. */
/* 0x001a2838 */
void njTextureClampMode(int mode)
{
    int clamp_u, clamp_v;

    switch (mode) {
    case 0:
        clamp_u = 0;
        clamp_v = 0;
        break;
    case 0x8000:
        clamp_u = 0;
        clamp_v = 1;
        break;
    case 0x10000:
        clamp_u = 1;
        clamp_v = 0;
        break;
    default:
        clamp_u = 1;
        clamp_v = 1;
        break;
    }
    ulGxSetTexClamp(clamp_u, clamp_v);
}

/* Renderer texture function. */
/* 0x001a2898 */
void njTextureShadingMode(int mode)
{
    int func;

    switch (mode) {
    case 0:
        func = 1;
        break;
    case 64:
        func = 0;
        break;
    case 128:
        func = 2;
        break;
    case 192:
        func = 0;
        break;
    default:
        func = 0;
        break;
    }
    ulGxSetTexFunc(func);
}
