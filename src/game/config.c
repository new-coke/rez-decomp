/*
 * Game configuration and save data: options, language, the system data block, rankings, stage
 * records and clear counts, and the unlock flags ConfigFlagCalc derives from them.
 */

#include "types_game.h"
#include "types_gw.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "libscf.h"
#include "nj_system.h"
#include "config.h"
#include "enemy_status.h"
#include "module.h"
#include "pad.h"
#include "result_message.h"
#include "system.h"
#include "color.h"

typedef struct { int lang; int idx; } LangMap;

typedef struct { char b[32]; } RankRecord;
typedef struct { RankRecord rec[10]; } RankTable;
typedef struct { char pad_00[4]; unsigned int score; float analyze; float shotdown; float item; char pad_14[12]; } ConfigRankRecord;

typedef struct { int w[8]; } ConfigCountRankRecordsRankRecord;
typedef struct { int idx; int max; } Lim;

/* Stage clear counts of GameState, as seen from its system data block. */
typedef struct CfgClearRec {
    char pad_00[2820];
    int clear[5];                       /* 0xb04: GameState.clear_count */
} CfgClearRec;

/* Hundredths (cs) computed but unused. */
#define FRAMES_SPLIT(frames, min, sec, s, cs) \
    do { \
        (min) = (frames) / 3600; \
        (sec) = (frames) / 60; \
        (s) = (sec) % 60; \
        (cs) = (frames) % 60 % 100; \
    } while (0)

extern GameState game_state;
extern LangMap config_lang_map[];
extern char str_config_lang_index_size[];
extern char str_config_sce_lang[];
extern int sys_state;
extern char config_default_rankings[];
extern char gw_varedit_root[];
extern GwVarDef config_vardef[];
extern GwVarDef config_vardefs[];
extern GwVarDef config_country_vardefs[];
extern GwVarDef config_extra_vardef[];
extern GwVarDef config_extra_vardefs[];
extern GwVarDef config_clrnum_vardef[];
extern GwVarDef config_clrnum_vardefs[];
extern GwVarDef config_ssselflag_vardef[];
extern GwVarDef config_ssselflag_vardefs[];
extern GwVarDef config_srselflag_vardef[];
extern GwVarDef config_srselflag_vardefs[];
extern GwVarDef config_stage01_vardef[];
extern GwVarDef config_stage01_vardefs[];
extern GwVarDef config_stage02_vardef[];
extern GwVarDef config_stage02_vardefs[];
extern GwVarDef config_stage03_vardef[];
extern GwVarDef config_stage03_vardefs[];
extern GwVarDef config_stage04_vardef[];
extern GwVarDef config_stage04_vardefs[];
extern GwVarDef config_stage05_vardef[];
extern GwVarDef config_stage05_vardefs[];
extern int game_state_unk_04;
extern char str_system_data_version_failed[];
extern char str_system_data_load[];
extern char str_config_stereo[];
extern char str_config_vol_se[];
extern char str_config_vol_seq[];
extern char str_config_updown[];
extern char str_config_shot_button[];
extern char str_config_od_button[];
extern char str_config_vibration[];
extern char str_config_disp_console[];
extern char str_config_disp_score[];
extern char str_config_vms_connect[];
extern char str_config_vms_disconnect[];
extern StageRecord config_stage_records[];
extern int player_status_get_game_analyzed(void);
extern int player_status_get_game_analyze_total(void);
extern int player_status_get_stage_analyzed(void);
extern int player_status_get_stage_analyze_total(void);
extern int player_status_get_game_item_total(void);
extern int player_status_get_game_items_got(void);
extern int player_status_get_stage_item_total(void);
extern int player_status_get_stage_items_got(void);
extern RankTable config_rankings[];
extern int player_status_get_score(void);
extern char str_config_regist_area05[];
extern char str_config_regist_ending[];
extern char str_config_set_clear_flag[];
extern char str_config_set_clear_flag_bad_end[];
extern int config_game_clears;
extern int config_score_attack_clears;
extern void sound_set_stereo(void);
extern int config_vol_se;
extern void sound_set_se_volume_level(int);
extern int config_vol_seq;
extern void sound_set_seq_volume_level(int);
extern int config_updown;
extern int pad_updown;
extern int config_shot_button;
extern int config_od_button;
extern int config_vibration;
extern int config_disp_console;
extern void player_status_set_disp_flags(unsigned int);
extern void player_status_clear_disp_flags(unsigned int);
extern int config_disp_score;
extern int extype;
extern Lim config_setting_limits[];
extern char str_config_setting_max[];
extern CfgClearRec system_data;
extern int config_play_hour_levels[];
extern char str_config_flag_calc[];
extern char str_config_play_time[];
extern char str_config_analyze_95[];
extern char str_config_analyze_100[];
extern char str_config_shotdown_95[];
extern char str_config_shotdown_100[];
extern char str_config_item_100[];
extern char str_config_top_rank[];
extern char str_config_rankin[];
extern char str_config_score_attack_clear[];
extern char str_config_flag_calc_end[];
extern RankRecord *ConfigGetRankRecord(int table, int i);
extern int ConfigRankingFind(int table, ConfigRankRecord *rec);

