/*
 * Entry and main loop: boot, PAL 50/60 Hz choice, game systems up, then one update and draw per
 * display frame with catch-up updates when behind.
 */

#include "debug.h"
#include "libc.h"
#include "adx_dvci.h"
#include "adxf.h"
#include "adxm.h"
#include "adxt_lib.h"
#include "gw_profiler.h"
#include "gw_system.h"
#include "gw_window.h"
#include "libuliop.h"
#include "nj_light.h"
#include "nj_system.h"
#include "nj_view.h"
#include "ps_pad.h"
#include "ps_timer.h"
#include "rofs_if.h"
#include "rofs_mai.h"
#include "ul_console.h"
#include "ul_fs.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_texture.h"
#include "ulsnd.h"
#include "main.h"
#include "anim.h"
#include "callback.h"
#include "camera.h"
#include "command.h"
#include "config.h"
#include "console_com.h"
#include "dir_read.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "heap.h"
#include "light.h"
#include "lockon.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "opdemo.h"
#include "pad.h"
#include "pause.h"
#include "pcfile.h"
#include "perform.h"
#include "peri_act.h"
#include "peri_vib.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "replay.h"
#include "screen.h"
#include "sequence.h"
#include "sound_bank.h"
#include "sound_main.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "texforce.h"
#include "tv_set.h"

typedef struct GfxCfg {
    int buf;
    int buf_size;
    int w;
    int h;
    int font;
    int font_mode;
    int pad_port;
    float scale;
    int console_buf_size;
    int console_lines;
    int console_reserved;
    int serial_out;
} GfxCfg;

typedef struct DispCfg {
    int mode;
    int tv_mode;
} DispCfg;

#ifdef VERSION_US

/* Source line recorded in an allocation tag. */
#define SRC_LINE(early, us) (us)

#else

#define SRC_LINE(early, us) (early)

#endif /* !VERSION_US */

#include "types_main.h"
#include "color.h"

typedef struct FrameTime {
    int disp_mode;
    char pad_04[8];
    int stamp;          /* 0x0c */
    char pad_10[8];
    float dt0;          /* 0x18 */
    float dt1;          /* 0x1c */
    float dt2;          /* 0x20 */
    float dt3;          /* 0x24 */
} FrameTime;

#ifdef VERSION_US

#endif /* VERSION_US */

extern char main_rofs_work[];
extern volatile int sys_state_unk_04;
extern int main_profiler_group;
extern int main_render_ready;
extern volatile int frame_ready;
extern int display_fps;
extern int sys_state;
extern int gxDesc;
extern int tex_heap_size;
extern char gx_vertex_buffer[];
extern int tex_heap_work;
extern char main_console_buf[];
extern char main_ninja_heap[];
extern char main_fs_ctx[];
extern char main_fs_entries[];
extern int scr_root;
extern char system2_root[];
extern char system1_root[];
extern unsigned int system_flags;
extern int frame_update_count;

/* Reboots the IOP with the game image; pad, sound, card, USB modules; vibration; sound buffers. */
#ifdef VERSION_US
/* Retries until it loads. */
#define LOAD_MODULE(name, argc, argv)     do {     } while (ulFsLoadModule(name, argc, argv) < 0)

