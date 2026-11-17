/*
 * Debug variable editor: tree of groups and variables (int, float, colour, angle, bool, bit field,
 * function) from tables; 'varlist', 'varsearch', 'varset'; save and load as one block.
 */

#include "types_gw.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_varedit.h"
#include "gw_command.h"
#include "gw_console.h"
#include "gw_desktop.h"
#include "gw_heap.h"
#include "gw_menu.h"
#include "gw_parse.h"
#include "gw_varedit_win.h"

static void gw_varedit_add_menu(void);
static void gw_varedit_free_tree(GwVarNode *n);
static GwVarNode *gw_varedit_find(GwVarNode **root, GwVarNode *parent, char *path);
static GwVarNode **gw_varedit_insert_pos(GwVarNode **root, GwVarNode *parent, GwVarDef *def);
static void gw_varedit_add_commands(void);
static void gw_varedit_remove_commands(void);

typedef struct GwVarNodeView {
    GwVarDef def;
    GwVarNode *next;
} GwVarNodeView;

extern int gw_varedit_root;
extern int gindows_vars_group;
extern int gindows_vardef[];
extern int gw_varedit_menu;
extern int gw_varedit_menu_defs[];
extern char *gw_varedit_kind_names[];
extern GwCmdDef gw_varedit_commands[];
extern int gw_varedit_save_magic;
extern char str_gw_varedit_empty[8];

/* Empty tree, commands, the GUI's own group (gindows_vars_group), desktop menu entry. */
/* 0x00198780 */
void gw_varedit_init(void)
{
    gw_varedit_root = 0;
    gw_varedit_add_commands();
    gindows_vars_group = gw_varedit_add(&gw_varedit_root, 0, gindows_vardef);
    gw_varedit_add_menu();
}

/* Removes its commands, frees the tree. */
/* 0x001987b8 */
static void gw_varedit_exit(void)
{
    gw_varedit_remove_commands();
    gw_varedit_free_tree(gw_varedit_root);
    gw_varedit_root = 0;
}

/* 0x001987e0 */
static void gw_varedit_toggle_window(void)
{
    gw_varedit_win_set(!gw_varedit_win_is_open());
}

/* From gw_varedit_menu_defs. */
/* 0x00198800 */
static void gw_varedit_add_menu(void)
{
    int root;

    root = gw_desktop_get_menu();
    gw_varedit_menu = gw_menu_build(0, gw_varedit_menu_defs);
    gw_menu_append(root, gw_varedit_menu);
}

/* Recurses into group children. */
/* 0x00198848 */
static void gw_varedit_free_tree(GwVarNode *n)
{
    GwVarNode *next;

    if (n) {
        do {
            next = n->next;
            if (n->kind == 0) {
                if (n->child)
                    gw_varedit_free_tree(n->child);
            }
            gw_dc_free(n, "gw_varedit.c", 173);
            n = next;
        } while (next);
    }
}

/* 0x001988c0 */
void gw_varedit_init_root(int *root)
{
    *root = 0;
}

/* Leaves it empty. */
/* 0x001988c8 */
void gw_varedit_free_root(int *root)
{
    gw_varedit_free_tree(*root);
    *root = 0;
}

/* The GUI's own group; other modules add their variables to it. */
/* 0x001988f8 */
int gw_varedit_get_root(void)
{
    return gindows_vars_group;
}

/* Tree top for a null parent, a group's first child, 0 for a variable. */
/* 0x00198900 */
GwVarNode *gw_varedit_first_child(GwVarNode **root, GwVarNode *parent)
{
    if (parent == 0)
        return *root;
    if (parent->kind == 0)
        return parent->child;
    return 0;
}

/* 0x00198928 */
int gw_varedit_count(GwVarNode *n)
{
    int cnt;

    cnt = 0;
    for (; n; n = n->next)
        cnt++;
    return cnt;
}

/* name up to its first '.'; start null = tree top. The list length when not found. */
/* 0x00198958 */
int gw_varedit_find_index(GwVarNode **root, GwVarNode *start, char *name)
{
    GwVarNode *n;
    int i;

    n = start;
    if (n == 0)
        n = *root;
    for (i = 0; n; i++) {
        if (gw_word_equal(n->name, name, '.'))
            break;
        n = n->next;
    }
    return i;
}

/* The last one reached if the list is shorter. */
/* 0x001989c8 */
GwVarNode *gw_varedit_nth(GwVarNode *n, int i)
{
    while (i-- > 0 && n)
        n = n->next;
    return n;
}

