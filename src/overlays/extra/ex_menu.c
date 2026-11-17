/*
 * Beyond menu pages: top, mode (direct assault, lost area, trance mission, boss rush) and options
 * (player, immortal, beam, overdrive infinity, camera, default). Locked items show "? ? ?".
 */

#include "extra_shared.h"
#include "types_overlays.h"
#include "billboard.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "float_bits.h"
#include "libc.h"
#include "module.h"
#include "nj_strip.h"
#include "pad.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ex_menu.h"
#include "ex_explain.h"
#include "ex_menu_wire.h"
#include "game.h"

extern PadSystem pad_system;
extern unsigned int pad_decide_mask;

extern Pad *pad_main;

typedef struct ExPad {
    char pad_00[108];
    unsigned int rep;           /* 0x6c */
} ExPad;

extern struct TaskDesc ex_menu_desc;
extern unsigned char *color_scheme_captions[];
extern unsigned char *color_scheme_names[][6];
extern unsigned char *player_select_labels[][8];
extern unsigned char *on_off_labels[][2];
extern unsigned char *beam_type_labels[][8];
extern unsigned char *camera_view_labels[][5];
extern ExPage ex_pages[];
extern void *system_ptm;
extern unsigned char *new_label_texts[];
extern unsigned char *track_label_texts[];
extern unsigned char *lost_area_artist;
extern unsigned char *trance_mission_artist;

/* Pad repeat, kept within 0..max. */
/* 0x00ca0a10 */
void ex_menu_adjust_setting(int sel_id, int max)
{
    ExPad *pad;
    int cur, next;

    cur = ConfigGetSelect(sel_id);
    pad = (ExPad *)pad_main;
    next = cur;
    if (pad->rep & PAD_LEFT)
        next--;
    if (pad->rep & PAD_RIGHT)
        next++;
    if (!(next > -1))
        next = 0;
    if (next < max)
        max = next;
    if (cur != max) {
        SoundRhythmSe_Common(0);
        ConfigSetSelect(sel_id, max);
    }
}

/* 0x00ca0aa0 */
void ex_menu_draw_value_box(ExItem *it, float z)
{
    TrailVtx vtx[4];

    vtx[0].c = vtx[1].c = vtx[2].c = vtx[3].c = RGBA(0x26, 0x19, 0x12, 0x80);
    vtx[0].q = vtx[1].q = vtx[2].q = vtx[3].q = z;
    vtx[1].x = vtx[0].x = it->x0;
    vtx[3].x = vtx[2].x = it->x1;
    vtx[2].y = vtx[0].y = it->y0;
    vtx[3].y = vtx[1].y = it->y1;
    nj_color_strip_begin(0);
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
}

/* 0x00ca0b30 */
void ex_menu_draw_new_mark(ExItem *it, float z)
{
    GameState *gs = &game_state;
    unsigned char *s;
    int col;
    float x, y, half;

    half = 0.5f;
    s = new_label_texts[gs->lang];
    col = gs->menu_color;
    x = floorf((it->bx0 + it->bx1 - line_length((char *)s) * 12) * half);
    y = floorf((it->by0 + it->by1 - 19.0f) * half);
    font_print(x, y, z, col, s);
}

/* 0x00ca0c10 */
void ex_menu_draw_setting(int sel_id, ExItem *it, unsigned char **labels, float z)
{
    unsigned char *s;
    int val, col;
    float x, y, half, inset, ax;

    if (it->draw)
        ex_menu_draw_value_box(it, z);
    z = float_add_bits16(1, z);
    half = 0.5f;
    val = ConfigGetSelect(sel_id);
    inset = 10.0f;
    col = game_menu_color;
    x = floorf((it->x0 + it->x1) * half);
    y = floorf((it->y0 + it->y1 - 19.0f) * half);
    font_set(2);
    s = labels[val];
    font_print(x - floorf(line_length((char *)s) * 12 * half), y, z, col, s);
    ax = it->x0 + inset;
    y = floorf((it->y0 + it->y1) * half);
    ex_menu_draw_sprite(22, ax, y, z);
    ex_menu_draw_sprite(23, it->x1 - inset, y, z);
}