/* All 8 tables. */
/* 0x0010a070 */
void ConfigRankingCopy(char *dst, char *src)
{
    int i;

    for (i = 8; i != 0; i--) {
        nj_copy8(dst, src, 320);
        src += 320;
        dst += 320;
    }
}

/* From the console language via config_lang_map (6 entries); country and language default to 1. */
/* 0x0010a0c8 */
void ConfigLanguageInit(void)
{
    GameState *game = &game_state;
    int lang;
    unsigned int i;
    LangMap *map;

    game->country = 1;
    game->lang = 1;
    lang = sceScfGetLanguage();
    printf(str_config_lang_index_size, 6);
    printf(str_config_sce_lang, lang);
    for (i = 0, map = config_lang_map; i < 6; i++, map++) {
        if (map->lang == lang)
            game->lang = map->idx;
    }
}

/*
 * 50 or 60 fps by video mode, stereo, volumes 4, pad's current buttons, console shown, default
 * rankings and colours; select entry 4 set, others clear.
 */
/* 0x0010a158 */
void game_state_init(void)
{
    GameState *game = &game_state;

    nj_fill8(game, 0, 3144);
    ConfigLanguageInit();
    if (sys_state == 2)
        game->fps = 50;
    else
        game->fps = 60;
    game->cfg.stereo = 1;
    game->cfg.unk_48 = 1;
    game->autosave_on = 0;
    game->vms_slot = 0;
    game->cfg.vol_se = 4;
    game->cfg.vol_seq = 4;
    game->cfg.updown = 0;
    game->cfg.shot_button = pad_button_assign_get(0, 0);
    game->cfg.od_button = pad_button_assign_get(0, 1);
    game->cfg.disp_console = 1;
    game->cfg.vibration = 0;
    game->cfg.disp_score = 0;
    game->vms_link = 0;
    game->vms_crc = 0;
    game->data_crc = 0;
    ConfigRankingCopy((char *)game + 108, config_default_rankings);
    game->line_color = RGBA(0xff, 0x8c, 0, 0x80);
    game->menu_color = RGBA(0x80, 0x46, 0, 0x80);
    game->unk_c20 = 0;
    ConfigSetSelect(0, 0);
    ConfigSetSelect(1, 0);
    ConfigSetSelect(2, 0);
    ConfigSetSelect(3, 0);
    ConfigSetSelect(4, 1);
    ConfigSetSelect(5, 0);
    ConfigSetSelect(6, 0);
    ConfigSetSelect(7, 0);
    ConfigSetSelect(8, 0);
}

/* 0x0010a2a8 */
void config_vars_register(void)
{
    struct GwVarNode *parent;

    parent = gw_varedit_add(gw_varedit_root, 0, config_vardef);
    gw_varedit_add_table(gw_varedit_root, parent, config_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_extra_vardef), config_extra_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_clrnum_vardef), config_clrnum_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_ssselflag_vardef), config_ssselflag_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_srselflag_vardef), config_srselflag_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_stage01_vardef), config_stage01_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_stage02_vardef), config_stage02_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_stage03_vardef), config_stage03_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_stage04_vardef), config_stage04_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, parent, config_stage05_vardef), config_stage05_vardefs);
    gw_varedit_add_table(gw_varedit_root, parent, config_country_vardefs);
}

