#ifndef REZ_PAUSE_H
#define REZ_PAUSE_H

struct Pause1Work;
struct Pause1WorkView;
struct Pause1State3ExecPause1Work;
struct Pause1State5ExecPause1Work;
struct Pause2Work;

void pause_task_create(void);
void *pause2_get_task(void);

#endif /* REZ_PAUSE_H */
