#ifndef DRAW_H
#define DRAW_H 1

#include "glext.h"

#include "solid_draw.h"
#include "solid_vary.h"

/*---------------------------------------------------------------------------*/

enum
{
    POSE_NONE = 0,
    POSE_LEVEL,
    POSE_BALL
};

struct renderer
{
    void (*draw_back)(struct s_rend *, int, int, float);
    void (*draw_balls)(struct s_rend *, struct s_vary *, float *, int, float);
    void (*draw_beams)(struct s_rend *, struct s_vary *);
    /* Everything below may be null. */
    void (*draw_goals)(struct s_rend *, struct s_vary *, float);
    void (*draw_jumps)(struct s_rend *, struct s_vary *, float);
    void (*rotate_tilt)(int);
};

void common_draw(int, float, float, struct s_draw *, float [3], float [3], float [3][3], struct renderer *);
void common_draw_balls(struct s_rend *, float *, float, struct v_ball, GLfloat *);
void common_draw_beams(struct s_rend *, struct s_vary *, int, int, GLfloat, GLfloat *(*)(int));
void common_draw_goals(struct s_rend *, struct s_vary *, float, int, GLfloat, GLfloat *(*)(int));
void common_draw_jumps(struct s_rend *, struct s_vary *, float);

/*---------------------------------------------------------------------------*/

#endif