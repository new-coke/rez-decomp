/* Ending: closing message. */

#ifndef REZ_ENDING_END_MESSAGE_H
#define REZ_ENDING_END_MESSAGE_H

struct EndMessage;

void end_message_center_console(struct EndMessage *w);
void end_message_init(struct EndMessage *w);
void end_message_release(struct EndMessage *w);
void end_message_exec_end(struct EndMessage *w);
void end_message_exec(struct EndMessage *w);
void end_message_disp(void);
void *end_message_create(void *parent);

#endif /* REZ_ENDING_END_MESSAGE_H */
