/*
 * System: boot setup (state, start flags, vsync handler); root tasks "System1" (idle timer, screen
 * saver, debug keys, pause and frame step) and "System2"; play time; "System" window; variable
 * editor save/load; system textures, models, font (2D and 3D text) and sprites.
 */

#include "types_game.h"
#include "color.h"
#include "task.h"
#include "sdk_compat.h"
#include "system.h"
#include "debug.h"
#include "libc.h"
#include "gw_console.h"
#include "gw_desktop.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_varedit_win.h"
#include "gw_window.h"
#include "libgraph.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "backup.h"
#include "billboard.h"
#include "bs_glyph.h"
#include "callback.h"
#include "camera.h"
#include "command.h"
#include "game.h"
#include "game_flow_editor.h"
#include "heap.h"
#include "item.h"
#include "model.h"
#include "pad.h"
#include "pad_vib.h"
#include "perform.h"
#include "player_status.h"
#include "pnb.h"
#include "ptcl.h"
#include "ptcl_edit.h"
#include "replay.h"
#include "screen.h"
#include "screen_lens.h"
#include "sound_main.h"
#include "config.h"
#include "main.h"
#include "debugmenu.h"

typedef struct TimeAcc {
    int total[3];               /* 0x00 */
    int cur[3];                 /* 0x0c */
} TimeAcc;

/* start flags word */
typedef enum { SYS_START_FLAGS = 0x22100 } SysStartFlags;

typedef struct BootInfo {
    unsigned char id[6];        /* 0x00 */
    char pad_06[2];
    unsigned int lang;          /* 0x08 */
} BootInfo;

typedef struct Kbd {
    char pad_00[5];
    unsigned char key;          /* 0x05 */
    unsigned char mod;          /* 0x06 */
} Kbd;

typedef struct Input {
    char pad_00[48];
    Pad *cur_pad;                   /* 0x30 */
} Input;

/* "System1" work (0x0c bytes). */
typedef struct Sys1Work {
    int demo;                   /* 0x00: task work created by scr_saver_create */
    int timer;                  /* 0x04: idle frames left */
    int frames;                 /* 0x08 */
} Sys1Work;

typedef struct SystemPad {
    char pad_00[104];
    int any;                    /* 0x68 */
} SystemPad;

typedef struct SystemTimeAcc {
    Time3 total;                /* 0x00 */
    Time3 cur;                  /* 0x0c */
} SystemTimeAcc;

#define FRAMES_SPLIT(frames, min, sec, s, cs) \
    do { \
        (min) = (frames) / 3600; \
        (sec) = (frames) / 60; \
        (s) = (sec) % 60; \
        (cs) = (frames) % 60 % 100; \
    } while (0)

#ifdef VERSION_US

/* Source line for an allocation tag. */
#define SRC_LINE(early, us) (us)

#else

#define SRC_LINE(early, us) (early)

#endif /* !VERSION_US */

typedef struct TexFile {
    int type;                   /* 1 */
    char *name;
} TexFile;

typedef enum FontId {
    FONT_0,
    FONT_1,
    FONT_2,
    FONT_3
} FontId;

extern TimeAcc system_time;
extern SysState sys_state;
extern unsigned int system_flags;
extern char *bits_to_string(unsigned int bits, int n, const char *on, const char *off);
extern int system_debug_drive;

extern int system_fps;
extern int scr_root;
extern int system_vars_group;
extern struct GwVarNode *gw_varedit_root;
extern int system_vardef;
extern int system_vardefs;
extern int system_flag_vardef;
extern int system_flag_vardefs;
extern int system_func_vardefs;
extern void soft_reset(const char *name);

extern BootInfo boot_info;

extern int debug_log_saving;

extern Kbd kbd_state;
extern Input pad_system;
extern unsigned int stage_exit_flags;
/* idle time in frames */
extern int system_idle_time;

extern SystemPad *pad_main;

extern int system1_root;
extern int system2_root;
/* "System1" */
extern TaskDesc system1_desc;

extern TaskDesc system2_desc;

extern volatile int frame_ready;

extern int debug_disp_level;

extern FlagName system_flag_names[];
extern GameState game_state;

extern void *system_debug_win;
extern DebugMenu screen_layout;

extern char str_varedit_file[];

extern void *system1_work;
extern TaskDesc varedit_save_desc;
extern TaskDesc varedit_load_desc;
extern int model_test_on;
extern TexFile system_tex_files[];
/* item.ptm */
extern void *item_textures;

/* system.pnb */
extern void *system_pnb;

/* effect.ptm */
extern void *lens_flare_tex;

/* item.pnb */
extern void *item_models;

/* system.ptm */
extern void *system_ptm;

/* particle.ptm */
extern void *ptcl_texture;

extern int system_pnb_ids[4];

extern int system_pnb_model_id[1];
extern int system_pnb_entries[];
extern void ulGxBeginSprite(int a);
extern void ulGxEndSprite(void *buf);
extern void ulGxAddSprites(FontVtx *v, int n);
extern float font0_cols[][2];
extern float font0_rows[][2];
extern float font1_cols[][2];
extern float font1_rows[][2];
extern float font2_cols[][2];
extern float font2_rows[][2];
extern float font3_cols[][2];
extern float font3_rows[][2];

extern void ulGxAddTexStrip(FontVtx *v, int n);
extern Vec3 font_corners[][4];
extern void ulGxBeginTexStrip(int a);
extern void ulMatrixPush(void);
extern void ulGxEndTexStrip(void);
extern float font_glyph_widths[];
extern SpriteUV sprite_uv_table[];
extern SpriteDef sprite_defs[];
extern int sprite_lang_tex[];