/* For the mode page's "[available/total]" and new mark. */
/* 0x00ca0d90 */
void beyond_mode_count(int *available, int *seen, int *total)
{
    int num_avail, num_seen;

    num_avail = ConfigGetSetting(0);
    num_avail += ConfigGetSetting(1);
    num_avail += ConfigGetSetting(2);
    num_avail += ConfigGetSetting(3);
    *available = num_avail;
    num_seen = ConfigGetSettingSeen(0);
    num_seen += ConfigGetSettingSeen(1);
    num_seen += ConfigGetSettingSeen(2);
    num_seen += ConfigGetSettingSeen(3);
    *seen = num_seen;
    *total = 9;
}

/* 0x00ca0e40 */
void beyond_options_count(int *available, int *seen, int *total)
{
    int num_avail, num_seen;

    num_avail = ConfigGetSetting(4);
    num_avail += ConfigGetSetting(5);
    num_avail += ConfigGetSetting(6);
    num_avail += ConfigGetSetting(7);
    num_avail += ConfigGetSetting(8);
    *available = num_avail;
    num_seen = ConfigGetSettingSeen(4);
    num_seen += ConfigGetSettingSeen(5);
    num_seen += ConfigGetSettingSeen(6);
    num_seen += ConfigGetSettingSeen(7);
    num_seen += ConfigGetSettingSeen(8);
    *seen = num_seen;
    *total = 22;
}

/* 0x00ca0f08 */
void beyond_mode_draw(ExMenu *mn, ExItem *it, float z)
{
    char buf[32];
    int available, seen, total, col;
    float x, y, half;

    col = game_menu_color;
    beyond_mode_count(&available, &seen, &total);
    half = 0.5f;
    sprintf(buf, "[%2d/%2d]", available, total);
    x = floorf((it->x0 + it->x1 - line_length(buf) * 12) * half);
    y = floorf((it->by0 + it->by1 - 19.0f) * half);
    font_set(2);
    font_print(x, y, z, col, (unsigned char *)buf);
}

/* 0x00ca1000 */
void beyond_mode_select(ExMenu *mn)
{
    mn->state = 2;
    mn->result = 1;
    SoundRhythmSe_Common(1);
}

/* 0x00ca1018 */
int beyond_mode_marked(ExMenu *mn)
{
    int cnt[3];

    beyond_mode_count(&cnt[0], &cnt[1], &cnt[2]);
    return cnt[1] < cnt[0];
}

/* 0x00ca1048 */
int beyond_mode_enabled(ExMenu *mn)
{
    int cnt[3];

    beyond_mode_count(&cnt[0], &cnt[1], &cnt[2]);
    return cnt[0] > 0;
}

/* 0x00ca1078 */
void beyond_options_draw(ExMenu *mn, ExItem *it, float z)
{
    char buf[32];
    int available, seen, total, col;
    float x, y, half;

    col = game_menu_color;
    beyond_options_count(&available, &seen, &total);
    half = 0.5f;
    sprintf(buf, "[%2d/%2d]", available, total);
    x = floorf((it->x0 + it->x1 - line_length(buf) * 12) * half);
    y = floorf((it->by0 + it->by1 - 19.0f) * half);
    font_set(2);
    font_print(x, y, z, col, (unsigned char *)buf);
}

/* 0x00ca1170 */
void beyond_options_select(ExMenu *mn)
{
    mn->state = 2;
    mn->result = 2;
    SoundRhythmSe_Common(1);
}

/* 0x00ca1188 */
int beyond_options_marked(ExMenu *mn)
{
    int cnt[3];

    beyond_options_count(&cnt[0], &cnt[1], &cnt[2]);
    return cnt[1] < cnt[0];
}

