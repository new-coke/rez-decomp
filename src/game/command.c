/* Debug command line: name-sorted command list, one-line editor fed by the USB keyboard. */

#include "types_game.h"
#include "libc.h"
#include "gw_console.h"
#include "command.h"
#include "command_set.h"
#include "debug.h"
#include "heap.h"

typedef struct Node { char *name; char pad_04[8]; struct Node *next; } Node;
/* steer(alias): the code that matched reads Node through these member types; one view
 * shared by those sites (src/game/command.c). */
typedef struct NodeView {
    char *name;
    char pad_04[8];
    void *next;
} NodeView;
typedef struct { char *name; int help; int (*run)(int, char **); void *next; } Cmd;
typedef struct { char buf[128]; int cur; int state; } CmdLine;
typedef struct { char pad_00[6]; unsigned char key; char pad_07[4]; unsigned char ch; } PadIn;

extern char command_line[];
extern Node *command_list;
extern PadIn kbd_state;
extern int command_line_key(CmdLine *line);

/* Also registers the built-in tables. */
/* 0x00109638 */
void command_reset(void)
{
    command_list = 0;
    command_line_clear(command_line);
    command_tables_register();
}

/* Also frees any commands left. */
/* 0x00109660 */
void command_shutdown(void)
{
    Node *node;
    Node *next;

    command_tables_unregister();
    node = command_list;
    if (node) {
        do {
            next = node->next;
            heap_free(node, 0);
            node = next;
        } while (node);
    }
    command_list = 0;
}

/* 0 if absent. */
/* 0x001096b8 */
Node *command_find(char *name)
{
    Node *node;

    for (node = command_list; node != 0; node = node->next) {
        if (strcmp(node->name, name) == 0)
            return node;
    }
    return 0;
}

/* Link where name goes in sort order. */
/* 0x00109718 */
Node **command_find_insert_point(char *name)
{
    Node **pp = &command_list;

    while (*pp != 0) {
        if (token_compare((*pp)->name, name) < 0)
            break;
        pp = &(*pp)->next;
    }
    return pp;
}

/* Copies cmd. 0 if the name is taken. */
/* 0x00109778 */
int command_register(CmdDef *cmd)
{
    NodeView *node;
    void **pp;

    if (command_find(cmd->name) != 0)
        return 0;
    node = heap_block_tag(game_malloc(16), "command.c", 180);
    if (node == 0)
        return 0;
    *(CmdDef *)node = *cmd;
    node->next = 0;
    pp = command_find_insert_point(cmd->name);
    if (pp == 0) {
        heap_free(node, 0);
        return 0;
    }
    node->next = *pp;
    *pp = node;
    return 1;
}

/* Table ends at a null name. */
/* 0x00109828 */
void command_register_table(CmdDef *table)
{
    while (table->name != 0)
        command_register(table++);
}

/* *key: a descriptor's name. */
/* 0x00109870 */
void command_unregister(char **key)
{
    Node *node;
    Node *prev;

    prev = command_list;
    for (node = command_list; node; node = node->next) {
        if (strcmp(node->name, *key) == 0) {
            if (node == command_list)
                command_list = node->next;
            else
                prev->next = node->next;
            heap_free(node, 0);
            break;
        }
        prev = node;
    }
}

/* Table ends at a null name. */
/* 0x00109910 */
void command_unregister_table(CmdDef *table)
{
    while (table->name != 0)
        command_unregister(table++);
}

/*
 * Splits in place at spaces, up to 10 arguments. Lookup uses the unsplit line as the name, so a
 * line with arguments finds no command.
 */
/* 0x00109958 */
void command_exec(char *line)
{
    char *argv[11];
    char **ap;
    Cmd *cmd;
    int argc;
    int start;
    int none;

    cmd = command_find(line);
    none = cmd == 0;
    ap = argv;
    argc = 0;
    start = 1;
    for (; *line != 0 && argc < 10; line++) {
        if (*line == 32) {
            *line = 0;
            start = 1;
        } else if (start) {
            *ap++ = line;
            argc++;
            start = 0;
        }
    }
    *ap = 0;
    if (!none) {
        if (cmd->run != 0)
            cmd->run(argc, argv);
    }
}

/* Last character drops off. */
/* 0x00109a10 */
void command_line_insert_gap(char *p, int idx)
{
    int i;

    for (i = 126; i != idx; i--)
        p[i] = p[i - 1];
}

/* 0x00109a48 */
void command_line_delete_char(char *p, int idx)
{
    if (p[idx] != 0 && idx < 127) {
        do {
            p[idx] = p[idx + 1];
            idx++;
        } while (p[idx] != 0 && idx < 127);
    }
}

/* 0x00109a90 */
void command_line_clear(char *line)
{
    line[0] = 0;
    line[0x7e] = 0;
    *(int *)(line + 0x80) = 0;
}

/* Typed char -> 1; USB keys 80/79 left/right, 42 backspace, 76 delete; 40 enter -> 2; else 0. */
/* 0x00109aa0 */
int command_line_key(CmdLine *line)
{
    int key = kbd_state.key;
    int ch = kbd_state.ch;

    line->state = 0;
    if (ch) {
        command_line_insert_gap(line->buf, line->cur);
        line->buf[line->cur] = ch;
        if (line->cur < 126)
            line->cur++;
        line->state = 1;
    } else {
        switch (key) {
        case 80:
            if (line->cur > 0)
                line->cur--;
            break;
        case 79:
            if (line->buf[line->cur] != 0 && line->cur < 126)
                line->cur++;
            break;
        case 42:
            if (line->cur > 0) {
                line->cur--;
                command_line_delete_char(line->buf, line->cur);
            }
            break;
        case 76:
            command_line_delete_char(line->buf, line->cur);
            break;
        case 40:
            line->state = 2;
            break;
        }
    }
    return line->state;
}

/* Only while the console window is hidden; runs the line on enter. */
/* 0x00109bb8 */
int command_line_update(void)
{
    if (gw_console_is_visible() == 0) {
        if (command_line_key(command_line) == 2) {
            command_exec(command_line);
            return ((int (*)(void *))command_line_clear)(command_line); /* steer(cast) */
        }
    }
}
