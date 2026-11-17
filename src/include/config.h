/* Options and save data: rankings, stage records, unlocks. */

#ifndef REZ_CONFIG_H
#define REZ_CONFIG_H

#include "types_game.h"

struct StageRecord;

extern int config_stereo;
extern int play_time;

void ConfigRankingCopy(char *dst, char *src);
void ConfigLanguageInit(void);
void game_state_init(void);
void config_vars_register(void);
void ConfigStageLoad(void);
void config_replay_state_set(int *src);
void config_replay_state_get(int *dst);
int system_data_get_size(void);
int *system_data_init(void);
int system_data_load(int *src);
void ConfigSetVmsLink(int link);
int ConfigVmsConnect(int slot);
void ConfigVmsDisconnect(int slot);
struct StageRecord *ConfigGetStageRecord(int i);
float ConfigGameAnalyzeRate(void);
float ConfigStageAnalyzeRate(void);
float ConfigGameShotdownRate(void);
float ConfigStageShotdownRate(void);
float ConfigGameItemRate(void);
float ConfigStageItemRate(void);
int ConfigRankingInsert(int table, void *rec);
void ConfigMakeGameRecord(ResultRecord *rec);
void ConfigMakeStageRecord(ResultRecord *rec);
void ConfigStageDataRegist(int stage, float *data);
void ConfigSetClearFlag(int stage, int flag);
int ConfigGetClearCount(int i);
void ConfigAddGameClear(int cnt);
int ConfigGetGameClear(void);
void ConfigAddScoreAttackClear(int kind, int cnt);
int ConfigGetScoreAttackClear(void);
int ConfigGetStereo(void);
void ConfigSetStereo(int stereo);
int ConfigGetVolSe(void);
void ConfigSetVolSe(int vol);
int ConfigGetVolSeq(void);
void ConfigSetVolSeq(int vol);
int ConfigGetUpdown(void);
void ConfigSetUpdown(int updown);
int ConfigGetShotButton(void);
void ConfigSetShotButton(int button);
int ConfigGetOdButton(void);
void ConfigSetOdButton(int button);
int ConfigGetVibration(void);
void ConfigSetVibration(int on);
int ConfigGetDispConsole(void);
void ConfigSetDispConsole(int disp);
int ConfigGetDispScore(void);
void ConfigSetDispScore(int disp);
void ConfigSetExtraType(int type);
int ConfigGetExtraType(void);
void ConfigSetSelect(int i, int val);
void ConfigSetSetting(int i, int val);
void ConfigSetSettingSeen(int i, int val);
int ConfigGetSelect(int i);
int ConfigGetSetting(int i);
int ConfigGetSettingSeen(int i);
void ConfigStageRelease(void);
void ConfigCountRankRecords(int *first, int *total);
void ConfigLimitCheck(void);
void ConfigFlagCalc(void);

#endif /* REZ_CONFIG_H */