/* Dotted path; 0 if none. */
/* 0x001989f0 */
GwVarNode *gw_varedit_find_group(GwVarNode **root, GwVarNode *parent, char *path)
{
    GwVarNode *n;

    n = gw_varedit_find(root, parent, path);
    if (n && n->kind == 0)
        return n;
    return 0;
}

/* Lowest set bit of the mask; 0 for an empty mask. */
/* 0x00198a20 */
static void gw_varedit_calc_shift(GwVarNode *n)
{
    unsigned mask;
    int shift;

    mask = n->mask;
    shift = 0;
    if (mask) {
        while (!(mask & 1)) {
            mask >>= 1;
            shift++;
        }
    }
    n->shift = shift;
}

/*
 * parent null = tree top; an existing name is reported and returned. Flag bit 0 writes the start
 * value (angles from degrees to 65536 per turn). 0 without memory.
 */
/* 0x00198a60 */
GwVarNode *gw_varedit_add(GwVarNode **root, GwVarNode *parent, GwVarDef *def)
{
    GwVarNodeView *n;
    GwVarNode *found;
    GwVarNode **link;
    char *file;
    unsigned k;

    found = gw_varedit_find(root, parent, def->name);
    if (found) {
        gw_console_printf("gwVarEdit(E) : \225\317\220\224\221\275\217d\223o\230^ %s\n", def->name);
        return found;
    }
    file = "gw_varedit.c";
    n = gw_dc_malloc(44, file, 364);
    if (n == 0) {
        gw_console_printf("gwVarEdit(E) : No Heap\n");
        return 0;
    }
    n->def = *def;
    n->next = 0;
    link = gw_varedit_insert_pos(root, parent, def);
    if (link == 0) {
        gw_dc_free(n, file, 375);
        return 0;
    }
    n->next = *link;
    *link = (GwVarNode *)n;
    if (n->def.flags & 1) {
        k = n->def.kind;
        switch (k) {
        case 3:
            if (n->def.ptr)
                *(float *)n->def.ptr = n->def.fval;
            break;
        case 5:
            if (n->def.ptr)
                *(int *)n->def.ptr = (int)(n->def.fval * ANG_TURN / 360.0f);
            break;
        case 2:
        case 6:
            if (n->def.ptr)
                *(int *)n->def.ptr = (int)n->def.fval;
            break;
        }
    } else
        k = n->def.kind;
    if (k == 7)
        gw_varedit_calc_shift((GwVarNode *)n);
    return (GwVarNode *)n;
}

/* Table ends at kind 9. */
/* 0x00198c78 */
void gw_varedit_add_table(GwVarNode **root, GwVarNode *parent, GwVarDef *defs)
{
    if (defs) {
        while (defs->kind != 9) {
            gw_varedit_add(root, parent, defs++);
        }
    }
}

/* parent null = tree top; groups go with their children. */
/* 0x00198cf0 */
void gw_varedit_remove(GwVarNode **root, GwVarNode *parent, GwVarDef *def)
{
    GwVarNode **pp;
    GwVarNode *n;

    if (def == 0)
        return;
    if (parent == 0) {
        pp = root;
        n = *pp;
    } else {
        if (parent->kind != 0)
            return;
        n = parent->child;
        pp = &parent->child;
    }
    while (n) {
        if (strcmp(n->name, def->name) == 0) {
            if (def->kind == 0)
                gw_varedit_free_tree(n->child);
            *pp = n->next;
            gw_dc_free(n, "gw_varedit.c", 477);
            break;
        }
        pp = &(*pp)->next;
        n = *pp;
    }
}

/* Table ends at kind 9. */
/* 0x00198db0 */
static void gw_varedit_remove_table(GwVarNode **root, GwVarNode *parent, GwVarDef *defs)
{
    while (defs->kind != 9) {
        gw_varedit_remove(root, parent, defs);
        defs++;
    }
}

/* "group.sub.var"; parent null = tree top, a variable parent finds nothing. */
/* 0x00198e20 */
static GwVarNode *gw_varedit_find(GwVarNode **root, GwVarNode *parent, char *path)
{
    GwVarNode *n;
    char *dot;

    if (parent == 0)
        n = *root;
    else {
        if (parent->kind != 0)
            return 0;
        n = parent->child;
    }
    while (n) {
        if (gw_word_equal(n->name, path, '.')) {
            dot = strchr(path, '.');
            if (dot == 0)
                return n;
            path = dot + 1;
            n = n->child;
        } else
            n = n->next;
    }
    return 0;
}

