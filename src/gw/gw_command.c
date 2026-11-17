/* Debug console commands: name-sorted command list, line parser, built-in 'help'. */

#include "types_gw.h"
#include "libc.h"
#include "gw_command.h"
#include "gw_console.h"
#include "gw_heap.h"

static GwCmd *gw_command_find(char *line);
static void gw_command_add_builtins(void);
static void gw_command_remove_builtins(void);
static int gw_word_compare(char *name, char *word);

extern GwCmd *gw_command_list;
extern int gw_command_result;
extern GwCmdDef gw_command_builtins[];

/* Adds the built-ins. */
/* 0x001960a8 */
void gw_command_init(void)
{
    gw_command_list = 0;
    gw_command_add_builtins();
}

/* 0x001960b0 */
static void gw_command_exit(void)
{
    GwCmd *n;
    GwCmd *cmd;

    gw_command_remove_builtins();
    n = gw_command_list;
    if (n) {
        do {
            cmd = n;
            n = n->next;
            gw_dc_free(cmd, "gw_command.c", 77);
        } while (n);
    }
    gw_command_list = 0;
}

/* 0x00196108 */
static int gw_command_get_result(void)
{
    return gw_command_result;
}

/* Splits in place into up to ten space-separated words; unknown commands are ignored. */
/* 0x00196110 */
void gw_command_exec(char *line)
{
    char *argv[11];
    char **av;
    int argc;
    int start;
    int none;
    GwCmd *cmd;

    cmd = gw_command_find(line);
    av = argv;
    none = (cmd == 0);
    argc = 0;
    start = 1;
    while (*line && argc < 10) {
        if (*line == ' ') {
            *line = 0;
            start = 1;
        } else if (start) {
            *av++ = line;
            argc++;
            start = 0;
        }
        line++;
    }
    *av = 0;
    if (!none) {
        if (cmd->def.fn)
            gw_command_result = cmd->def.fn(argc, argv);
    }
}

/* By the first word of line; 0 if none. */
/* 0x001961d0 */
static GwCmd *gw_command_find(char *line)
{
    GwCmd *n;

    for (n = gw_command_list; n; n = n->next) {
        if (gw_word_equal(n->def.name, line, 32))
            return n;
    }
    return 0;
}

/* Returns the link (head or a node's next) to insert at. */
/* 0x00196228 */
static GwCmd **gw_command_insert_pos(char *name)
{
    GwCmd **pp;

    pp = &gw_command_list;
    while (*pp && gw_word_compare((*pp)->def.name, name) >= 0)
        pp = &(*pp)->next;
    return pp;
}

/* Copies def, in name order. 0 if the name is taken or no memory. */
/* 0x00196288 */
int gw_command_add(GwCmdDef *def)
{
    GwCmd *n;
    GwCmd **pp;

    if (gw_command_find(def->name))
        return 0;
    n = gw_dc_malloc(16, "gw_command.c", 215);
    if (n == 0)
        return 0;
    n->def = *def;
    n->next = 0;
    pp = gw_command_insert_pos(def->name);
    if (pp == 0) {
        gw_dc_free(n, "gw_command.c", 224);
        return 0;
    }
    n->next = *pp;
    *pp = n;
    return 1;
}

/* Table ends at a null name. */
/* 0x00196340 */
void gw_command_add_table(GwCmdDef *table)
{
    while (table->name)
        gw_command_add(table++);
}

/* 0x00196388 */
void gw_command_remove(GwCmdDef *def)
{
    GwCmd *n;
    GwCmd *prev;

    for (prev = n = gw_command_list; n; n = n->next) {
        if (strcmp(n->def.name, def->name) == 0) {
            if (n == gw_command_list)
                gw_command_list = n->next;
            else
                prev->next = n->next;
            gw_dc_free(n, "gw_command.c", 270);
            break;
        }
        prev = n;
    }
}

/* Table ends at a null name. */
/* 0x00196420 */
void gw_command_remove_table(GwCmdDef *table)
{
    while (table->name)
        gw_command_remove(table++);
}

/* Lists every command with its help text. */
/* 0x00196468 */
static int gw_command_help(void)
{
    GwCmd *n;

    n = gw_command_list;
    gw_console_puts("COMMAND######HELP#########\n");
    for (; n; n = n->next)
        gw_console_printf("%-12s %s\n", n->def.name, n->def.help);
    return 0;
}

/* 0x001964c8 */
static void gw_command_add_builtins(void)
{
    gw_command_add_table(gw_command_builtins);
}

/* 0x001964d8 */
static void gw_command_remove_builtins(void)
{
    gw_command_remove_table(gw_command_builtins);
}

/* Up to the end of the string or a newline. */
/* 0x001964e8 */
int gw_line_length(char *s)
{
    int n;

    n = 0;
    while (*s && *s != 10) {
        s++;
        n++;
    }
    return n;
}

/* Each word ends at the end of the string or delim. */
/* 0x00196528 */
int gw_word_equal(char *s1, char *s2, char delim)
{
    while (*s1 != 0 && *s1 != delim && *s2 != 0 && *s2 != delim) {
        if (*s1++ != *s2++)
            return 0;
    }
    if ((*s1 == 0 || *s1 == delim) && (*s2 == 0 || *s2 == delim))
        return 1;
    return 0;
}

/* word ends at a space or the end. 0 equal, -1 name after word, 1 word after name. */
/* 0x001965b8 */
static int gw_word_compare(char *name, char *word)
{
    while (*name != 0 && *word != 0 && *word != ' ') {
        if (*name != *word)
            return *name <= *word ? 1 : -1;
        name++;
        word++;
    }
    if (*name == 0) {
        if (*word == 0 || *word == ' ')
            return 0;
        return 1;
    }
    if (*word == 0 || *word == ' ')
        return -1;
    return 0;
}