/* 0x00ca11b8 */
int beyond_options_enabled(ExMenu *mn)
{
    int cnt[3];

    beyond_options_count(&cnt[0], &cnt[1], &cnt[2]);
    return cnt[0] > 0;
}

/* 0x00ca11e8 */
void return_select(ExMenu *mn)
{
    mn->state = 2;
    if (mn->id == 0)
        mn->result = -1;
    else
        mn->result = 0;
    SoundRhythmSe_Common(6);
}

/* 0x00ca1210 */
void direct_assault_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(0) - 1;

    ex_menu_adjust_setting(0, top < 6 ? top : 5);
}

/* 0x00ca1240 */
void direct_assault_draw(ExMenu *mn, ExItem *it, float z)
{
    GameState *gs;
    unsigned char *s;
    int lang, val, col, char_w;
    float x, y, half, inset;

    char_w = 12;
    ex_menu_draw_value_box(it, z);
    half = 0.5f;
    z = float_add_bits16(1, z);
    gs = &game_state;
    lang = gs->lang;
    val = ConfigGetSelect(0);
    col = gs->menu_color;
    x = floorf((it->x0 + it->x1) * half);
    y = it->y0 + 6.0f;
    font_set(2);
    s = color_scheme_captions[lang];
    font_print(x - floorf(line_length((char *)s) * char_w * half), y, z, col, s);
    y += floorf(28.5f);
    s = color_scheme_names[lang][val];
    font_print(x - floorf(line_length((char *)s) * char_w * half), y, z, col, s);
    if (ConfigGetSetting(0) - 1 > 0) {
        inset = 10.0f;
        y += floorf(9.5f);
        ex_menu_draw_sprite(22, it->x0 + inset, y, z);
        ex_menu_draw_sprite(23, it->x1 - inset, y, z);
    }
}

/* 0x00ca1458 */
void direct_assault_select(ExMenu *mn)
{
    ConfigSetExtraType(0);
    mn->result = 0;
    mn->state = 1;
    SoundRhythmSe_Common(1);
    module_set_clear_flag(2, 16);
}

/* 0x00ca14a0 */
int direct_assault_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(0);

    return seen_cnt < ConfigGetSetting(0);
}

/* 0x00ca14d8 */
int direct_assault_enabled(ExMenu *mn)
{
    return ConfigGetSetting(0) > 0;
}

/* 0x00ca14f8 */
void direct_assault_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(0, ConfigGetSetting(0));
}

/* 0x00ca1520 */
void lost_area_draw(ExMenu *mn, ExItem *it, float z)
{
    GameState *gs;
    unsigned char *s;
    int col;
    float x, y, half;

    ex_menu_draw_value_box(it, z);
    half = 0.5f;
    z = float_add_bits16(1, z);
    gs = &game_state;
    col = gs->menu_color;
    x = floorf((it->x0 + it->x1) * half);
    y = it->y0 + 6.0f;
    font_set(1);
    s = track_label_texts[gs->lang];
    font_print(x - floorf(line_length((char *)s) * 10 * half), y, z, col, s);
    y += 14.0f;
    font_set(2);
    s = lost_area_artist;
    font_print(x - floorf(line_length((char *)s) * 12 * half), y, z, col, s);
}

/* 0x00ca1670 */
void lost_area_select(ExMenu *mn)
{
    ConfigSetExtraType(1);
    mn->state = 1;
    mn->result = 8;
    SoundRhythmSe_Common(1);
    module_set_clear_flag(2, 17);
}

/* 0x00ca16b8 */
int lost_area_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(1);

    return seen_cnt < ConfigGetSetting(1);
}

/* 0x00ca16f0 */
int lost_area_enabled(ExMenu *mn)
{
    return ConfigGetSetting(1) > 0;
}

