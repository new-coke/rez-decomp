/* Keyframe animation: camera, motion hierarchies, shape blending. */

#ifndef REZ_ANIM_H
#define REZ_ANIM_H

struct AnimHeaderOut;
struct AnimHeader;
struct AnimNode;
struct AnimCursorSetSrc;
struct C17WNode;
struct AnimStartRec;

void anim_eval_header(float t, float *src, void *hdr, struct AnimHeaderOut *out);
void anim_record_advance(void);
void anim_start(void *src, float t);
void anim_apply_node(struct AnimNode *node);
void anim_eval_hierarchy(int node, int motion, struct AnimCursorSetSrc *cursors, float t);
void anim_cursor_next(void);
void anim_cursor_start(struct AnimStartRec *rec, float t);
void anim_select_cursor(int idx);
void anim_cache_init(int *buf, unsigned int bytes);
void anim_set_slot_table(int table);
void anim_cursor_play(unsigned char **pp);

#endif /* REZ_ANIM_H */