/* Groups and functions first, ordered by kind; other variables in add order. Returns the link. */
/* 0x00198ec0 */
static GwVarNode **gw_varedit_insert_pos(GwVarNode **root, GwVarNode *parent, GwVarDef *def)
{
    GwVarNode **pp;
    GwVarNode *n;

    if (!parent)
        pp = root;
    else if (parent->kind != 0)
        pp = root;
    else
        pp = &parent->child;
    for (; (n = *pp) != 0; pp = &n->next) {
        if (n->kind == def->kind)
            continue;
        if (n->kind < 2) {
            if (def->kind < 2 && def->kind < n->kind)
                break;
        } else if (def->kind < 2)
            break;
    }
    return pp;
}

/*
 * "name : old -> new" by kind (colour as four bytes, angle in degrees, TRUE/FALSE, 32 binary
 * digits).
 */
/* 0x00198f30 */
static void gw_varedit_report_change(GwVarNode *n, int old)
{
    int *val;
    unsigned cur;
    char old_bits[48];
    char new_bits[48];

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    gw_console_puts("----------------\n");
    gw_console_printf("%s : ", n->name);
    switch (n->kind) {
    case 0:
        gw_console_puts("Group\n");
        break;
    case 1:
        gw_console_puts("Function\n");
        break;
    case 2:
        gw_console_printf("%d -> %d\n", old, *val);
        break;
    case 3:
        gw_console_printf("%f -> %f\n", *(float *)&old, *(float *)val);
        break;
    case 4:
        cur = *val;
        gw_console_printf("%d %d %d %d -> ", (unsigned)old >> 24, (old >> 16) & 0xff, (old >> 8) & 0xff, old & 0xff);
        gw_console_printf("%d %d %d %d\n", cur >> 24, (cur >> 16) & 0xff, (cur >> 8) & 0xff, cur & 0xff);
        break;
    case 5:
        gw_console_printf("%f -> %f\n", (float)(unsigned)old * DEG_PER_ANG, (float)(unsigned)*val * DEG_PER_ANG);
        break;
    case 6:
        gw_console_printf("%s -> %s\n", old ? "TRUE" : "FALSE", *val ? "TRUE" : "FALSE");
        break;
    case 7:
        gw_console_printf("%s -> %s\n", gw_format_bits(old, 32, old_bits, 0, 0), gw_format_bits(*val, 32, new_bits, 0, 0));
        break;
    }
}

/* Name, help, type and value by kind. */
/* 0x001991b0 */
static void gw_varedit_describe(GwVarNode *n)
{
    int *val;
    char buf[48];
    unsigned value;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    gw_console_puts("----------------\n");
    gw_console_printf("%s\n", n->name);
    gw_console_printf("%s\n", n->help);
    gw_console_printf("%s : ", gw_varedit_kind_names[n->kind]);
    switch (n->kind) {
    case 0:
        gw_console_puts("Group\n");
        break;
    case 1:
        gw_console_puts("Function\n");
        break;
    case 2:
        gw_console_printf("%d\n", *val);
        break;
    case 3:
        gw_console_printf("%f\n", *(float *)val);
        break;
    case 4:
        value = *val;
        gw_console_printf("%d %d %d %d\n", value >> 24, ((unsigned char *)val)[2], ((unsigned char *)val)[1], value & 0xff);
        break;
    case 5:
        gw_console_printf("%f\n", (float)(unsigned)*val * DEG_PER_ANG);
        break;
    case 6:
        gw_console_printf("%s\n", *val ? "TRUE" : "FALSE");
        break;
    case 7:
        gw_console_printf("%s\n", gw_format_bits(*val, 32, buf, 0, 0));
        break;
    }
}

/* Highlighted "GROUP [path]" before the first variable, a line per variable, then each group. */
/* 0x00199398 */
static void gw_varedit_list(GwVarNode *n, char *prefix)
{
    int shown;
    int attr;
    unsigned int k;
    char path[128];

    shown = 0;
    while (n) {
        k = n->kind;
        if (!shown && k != 0) {
            shown = 1;
            attr = gw_console_set_attr(0xff802040);
            if (!prefix)
                gw_console_puts("GROUP []\n");
            else
                gw_console_printf("GROUP [%s]\n", prefix);
            gw_console_set_attr(attr);
        }
        k = n->kind;
        if (k != 0) {
            if (k < 8)
                gw_console_printf("%s %-16s %s\n", gw_varedit_kind_names[k], n->name, n->help);
        } else {
            if (!prefix)
                sprintf(path, "%s", n->name);
            else
                sprintf(path, "%s.%s", prefix, n->name);
            gw_varedit_list(n->child, path);
        }
        n = n->next;
    }
}

/* 'varlist'. */
/* 0x001994f0 */
static int gw_varedit_cmd_list(void)
{
    GwVarNode *root;

    root = gw_varedit_root;
    gw_console_puts("TYPE##VARIABLE#########HELP#############\n");
    gw_varedit_list(root, 0);
    return 0;
}