/* Video output, console id, language and sound, start flags; resets the play time clock. */
/* 0x0016d068 */
void system_info_print(void)
{
    TimeAcc *tm = &system_time;
    SysState *sys;

    DEBUG_PRINT("------------------------------\n");
    sys = &sys_state;
    system_print_video();
    system_print_bootrom();
    sys->unk_08 = 3;
    tm->total[0] = 0;
    tm->total[1] = 0;
    tm->total[2] = 0;
    tm->cur[0] = 0;
    tm->cur[1] = 0;
    tm->cur[2] = 0;
    DEBUG_PRINTF("StartFlag = %s\n", bits_to_string(system_flags, 16, "1111111111111111", "_..._..._..._..."));
    DEBUG_PRINT("------------------------------\n");
}

/* State, start flags, play time clock, vsync handler. */
/* 0x0016d128 */
void system_init(void)
{
    SysState *sys = &sys_state;
    TimeAcc *tm = &system_time;

    sys->unk_04 = 0;
    (*(SysStartFlags *)&system_flags) = 0x22100; /* steer(cast) */
    system_state_clear();
    tm->total[0] = 0;
    tm->total[1] = 0;
    tm->total[2] = 0;
    tm->cur[0] = 0;
    tm->cur[1] = 0;
    tm->cur[2] = 0;
    sys->unk_28 = 6;
    sys->unk_08 = 0;
    sceGsSyncVCallback(vsync_handler);
    sys->unk_2c = 3;
}

/* 0x0016d1b0 */
void vsync_handler_install(void)
{
    sceGsSyncVCallback(vsync_handler);
}

/*
 * "System" soft reset; textures, item parameters, glyphs; "System1"/"System2"; particle data and
 * editor; variables, "System" window; banner; KPRO menu on desktop left click; vibration pack.
 */
/* 0x0016d1c0 */
void system_start(void)
{
    int group;

    soft_reset("System");
    system_flags &= SYS_TITLE_LOOP | SYS_WARNING_PENDING | SYS_NO_PAUSE | SYS_HEAP_FILL
                    | SYS_HEAP_CHECK;
    system_load_textures();
    item_params_register();
    glyph_init();
    debug_off(system_debug_drive);
    system_tasks_create();
    frame_rate = 2;
    system_fps = 0;
    ptcl_data_load_default();
    ptcl_edit_menu_register(scr_root);
    group = gw_varedit_add(&gw_varedit_root, 0, &system_vardef);
    system_vars_group = group;
    gw_varedit_add_table(&gw_varedit_root, group, &system_vardefs);
    group = gw_varedit_add(&gw_varedit_root, system_vars_group, &system_flag_vardef);
    gw_varedit_add_table(&gw_varedit_root, group, &system_flag_vardefs);
    gw_varedit_add_table(&gw_varedit_root, 0, &system_func_vardefs);
    menu_item_add(info_menu_name, "System", system_debug_open, 0, 0);
    system_info_print();
    gw_desktop_set_left_click(system_kpro_menu_toggle);
    peri_act_pak_open();
}

/* 0x0016d300 */
void peri_act_pak_shutdown(void)
{
    peri_act_pak_close();
}

/* Video mode, broadcast standard, cable type; the first line is also labelled "Cable". */
/* 0x0016d308 */
void system_print_video(void)
{
    char *video;
    char *bcast;
    char *cable;

    video = str_empty;
    switch ((*(unsigned int *)&sys_state)) { /* steer(cast) */
    case 1:
        video = "VGA";
        break;
    case 0:
        video = "NTSC";
        break;
    case 2:
        video = "PAL";
        break;
    }
    DEBUG_PRINTF("Cable....... %s\n", video);
    bcast = str_empty;
    switch (nj_video_get_broadcast()) {
    case 0:
        bcast = "NTSC Japan/USA";
        break;
    case 1:
        bcast = "PAL Europe";
        break;
    case 2:
        bcast = "PAL-M Brazil";
        break;
    case 3:
        bcast = "PAL-N Argentina";
        break;
    }
    DEBUG_PRINTF("Broadcast... %s\n", bcast);
    cable = str_empty;
    switch (nj_video_get_cable()) {
    case 0:
        cable = "VGA";
        break;
    case 2:
        cable = "RGB cable(Multi21P)";
        break;
    case 3:
        cable = "VBS/S, RF";
        break;
    }
    DEBUG_PRINTF("Cable....... %s\n", cable);
}

/* Video output mode 0 (NTSC). */
/* 0x0016d488 */
void system_state_clear(void)
{
    (*(int *)&sys_state) = 0; /* steer(cast) */
}

/* Console id, system language and sound settings. */
/* 0x0016d498 */
void system_print_bootrom(void)
{
    BootInfo *boot = &boot_info;
    char *str = str_empty;

    DEBUG_PRINTF("IndividualID : %02x %02x %02x %02x %02x %02x\n",
                 boot->id[0], boot->id[1], boot->id[2], boot->id[3], boot->id[4], boot->id[5]);
    switch (boot->lang) {
    case 0:
        str = "Japanese";
        break;
    case 1:
        str = "English";
        break;
    case 2:
        str = "German";
        break;
    case 3:
        str = "French";
        break;
    case 4:
        str = "Spanish";
        break;
    case 5:
        str = "Italian";
        break;
    }
    DEBUG_PRINTF("[BootRom]Language : %s\n", str);
    if (config_stereo)
        str = "STEREO";
    else
        str = "MONAURAL";
    DEBUG_PRINTF("[BootRom]Sound : %s\n", str);
}

