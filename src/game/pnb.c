/*
 * Model packs and texture objects: "NJB" pack loader (versions 5, 6) dispatching tagged chunks to
 * registered handlers and relocating offsets; texture lists from PTM files; loader memory hooks.
 */

#include "types_game.h"
#include "libc.h"
#include "libgraph.h"
#include "ul_gx.h"
#include "ul_texture.h"
#include "pnb.h"
#include "billboard.h"

/* {tag, fn} entry and next node (12 bytes). */
typedef struct PnbHandlerNode PnbHandlerNode;

struct PnbHandlerNode {
    PnbHandler entry;
    PnbHandlerNode *next;       /* 8 */
};

typedef struct PnbHooks {
    void *(*alloc)(int);
    void *(*alloc_table)(int);
    void *cb;
} PnbHooks;

typedef struct PnbHead {
    int magic;
    char pad_04[4];
    unsigned int count;
    char pad_0c[4];
} PnbHead;

typedef struct C17Nd C17Nd;

struct C17Nd {
    char pad_00[4];
    int obj;
    char pad_08[36];
    C17Nd *child;               /* 44 */
    C17Nd *next;                /* 48 */
};

typedef struct RelocEntry {
    int a;
    int off;
} RelocEntry;

typedef struct RelocBlock {
    int off;
    char pad_04[4];
    unsigned short flags;       /* 8 */
    unsigned short kind;        /* 10 */
} RelocBlock;

typedef struct TexObject {
    int tab;
    int count;
    int ents;
    int src;
    char pad_10[4];
    int loaded;
    int unk_18; /* cleared by tex_object_clear; never read */
    int unk_1c; /* cleared by tex_object_clear; never read */
} TexObject;

typedef struct TexObjectNode TexObjectNode;

struct TexObjectNode {
    char pad_00[8];
    TexObjectNode *child;
};

typedef struct TexPtmObject {
    char pad_00[8];
    void *ents;                 /* 8 */
    TexSource *src;              /* 12 */
    int user;                   /* 16 */
} TexPtmObject;

typedef struct TexObjectHead {
    char pad_00[12];
    TexSource *src;             /* 12 */
    int tex;                    /* 16: first texture id */
    int loaded;                 /* 20 */
} TexObjectHead;

extern void *(*pnb_hook_alloc)(int);
extern void *(*pnb_hook_alloc_table)(int);
extern PnbHandlerNode *pnb_handler_list;
extern PnbHandlerNode pnb_chunk_handlers[];
extern PnbHandlerNode *pnb_free_list;
extern void (*pnb_hook_free)(PnbHandlerNode *);
extern float screen_image_uvs[];
extern float screen_image_rects[];
extern int mem_hook_alloc_func;
extern int mem_hook_free_func;

static int pnb_load(int *data);
static int pnb_load_v6(int *data, int off, int base);
static int pnb_load_v5(char *data, int n);
static PnbHandlerNode *pnb_find_handler(char *key);
static int tex_object_init(void *obj, unsigned int *src, int user);
static int tex_object_init_pvb(void);
static int tex_object_init_pvm(void);
static int tex_object_init_ptm(TexPtmObject *obj, TexSource *src, int user);
static int tex_object_upload(TexObjectHead *obj);
static void tex_object_reload_handle(char *obj);
static void tex_object_update_handle(char *obj, int limit);

/* Hooks: allocate, allocate a table, free. Handler list starts with the built-ins. */
/* 0x0017c180 */
void pnb_install_hooks(PnbHooks *hooks)
{
    PnbHandlerNode *node;
    int i;

    pnb_hook_alloc = hooks->alloc;
    pnb_hook_alloc_table = hooks->alloc_table;
    pnb_hook_free = hooks->cb;
    node = pnb_chunk_handlers;
    pnb_handler_list = node;
    for (i = 7; i != -1; i--) {
        node->next = node + 1;
        node++;
    }
}

/* 0x0017c1e8 */
void pnb_free_handlers(void)
{
    PnbHandlerNode *node = pnb_free_list;
    PnbHandlerNode *next;

    while (node) {
        next = node->next;
        pnb_hook_free(node);
        node = next;
    }
}

/* Appended to the list. */
/* 0x0017c238 */
void pnb_add_handler(PnbHandler *src)
{
    PnbHandlerNode *node;
    PnbHandlerNode **pp = &pnb_handler_list;

    if (src) {
        node = pnb_hook_alloc(12);
        node->entry = *src;
        *(int *)&node->next = 0; /* steer(alias): an int store, not a pointer one, for the schedule */
        if (!pnb_handler_list) {
            pnb_handler_list = node;
        } else {
            while ((*pp)->next)
                pp = &(*pp)->next;
            (*pp)->next = node;
        }
    }
}