/* 'varsearch path ...'. */
/* 0x00199530 */
static int gw_varedit_cmd_search(int argc, char **argv)
{
    GwVarNode *n;

    if (argc >= 2) {
        do {
            argv++;
            n = gw_varedit_find(&gw_varedit_root, 0, *argv);
            argc--;
            if (n == 0)
                gw_console_printf("Not Found : %s\n", *argv);
            else
                gw_varedit_describe(n);
        } while (argc >= 2);
    }
    return 0;
}

/* From argv[0]; calls the change callback. */
/* 0x001995c0 */
static void gw_varedit_set_int(GwVarNode *n, int argc, char **argv)
{
    int value;
    int *val;

    value = gw_parse_int(argv[0]);
    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    *val = value;
    if (n->cb)
        n->cb(value);
}

/* From argv[0]; calls the change callback. */
/* 0x00199610 */
static void gw_varedit_set_float(GwVarNode *n, int argc, char **argv)
{
    float value;
    float *val;

    value = gw_parse_float(argv[0]);
    val = (float *)n->child;
    if (val == 0)
        val = (float *)&n->mask;
    *val = value;
    if (n->cb)
        ((void (*)(float))n->cb)(value);
}

/* r g b a, each clamped 0..255, r in the top byte; fewer than four arguments ignored. */
/* 0x00199658 */
static void gw_varedit_set_color(GwVarNode *n, int argc, char **argv)
{
    int *val;
    int r;
    int g;
    int b;
    int a;
    int red;
    int green;
    int blue;
    int alpha;
    long packed;

    if (argc < 4)
        return;
    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    r = gw_parse_int(*argv++);
    g = gw_parse_int(*argv++);
    b = gw_parse_int(*argv++);
    a = gw_parse_int(*argv);
    if (r < 0) r = 0;
    red = r < 256 ? r : 255;
    if (g < 0) g = 0;
    green = g < 256 ? g : 255;
    if (b < 0) b = 0;
    blue = b < 256 ? b : 255;
    if (a < 0) a = 0;
    alpha = a < 256 ? a : 255;
    packed = ((long)red << 24) | ((long)green << 16) | ((long)blue << 8) | alpha;
    *val = packed;
    if (n->cb)
        n->cb((int)packed);
}

/* Degrees in, 65536 per turn stored. */
/* 0x00199770 */
static void gw_varedit_set_angle(GwVarNode *n, int argc, char **argv)
{
    int value;
    int *val;

    value = (int)(gw_parse_float(argv[0]) * ANG_PER_DEG);
    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    *val = value;
    if (n->cb)
        n->cb(value);
}

/* True when argv[0] starts with 'T', 't' or '1'. */
/* 0x001997c8 */
static void gw_varedit_set_bool(GwVarNode *n, int argc, char **argv)
{
    int value;
    int *val;

    val = (int *)n->child;
    if (val == 0)
        val = (int *)&n->mask;
    switch (argv[0][0]) {
    case 'T':
    case 't':
    case '1':
        value = 1;
        break;
    default:
        value = 0;
        break;
    }
    *val = value;
    if (n->cb)
        n->cb(value);
}

/*
 * With values: sets by kind (functions are called) and reports; else describes. Bit fields
 * unsupported.
 */
/* 0x00199840 */
static void gw_varedit_cmd_set(void *root, GwVarNode *parent, int argc, char **argv)
{
    GwVarNode *n;
    int *val;
    int old;

    if (argc <= 0)
        return;
    n = gw_varedit_find(root, parent, *argv);
    if (n == 0)
        return;
    argc--;
    if (argc > 0) {
        val = (int *)n->child;
        if (val == 0)
            val = (int *)&n->mask;
        argv++;
        old = *val;
        switch (n->kind) {
        case 0:
            break;
        case 1:
            if (n->cb)
                n->cb();
            break;
        case 2:
            gw_varedit_set_int(n, argc, argv);
            break;
        case 3:
            gw_varedit_set_float(n, argc, argv);
            break;
        case 4:
            gw_varedit_set_color(n, argc, argv);
            break;
        case 5:
            gw_varedit_set_angle(n, argc, argv);
            break;
        case 6:
            gw_varedit_set_bool(n, argc, argv);
            break;
        case 7:
            break;
        }
        gw_varedit_report_change(n, old);
    } else
        gw_varedit_describe(n);
}

