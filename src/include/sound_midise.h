/* Beat-timed SE requests, SE ports and volumes. */

#ifndef REZ_SOUND_MIDISE_H
#define REZ_SOUND_MIDISE_H

struct MidiEvent;
struct SeRhythm;
struct SeChannelInit;
struct SeRequest;
struct ChangeSymbolSe;

void se_midi_open(int *handle);
void se_midi_close(int handle);
void se_midi_send3(int handle, unsigned char status, unsigned char data1, unsigned char data2);
void se_midi_send_event(int handle, struct MidiEvent *ev);
void se_midi_bank_select(int handle, char ch, char bank);
void se_midi_program_change(int handle, char ch, char prg);
void se_midi_volume(int handle, char ch, char vol);
void se_channel_setup_port3(void);
void se_channel_setup_port1(void);
void se_channel_setup_port2(void);
void se_all_sound_off(void);
void se_port0_set_volume(float vol, int id);
void se_ports_set_volume(float vol, int id);
void *se_rhythm_create(void *parent);
void se_special_trigger(int k);
void SoundRhythmSe_Lockon(int n);
void SoundRhythmSe_ChangeSymbol(int n);
void SoundRhythmSe_ChangeSymbolStop(void);
void change_symbol_se_set_data(int ids, int obj, int pack);
void change_symbol_se_clear_data(void);
void SoundRhythmSe_Laser(int base, int idx);
void se_laser_bank_set(int pack);
void se_laser_bank_clear(void);
void se_stg_common_clear(void);
void SoundRhythmSe_StgCommon(int id);
void SoundRhythmSe_StgCommonStart(int id);
void SoundRhythmSe_StgCommonStop(int id);
void SoundRhythmSe_Common(int id);
void SoundRhythmSe_Enemy(int id, unsigned int se);
void SoundRhythmSe_EnemyPos(int id, unsigned int se, float *pos);
void SoundRhythmSe_Bullet(int id, int se);
void SoundRhythmSe_BulletPos(int id, int se, float *pos);
void se_note_on(int prg, int note);
void se_note_off(int se);
void SoundRhythmSe_Boss(int ch, int note, int grid);
void SoundRhythmSe_BossStart(int ch, int note, int grid);
void SoundRhythmSe_BossStop(int ch, int note, int grid);
void *change_symbol_se_create(void *parent, int idx);
void se_direct_note_on(int bank, int prg, int note);
void SoundMidiSeEnable(int enable);
void SoundMidiSeEnableLaser(int enable);

#endif /* REZ_SOUND_MIDISE_H */