/* Home key (code 74) starts saving "KPRJ____.DBG" when the request is free; done once idle. */
/* 0x0016d598 */
void debug_log_save(void)
{
    int *busy = &debug_log_saving;

    if (*busy) {
        if (filereq_is_idle()) {
            filereq_clear();
            *busy = 0;
        }
    } else if (kbd_key == 74) {
        if (filereq_is_free()) {
            filereq_set(2, auto_load_drive, "KPRJ____.DBG", busy, 0, 0);
            BackupSaveTaskSet();
            *busy = 1;
        }
    }
}

/*
 * Debug mode only. F1..F9 open tools, ` the console; F12, Print Screen, Pause: flag 0x4000,
 * display level, debug pause; Insert: overrun check; modifier + 1..5: frame rate.
 */
/* steer(cast) */
/* 0x0016d640 */
void debug_keys(void)
{
    int key = kbd_state.key;
    unsigned int flags;

    switch (key) {
    case 58:
        perform_window_toggle();
        break;
    case 59:
        heap_menu_toggle();
        break;
    case 60:
        game_flow_editor_toggle(0);
        break;
    case 61:
        game_info_window_toggle();
        break;
    case 62:
        camera_window_toggle();
        break;
    case 63:
        if (system_flags & SYS_STABILIZER) {
            system_flags &= ~SYS_STABILIZER;
            debug_message_window(1, 1, 60, RGBA(0x14, 0x40, 0x14, 0x80), "Frame Stabilizer Disable");
        } else {
            system_flags |= SYS_STABILIZER;
            debug_message_window(1, 1, 60, RGBA(0x14, 0x40, 0x14, 0x80), "Frame Stabilizer Enable");
        }
        break;
    case 64:
        gw_varedit_win_set(!gw_varedit_win_is_open());
        break;
    case 65:
        camera_free_toggle();
        break;
    case 66:
        system_debug_open();
        break;
    case 69:
        system_flags ^= SYS_DEBUG_INPUT;
        break;
    case 53:
        gw_console_set_visible(!gw_console_is_visible());
        break;
    case 70:
        if (++(*(unsigned int *)&debug_disp_level) >= 3) /* steer(cast) */
            (*(unsigned int *)&debug_disp_level) = 0; /* steer(cast) */
        break;
    case 72:
        system_flags ^= SYS_PAUSE_KBD;
        break;
    case 139:
        ((void (*)())system_kpro_menu_toggle)(0);
        break;
    case 73:
        system_flags ^= SYS_HEAP_CHECK;
        if (system_flags & SYS_HEAP_CHECK)
            debug_message_window(1, 1, 60, RGBA(0x14, 0x40, 0x14, 0x80), "Memory OverRun Check Start");
        else
            debug_message_window(1, 1, 60, RGBA(0x14, 0x40, 0x14, 0x80), "Memory OverRun Check End");
        break;
    }
    if (pad_keyboard_modifiers_match(17) && key < 35 && key >= 30)
        system_set_frame_rate(key - 29);
    flags = system_flags;
    if (flags & SYS_SLOW_MOTION) {
        system_flags = (flags & ~SYS_SLOW_MOTION) | SYS_PAUSE_KBD;
        flags = system_flags;
    }
    if (flags & SYS_PAUSE_KBD) {
        if (kbd_state.mod == 71)
            system_flags = (flags & ~SYS_PAUSE_KBD) | SYS_SLOW_MOTION;
    }
    if (system_flags & SYS_HEAP_CHECK)
        heap_check_default();
    if (debug_flags_test(1)) {
        if (!pad_keyboard_modifiers_match(68))
            key = 0;
        switch (key) {
        case 29:
            player_status_add_damage(1.0f);
            break;
        case 6:
            stage_exit_flags |= 2;
            break;
        }
    }
    if (pad_system.cur_pad->raw.trig & PAD_L2)
        system_flags ^= SYS_PAUSE_KBD;
    if ((system_flags & SYS_PAUSE_KBD) && (pad_system.cur_pad->raw.hold & PAD_R2))
        system_flags = (system_flags & ~SYS_PAUSE_KBD) | SYS_SLOW_MOTION;
    if (pad_system.cur_pad->raw.trig & PAD_L3)
        camera_free_toggle();
    if (pad_system.cur_pad->raw.trig & PAD_R3) {
        if (++(*(unsigned int *)&debug_disp_level) >= 3) /* steer(cast) */
            (*(unsigned int *)&debug_disp_level) = 0; /* steer(cast) */
    }
}

/* Debug mode (system flag 0x8000): shortcuts, player 0's pad restored before, cleared after. */
/* 0x0016d9e8 */
void system_debug_update(void)
{
    pad_state_restore_owned(0);
    debug_log_save();
    if (system_flags & SYS_DEBUG)
        debug_keys();
    pad_state_clear_owned(0);
}

/*
 * System flag 0x400 or any pad input ends the saver and restarts the idle countdown; at 0 the
 * saver starts.
 */
/* 0x0016da30 */
void system_idle_timer(Sys1Work *w)
{
    if (system_flags & SYS_NO_SCREEN_SAVER) {
        if (w->demo) {
            TASK_OF((void *)w->demo)->flags |= TASK_F_END;
            w->demo = 0;
        }
        w->timer = system_idle_time;
        return;
    }
    if (pad_main->any) {
        if (w->demo) {
            TASK_OF((void *)w->demo)->flags |= TASK_F_END;
            w->demo = 0;
        }
        w->timer = system_idle_time;
    }
    if (w->timer > 0) {
        if (--w->timer <= 0) {
            if (w->demo == 0)
                w->demo = scr_saver_create(w);
        }
    }
}

/* System task (parent of loaders and tools); no saver, full idle countdown. */
/* 0x0016daf8 */
void system1_task_init(int *w)
{
    system1_work = w;
    w[1] = system_idle_time;
    w[0] = 0;
}

/*
 * Game frames per drawn frame (system_fps), power-on time, play time while playing unpaused
 * (flag 0x1000); then idle timer, debug shortcuts, command line.
 */