/* 0x0010a478 */
void ConfigStageLoad(void)
{
}

/* First game state word. */
/* 0x0010a480 */
void config_replay_state_set(int *src)
{
    (*(int *)&game_state) = *src; /* steer(cast) */
}

/* Reads the second word (game_state_unk_04) while config_replay_state_set writes the first. */
/* 0x0010a490 */
void config_replay_state_get(int *dst)
{
    *dst = game_state_unk_04;
}

/* 0x0010a4a0 */
int system_data_get_size(void)
{
    return 2944;
}

/* Stamps version 18, play time, clock time; marks the game linked to its card. */
/* steer(cast) */
/* 0x0010a4a8 */
int *system_data_init(void)
{
    GameState *game = &game_state;
    int *sysdata = (int *)game->sysdata;

    sysdata[1] = 18;
    system_time_save(game->sysdata + 20);
    ((void (*)(void *))nj_clock_read)(game->save_time);
    nj_copy8(sysdata, sysdata, 2944);
    game->data_crc = 0;
    game->vms_link = 1;
    return sysdata;
}

/* Rejects anything but version 18; reapplies every option setter and the unlock flags. */
/* 0x0010a510 */
int system_data_load(int *src)
{
    GameState *game = &game_state;

    if (src[1] != 18) {
        ERROR_PRINTF(str_system_data_version_failed, src[1], 18);
        return 0;
    }
    system_time_load(src + 5);
    ConfigGetStereo();
    nj_copy8(game->sysdata, src, 2944);
    DEBUG_PRINTF(str_system_data_load);
    DEBUG_PRINTF(str_config_stereo, ConfigGetStereo());
    DEBUG_PRINTF(str_config_vol_se, ConfigGetVolSe());
    DEBUG_PRINTF(str_config_vol_seq, ConfigGetVolSeq());
    DEBUG_PRINTF(str_config_updown, ConfigGetUpdown());
    DEBUG_PRINTF(str_config_shot_button, ConfigGetShotButton());
    DEBUG_PRINTF(str_config_od_button, ConfigGetOdButton());
    DEBUG_PRINTF(str_config_vibration, ConfigGetVibration());
    DEBUG_PRINTF(str_config_disp_console, ConfigGetDispConsole());
    DEBUG_PRINTF(str_config_disp_score, ConfigGetDispScore());
    ConfigSetStereo(ConfigGetStereo());
    ConfigSetVolSe(ConfigGetVolSe());
    ConfigSetVolSeq(ConfigGetVolSeq());
    ConfigSetUpdown(ConfigGetUpdown());
    ConfigSetShotButton(ConfigGetShotButton());
    ConfigSetOdButton(ConfigGetOdButton());
    ConfigSetVibration(ConfigGetVibration());
    ConfigSetDispConsole(ConfigGetDispConsole());
    ConfigSetDispScore(ConfigGetDispScore());
    game->data_crc = 0;
    ConfigFlagCalc();
    result_message_state_init();
    return 1;
}

/* link: CRC of the card's system file. */
/* 0x0010a710 */
void ConfigSetVmsLink(int link)
{
    GameState *game = &game_state;
    game->vms_link = 1;
    game->vms_crc = link;
    game->data_crc = link;
}

/* Drops the link if slot is the linked one. Returns the card change count. */
/* 0x0010a730 */
int ConfigVmsConnect(int slot)
{
    DEBUG_PRINTF(str_config_vms_connect, slot);
    if (game_state.vms_slot == slot)
        game_state.vms_link = 0;
    return ++game_state.card_changed;
}

/* Drops the link if slot is the linked one. */
/* 0x0010a780 */
void ConfigVmsDisconnect(int slot)
{
    DEBUG_PRINTF(str_config_vms_disconnect, slot);
    if (game_state.vms_slot == slot) {
        game_state.vms_link = 0;
        game_state.vms_crc = 0;
    }
}

/* Stage 5 shares stage 4's record. */
/* 0x0010a7c8 */
StageRecord *ConfigGetStageRecord(int i)
{
    if (i == 5)
        i = 4;
    return &config_stage_records[i];
}

