/* Node-tree queries, angle helpers, sine easing. */

#ifndef REZ_NODE_TREE_H
#define REZ_NODE_TREE_H

struct IVec3;
struct NodeTreeNode;

int model_camera_angles(int *out);
int model_ivec3_length(struct IVec3 *vec);
int vec3_scale_to_length(float len, float *vec, float *out);
void node_tree_collect_xforms_at(struct NodeTreeNode *node, int tex, float scale, unsigned int mask, int max, float *pos,
                   int *rot, float *scl, char *pos_out, char *ang_out, char *scale_out);
void node_tree_collect_xforms_at_buf(struct NodeTreeNode *node, int tex, float scale, unsigned int mask, int max, float *pos,
                   int *rot, float *scl, char *ang_out);
void node_tree_collect_to(struct NodeTreeNode *node, unsigned int mask, struct NodeTreeNode **out);
void node_tree_draw_matching_at(int node, int mask, int count, int out, int flag);
int node_tree_collect_counted(struct NodeTreeNode *node, unsigned int mask, int max, int **out);
float ease_sine_in_out(float t);
float ease_sine_in_out_rev(float t);
float ease_sine_in_out_2x(float t);
float ease_sine_in_out_2x_neg(float t);

#endif /* REZ_NODE_TREE_H */
