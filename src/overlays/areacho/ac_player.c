/* Area choose screen: the player model; stands still until the area is decided, then flies in. */

#include "areacho.h"
#include "anim.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "ps_gs.h"
#include "system.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "ac_player.h"

extern void ulMatrixPush(void);

extern int ac_player_models[];
extern struct TaskDesc ac_player_desc;

/* steer(cast) */
/* 0x00ca1a28 */
void ac_player_init(AcPlayer *w)
{
    int mdl, lv;
    float zero;

    w->root = system_pnb_entry_get(0);
    zero = 0.0f;
    w->motion = (unsigned int *)*((int *)pnb_get_entry(w->models, 22));
    lv = player_status_get_level();
    mdl = *((int *)pnb_get_entry(w->models, ac_player_models[lv]));
    player_model_init();
    player_model_set(mdl);
    player_model_set_rate(zero);
    w->t = zero;
    w->speed = zero;
    task_set_dlist(w, 6);
}

/* 0x00ca1ac8 */
void ac_player_release(void)
{
    player_model_release();
}

/* 0x00ca1ad0 */
void ac_player_exec(AcPlayer *w)
{
    Vec3 scl;
    void *outs[3];

    w->t += w->speed;
    if ((float)w->motion[1] <= w->t)
        w->t = (float)w->motion[1] - 1.0f;
    outs[0] = &w->pos;
    outs[1] = w->ang;
    outs[2] = &scl;
    anim_eval_hierarchy(w->root, (int)w->motion, outs, w->t);
    player_model_set_rate(w->speed);
    player_model_update();
}

/* 0x00ca1bc0 */
void ac_player_disp(AcPlayer *w)
{
    ulMatrixPush();
    psGsSetZMask(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ac_player_rotate_yxz(w->ang);
    player_model_draw();
    psGsSetZMask(1);
    ulMatrixPop();
}

/* 0x00ca1c20 */
void *ac_player_create(void *parent)
{
    return task_create_inherit(parent, &ac_player_desc, 48);
}

/* 0x00ca1c48 */
void ac_player_play(AcPlayer *w)
{
    w->speed = 1.0f;
}

/* 0x00ca1c58 */
void ac_player_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