/* 'varset path [value ...]'. */
/* 0x00199980 */
static int gw_varedit_cmd_varset(int argc, char **argv)
{
    int n = argc - 1;
    if (n <= 0) return 0;
    gw_varedit_cmd_set(&gw_varedit_root, 0, n, argv + 1);
    return 0;
}

/* varlist, varsearch, varset. */
/* 0x001999b0 */
static void gw_varedit_add_commands(void)
{
    gw_command_add_table(gw_varedit_commands);
}

/* 0x001999c0 */
static void gw_varedit_remove_commands(void)
{
    gw_command_remove_table(gw_varedit_commands);
}

/*
 * n null = whole tree. Tag and padded name per node, value or children (empty groups left out), end
 * tag.
 */
/* 0x001999d0 */
static int gw_varedit_save_size(GwVarNode *n)
{
    int total;

    if (n == 0)
        n = gw_varedit_root;
    if (n == 0)
        goto empty;
    do {
        total = 4;
    } while (0);
loop:
    if (n->kind != 0 || n->child != 0) {
        int sum = total;

        total = strlen(n->name);
        sum += (total + 4) & 0xfffffffc;
        if (n->kind == 0) {
            sum += gw_varedit_save_size(n->child);
            total = sum + 4;
        } else
            total = sum + 8;
    }
    n = n->next;
    if (n)
        goto loop;
    goto out;
empty:
    total = 4;
out:
    return total;
}

/* 0x00199a80 */
int gw_varedit_save_total_size(void)
{
    return gw_varedit_save_size(0) + 4;
}

/* Returns the next word-aligned address. */
/* 0x00199aa0 */
static char *gw_varedit_save_name(GwVarNode *n, char *out)
{
    strcpy(out, n->name);
    return out + ((strlen(n->name) + 4) & 0xfffffffc);
}

/* Group: tag 1, name, children. Variable: tag 2, name, value (0 for a function). End: tag 16. */
/* 0x00199af8 */
static int *gw_varedit_save_tree(GwVarNode *n, int *out)
{
    int *val;
    int value;

    if (n == 0)
        n = gw_varedit_root;
    for (; n; n = n->next) {
        val = (int *)n->child;
        if (val == 0)
            val = (int *)&n->mask;
        switch (n->kind) {
        default:
            *out = 2;
            out = (int *)gw_varedit_save_name(n, (char *)(out + 1));
            value = *val;
            *out = value;
            out++;
            break;
        case 0:
            if (n->child == 0)
                break;
            *out = 1;
            out = (int *)gw_varedit_save_name(n, (char *)(out + 1));
            out = gw_varedit_save_tree(n->child, out);
            break;
        case 1:
            *out = 2;
            out = (int *)gw_varedit_save_name(n, (char *)(out + 1));
            *out = 0;
            out++;
            break;
        }
    }
    *out = 16;
    return out + 1;
}

/* After the 'SET0' signature. */
/* 0x00199bd8 */
void gw_varedit_save(int *out)
{
    *out = gw_varedit_save_magic;
    gw_varedit_save_tree(0, out + 1);
}

/*
 * Into the group at prefix; existing variables get their value and notify the window; others
 * skipped.
 */
/* 0x00199bf0 */
static int *gw_varedit_load_tree(char *prefix, int *data)
{
    GwVarNode *base;
    GwVarNode *n;
    int *val;
    int tag;
    char path[128];

    base = gw_varedit_find(&gw_varedit_root, 0, prefix);
    tag = *data;
    data++;
    if (tag != 16) {
        do {
            switch (tag) {
            case 1:
                strcpy(path, prefix);
                if (strlen(prefix))
                    strcat(path, ".");
                strcat(path, (char *)data);
                data = gw_varedit_load_tree(path, (int *)((char *)data + ((strlen((char *)data) + 4) & 0xfffffffc)));
                break;
            case 2:
                n = gw_varedit_find(&gw_varedit_root, base, (char *)data);
                data = (int *)((char *)data + ((strlen((char *)data) + 4) & 0xfffffffc));
                if (n) {
                    val = (int *)n->child;
                    if (val == 0)
                        val = (int *)&n->mask;
                    if (n->kind != 0) {
                        if (n->kind == 1)
                            data++;
                        else {
                            *val = *data;
                            data++;
                            gw_varedit_win_notify(n);
                        }
                    }
                } else
                    data++;
                break;
            }
            tag = *data;
            data++;
        } while (tag != 16);
    }
    return data;
}

/* Refused without the 'SET0' signature. */
/* 0x00199d60 */
void gw_varedit_load(int *data)
{
    if (*data++ != gw_varedit_save_magic)
        gw_console_puts("Illigal SetData\n");
    else
        gw_varedit_load_tree(str_gw_varedit_empty, data);
}