/* 0x0016db18 */
void system1_task_exec(Sys1Work *w)
{
    TimeAcc *tm = &system_time;
    unsigned int flags = system_flags;

    if (!(flags & (SYS_PAUSE_KBD | SYS_PAUSED)))
        w->frames++;
    if (flags & SYS_DISPLAY_PASS) {
        system_fps = w->frames;
        w->frames = 0;
    }
    tm->total[0]++;
    tm->cur[0]++;
    system_idle_timer(w);
    if ((system_flags & (SYS_PLAY_NOW | SYS_PAUSE_KBD | SYS_PAUSED)) == SYS_PLAY_NOW) {
        tm->total[1]++;
        tm->cur[1]++;
    }
    system_debug_update();
    command_line_update();
}

/* 0x0016dbd0 */
void system1_task_disp(void)
{
}

/* Becomes scr_root, the root of the screen tasks. */
/* 0x0016dbd8 */
void system2_task_init(int w)
{
    scr_root = w;
}

/* 0x0016dbe8 */
void system2_task_exec(void)
{
}

/* After the screen tasks: replay display, fade overlay, screen effects' second pass. */
/* 0x0016dbf0 */
void system2_task_disp(void)
{
    replay_debug_disp();
    screen_enable();
}

/* "System1" (runs while paused) under system1_root, "System2" under system2_root. */
/* 0x0016dc10 */
void system_tasks_create(void)
{
    void *w;

    w = task_create_root(&system1_root, &system1_desc, 12);
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    task_create_root(&system2_root, &system2_desc, 0);
}

/* Under the screen tasks. */
/* 0x0016dc60 */
void system_kpro_menu_toggle(void)
{
    kpro_menu_toggle(scr_root);
}

/* From the GS interrupt when a frame is drawn. */
/* 0x0016dc70 */
void system_gs_finish_hook(void)
{
}

/* Counts the vsync main waits on; vsync listeners (display list kick); sound vsync. */
/* 0x0016dc78 */
int vsync_handler(void)
{
    frame_ready++;
    listener_dispatch(0, 0);
    sound_vsync();
    ExitHandler();
    return 1;
}

/* System flags 0xc000 for the controller in drive, unless on; full debug display, menu redone. */
/* 0x0016dcc0 */
void debug_on(int drive)
{
    if (!(system_flags & SYS_DEBUG)) {
        system_debug_drive = drive;
        DEBUG_PRINTF("DEBUG ON! drive %d\n", drive);
        system_flags |= SYS_DEBUG | SYS_DEBUG_INPUT;
        debug_disp_level = 0;
        debug_menu_refresh();
    }
}

/* Only if turned on from drive: clears flags 0x8000/0x4000, display level 2, debug flags 0..4. */
/* 0x0016dd28 */
void debug_off(int drive)
{
    if (system_flags & SYS_DEBUG) {
        if (system_debug_drive != drive)
            return;
        system_debug_drive = -1;
        DEBUG_PRINTF("DEBUG Off! drive %d\n", drive);
        system_flags &= ~SYS_DEBUG;
        system_flags &= ~SYS_DEBUG_INPUT;
        debug_disp_level = 2;
        debug_flags_clear(31);
    }
}

/* For the save data; clears the counters since the last load first. */
/* 0x0016ddb0 */
void system_time_save(SystemTimeAcc *out)
{
    (*(SystemTimeAcc *)&system_time).cur.v[0] = 0; /* steer(cast) */
    (*(SystemTimeAcc *)&system_time).cur.v[1] = 0; /* steer(cast) */
    (*(SystemTimeAcc *)&system_time).cur.v[2] = 0; /* steer(cast) */
    *out = (*(SystemTimeAcc *)&system_time); /* steer(cast) */
}

/* Saved totals plus the time counted since the last save. */
/* 0x0016de00 */
void system_time_load(Time3 *in)
{
    (*(SystemTimeAcc *)&system_time).total.v[0] = in->v[0] + (*(SystemTimeAcc *)&system_time).cur.v[0]; /* steer(cast) */
    (*(SystemTimeAcc *)&system_time).total.v[1] = in->v[1] + (*(SystemTimeAcc *)&system_time).cur.v[1]; /* steer(cast) */
    (*(SystemTimeAcc *)&system_time).total.v[2] = in->v[2] + (*(SystemTimeAcc *)&system_time).cur.v[2]; /* steer(cast) */
}

/* "<label> h:mm:ss" at 60 frames per second. */
/* 0x0016de40 */
void debug_print_time(void *win, int x, int y, const char *label, int frames)
{
    int min, sec, hh, mm, ss, cs;

    FRAMES_SPLIT(frames, min, sec, ss, cs);
    hh = min / 60;
    mm = min % 60;
    gw_win_printf(win, x, y, "%s %3d:%02d:%02d", label, hh, mm, ss);
}

/*
 * 14 named system flags ("PAUSE", "S-RESET", "STABILIZER", ...), times, play counts, auto-save,
 * debug drive, memory card link, CRCs.
 */