/* 0x001000c8 */
static void load_iop_modules(void)
{
    uliop_init();
    uliop_set_path("cdrom0:/drivers/");
    uliop_reboot_iop("ioprp23.img", 0, 0);
    ulFsChangeDir("drivers");
    LOAD_MODULE("sio2man.irx", 0, 0);
    LOAD_MODULE("padman.irx", 0, 0);
    LOAD_MODULE("libsd.irx", 0, 0);
    LOAD_MODULE("modhsyn.irx", 0, 0);
    LOAD_MODULE("modmidi.irx", 0, 0);
    LOAD_MODULE("modmsin.irx", 0, 0);
    LOAD_MODULE("ulsnd.irx", 12, "-c1\0-o0\0-l0");
    LOAD_MODULE("mcman.irx", 0, 0);
    LOAD_MODULE("mcserv.irx", 0, 0);
    LOAD_MODULE("usbd.irx", 0, 0);
    LOAD_MODULE("usbtvr.irx", 0, 0);
    peri_vib_init(2);
    ulFsChangeDir("..");
    ulsnd_alloc_buffers(heap_block_tag(game_malloc(ulsnd_configure(4096, 0x40000, 0x20000)), "main.c", 362));
    ulsnd_connect();
    ulsnd_set_debug(0);
}
#else
/* 0x001000c8 */
void load_iop_modules(void)
{
    uliop_init();
    uliop_set_path("cdrom0:/drivers/");
    uliop_reboot_iop("ioprp23.img", 0, 0);
    ulFsChangeDir("drivers");
    ulFsLoadModule("sio2man.irx", 0, 0);
    ulFsLoadModule("padman.irx", 0, 0);
    ulFsLoadModule("libsd.irx", 0, 0);
    ulFsLoadModule("modhsyn.irx", 0, 0);
    ulFsLoadModule("modmidi.irx", 0, 0);
    ulFsLoadModule("modmsin.irx", 0, 0);
    ulFsLoadModule("ulsnd.irx", 12, "-c1\0-o0\0-l0");  /* three NUL-separated arguments */
    ulFsLoadModule("mcman.irx", 0, 0);
    ulFsLoadModule("mcserv.irx", 0, 0);
    ulFsLoadModule("usbd.irx", 0, 0);
    ulFsLoadModule("usbtvr.irx", 0, 0);
    peri_vib_init(2);
    ulFsChangeDir("..");
    ulsnd_alloc_buffers(heap_block_tag(game_malloc(ulsnd_configure(4096, 0x40000, 0x20000)), "main.c", 357));
    ulsnd_connect();
    ulsnd_set_debug(0);
}
#endif

/* Mounts rez.cvm as default volume VOL; hangs with a message on failure. */
/* steer(cast) */
/* 0x001002f0 */
static void rofs_init(void)
{
    struct {
        int a;
        int b;
        int c;
        char *d;
    } cfg;
    int ret;

    adx_fs_init_cdv(0);
    adxm_thread_init(0);
    adxt_lib_init();
    cfg.a = 16;
    cfg.b = 1;
    cfg.c = 64;
    cfg.d = main_rofs_work;
    ret = rofs_if_init(&cfg);
    if (ret < 0) {
        DEBUG_PRINTF("error : ROFS_Init()  ret:%d\n", ret);
        for (;;)
            ;
    }
    ((void (*)(int))adxf_rofs_add_device)(0);
    ret = adxf_rofs_add_volume("VOL", "CDV:rez.cvm");
    if (ret < 0) {
        DEBUG_PRINTF("error : ADXF_AddRofsVol()  ret:%d\n", ret);
        for (;;)
            ;
    }
    adxf_rofs_set_def_volume("VOL");
}

/* 0x001003d8 */
static void main_reset_init_hook(void)
{
}

/* 0x001003e0 */
static void main_reset_release_hook(void)
{
}

/* Debug window layer 640x480, 1024-line console; MAIN profiler group; default colours. */
/* steer(cast) */
/* 0x001003e8 */
static void main_render_init(void)
{
    GfxCfg cfg;
    int unused;
    int ret;

    ret = 1;
    cfg.pad_port = ret;
    cfg.buf = nj_malloc(0x40000);
    cfg.buf_size = 0x40000;
    cfg.w = 640;
    unused = sys_state_unk_04;
    cfg.h = 480;
    cfg.font = ret;
    cfg.font_mode = -512;
    cfg.scale = 0.15f;
    cfg.console_buf_size = 0x8000;
    cfg.console_lines = 1024;
    do {
        cfg.console_reserved = 0;
        cfg.serial_out = 0;
    } while (0);
    /* Never true: (x & 1) >> 1 is 0. */
    if ((unused & 1) >> 1) {
        cfg.console_reserved = unused;
    }
    gw_init(&cfg);
    main_profiler_group = ((int (*)(const char *))gw_profiler_group_create)("MAIN");
    {
        int col[4] = { RGBA(0x98, 0xb0, 0x8c, 0x80), RGBA(0x98, 0xb0, 0xc8, 0x80), RGBA(0x98, 0xb9, 0xc8, 0x80), RGBA(0x98, 0xb0, 0xbf, 0x80) };

        ret = ((int (*)(int *, unsigned int))gw_set_default_colors)(col, RGBA(0, 0, 0, 0xff));
    }
    main_render_ready = 1;
}