/* 0x00ca1710 */
void lost_area_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(1, ConfigGetSetting(1));
}

/* 0x00ca1738 */
void trance_mission_draw(ExMenu *mn, ExItem *it, float z)
{
    GameState *gs;
    unsigned char *s;
    int col;
    float x, y, half;

    ex_menu_draw_value_box(it, z);
    half = 0.5f;
    z = float_add_bits16(1, z);
    gs = &game_state;
    col = gs->menu_color;
    x = floorf((it->x0 + it->x1) * half);
    y = it->y0 + 6.0f;
    font_set(1);
    s = track_label_texts[gs->lang];
    font_print(x - floorf(line_length((char *)s) * 10 * half), y, z, col, s);
    y += 14.0f;
    font_set(2);
    s = trance_mission_artist;
    font_print(x - floorf(line_length((char *)s) * 12 * half), y, z, col, s);
}

/* 0x00ca1888 */
void trance_mission_select(ExMenu *mn)
{
    ConfigSetExtraType(2);
    mn->state = 1;
    mn->result = 9;
    SoundRhythmSe_Common(1);
    module_set_clear_flag(2, 18);
}

/* 0x00ca18d0 */
int trance_mission_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(2);

    return seen_cnt < ConfigGetSetting(2);
}

/* 0x00ca1908 */
int trance_mission_enabled(ExMenu *mn)
{
    return ConfigGetSetting(2) > 0;
}

/* 0x00ca1928 */
void trance_mission_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(2, ConfigGetSetting(2));
}

/* 0x00ca1950 */
void boss_rush_select(ExMenu *mn)
{
    ConfigSetExtraType(3);
    mn->state = 1;
    mn->result = 7;
    SoundRhythmSe_Common(1);
    module_set_clear_flag(2, 19);
}

/* 0x00ca1998 */
int boss_rush_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(3);

    return seen_cnt < ConfigGetSetting(3);
}

/* 0x00ca19d0 */
int boss_rush_enabled(ExMenu *mn)
{
    return ConfigGetSetting(3) > 0;
}

/* 0x00ca19f0 */
void boss_rush_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(3, ConfigGetSetting(3));
}

/* 0x00ca1a18 */
void player_select_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(4) - 1;

    ex_menu_adjust_setting(4, top < 9 ? top : 8);
}

/* 0x00ca1a48 */
void player_select_draw(ExMenu *mn, ExItem *it, float z)
{
    ex_menu_draw_setting(4, it, player_select_labels[game_lang], z);
}

/* 0x00ca1a68 */
int player_select_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(4);

    return seen_cnt < ConfigGetSetting(4);
}

/* 0x00ca1aa0 */
int player_select_enabled(ExMenu *mn)
{
    return ConfigGetSetting(4) > 0;
}

/* 0x00ca1ac0 */
void player_select_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(4, ConfigGetSetting(4));
}

/* 0x00ca1ae8 */
void immortal_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(5);

    if (top >= 2)
        top = 1;
    ex_menu_adjust_setting(5, top);
}

/* 0x00ca1b18 */
void immortal_draw(ExMenu *mn, ExItem *it, float z)
{
    ex_menu_draw_setting(5, it, on_off_labels[game_lang], z);
}

/* 0x00ca1b38 */
int immortal_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(5);

    return seen_cnt < ConfigGetSetting(5);
}

/* 0x00ca1b70 */
int immortal_enabled(ExMenu *mn)
{
    return ConfigGetSetting(5) > 0;
}

/* 0x00ca1b90 */
void immortal_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(5, ConfigGetSetting(5));
}

/* 0x00ca1bb8 */
void beam_type_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(6) - 1;

    ex_menu_adjust_setting(6, top < 8 ? top : 7);
}

/* 0x00ca1be8 */
void beam_type_draw(ExMenu *mn, ExItem *it, float z)
{
    ex_menu_draw_setting(6, it, beam_type_labels[game_lang], z);
}