/* 0x0016dea0 */
void system_debug_disp(void *win)
{
    int cw;
    int ch;
    int x;
    int y;
    unsigned int i;

    gw_get_cell_size(&cw, &ch);
    y = 0;
    for (i = 0; i < 14; i++) {
        if (system_flags & system_flag_names[i].bit)
            gw_win_printf(win, 4, y, "%s", system_flag_names[i].name);
        else
            gw_win_printf(win, 4, y, "          |");
        y += ch;
    }
    x = cw * 11 + 4;
    debug_print_time(win, x, 0, "PowerTime", system_time.total[0]);
    y = ch;
    debug_print_time(win, x, y, "         ", system_time.cur[0]);
    y += ch;
    debug_print_time(win, x, y, "PlayTime ", system_time.total[1]);
    y += ch;
    debug_print_time(win, x, y, "         ", system_time.cur[1]);
    y += ch;
    gw_win_printf(win, x, y, "PlayNum   %9d", system_time.total[2]);
    y += ch;
    gw_win_printf(win, x, y, "          %9d", system_time.cur[2]);
    y += ch;
    gw_win_printf(win, x, y, "AutoSaveOn    %d", game_state.autosave_on);
    y += ch;
    gw_win_printf(win, x, y, "AutoSaveDrive %d", game_state.vms_slot);
    y += ch;
    gw_win_printf(win, x, y, "DebugVmsDrive %d", system_debug_drive);
    y += ch;
    gw_win_printf(win, x, y, "VMS Link %s", game_state.vms_link ? "TRUE" : "FALSE");
    y += ch;
    gw_win_printf(win, x, y, "VMS  CRC %08x", game_state.vms_crc);
    gw_win_printf(win, x, y + ch, "DATA CRC %08x", game_state.data_crc);
}

/* 0x0016e160 */
void system_debug_close(void)
{
    system_debug_win = 0;
    menu_item_set_value(info_menu_name, "System", 0);
}

