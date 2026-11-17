/* Entry, main loop, frame rate, 50/60 Hz switch. */

#ifndef REZ_MAIN_H
#define REZ_MAIN_H

extern int frame_rate;
extern int stabilizer_rate;

void MainChangeDisplayPAL60(int pal60);
void game_frame_loop(void);
int main(int argc, char **argv);

#endif /* REZ_MAIN_H */
