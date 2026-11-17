/* Unlock messages, high score notice. */

#ifndef REZ_RESULT_MESSAGE_H
#define REZ_RESULT_MESSAGE_H

struct ResultMsgDispWork;

void *result_message_disp_create(void *parent, int alt_pos);
int result_message_is_done(struct ResultMsgDispWork *w);
void *result_message_create_child(void *parent);
void result_message_state_init(void);

#endif /* REZ_RESULT_MESSAGE_H */