/* Also F9; opens at 31x14 cells. */
/* 0x0016e180 */
void system_debug_open(void)
{
    int cw;
    int ch;
    void *win;

    if (system_debug_win) {
        gw_window_close(system_debug_win);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("System", 95, screen_layout.x + 4.0f, screen_layout.y + 32.0f,
                       cw * 31, ch * 14, 0);
    system_debug_win = win;
    if (win) {
        gw_window_set_handlers(win, 0, system_debug_disp, system_debug_close);
        menu_item_set_value(info_menu_name, "System", 1);
    }
}

/* Packs every variable-editor variable into a new buffer; runs while paused. */
/* 0x0016e270 */
void varedit_save_init(VarEditSave *w)
{
    w->size = gw_varedit_save_total_size();
    DEBUG_PRINTF("\225\317\220\224Save Size %d\n", w->size);
    w->buf = heap_block_tag(game_malloc(w->size), "system.c", SRC_LINE(1528, 1536));
    DEBUG_PRINTF("         Data %08x\n", w->buf);
    gw_varedit_save(w->buf);
    w->state = 0;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
}

/* 0x0016e2f0 */
void varedit_save_release(VarEditSave *w)
{
    heap_free(w->buf, 0);
}

/* Saves "KPRJ____.VAR" once the request is free; ends when done. */
/* 0x0016e300 */
void varedit_save_exec(VarEditSave *w)
{
    switch (w->state) {
    case 0:
        if (filereq_is_free()) {
            filereq_set(3, auto_load_drive, str_varedit_file, w->buf, w->size, 1);
            BackupSaveTaskSet();
            w->state++;
        }
        break;
    case 1:
        if (filereq_is_idle()) {
            filereq_get_state();
            filereq_clear();
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* 0x0016e3b8 */
void varedit_save_start(void)
{
    task_create(system1_work, &varedit_save_desc, 12);
}

/* Runs while paused. */
/* 0x0016e3d0 */
void varedit_load_init(int *w)
{
    *w = 0;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
}

/* 0x0016e3e8 */
void varedit_load_release(void)
{
}

/* Loads "KPRJ____.VAR" once the request is free; applies it on success (state 2); ends. */
/* 0x0016e3f0 */
void varedit_load_exec(int *w)
{
    void *buf;

    switch (*w) {
    case 0:
        if (filereq_is_free()) {
            filereq_set_path(auto_load_drive, str_varedit_file, 0);
            BackupLoadTaskSet(w);
            (*w)++;
        }
        break;
    case 1:
        if (filereq_is_idle()) {
            buf = filereq_get_buf();
            if (filereq_get_state() == 2)
                gw_varedit_load(buf);
            heap_free(buf, 0);
            filereq_clear();
            TASK_OF(w)->flags |= TASK_F_END;
        }
        break;
    }
}

/* 0x0016e4c8 */
void varedit_load_start(void)
{
    task_create(system1_work, &varedit_load_desc, 4);
}

/* System flag 0x100 ("*PAUSE" in the System window). */
/* 0x0016e4e0 */
void system_pause_disable(void)
{
    system_flags |= SYS_NO_PAUSE;
}

/* Except in game types 0 and 1. */
/* steer(cast) */
/* 0x0016e4f8 */
void system_pause_enable(void)
{
    if (((unsigned int (*)(void))game_type_get)() >= 2)
        system_flags &= ~SYS_NO_PAUSE;
}

/* Vsyncs per frame; 1 while the model test is on. */
/* 0x0016e538 */
void system_set_frame_rate(int rate)
{
    if (model_test_on)
        frame_rate = 1;
    else
        frame_rate = rate;
}

/* 0x0016e560 */
int system_get_frame_rate(void)
{
    return frame_rate;
}

/*
 * system.ptm, item.ptm, particle.ptm, effect.ptm, texforce.ptm at their VRAM offsets; system.pnb,
 * item.pnb; then the font and system model entries.
 */
/* steer(cast) */
/* 0x0016e570 */
void system_load_textures(void)
{
    TexFile *tex = system_tex_files;

    system_ptm = ((void *(*)(TexFile *, int))model_load_timed)(&tex[0], 0);
    system_pnb = model_pnb_load(&tex[1]);
    item_textures = ((void *(*)(TexFile *, int))model_load_timed)(&tex[2], 256);
    item_models = model_pnb_load(&tex[3]);
    ptcl_texture = ((void *(*)(TexFile *, int))model_load_timed)(&tex[4], 512);
    lens_flare_tex = ((void *(*)(TexFile *, int))model_load_timed)(&tex[5], 768);
    system_texforce_ptm = ((void *(*)(TexFile *, int))model_load_timed)(&tex[6], 1024);
    font_init();
    system_pnb_entries_init();
}

/* Four entries (system_pnb_ids) in system.pnb; wire model of system_pnb_model_id. */
/* steer(cast) */
/* 0x0016e620 */
void system_pnb_entries_init(void)
{
    int *src = system_pnb_ids;
    int *dst = system_pnb_entries;
    int i = 4;
    int *ent;

    do {
        *dst++ = *((int *(*)(void *, int))pnb_get_entry)(system_pnb, *src++);
    } while (--i != 0);
    ent = system_pnb_model_id;
    i = 1;
    do {
        model_pnb_get_wire(system_pnb, *ent++);
    } while (--i != 0);
}

/* i 0..3. */
/* 0x0016e6c0 */
int system_pnb_entry_get(int i)
{
    return system_pnb_entries[i];
}

/* Font texture in system.ptm, sprite list; returns the buffer for font_put_char (0). */
/* 0x0016e6d8 */
void *font_begin(int color, int id)
{
    ulGxSelectTexture(system_ptm, font_tex_ids[id]);
    ulGxBeginSprite(1);
    font_color = color;
    return 0;
}

/* 0x0016e730 */
void font_end(void *buf)
{
    ulGxEndSprite(buf);
}

/* All zero outside 32..240. */
/* 0x0016e738 */
void font_get_glyph(unsigned char ch, int *tex, float *u_left, float *v_top, float *u_right, float *v_bottom)
{
    float (*col)[2] = font_cols;
    float (*row)[2] = font_rows;
    unsigned char i = ch - 32;
    int cx;
    int cy;

    if (i > 208) {
        *u_left = 0;
        *v_top = 0;
        *u_right = 0;
        *tex = 0;
        *v_bottom = 0;
        return;
    }
    cx = i % font_per_row;
    cy = i / font_per_row;
    *u_left = col[cx][0];
    *v_top = row[cy][0];
    *u_right = col[cx][1];
    *v_bottom = row[cy][1];
    *tex = font_tex_ids[font_id];
}

/* width x height sprite in the print colour via the column/row tables; returns buf. */
/* 0x0016e7e8 */
void *font_put_char(void *buf, unsigned char ch, float (*col)[2], float (*row)[2], int cols,
                    float x, float y, float width, float height, float z)
{
    FontVtx vtx[8];
    unsigned char i = ch - 32;
    int cx = i % cols;
    int cy = i / cols;
    float u_left = col[cx][0];
    float v_top = row[cy][0];
    float u_right = col[cx][1];
    float v_bottom = row[cy][1];

    vtx[1].color = vtx[0].color = font_color;
    vtx[0].z = z;
    vtx[0].u = u_left;
    vtx[0].v = v_top;
    vtx[1].x = x + width;
    vtx[1].y = y + height;
    vtx[1].z = z;
    vtx[1].u = u_right;
    vtx[1].v = v_bottom;
    vtx[0].x = x;
    vtx[0].y = y;
    ulGxAddSprites(vtx, 2);
    return buf;
}

/* Glyphs 8x13, 10x14, 12x19, 36x42; 16 per row (14 for font 3). Out-of-range ids still stored. */
/* 0x0016e898 */
void font_set(FontId id)
{
    switch (id) {
    case FONT_0:
        font_cols = font0_cols;
        font_rows = font0_rows;
        font_w = 8.0f;
        font_h = 13.0f;
        font_per_row = 16;
        break;
    case FONT_1:
        font_cols = font1_cols;
        font_rows = font1_rows;
        font_w = 10.0f;
        font_h = 14.0f;
        font_per_row = 16;
        break;
    case FONT_2:
        font_cols = font2_cols;
        font_rows = font2_rows;
        font_w = 12.0f;
        font_h = 19.0f;
        font_per_row = 16;
        break;
    case FONT_3:
        font_cols = font3_cols;
        font_rows = font3_rows;
        font_w = 36.0f;
        font_h = 42.0f;
        font_per_row = 14;
        break;
    }
    font_id = id;
}

/* Snapped to whole pixels; '\n' returns to the first column. */
/* 0x0016e9d0 */
void font_print(float x, float y, float z, int color, unsigned char *str)
{
    float (*col)[2];
    float (*row)[2];
    float width;
    float height;
    float left;
    unsigned char ch;
    void *buf;

    x = floorf(x);
    y = floorf(y);
    left = x;
    col = font_cols;
    row = font_rows;
    width = font_w;
    height = font_h;
    buf = font_begin(color, font_id);
    while ((ch = *str++) != 0) {
        if (ch == '\n') {
            x = left;
            y += height;
        } else {
            buf = font_put_char(buf, ch, col, row, font_per_row, x, y, width, height, z);
            x += width;
        }
    }
    font_end(buf);
}

/* 0x0016eaf8 */
void *font_begin_current(int color)
{
    return font_begin(color, font_id);
}

/* 0x0016eb20 */
void font_end_current(void *buf)
{
    font_end(buf);
}

/* Scaled about the centre of its cell at (x, y). */
/* 0x0016eb28 */
void *font_put_char_scaled(void *buf, float x, float y, float z, unsigned char ch, float scale)
{
    float width = font_w * scale;
    float height = font_h * scale;

    return font_put_char(buf, ch, font_cols, font_rows, font_per_row,
                         x - (width - font_w) * 0.5f, y - (height - font_h) * 0.5f, width, height, z);
}

/* 16 glyphs step texels wide from x, as UVs of a size-texel texture (half a texel in). */
/* 0x0016eba0 */
void font_build_cols(float (*table)[2], float x, float step, float size)
{
    float next;
    int n;

    for (n = 16; n != 0; n--) {
        next = x + step;
        (*table)[0] = (x + 0.5f) / size;
        (*table)[1] = (next + 0.5f) / size;
        x = next;
        table++;
    }
}

/* Row table (v0, v1), 13 glyph rows. */
/* 0x0016ebf8 */
void font_build_rows(float (*table)[2], float x, float step, float size)
{
    float next;
    int n;

    for (n = 13; n != 0; n--) {
        next = x + step;
        (*table)[0] = (x + 0.5f) / size;
        (*table)[1] = (next + 0.5f) / size;
        x = next;
        table++;
    }
}

/* 8x13 in a 128-texel texture, 10x14 and 12x19 in 256, 36x42 in 512. */
/* 0x0016ec50 */
void font_init(void)
{
    font_build_cols(font0_cols, 0.0f, 8.0f, 128.0f);
    font_build_rows(font0_rows, 0.0f, 13.0f, 128.0f);
    font_build_cols(font1_cols, 0.0f, 10.0f, 256.0f);
    font_build_rows(font1_rows, 0.0f, 14.0f, 256.0f);
    font_build_cols(font2_cols, 0.0f, 12.0f, 256.0f);
    font_build_rows(font2_rows, 0.0f, 19.0f, 256.0f);
    font_build_cols(font3_cols, 0.0f, 36.0f, 512.0f);
    font_build_rows(font3_rows, 0.0f, 42.0f, 512.0f);
}

/*
 * Corners corner[0..3] in the current matrix; nothing if one is behind the camera or projects
 * beyond +-1024.
 */
/* 0x0016ed80 */
void font_put_char3d(unsigned char ch, Vec3 *corner, int color)
{
    FontVtx vtx[4];
    Vec4 view_pos;
    float s[4];
    int tex;
    float u_left;
    float v_top;
    float u_right;
    float v_bottom;
    float x, y;

    font_get_glyph(ch, &tex, &u_left, &v_top, &u_right, &v_bottom);

    ulMatrixApplyPoint(ulMatrixCur, &corner[0], &view_pos);
    if (view_pos.z >= -1.0f)
        return;
    vtx[0].z = project_point(&view_pos, s);
    x = s[0];
    y = s[1];
    if (x >= 1024.0f || x <= -1024.0f || y >= 1024.0f || y <= -1024.0f)
        return;
    vtx[0].x = x;
    vtx[0].y = y;
    vtx[0].u = u_left;
    vtx[0].v = v_top;
    vtx[0].color = color;

    ulMatrixApplyPoint(ulMatrixCur, &corner[1], &view_pos);
    if (view_pos.z >= -1.0f)
        return;
    vtx[1].z = project_point(&view_pos, s);
    x = s[0];
    y = s[1];
    if (x >= 1024.0f || x <= -1024.0f || y >= 1024.0f || y <= -1024.0f)
        return;
    vtx[1].x = x;
    vtx[1].y = y;
    vtx[1].u = u_right;
    vtx[1].v = v_top;
    vtx[1].color = color;

    ulMatrixApplyPoint(ulMatrixCur, &corner[2], &view_pos);
    if (view_pos.z >= -1.0f)
        return;
    vtx[3].z = project_point(&view_pos, s);
    x = s[0];
    y = s[1];
    if (x >= 1024.0f || x <= -1024.0f || y >= 1024.0f || y <= -1024.0f)
        return;
    vtx[3].x = x;
    vtx[3].y = y;
    vtx[3].u = u_right;
    vtx[3].v = v_bottom;
    vtx[3].color = color;

    ulMatrixApplyPoint(ulMatrixCur, &corner[3], &view_pos);
    if (view_pos.z >= -1.0f)
        return;
    vtx[2].z = project_point(&view_pos, s);
    x = s[0];
    y = s[1];
    if (x >= 1024.0f || x <= -1024.0f || y >= 1024.0f || y <= -1024.0f)
        return;
    vtx[2].x = x;
    vtx[2].y = y;
    vtx[2].u = u_left;
    vtx[2].v = v_bottom;
    vtx[2].color = color;

    ulGxAddTexStrip(vtx, 4);
}

/* Optional rot (Y, X, Z) and scale; the font's 3D glyph size. */
/* 0x0016f078 */
void font_draw_char3d(unsigned char ch, Vec3 *pos, IVec3 *rot, Vec3 *scale, int color)
{
    int tex;
    float u_left;
    float v_top;
    float u_right;
    float v_bottom;
    Vec3 *corner;

    font_get_glyph(ch, &tex, &u_left, &v_top, &u_right, &v_bottom);
    ulGxSelectTexture(system_ptm, tex);
    ulGxBeginTexStrip(1);
    corner = font_corners[font_id];
    ulMatrixPush();
    MATRIX_TRANSLATE(pos);
    if (rot)
        matrix_rotate_yxz(rot);
    if (scale)
        vu0_matrix_scale(ulMatrixCur, scale->x, scale->y, scale->z);
    font_put_char3d(ch, corner, color);
    ulMatrixPop();
    ulGxEndTexStrip();
}

/* Advances by step, or without one by the font's glyph width along x. */
/* 0x0016f190 */
void font_print3d(unsigned char *str, Vec3 *pos, Vec3 *step, IVec3 *rot, Vec3 *scale, int color)
{
    int tex;
    float u_left;
    float v_top;
    float u_right;
    float v_bottom;
    Vec3 *corner;
    float adv;
    unsigned char ch;

    if (step == 0)
        adv = font_glyph_widths[font_id];
    else
        adv = 0.0f;
    font_get_glyph(*str, &tex, &u_left, &v_top, &u_right, &v_bottom);
    ulGxSelectTexture(system_ptm, tex);
    ulGxBeginTexStrip(1);
    corner = font_corners[font_id];
    ulMatrixPush();
    MATRIX_TRANSLATE(pos);
    if (rot)
        matrix_rotate_yxz(rot);
    if (scale)
        vu0_matrix_scale(ulMatrixCur, scale->x, scale->y, scale->z);
    while ((ch = *str) != 0) {
        font_put_char3d(ch, corner, color);
        if (step)
            MATRIX_TRANSLATE(step);
        else
            vu0_matrix_translate(ulMatrixCur, adv, 0.0f, 0.0f);
        str++;
    }
    ulMatrixPop();
    ulGxEndTexStrip();
}

/*
 * Right-aligned: backwards, a step back per glyph; ',' '.' half a step, ' ' only a step. Stops
 * at a 0 byte, relying on the byte before the string being 0.
 */
/* 0x0016f338 */
void font_print3d_rev(unsigned char *str, Vec3 *pos, Vec3 *step, IVec3 *rot, Vec3 *scale, int color)
{
    Vec4 ofs[3];                  /* -step, 3/4 of it, 1/4 of step */
    int tex;
    float u_left;
    float v_top;
    float u_right;
    float v_bottom;
    Vec3 *corner;
    unsigned char ch;

    str += line_length(str) - 1;
    font_get_glyph(*str, &tex, &u_left, &v_top, &u_right, &v_bottom);
    ulGxSelectTexture(system_ptm, tex);
    ulGxBeginTexStrip(1);
    ofs[0].x = -step->x;
    ofs[0].y = -step->y;
    ofs[0].z = -step->z;
    ofs[2].x = ofs[0].x * -0.25f;
    ofs[2].y = ofs[0].y * -0.25f;
    ofs[2].z = ofs[0].z * -0.25f;
    ofs[1].x = ofs[0].x * 0.75f;
    ofs[1].y = ofs[0].y * 0.75f;
    ofs[1].z = ofs[0].z * 0.75f;
    corner = font_corners[font_id];
    ulMatrixPush();
    MATRIX_TRANSLATE(pos);
    if (rot)
        matrix_rotate_yxz(rot);
    if (scale)
        vu0_matrix_scale(ulMatrixCur, scale->x, scale->y, scale->z);
    for (; (ch = *str) != 0; str--) {
        if (ch == ',' || ch == '.') {
            MATRIX_TRANSLATE(&ofs[2]);
            font_put_char3d(ch, corner, color);
            MATRIX_TRANSLATE(&ofs[1]);
        } else if (ch == ' ') {
            MATRIX_TRANSLATE(&ofs[0]);
        } else {
            font_put_char3d(ch, corner, color);
            MATRIX_TRANSLATE(&ofs[0]);
        }
    }
    ulMatrixPop();
    ulGxEndTexStrip();
}

/* Through the non-null pointers; id at most 23. Texture id 1: the current language's texture. */
/* 0x0016f598 */
void sprite_get(unsigned int id, int *tex, SpriteUV **uv, float *width, float *height)
{
    SpriteDef *def;
    int tid;

    if (id > 23)
        id = 23;
    if (uv)
        *uv = &sprite_uv_table[id];
    def = &sprite_defs[id];
    if (tex) {
        tid = def->tex;
        if (tid == 1)
            tid = sprite_lang_tex[game_lang];
        *tex = tid;
    }
    if (width)
        *width = def->w;
    if (height)
        *height = def->h;
}

/* halign/valign 0: left/top edge at the point, 1 centre (whole pixels), 2 right/bottom. */
/* 0x0016f630 */
void sprite_draw(float x, float y, float z, int col, int id, int halign, int valign)
{
    int tex;
    SpriteUV *uv;
    float width;
    float height;

    ulGxSetTextureList(system_ptm);
    sprite_get(id, &tex, &uv, &width, &height);
    switch (halign) {
    case 0:
        break;
    case 1:
        x -= floorf(width * 0.5f);
        break;
    case 2:
        x -= width;
        break;
    }
    switch (valign) {
    case 0:
        break;
    case 1:
        y -= floorf(height * 0.5f);
        break;
    case 2:
        y -= height;
        break;
    }
    tex_rect_draw_uv(tex, col, uv, x, y, z, width, height);
}

/* Billboard; halign 0/2 shift right/left, valign 0/2 up/down by half its size; 1 centred. */
/* 0x0016f770 */
void sprite_draw3d(Vec3 *pos, float scale, int col, int id, int halign, int valign)
{
    Vec3 draw_pos;
    int tex;
    SpriteUV *uv;
    float width;
    float height;

    ulGxSetTextureList(system_ptm);
    sprite_get(id, &tex, &uv, &width, &height);
    draw_pos.x = pos->x;
    draw_pos.y = pos->y;
    draw_pos.z = pos->z;
    switch (halign) {
    case 0:
        draw_pos.x += floorf(width * 0.5f);
        break;
    case 1:
        break;
    case 2:
        draw_pos.x -= floorf(width * 0.5f);
        break;
    }
    switch (valign) {
    case 0:
        draw_pos.y -= floorf(height * 0.5f);
        break;
    case 1:
        break;
    case 2:
        draw_pos.y += floorf(height * 0.5f);
        break;
    }
    billboard_draw_uv(tex, col, &draw_pos, uv, width * scale, height * scale);
}

/* 0x0016f910 */
void matrix_rotate_yxz(IVec3 *rot)
{
    vu0_matrix_rotate_y(ulMatrixCur, rot->y);
    vu0_matrix_rotate_x(ulMatrixCur, rot->x);
    vu0_matrix_rotate_z(ulMatrixCur, rot->z);
}