/* Shuts the debug window layer down if set up. */
/* 0x001004c8 */
static void main_render_kick(void)
{
    if (main_render_ready)
        gw_exit();
}

/* Default heap, tagged "Graphic_K". */
/* 0x001004e8 */
static int graphic_k_create(int size)
{
    int heap = heap_get_default();

    heap = heap_alloc_front(heap, size, 0);
    return heap_block_tag(heap, "Graphic_K", -1);
}

/* graphic_k_create and heap_free as the memory hooks. */
/* 0x00100530 */
static void main_register_callbacks(void)
{
    struct { void *a; void *b; } hooks;

    hooks.a = graphic_k_create;
    hooks.b = heap_free;
    mem_hooks_set(&hooks);
}

/* 0x00100568 */
static void main_unregister_callbacks(void)
{
    mem_hooks_init();
}

/* Message 0: ADX interrupt handler. */
/* 0x00100570 */
static int main_adx_listener(void)
{
    adxm_call_int_handler(0);
    return 1;
}

/* Re-inits graphics, VRAM heap, ROFS, vertex buffer, console, vsync handler; waits 3 frames. */
/* 0x00100590 */
void MainChangeDisplayPAL60(int pal60)
{
    printf("MainChangeDisplayPAL60 : %d\n", pal60);
    if (pal60) {
        gxDesc = 4;
        ((DispCfg *)&gxDesc)->tv_mode = 2;
        display_fps = 60;
        sys_state = 0;
    } else {
        gxDesc = 6;
        ((DispCfg *)&gxDesc)->tv_mode = 3;
        display_fps = 50;
        sys_state = 2;
    }
    ulGxInit(&gxDesc);
    ulTextureSetVramHeap(ulTextureInitVramHeap(tex_heap_work, tex_heap_size, 0x7b000, 0x85000, 1024));
    ((void (*)(int))rofs_init)(0); /* steer(cast) */
    ulGxSetVertexBuffer_thunk(gx_vertex_buffer, 0x80000);
    anim_cache_init(heap_base_end, 0x8000);
    ulMatrixStackInit(0x70000000, 64);
    ulConsoleInit(main_console_buf, 40, 30);
    ulConsoleSetCellSize(16, 16);
#ifdef VERSION_US
    debug_init();
#endif
    vsync_handler_install();
    render_init();
    while (frame_ready <= 0)
        ;
    frame_ready = 0;
    while (frame_ready <= 0)
        ;
    frame_ready = 0;
    while (frame_ready <= 0)
        ;
    frame_ready = 0;
}

/* Heaps, files, IOP modules, timer, pads, graphics (60 Hz NTSC), VRAM heap, ROFS, console. */
/* 0x00100740 */
static void game_init(void)
{
    char buf[256];
    GxDesc *gx;

    heap_system_init();
    nj_heap_create(main_ninja_heap, 0x80000);
    listener_table_clear();
    filer_boot();
    model_install_allocator();
    ulFsInit(16, main_fs_ctx, 30, main_fs_entries);
    ulFsSetDevice(0);
    buf[0] = 0;
    ulFsSetRootDir(buf);
    load_iop_modules();
    psTimerInit();
    psPadInit();
    gx = &gxDesc;
    gx->mode = 4;
    gx->tv_mode = 2;
    gx->field_mode = 1;
    gx->ntex = 1024;
    gx->size2 = 0x100000;
    gx->size1 = 0;
    gx->gxWorkSize = ulGxGetWorkSize(gx);
    gx->gxWork = calloc(1, gx->gxWorkSize);
    printf("ulGxInit() gxDesc.gxWorkSize = % 10d(%08x)\n", gx->gxWorkSize, gx->gxWorkSize);
    printf("ulGxInit() gxDesc.gxWork     = %08x\n", gx->gxWork);
    ulGxInit(gx);
    tex_heap_size = 0x8040;
    tex_heap_work = (int)heap_block_tag(game_malloc(0x8040), "main.c", SRC_LINE(897, 904));
    printf("TexHeap WorkSize : %08x   %08x\n", tex_heap_size, tex_heap_work);
    ulTextureSetVramHeap(ulTextureInitVramHeap(tex_heap_work, tex_heap_size, 0x7b000, 0x85000, 1024));
    ((void (*)(void *))rofs_init)(buf); /* steer(cast) */
    listener_add(0, main_adx_listener);
    ulGxSetVertexBuffer_thunk(gx_vertex_buffer, 0x80000);
    anim_cache_init(heap_base_end, 0x8000);
    ulMatrixStackInit(0x70000000, 64);
    ulConsoleInit(main_console_buf, 40, 30);
    ulConsoleSetCellSize(16, 16);
#ifdef VERSION_US
    debug_init();
#endif
    system_init();
    tvset_slots_clear();
    render_init_all();
    pad_init();
    game_state_init();
}