/* key: four-character tag. */
/* 0x0017c2e0 */
void pnb_remove_handler(char *key)
{
    PnbHandlerNode **pp;

    for (pp = &pnb_handler_list; *pp; pp = &(*pp)->next) {
        if (!strncmp(*pp, key, 4)) {
            PnbHandlerNode *node = *pp;

            *pp = node->next;
            pnb_hook_free(node);
            break;
        }
    }
}

/* 0 when not a model pack. */
/* 0x0017c358 */
void *pnb_register(void *data)
{
    if (data == 0)
        return 0;
    if (!pnb_load(data))
        data = 0;
    return data;
}

/* Fourth byte of the magic. */
/* 0x0017c398 */
unsigned char pnb_get_version(unsigned char *hdr)
{
    return hdr[3];
}

/* Version 6 only; 0 for other versions or idx out of range. */
/* 0x0017c3a0 */
PnbEntry *pnb_get_entry(PnbHead *head, unsigned int idx)
{
    if (head->magic == 0x06424a4e) {
        if (idx >= head->count)
            return 0;
        {
            PnbEntry *base = (PnbEntry *)(head + 1);

            return base + idx;
        }
    }
    return 0;
}

/* "NJB" 5 or 6; 0 otherwise. */
/* 0x0017c3e0 */
static int pnb_load(int *data)
{
    int ok = 0;

    switch (data[0]) {
    case 0x06424a4e:
        ok = pnb_load_v6(data + 2, data[1], data);
        break;
    case 0x05424a4e:
        ok = pnb_load_v5(data + 2, data[1]);
        break;
    }
    return ok;
}

/* Once (marked loaded): entry offsets -> pointers, each chunk to its tag's handler. Returns 1. */
/* 0x0017c448 */
static int pnb_load_v6(int *data, int off, int base)
{
    char *cur = (char *)data;
    int *ent;
    int n;
    int size = off;
    PnbHandlerNode *handler;
    char *chunk;

    ent = (int *)cur + 2;
    if (((int *)cur)[1])
        return 1;
    ((unsigned int *)cur)[1] = 0xffffffff;
    cur += size;
    n = data[0];
    if (n > 0) {
        do {
            ent[0] += base;
            ent += 4;
        } while (--n != 0);
    }
    n = data[0];
    if (n > 0) {
        do {
            chunk = cur;
            cur += 8;
            size = ((int *)chunk)[1];
            handler = pnb_find_handler(chunk);
            if (handler) {
                if (handler->entry.fn)
                    handler->entry.fn(cur, size);
            }
            cur += size;
        } while (--n != 0);
    }
    return 1;
}

/* n chunks, or with n 0 up to the first unknown tag. Returns 1. */
/* 0x0017c518 */
static int pnb_load_v5(char *data, int n)
{
    char *chunk;
    int size;
    PnbHandlerNode *handler;

    if (n) {
        do {
            chunk = data;
            data += 8;
            size = *(int *)(chunk + 4);
            handler = pnb_find_handler(chunk);
            if (handler && handler->entry.fn)
                handler->entry.fn(data, size);
            data += size;
        } while (--n != 0);
    } else {
        do {
            chunk = data;
            data += 8;
            size = *(int *)(chunk + 4);
            handler = pnb_find_handler(chunk);
            if (!handler)
                return 1;
            if (handler->entry.fn)
                handler->entry.fn(data, size);
            data += size;
        } while (1);
    }
    return 1;
}

/* 0 if none. */
/* 0x0017c5c8 */
static PnbHandlerNode *pnb_find_handler(char *key)
{
    PnbHandlerNode *node = pnb_handler_list;
    PnbHandlerNode *found = 0;

    while (node) {
        if (!strncmp(node, key, 4)) {
            found = node;
            break;
        }
        node = node->next;
    }
    return found;
}

/* Adds base; values above 0x0c000000 are taken as pointers already. */
/* 0x0017c630 */
static void reloc_offset_pair(unsigned int *pair, unsigned int base)
{
    if (pair[0]) {
        if (pair[0] <= 0x0c000000)
            pair[0] += base;
    }
    if (pair[1]) {
        if (pair[1] <= 0x0c000000)
            pair[1] += base;
    }
}

/* Each node's object, child and next. */
/* 0x0017c678 */
static void reloc_node_chain(C17Nd *node, int base)
{
    do {
        if (node->obj) {
            node->obj = node->obj + base;
            reloc_offset_pair(node->obj, base);
        }
        if (node->child) {
            node->child = (C17Nd *)((int)node->child + base);
            reloc_node_chain(node->child, base);
        }
        if (!node->next)
            break;
        node->next = (C17Nd *)((int)node->next + base);
        node = node->next;
    } while (1);
}

