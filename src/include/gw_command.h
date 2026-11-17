/* Debug console commands and parser. */

#ifndef REZ_GW_COMMAND_H
#define REZ_GW_COMMAND_H

struct GwCmd;
struct GwCmdDef;

void gw_command_init(void);
void gw_command_exec(char *line);
int gw_command_add(struct GwCmdDef *def);
void gw_command_add_table(struct GwCmdDef *table);
void gw_command_remove(struct GwCmdDef *def);
void gw_command_remove_table(struct GwCmdDef *table);
int gw_line_length(char *s);
int gw_word_equal(char *s1, char *s2, char delim);

#endif /* REZ_GW_COMMAND_H */
