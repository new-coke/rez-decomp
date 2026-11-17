/* Ending: model scene. */

#ifndef REZ_ENDING_END_MODEL_H
#define REZ_ENDING_END_MODEL_H

struct EndModel;

void end_model_init(struct EndModel *w);
void end_model_release(struct EndModel *w);
void end_model_exec(struct EndModel *w);
void end_model_disp(struct EndModel *w);
void *end_model_create(void *parent, int prm);

#endif /* REZ_ENDING_END_MODEL_H */