/* 0x00ca1c08 */
int beam_type_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(6);

    return seen_cnt < ConfigGetSetting(6);
}

/* 0x00ca1c40 */
int beam_type_enabled(ExMenu *mn)
{
    return ConfigGetSetting(6) > 0;
}

/* 0x00ca1c60 */
void beam_type_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(6, ConfigGetSetting(6));
}

/* 0x00ca1c88 */
void overdrive_infinity_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(7);

    if (top >= 2)
        top = 1;
    ex_menu_adjust_setting(7, top);
}

/* 0x00ca1cb8 */
void overdrive_infinity_draw(ExMenu *mn, ExItem *it, float z)
{
    ex_menu_draw_setting(7, it, on_off_labels[game_lang], z);
}

/* 0x00ca1cd8 */
int overdrive_infinity_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(7);

    return seen_cnt < ConfigGetSetting(7);
}

/* 0x00ca1d10 */
int overdrive_infinity_enabled(ExMenu *mn)
{
    return ConfigGetSetting(7) > 0;
}

/* 0x00ca1d30 */
void overdrive_infinity_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(7, ConfigGetSetting(7));
}

/* 0x00ca1d58 */
void camera_view_adjust(ExMenu *mn, ExItem *it)
{
    int top = ConfigGetSetting(8) - 1;

    ex_menu_adjust_setting(8, top < 5 ? top : 4);
}

/* 0x00ca1d88 */
void camera_view_draw(ExMenu *mn, ExItem *it, float z)
{
    ex_menu_draw_setting(8, it, camera_view_labels[game_lang], z);
}

/* 0x00ca1db0 */
int camera_view_marked(ExMenu *mn)
{
    int seen_cnt = ConfigGetSettingSeen(8);

    return seen_cnt < ConfigGetSetting(8);
}

/* 0x00ca1de8 */
int camera_view_enabled(ExMenu *mn)
{
    return ConfigGetSetting(8) > 0;
}

/* 0x00ca1e08 */
void camera_view_commit(ExMenu *mn)
{
    ConfigSetSettingSeen(8, ConfigGetSetting(8));
}

/* 0x00ca1e30 */
void default_select(ExMenu *mn)
{
    SoundRhythmSe_Common(1);
    ConfigSetSelect(4, 1);
    ConfigSetSelect(5, 0);
    ConfigSetSelect(6, 0);
    ConfigSetSelect(7, 0);
    ConfigSetSelect(8, 0);
}

/* 0x00ca1e88 */
void ex_menu_draw_sprite(int id, float x, float y, float z)
{
    int tex;
    float *uv;
    float width, height, half;
    int col;

    col = game_menu_color;
    sprite_get(id, &tex, &uv, &width, &height);
    half = 0.5f;
    x -= floorf(width * half);
    y -= floorf(height * half);
    ulGxSetTextureList(system_ptm);
    tex_rect_draw_uv(tex, col, uv, x, y, z, width, height);
}

/* 0x00ca1f48 */
void ex_menu_draw_cursor(float x, float y, float z)
{
    int tex;
    float *uv;
    float width, height, half;
    int col;

    col = game_menu_color;
    sprite_get(20, &tex, &uv, &width, &height);
    half = 0.5f;
    x -= floorf(width * half);
    y -= floorf(height * half);
    ulGxSetTextureList(system_ptm);
    tex_rect_draw_uv(tex, col, uv, x, y, z, width, height);
}

/* 0x00ca2008 */
void ex_menu_init(ExMenu *mn, int id, int sel)
{
    mn->id = id;
    mn->state = 0;
    mn->sel = sel;
    mn->page = &ex_pages[id];
    mn->hint = ex_explain_create(mn);
    mn->frame = ex_menu_wire_create(mn, id);
}