/* Layers analyzed / all layers, percent; 100 when none. Rate functions below likewise. */
/* 0x0010a7e8 */
float ConfigGameAnalyzeRate(void)
{
    int n = player_status_get_game_analyzed();
    int total = player_status_get_game_analyze_total();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* 0x0010a848 */
float ConfigStageAnalyzeRate(void)
{
    int n = player_status_get_stage_analyzed();
    int total = player_status_get_stage_analyze_total();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* Destroyed / appeared, percent. */
/* 0x0010a8a8 */
float ConfigGameShotdownRate(void)
{
    int total = enemy_status_get_gm_appear();
    int n = enemy_status_get_gm_die();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* 0x0010a908 */
float ConfigStageShotdownRate(void)
{
    int total = enemy_status_get_stg_appear();
    int n = enemy_status_get_stg_die();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* Taken / appeared, percent. */
/* 0x0010a968 */
float ConfigGameItemRate(void)
{
    int total = player_status_get_game_item_total();
    int n = player_status_get_game_items_got();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* 0x0010a9c8 */
float ConfigStageItemRate(void)
{
    int total = player_status_get_stage_item_total();
    int n = player_status_get_stage_items_got();

    if (total <= 0)
        return 100.0f;
    return (float)n * 100.0f / (float)total;
}

/* i clamped to 0..9. */
/* 0x0010aa28 */
RankRecord *ConfigGetRankRecord(int table, int i)
{
    if (i < 0)
        i = 0;
    if (i > 9)
        i = 9;
    return &config_rankings[table].rec[i];
}

/* 0..9, 10 for none. Order: score, analyze, shot-down, item (whole percent). Ties rank below. */
/* 0x0010aa68 */
int ConfigRankingFind(int table, ConfigRankRecord *rec)
{
    ConfigRankRecord *ent = config_rankings[table].rec;
    int rank;
    int rec_pct;

    for (rank = 0; rank < 10; rank++, ent++) {
    int ent_pct;
        if (rec->score < ent->score)
            continue;
        if (ent->score < rec->score)
            break;
        ent_pct = (int)(ent->analyze * 100.0f);
        rec_pct = (int)(rec->analyze * 100.0f);
        if (rec_pct < ent_pct)
            continue;
        if (ent_pct < rec_pct)
            break;
        ent_pct = (int)(ent->shotdown * 100.0f);
        rec_pct = (int)(rec->shotdown * 100.0f);
        if (rec_pct < ent_pct)
            continue;
        if (ent_pct < rec_pct)
            break;
        ent_pct = (int)(ent->item * 100.0f);
        rec_pct = (int)(rec->item * 100.0f);
        if (ent_pct < rec_pct)
            break;
    }
    return rank;
}

/* Last entry drops out. Returns the rank, 10 if unranked. */
/* 0x0010ab78 */
int ConfigRankingInsert(int table, void *rec)
{
    int pos = ConfigRankingFind(table, rec);
    RankRecord *dst;
    RankRecord *p;

    if (pos < 10) {
        dst = &config_rankings[table].rec[pos];
        for (p = &config_rankings[table].rec[9]; p > dst; p--)
            nj_copy8(p, p - 1, 32);
        nj_copy8(dst, rec, 32);
        ConfigFlagCalc();
    }
    return pos;
}

/* Whole-game percentages, empty name. */
/* 0x0010ac38 */
void ConfigMakeGameRecord(ResultRecord *rec)
{
    rec->name[0] = 0;
    rec->name[1] = 0;
    rec->name[2] = 0;
    rec->name[3] = 0;
    rec->score = player_status_get_score();
    rec->analyze = ConfigGameAnalyzeRate();
    rec->shotdown = ConfigGameShotdownRate();
    rec->item = ConfigGameItemRate();
    rec->one = 1;
}

/* Current stage's percentages. */
/* 0x0010ac90 */
void ConfigMakeStageRecord(ResultRecord *rec)
{
    rec->name[0] = 0;
    rec->name[1] = 0;
    rec->name[2] = 0;
    rec->name[3] = 0;
    rec->score = player_status_get_score();
    rec->analyze = ConfigStageAnalyzeRate();
    rec->shotdown = ConfigStageShotdownRate();
    rec->item = ConfigStageItemRate();
    rec->one = 1;
}

/*
 * Keeps the best percentages. Area 5 (stage 4) records nothing; stage 5 counts as stage 4, only
 * with module clear flag 1.
 */
/* 0x0010ace8 */
void ConfigStageDataRegist(int stage, float *data)
{
    StageRecord *rec;

    if (stage == 4) {
        DEBUG_PRINTF(str_config_regist_area05);
        return;
    }
    if (stage == 5) {
        if (module_get_clear_flag(1) == 0) {
            DEBUG_PRINTF(str_config_regist_ending);
            return;
        }
        stage = 4;
    }
    rec = ConfigGetStageRecord(stage);
    if (rec->analyze <= data[2])
        rec->analyze = data[2];
    if (rec->shotdown <= data[3])
        rec->shotdown = data[3];
    if (rec->item <= data[4])
        rec->item = data[4];
    ConfigFlagCalc();
}

/* Stage 4/5 rules as ConfigStageDataRegist; records the stage in module clear flag 4. */
/* 0x0010adb0 */
void ConfigSetClearFlag(int stage, int flag)
{
    GameState *game = &game_state;
    int ok = 1;

    DEBUG_PRINTF(str_config_set_clear_flag, stage, flag);
    if (stage == 4)
        return;
    if (stage == 5) {
        stage = 4;
        if (module_get_clear_flag(1) == 0)
            ok = 0;
    }
    if (!ok)
        DEBUG_PRINTF(str_config_set_clear_flag_bad_end);
    else if (flag)
        game->clear_count[stage]++;
    ConfigFlagCalc();
    module_set_clear_flag(4, stage + 1);
}

/* 0x0010ae90 */
int ConfigGetClearCount(int i)
{
    return game_state.clear_count[i];
}

/* Select entry 0 == 5 sets setting 5; setting 0 rises to select entry 0 + 2 (max 6). */
/* 0x0010aea8 */
void ConfigAddGameClear(int cnt)
{
    GameState *game = &game_state;
    int *sysdata = (int *)game->sysdata;
    int a;
    int b;

    sysdata[2864 / 4] += cnt;
    a = ConfigGetSelect(0);
    b = a + 1;
    if (b == 6)
        ConfigSetSetting(5, 1);
    b = a + 2;
    if (b > 6)
        b = 6;
    ConfigSetSetting(0, b < ConfigGetSetting(0) ? ConfigGetSetting(0) : b);
    ConfigFlagCalc();
}

/* 0x0010af50 */
int ConfigGetGameClear(void)
{
    return config_game_clears;
}

/* Not for kind 4. */
/* 0x0010af60 */
void ConfigAddScoreAttackClear(int kind, int cnt)
{
    GameState *game = &game_state;
    int *sysdata = (int *)game->sysdata;
    if (kind == 4)
        return;
    sysdata[2868 / 4] += cnt;
    ConfigFlagCalc();
}

/* 0x0010af90 */
int ConfigGetScoreAttackClear(void)
{
    return config_score_attack_clears;
}

/* 0x0010afa0 */
int ConfigGetStereo(void)
{
    return config_stereo;
}

/* 0x0010afb0 */
void ConfigSetStereo(int stereo)
{
    config_stereo = stereo;
    sound_set_stereo();
}

/* 0x0010afc0 */
int ConfigGetVolSe(void)
{
    return config_vol_se;
}

/* 0..4. */
/* 0x0010afd0 */
void ConfigSetVolSe(int vol)
{
    if (vol < 0)
        vol = 0;
    if (vol > 4)
        vol = 4;
    config_vol_se = vol;
    sound_set_se_volume_level(vol);
}

/* 0x0010aff8 */
int ConfigGetVolSeq(void)
{
    return config_vol_seq;
}

/* 0..4. */
/* 0x0010b008 */
void ConfigSetVolSeq(int vol)
{
    if (vol < 0)
        vol = 0;
    if (vol > 4)
        vol = 4;
    config_vol_seq = vol;
    sound_set_seq_volume_level(vol);
}

/* 0x0010b030 */
int ConfigGetUpdown(void)
{
    return config_updown;
}

/* Vertical inversion. */
/* 0x0010b040 */
void ConfigSetUpdown(int updown)
{
    pad_updown = updown;
    config_updown = updown;
}

/* 0x0010b058 */
int ConfigGetShotButton(void)
{
    return config_shot_button;
}

/* Pad 0. */
/* 0x0010b068 */
void ConfigSetShotButton(int button)
{
    config_shot_button = button;
    pad_button_assign_set(0, 0, button);
}

/* 0x0010b080 */
int ConfigGetOdButton(void)
{
    return config_od_button;
}

/* Pad 0. */
/* 0x0010b090 */
void ConfigSetOdButton(int button)
{
    config_od_button = button;
    pad_button_assign_set(0, 1, button);
}

/* 0x0010b0a8 */
int ConfigGetVibration(void)
{
    return config_vibration;
}

/* 0x0010b0b8 */
void ConfigSetVibration(int on)
{
    config_vibration = on;
}

/* 0x0010b0c8 */
int ConfigGetDispConsole(void)
{
    return config_disp_console;
}

/* Off sets player display flag 0x80000000. */
/* 0x0010b0d8 */
void ConfigSetDispConsole(int disp)
{
    config_disp_console = disp;
    if (disp)
        player_status_clear_disp_flags(0x80000000);
    else
        player_status_set_disp_flags(0x80000000);
}

/* 0x0010b0f8 */
int ConfigGetDispScore(void)
{
    return config_disp_score;
}

/* On sets player display flag 0x40000000. */
/* 0x0010b108 */
void ConfigSetDispScore(int disp)
{
    config_disp_score = disp;
    if (disp)
        player_status_set_disp_flags(0x40000000);
    else
        player_status_clear_disp_flags(0x40000000);
}

/* 0x0010b128 */
void ConfigSetExtraType(int type)
{
    extype = type;
}

/* 0x0010b138 */
int ConfigGetExtraType(void)
{
    return extype;
}

/* 0x0010b148 */
void ConfigSetSelect(int i, int val)
{
    game_state.select[i] = val;
}

/* 0x0010b160 */
void ConfigSetSetting(int i, int val)
{
    game_state.setting.all[i] = val;
}

/* 0x0010b178 */
void ConfigSetSettingSeen(int i, int val)
{
    game_state.setting_seen[i] = val;
}

/* 0x0010b190 */
int ConfigGetSelect(int i)
{
    return game_state.select[i];
}

/* 0x0010b1a8 */
int ConfigGetSetting(int i)
{
    return game_state.setting.all[i];
}

/* 0x0010b1c0 */
int ConfigGetSettingSeen(int i)
{
    return game_state.setting_seen[i];
}

/* 0x0010b1d8 */
void ConfigStageRelease(void)
{
    ConfigFlagCalc();
}

/* First 5 tables: *total with a player record, *first with one on top (defaults unmarked). */
/* 0x0010b1e0 */
void ConfigCountRankRecords(int *first, int *total)
{
    int i;
    int j;
    int top = 0;
    int n = 0;

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 10; j++) {
            if (((ConfigCountRankRecordsRankRecord * (*)(int, int))ConfigGetRankRecord)(i, j)->w[5] != 0) { /* steer(cast) */
                n++;
                if (j == 0)
                    top++;
                break;
            }
        }
    }
    *first = top;
    *total = n;
}

/* The 9 settings in config_setting_limits. */
/* 0x0010b298 */
void ConfigLimitCheck(void)
{
    unsigned int i;
    Lim *lim = config_setting_limits;
    int idx;
    int mx;
    int cur;

    for (i = 0; i < 9; i++) {
        idx = lim->idx;
        mx = lim->max;
        cur = ConfigGetSetting(idx);
        lim++;
        if (mx < cur) {
            DEBUG_PRINTF(str_config_setting_max, cur);
            cur = mx;
        }
        ConfigSetSetting(idx, cur);
    }
}

/*
 * Inputs: clears, stage records at 95/100%, ranking tables 6-7, score attack clears (a level
 * per 5, max 7), play time (5 h or a stage 5 clear -> settings 1, 17, 28).
 */
/* 0x0010b338 */
void ConfigFlagCalc(void)
{
    GameState *game = &game_state;
    CfgClearRec *clear_rec;
    int i;
    int ana95, ana100, shot95, shot100, item;
    int rankin, top;
    int clr;
    int frames, min, sec, ss, cs, hh, mm;
    int fl, lv, hour_lv, unlock;
    float *stage_rec;
    int *setting, *cp;

    frames = play_time;
    FRAMES_SPLIT(frames, min, sec, ss, cs);
    clear_rec = &system_data;
    do { } while (0);
    hh = min / 60;
    mm = min % 60;
    ana95 = 0;
    ana100 = 0;
    shot95 = 0;
    shot100 = 0;
    item = 0;
    for (i = 0, setting = &game->setting.all[20], cp = game->clear_count; i < 5U; i++) {
        cp += ((int)cp & 1) >> 1; /* steer(zero) */
        if (*cp > 0) {
            setting[-11] = 1;
            *setting = 1;
            stage_rec = ConfigGetStageRecord(i);
            if (stage_rec[0] >= 95.0f)
                ana95++;
            if (stage_rec[0] >= 100.0f)
                ana100++;
            if (stage_rec[1] >= 95.0f)
                shot95++;
            if (stage_rec[1] >= 100.0f)
                shot100++;
            if (stage_rec[2] >= 100.0f)
                item++;
        }
        setting++;
        cp++;
    }
    ConfigCountRankRecords(&top, &rankin);
    clr = ConfigGetScoreAttackClear();
    DEBUG_PRINT(str_config_flag_calc);
    frames += (clr & 1) >> 1; /* steer(zero) */
    DEBUG_PRINTF(str_config_play_time, frames, hh, mm, ss);
    DEBUG_PRINTF(str_config_analyze_95, ana95);
    DEBUG_PRINTF(str_config_analyze_100, ana100);
    DEBUG_PRINTF(str_config_shotdown_95, shot95);
    DEBUG_PRINTF(str_config_shotdown_100, shot100);
    DEBUG_PRINTF(str_config_item_100, item);
    DEBUG_PRINTF(str_config_top_rank, top);
    DEBUG_PRINTF(str_config_rankin, rankin);
    DEBUG_PRINTF(str_config_score_attack_clear, clr);
    DEBUG_PRINT(str_config_flag_calc_end);
    if (clear_rec->clear[0] > 0)
        game->setting.all[10] = 1;
    if (clear_rec->clear[1] > 0)
        game->setting.all[11] = 1;
    if (clear_rec->clear[2] > 0)
        game->setting.all[12] = 1;
    if ((ana100 & 1) >> 1) /* steer(zero) */
        game->setting.all[13] = 0;
    if (ana100 > 3)
        game->setting.all[13] = 1;
    if (clear_rec->clear[4] > 0 && ConfigGetSetting(0) <= 0)
        ConfigSetSetting(0, 1);
    unlock = clear_rec->clear[4] > 0;
    if (hh >= 5)
        unlock = 1;
    if (unlock) {
        game->setting.all[17] = 1;
        game->setting.all[28] = 1;
        ConfigSetSetting(1, 1);
    }
    if (((int * (*)(int, int))ConfigGetRankRecord)(7, 0)[5] != 0) { /* steer(cast) */
        game->setting.all[18] = 1;
        ConfigSetSetting(2, 1);
    }
    if (shot95 > 4) {
        game->setting.all[16] = 1;
        game->setting.all[27] = 1;
        ConfigSetSetting(3, 1);
    }
    lv = 0;
    if (shot100 > 0)
        lv = shot100 + 2;
    if (shot100 > 4 && item > 4)
        lv = 8;
    for (hour_lv = 0; hour_lv < 8U; hour_lv++)
        if (hh < config_play_hour_levels[hour_lv])
            break;
    if (lv < hour_lv)
        lv = hour_lv;
    ConfigSetSetting(4, lv);
    {
        int fives = clr / 5;
        int level = fives > 0;

        level += fives;
        if (level > 7)
            level = 7;
        ConfigSetSetting(6, level);
    }
    if (((int * (*)(int, int))ConfigGetRankRecord)(6, 0)[5] != 0) /* steer(cast) */
        ConfigSetSetting(7, 1);
    if (top > 1)
        ConfigSetSetting(8, top);
    ConfigLimitCheck();
    unlock = 4; /* steer(dead) */
}

static inline CfgClearRec *cfg_clear_rec(void)
{
    return &system_data;
}
