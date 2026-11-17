/* Debug variable tree, commands, saving. */

#ifndef REZ_GW_VAREDIT_H
#define REZ_GW_VAREDIT_H

struct GwVarNode;
struct GwVarDef;

void gw_varedit_init(void);
void gw_varedit_init_root(int *root);
void gw_varedit_free_root(int *root);
int gw_varedit_get_root(void);
struct GwVarNode *gw_varedit_first_child(struct GwVarNode **root, struct GwVarNode *parent);
int gw_varedit_count(struct GwVarNode *n);
int gw_varedit_find_index(struct GwVarNode **root, struct GwVarNode *start, char *name);
struct GwVarNode *gw_varedit_nth(struct GwVarNode *n, int i);
struct GwVarNode *gw_varedit_find_group(struct GwVarNode **root, struct GwVarNode *parent, char *path);
struct GwVarNode *gw_varedit_add(struct GwVarNode **root, struct GwVarNode *parent, struct GwVarDef *def);
void gw_varedit_add_table(struct GwVarNode **root, struct GwVarNode *parent, struct GwVarDef *defs);
void gw_varedit_remove(struct GwVarNode **root, struct GwVarNode *parent, struct GwVarDef *def);
int gw_varedit_save_total_size(void);
void gw_varedit_save(int *out);
void gw_varedit_load(int *data);

#endif /* REZ_GW_VAREDIT_H */
