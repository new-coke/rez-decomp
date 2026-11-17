/* Vibration cues on beats and game events. */

#ifndef REZ_PAD_VIB_H
#define REZ_PAD_VIB_H

struct Pair;

void peri_act_beat_pulse(void);
void peri_act_play_button(void);
void peri_act_play_indexed(void);
void peri_act_play_if_less(struct Pair *pair);
void peri_act_vibration_start(void);
void peri_act_pattern_start(void);
void peri_act_play_symbol_end(void);
void peri_act_sync_scene(int *p);
void peri_act_play_player_dead(void);
void peri_act_pak_open(void);
void peri_act_pak_close(void);
void peri_act_start(int idx);
void peri_act_setup(void);
void peri_act_select_reset(int row);
void peri_act_update(void);
void peri_act_set_port(int port);
void peri_act_set_selection(int row);
void peri_act_play_number(int n);
void peri_act_select_boss(void);
void peri_act_beat_tick(void);
void peri_act_play_boss_break(void);
void peri_act_play_boss_finish(void);

#endif /* REZ_PAD_VIB_H */