/* After boot: screen and debug layers, tasks, files, sound, camera, ..., lock-on, frame rate. */
/* steer(cast) */
/* 0x00100960 */
static void game_soft_reset_init(void)
{
    texforce_surface_create();
    screen_init();
    main_reset_init_hook();
    main_render_init();
    ((void (*)(int))nj_system_reset)(0);
    ((void (*)(int, int, int))nj_set_clear_color)(1 << 31, 1 << 31, 1 << 31);
    ((void (*)(int))opdemo_reset)(0);
    DEBUG_PRINT("\n\n\nInitialize... Hard Reset\n");
    debug_init();
    perform_hard_reset();
    render_vars_register();
    model_render_state_init();
    DEBUG_PRINT("Initialize... Soft Reset\n");
    compiled_alloc_register();
    game_varedit_register();
    modeltest_menu_create();
    config_vars_register();
    task_system_init();
    command_reset();
    debug_soft_init();
    filer_init();
    rofs_dir_read_all();
    system_start();
    console_com_task_create(scr_root);
    fade_create(scr_root);
    pause_task_create();
    sound_sequence_start();
    module_heap_init();
    camera_system_init();
    light_init();
    pad_module_init();
    perform_soft_reset();
    pcfile_init();
    screen_menu_init();
    game_core_init();
    stage_debug_menu_init();
    player_setup();
    replay_reset();
    model_register_callbacks();
    sound_init();
    fog_create(scr_root);
    lockon_vars_register();
    (*(int *)&frame_ready) = 0; /* steer(cast) */
    system_set_frame_rate(1);
    stabilizer_rate = frame_rate;
    main_register_callbacks();
    DEBUG_PRINT("Initialize... Soft Reset End!\n");
    DEBUG_PRINT("------------------------------\n\n");
}

/* 0x00100b18 */
static void game_reset_systems(void)
{
    main_unregister_callbacks();
    model_pnb_free_handlers();
    main_reset_release_hook();
    nj_console_close();
    main_render_kick();
    pad_listener_remove();
    adxf_rofs_del_device();
    rofs_if_finish();
    adxf_lib_finish();
    peri_act_pak_shutdown();
    command_shutdown();
    nj_light_exit();
    nj_view_exit();
}

/* Pads, both task lists (update and draw), screen, peripherals, file server. */
/* 0x00100b90 */
static void game_frame_tasks_run(void)
{
    pad_update();
    task_prof_reset();
    task_run_all(system1_root);
    task_run_all(system2_root);
    screen_update();
    peripherals_update();
    filer_is_ready();
    filer_server_step();
    pcfile_update();
}

/* Task updates only, no draw. Debug window input while SYS_DEBUG_INPUT. */
/* 0x00100bf0 */
static void game_frame_tasks_exec(void)
{
    if (system_flags & SYS_DEBUG_INPUT)
        gw_input_update();
    pad_update();
    task_frame_begin();
    task_run_all_exec(system1_root);
    task_run_all_exec(system2_root);
    peripherals_update();
    filer_is_ready();
    filer_server_step();
    pcfile_update();
}

/*
 * Full update and draw, stabilizer_rate - 1 more updates (one per vsync), catch-ups for missed
 * vsyncs (unless SYS_STABILIZER). stabilizer_rate eases one step per frame to frame_rate.
 */
