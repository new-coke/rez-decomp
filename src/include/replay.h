/* Replay: pad streams, playback, file format. */

#ifndef REZ_REPLAY_H
#define REZ_REPLAY_H

#include "types_game.h"

struct Replay;
struct ReplayStream;
struct ReplayPad;

void replay_reset(void);
int replay_is_playing(void);
void replay_dig_stream_init(struct Replay *replay, struct ReplayStream *stream, unsigned short *buf);
void replay_ana_stream_init(struct Replay *replay, struct ReplayStream *stream, signed char *buf);
void replay_streams_init(struct Replay *replay);
void replay_set_mode(ReplayMode mode);
void replay_suspend(void);
void replay_resume(void);
void ReplayStart(void);
void ReplayEnd(void);
int replay_get_frame(void);
void replay_frame(struct ReplayPad *pad);
void replay_debug_disp(void);
void replay_read_pad(struct ReplayPad *pad);
void replay_read_ana_stream(struct ReplayStream *stream, short *out);
void replay_read_ana(short *out);
int replay_calc_size(void);
char *ReplaySaveHeader(struct Replay *replay, char *buf);
ReplayDigEnt *ReplaySaveDigital(struct Replay *replay, ReplayDigEnt *dst, ReplayDigEnt *src, struct ReplayStream *stream);
ReplayAnaEnt *ReplaySaveAnalog(struct Replay *replay, ReplayAnaEnt *dst, ReplayAnaEnt *src, struct ReplayStream *stream);
void ReplayMakeData(char *buf);
char *ReplayLoadHeader(struct Replay *replay, int *src, int *ok);
ReplayDigEnt *ReplayLoadDigital(struct Replay *replay, ReplayDigEnt *src, ReplayDigEnt *dst, struct ReplayStream *stream);
ReplayAnaEnt *ReplayLoadAnalog(struct Replay *replay, ReplayAnaEnt *src, ReplayAnaEnt *dst, struct ReplayStream *stream);
void ReplayLoadData(char *buf);

#endif /* REZ_REPLAY_H */
