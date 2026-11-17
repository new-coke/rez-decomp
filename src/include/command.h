/* Debug command line. */

#ifndef REZ_COMMAND_H
#define REZ_COMMAND_H

#include "types_game.h"

struct Node;

void command_reset(void);
void command_shutdown(void);
struct Node *command_find(char *name);
struct Node **command_find_insert_point(char *name);
int command_register(CmdDef *cmd);
void command_register_table(CmdDef *table);
void command_unregister(char **key);
void command_unregister_table(CmdDef *table);
void command_exec(char *line);
void command_line_insert_gap(char *p, int idx);
void command_line_delete_char(char *p, int idx);
void command_line_clear(char *line);
int command_line_update(void);

#endif /* REZ_COMMAND_H */