/* 0x00ca2070 */
void ex_menu_release(ExMenu *mn)
{
    ExItem *it;
    int n;

    n = mn->page->count;
    it = mn->page->items;
    if (n > 0)
        do {
            if (it->commit)
                it->commit(mn);
            it++;
        } while (--n);
}

/* 0x00ca20d0 */
void ex_menu_exec(ExMenu *mn)
{
    ExPage *pg;
    ExItem *it;
    ExPad *pad;
    int sel, en, n;

    pg = mn->page;
    n = pg->count;
    switch ((unsigned int)mn->state) {
    case 0:
        pad = (ExPad *)pad_system.pad[0];
        sel = mn->sel;
        if (pad->rep & PAD_UP) {
            sel--;
            if (sel < 0)
                sel = n - 1;
        }
        if (pad->rep & PAD_DOWN) {
            sel++;
            if (sel >= n)
                sel = 0;
        }
        if (mn->sel != sel) {
            mn->sel = sel;
            pg->sel = sel;
            SoundRhythmSe_Common(0);
        }
        it = &pg->items[mn->sel];
        en = 1;
        if (it->enabled)
            en = it->enabled(mn);
        if (en) {
            if (it->adjust)
                it->adjust(mn, it);
            ex_explain_set(mn->hint, it->hint, it->adjust != 0, it->accept != 0);
        } else {
            ex_explain_set(mn->hint, -1, 0, 0);
        }
        if (((ExPad *)pad_system.pad[0])->rep & pad_decide_mask) {
            if (en) {
                if (it->accept)
                    it->accept(mn);
            } else {
                SoundRhythmSe_Common(6);
            }
        }
        if (((ExPad *)pad_system.pad[0])->rep & pad_cancel_mask) {
            mn->state = 2;
            if (mn->id == 0)
                mn->result = -1;
            else
                mn->result = 0;
            SoundRhythmSe_Common(6);
        }
        break;
    case 1:
        break;
    case 2:
        ex_menu_wire_close(mn->frame);
        break;
    }
}

/* 0x00ca22d8 */
void ex_menu_disp(ExMenu *mn)
{
    GameState *gs = &game_state;
    ExPage *pg;
    ExItem *it;
    char *s;
    void *fbuf;
    int lang, col, i, en, n;
    float x, y, adv;

    lang = gs->lang;
    pg = mn->page;
    col = gs->menu_color;
    font_set(3);
    x = 33.0f;
    s = (char *)pg->name[lang];
    fbuf = font_begin_current(col);
    adv = 31.0f;
    while (*s) {
        if (*s == ' ') {
            x += adv * 0.8f;
        } else {
            fbuf = font_put_char_scaled(fbuf, x, 24.0f, 2.2222223f, *s, 1.0f);
            x += adv;
        }
        s++;
    }
    font_end_current(fbuf);
    n = pg->count;
    for (i = 0; i < n; i++) {
        it = &pg->items[i];
        en = it->enabled ? it->enabled(mn) : 1;
        x = it->x;
        y = it->y;
        if (i == mn->sel)
            ex_menu_draw_cursor(x - 16.0f, y + 10.0f, 2.2222223f);
        font_set(2);
        if (en) {
            font_print(x, y, 2.2222223f, col, it->name[lang]);
            if (it->draw)
                it->draw(mn, it, 2.2222223f);
        } else {
            font_print(x, y, 2.2222223f, col, (unsigned char *)"? ? ?");
        }
        if (it->marked && it->marked(mn))
            ex_menu_draw_new_mark(it, 2.2222223f);
    }
}

/* 0x00ca2538 */
void *ex_menu_create(void *parent, int id, int sel)
{
    ExMenu *mn;

    mn = task_create_inherit_arg(parent, &ex_menu_desc, 36, id);
    if (mn)
        ex_menu_init(mn, id, sel);
    return mn;
}

/* 0x00ca25a0 */
int ex_menu_get_result(ExMenu *mn, int *res)
{
    int st = mn->state;

    *res = mn->result;
    return st;
}