/* Offsets count from its first node. */
/* 0x0017c6f8 */
static void reloc_node_chain_self(int node)
{
    reloc_node_chain(node, node);
}

/* {offset, count} pairs and their targets; ends at the first relocated target or limit. */
/* 0x0017c700 */
static void reloc_pair_table(int *ent, int *limit, int base)
{
    int *item;
    int n;
    int tgt;

    while (!(limit < ent + 2)) {
        if (*ent) {
            *ent += base;
            item = (int *)*ent;
            n = ent[1];
            if (n > 0) {
                do {
                    tgt = *item + base;
                    *item = tgt;
                    if (!(limit < (int *)tgt) && !((int *)tgt < item))
                        limit = (int *)tgt;
                    item += 2;
                } while (--n != 0);
            }
        }
        ent += 2;
    }
}

/* 0x0017c778 */
static void reloc_entry_words(RelocEntry *ent, int n, int base)
{
    if (n > 0) {
        do {
            ent->off += base;
            ent++;
        } while (--n != 0);
    }
}

/*
 * Rows of n offsets, each followed by n counts, up to the first target or limit; flag: targets'
 * entries too.
 */
/* 0x0017c7a8 */
static void reloc_row_table(int *rows, int *limit, int base, int n, int flag)
{
    int *end = rows + n * 2;
    int *ent;
    int j;
    int *tgt;
    int *nx;

    if (limit < end) return;
    do {
        ent = rows;
        for (j = 0; j < n; j++) {
            if (*ent) {
                *ent += base;
                tgt = (int *)*ent;
                if (!(limit < tgt) && !(tgt < ent))
                    limit = tgt;
                if (flag)
                    reloc_entry_words(tgt, ent[n], base);
            }
            ent++;
        }
        rows = end;
        nx = rows + n * 2;
        end = nx;
    } while (!(limit < nx));
}

/* Table as {offset, count} pairs (flag 0x4000) or rows of (hdr->kind & 15) offsets. */
/* 0x0017c890 */
static void reloc_block(RelocBlock *hdr, int size)
{
    if (hdr->off) {
        int *tab = (int *)(hdr->off + (int)hdr);
        int n = hdr->kind & 0xf;

        hdr->off = (int)tab;
        if (hdr->flags & 0x4000)
            reloc_pair_table(tab, (int *)((char *)hdr + size), hdr);
        else
            reloc_row_table(tab, (int *)((char *)hdr + size), hdr, n, 0);
    }
}

/* Entry table, entries, and each entry's following offsets up to the first value above 0xffff. */
/* 0x0017c8d8 */
static void reloc_entry_block(int base)
{
    int *hdr = (int *)base;
    int **tab;
    int n;
    int *ent;
    int *word;
    int val;

    hdr[0] += base;
    tab = (int **)hdr[0];
    n = hdr[1];
    if (n > 0) {
        do {
            if (tab[0] != 0) {
                tab[0] = (int *)((int)tab[0] + base);
                ent = tab[0];
                word = ent;
                *word++ += base;
                val = ent[1];
                if (!((unsigned int)val > 0xffff)) {
                    do {
                        *word = val + base;
                        word++;
                        val = *word;
                    } while (!((unsigned int)val > 0xffff));
                }
            }
            tab += 2;
        } while (--n != 0);
    }
}

/* 0x0017c970 */
static void tex_object_clear(TexObject *tex)
{
    tex->tab = 0;
    tex->count = 0;
    tex->ents = 0;
    tex->src = 0;
    tex->loaded = 0;
    tex->unk_18 = 0;
    tex->unk_1c = 0;
}

/* 0 when out of memory. */
/* 0x0017c990 */
static void *tex_object_alloc(void)
{
    void *obj = pnb_hook_alloc(32);

    if (obj)
        tex_object_clear(obj);
    return obj;
}

/* 0x0017c9d8 */
static void tex_object_free(TexObjectNode *obj)
{
    if (obj->child)
        pnb_hook_free(obj->child);
    pnb_hook_free(obj);
}

/* Null allowed. */
/* 0x0017ca30 */
void tex_object_release(void *obj)
{
    if (obj) {
        ulTexListFree(obj);
        tex_object_free(obj);
    }
}