/* 0x00100c60 */
void game_frame_loop(void)
{
    int stamp;
    int prof;
    int steps;
    int n = 1;
    int next;
    FrameTime *ft = &(*(FrameTime *)&sys_state); /* steer(cast) */

    stamp = perform_timer_read();
    ft->stamp = stamp;
    ulConsoleClear();
    ulConsoleSetCursor(0, 0);
    prof = gw_profiler_begin(main_profiler_group, ARGB(0x80, 0x32, 0x32, 0x32));
    game_frame_tasks_run();
    frame_ready--;
    gw_profiler_end(main_profiler_group, prof);
    ft->dt0 = perform_elapsed(stamp);
    steps = stabilizer_rate - 1;
    ft->dt2 = ft->dt0 * (0.1f / ((float)frame_rate * 16.666666f));
    RenderSetCurrentDisplayList(13);
    prof = gw_profiler_begin(main_profiler_group, ARGB(0x80, 0, 0x80, 0x80));
    if (system_flags & SYS_DEBUG_INPUT) {
        screen_frame_hook();
        gw_update();
    }
    gw_profiler_end(main_profiler_group, prof);
    debug_hud();
    ulConsoleDraw();
    while (steps > 0) {
        next = n + 1;
        steps--;
        while (frame_ready <= 0)
            ;
        prof = gw_profiler_begin(main_profiler_group, ARGB(0x80, 0x80, 0x80, 0));
        n = next;
        game_frame_tasks_exec();
        frame_ready--;
        gw_profiler_end(main_profiler_group, prof);
    }
    if (!(system_flags & SYS_STABILIZER))
        frame_ready = 0;
    prof = gw_profiler_begin(main_profiler_group, ARGB(0x80, 0, 0x80, 0));
    while (frame_ready > 0) {
        game_frame_tasks_exec();
        n++;
        frame_ready--;
    }
    gw_profiler_end(main_profiler_group, prof);
    if (stabilizer_rate < frame_rate)
        stabilizer_rate = stabilizer_rate + 1;
    else if (frame_rate < stabilizer_rate)
        stabilizer_rate = stabilizer_rate - 1;
    render_flip();
    frame_update_count = n;
    ft->dt1 = perform_elapsed(stamp);
    ft->dt3 = (*(FrameTime *)&sys_state).dt1 * (0.1f / ((float)frame_rate * 16.666666f)); /* steer(cast) */
}

/* PAL 50/60 Hz choice in sys_state 2; main loop forever. */
/* 0x00100f28 */
int main(int argc, char **argv)
{
    int i;
    int done;
    int sel;
    int prof;

    printf("argc = %d\n", argc);
    for (i = 0; i < argc; i++)
        printf("argv[%d] = %s\n", i, argv[i]);
    ((void (*)(int, char * *))game_init)(argc, argv); /* steer(cast) */
    if (sys_state == 2) {
        do {
            while (frame_ready <= 0)
                ;
            frame_ready = 0;
            done = 0;
            ulConsoleClear();
            ulConsoleSetCursor(0, 0);
            ulGxResetCounters();
            render_submit_frame(1, -1);
            pad_update();
            sel = tv_set_select();
            filer_is_ready();
            ulConsoleDraw();
            render_flip();
            ulGxSyncPath();
            switch (sel) {
            case 0:
                break;
            case 1:
                MainChangeDisplayPAL60(0);
                break;
            case 2:
                MainChangeDisplayPAL60(1);
                break;
            case 3:
                MainChangeDisplayPAL60(0);
                done = 1;
                break;
            case 4:
                MainChangeDisplayPAL60(1);
                done = 1;
                break;
            }
        } while (!done);
    }
    DEBUG_PRINT("\n\n");
    DEBUG_PRINT("***-----------------***\n");
    DEBUG_PRINT("*** K-Project Start ***\n");
    DEBUG_PRINT("***-----------------***\n");
    game_soft_reset_init();
    do {
        for (;;) {
            while (frame_ready <= 0)
                do { ; } while (0);
            ulGxResetCounters();
            perform_timer_read();
            prof = gw_profiler_begin(main_profiler_group, ARGB(0x80, 0x80, 0x80, 0x80));
            if (system_flags & SYS_DEBUG_INPUT)
                gw_input_update();
            game_frame_loop();
            gw_profiler_end(main_profiler_group, prof);
            sound_host_command_sample();
            ulGxSyncPath();
        }
    } while (0);
}

/* SYS_NO_VIBRATION; peripherals and ADX server threads run twice more to settle. */
/* 0x00101198 */
static void system_reset(void)
{
    system_flags |= SYS_NO_VIBRATION;
    peri_act_update_all();
    adxm_run_svr_threads();
    peri_act_update_all();
    adxm_run_svr_threads();
    game_reset_systems();
}
