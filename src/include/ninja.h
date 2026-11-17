/* Ninja state shared by nj/: ul graphics state, matrix, view, clip, projection, lights. */

#ifndef NINJA_H
#define NINJA_H

#include "types_nj.h"

/* Used when a matrix argument is 0. */
extern NjMatrix *ulMatrixCur;

extern NjSys ulGxState;

extern NjView screen_cfg;

extern NjClip cam_far;

extern NjProjView ulGxProj;

extern NjLight nj_lights[];

#endif