/* Also uploads; 0 on failure. */
/* 0x0017ca70 */
void *tex_object_create_upload(void *src, int user)
{
    void *obj;

    if (src) {
        obj = tex_object_alloc();
        if (obj) {
            if (tex_object_init(obj, src, user)) {
                if (tex_object_upload(obj))
                    return obj;
            }
            tex_object_free(obj);
        }
    }
    return 0;
}

/* 0 on failure or null src. */
/* 0x0017caf0 */
void *tex_object_create(void *src, int user)
{
    void *obj;

    if (src) {
        obj = tex_object_alloc();
        if (obj) {
            if (tex_object_init(obj, src, user))
                return obj;
            tex_object_free(obj);
        }
    }
    return 0;
}

/* Uploads if never loaded. */
/* 0x0017cb60 */
int tex_object_reload(int *obj)
{
    if (obj[5]) {
        tex_object_reload_handle(obj);
        return 1;
    }
    return tex_object_upload(obj);
}

/* Textures above limit only. */
/* 0x0017cb98 */
int tex_object_update(int *obj, int x)
{
    if (obj[5]) {
        tex_object_update_handle(obj, x);
        return 1;
    }
    return tex_object_upload(obj);
}

/* By tag: "PVMH" and "PVB" as they are, "PTMH" gets a texture list; 0 for unknown. */
/* 0x0017cbd0 */
static int tex_object_init(void *obj, unsigned int *src, int user)
{
    int ret = 0;

    switch (src[0]) {
    case 0x484d5650:
        ret = ((int (*)(void *, unsigned int *, int))tex_object_init_pvm)(obj, src, user); /* steer(cast) */
        break;
    case 0x484d5450:
        ret = tex_object_init_ptm(obj, src, user);
        break;
    case 0x02425650:
        ret = ((int (*)(void *, unsigned int *, int, unsigned int *))tex_object_init_pvb)(obj, src, user, src); /* steer(cast) */
        break;
    }
    return ret;
}

/* 0x0017cc58 */
static int tex_object_init_pvb(void)
{
    return 1;
}

/* 0x0017cc60 */
static int tex_object_init_pvm(void)
{
    return 1;
}

/* One list entry per texture. Returns 1. */
/* 0x0017cc68 */
static int tex_object_init_ptm(TexPtmObject *obj, TexSource *src, int user)
{
    obj->ents = pnb_hook_alloc_table(src->count * 12);
    ulTexListInit(obj, obj->ents, src->count);
    obj->user = user;
    obj->src = src;
    return 1;
}

/* Waits for the transfer; numbered from obj->tex. 0 without a texture file. */
/* 0x0017cce0 */
static int tex_object_upload(TexObjectHead *obj)
{
    int ok;
    int i;
    int n;
    int tex;

    if (obj->src) {
        ulGxLock(1);
        ok = 1;
        i = 0;
        ulTextureLoadSectionDirect(obj->src, obj);
        ulGxUnlock(1);
        sceGsSyncPath(0, 0);
        obj->loaded = ok;
        n = obj->src->count;
        tex = obj->tex;
        for (; i < n; i++, tex++)
            ulTextureSetId(ulTexListGet(obj, i), tex);
    } else {
        ok = 0;
    }
    return ok;
}

/* 0x0017cda8 */
static void tex_object_reload_handle(char *obj)
{
    void *sec = *(void **)(obj + 12);

    if (sec)
        ulTextureReloadSection(sec, obj);
}

/* 0x0017cdc8 */
static void tex_object_update_handle(char *obj, int limit)
{
    void *sec = *(void **)(obj + 12);

    if (sec)
        ulTextureReloadSectionAbove(sec, obj, limit);
}

/* Five pieces, textures idx onward. */
/* 0x0017cde8 */
void screen_image_draw(int idx, int col, float z)
{
    float *src;
    float *dst;
    unsigned int i;

    ulGxSetTexFilter(1);
    for (i = 0; i < 5; i++, idx++) {
        src = &screen_image_rects[i * 4];
        dst = &screen_image_uvs[i * 4];
        tex_rect_draw_uv(idx, col, dst, src[0], src[1], z, src[2], src[3]);
    }
}

/* 0x0017ce90 */
void mem_hooks_set(int *hooks)
{
    mem_hook_alloc_func = hooks[0];
    mem_hook_free_func = hooks[1];
}

/* 0x0017ceb0 */
void mem_hooks_init(void)
{
}

/* 0x0017ceb8 */
static void mem_hook_alloc(void)
{
    (*(int (*(*))(void))&mem_hook_alloc_func)(); /* steer(cast) */
}

/* 0x0017cee0 */
static void mem_hook_free(void)
{
    (*(int (*(*))(void))&mem_hook_free_func)(); /* steer(cast) */
}
