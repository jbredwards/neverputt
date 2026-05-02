/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include "vec3.h"
#include "glext.h"
#include "ball.h"
#include "part.h"
#include "geom.h"
#include "config.h"
#include "video.h"

#include "solid_all.h"
#include "solid_draw.h"

#include "game_client.h"
#include "game_draw.h"

/*---------------------------------------------------------------------------*/

static void game_draw_balls(struct s_rend *rend,struct s_vary *vary, float *bill_M, int d, float t)
{
    float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    common_draw_balls(rend, bill_M, t, vary->uv[0], c);
}

static void game_draw_beams(struct s_rend *rend, struct s_base *base, struct s_vary *vary)
{
    static GLfloat goal_c[4] = { 1.0f, 1.0f, 0.0f, 0.5f };
    const struct game_draw *gd = curr_game_draw();

    common_draw_beams(rend, base, vary, gd->jump_e, gd->goal_e, gd->goal_k, goal_c);
}

static void game_draw_goals(struct s_rend *rend, struct s_base *base, float t)
{
    const struct game_draw *gd = curr_game_draw();
    const struct s_vary *vary = &gd->vary;

    float goal_p[3], goal_e[4], u[3], a;
    int i;

    if (gd->goal_e)
        for (i = 0; i < base->zc; i++)
        {
            sol_entity_p(goal_p, vary, vary->zv[i].mi, vary->zv[i].mj);
            sol_entity_e(goal_e, vary, vary->zv[i].mi, vary->zv[i].mj);

            q_as_axisangle(goal_e, u, &a);

            glPushMatrix();
            {
                glTranslatef(goal_p[0], goal_p[1], goal_p[2]);
                glRotatef(V_DEG(a), u[0], u[1], u[2]);
                goal_draw(rend, base->zv[i].p, base->zv[i].r, gd->goal_k, t);
            }
            glPopMatrix();
        }
}

static void game_draw_jumps(struct s_rend *rend, struct s_base *base, float t)
{
    const struct s_vary *vary = &curr_game_draw()->vary;

    float jump_p[3], jump_e[4], u[3], a;
    int i;

    for (i = 0; i < base->jc; i++)
    {
        sol_entity_p(jump_p, vary, vary->jv[i].mi, vary->jv[i].mj);
        sol_entity_e(jump_e, vary, vary->jv[i].mi, vary->jv[i].mj);

        q_as_axisangle(jump_e, u, &a);

        glPushMatrix();
        {
            glTranslatef(jump_p[0], jump_p[1], jump_p[2]);
            glRotatef(V_DEG(a), u[0], u[1], u[2]);
            jump_draw(rend, base->jv[i].p, base->jv[i].r, 1.0f);
        }
        glPopMatrix();
    }
}

/*---------------------------------------------------------------------------*/

static void game_draw_tilt(int d)
{
    const struct game_draw *gd = curr_game_draw();
    const struct game_tilt *tilt = &gd->tilt;
    const float *ball_p = gd->vary.uv[0].p;

    /* Rotate the environment about the position of the ball. */

    glTranslatef(+ball_p[0], +ball_p[1] * d, +ball_p[2]);
    glRotatef(-tilt->rz * d, tilt->z[0], tilt->z[1], tilt->z[2]);
    glRotatef(-tilt->rx * d, tilt->x[0], tilt->x[1], tilt->x[2]);
    glTranslatef(-ball_p[0], -ball_p[1] * d, -ball_p[2]);
}

static void game_draw_back(struct s_rend *rend, int pose, int d, float t)
{
    if (pose == POSE_BALL)
        return;

    glPushMatrix();
    {
        const struct game_draw *gd = curr_game_draw();
        const struct game_view *view = &gd->view;

        if (d < 0)
        {
            const struct game_tilt *tilt = &gd->tilt;

            glRotatef(tilt->rz * 2, tilt->z[0], tilt->z[1], tilt->z[2]);
            glRotatef(tilt->rx * 2, tilt->x[0], tilt->x[1], tilt->x[2]);
        }

        glTranslatef(view->p[0], view->p[1] * d, view->p[2]);

        if (config_get_d(CONFIG_BACKGROUND))
        {
            back_draw(rend);
            sol_back(&gd->back.draw, rend, 0, FAR_DIST, t);
        }
        else back_draw(rend);
    }
    glPopMatrix();
}

/*---------------------------------------------------------------------------*/

struct renderer r_instance = {
    game_draw_back,
    game_draw_balls,
    game_draw_beams,
    game_draw_goals,
    game_draw_jumps,
    game_draw_tilt
};

void game_draw(struct game_draw *gd, int pose, float t)
{
    float fov = (float) config_get_d(CONFIG_VIEW_FOV);

    if (gd->jump_b) fov *= 2.f * fabsf(gd->jump_dt - 0.5f);

    if (gd->state) common_draw(pose, t, fov, 0, &gd->draw, gd->view.c, gd->view.p, gd->view.e, &r_instance);
}

/*---------------------------------------------------------------------------*/

#define CURR 0
#define PREV 1

void game_lerp_init(struct game_lerp *gl, struct game_draw *gd)
{
    gl->alpha = 1.0f;

    sol_load_lerp(&gl->lerp, &gd->vary);

    gl->tilt[PREV] = gl->tilt[CURR] = gd->tilt;
    gl->view[PREV] = gl->view[CURR] = gd->view;

    gl->goal_k[PREV] = gl->goal_k[CURR] = gd->goal_k;
    gl->jump_dt[PREV] = gl->jump_dt[CURR] = gd->jump_dt;
}

void game_lerp_free(struct game_lerp *gl)
{
    sol_free_lerp(&gl->lerp);
}

void game_lerp_copy(struct game_lerp *gl)
{
    sol_lerp_copy(&gl->lerp);

    gl->tilt[PREV] = gl->tilt[CURR];
    gl->view[PREV] = gl->view[CURR];

    gl->goal_k[PREV] = gl->goal_k[CURR];
    gl->jump_dt[PREV] = gl->jump_dt[CURR];
}

void game_lerp_apply(struct game_lerp *gl, struct game_draw *gd)
{
    float a = gl->alpha;

    /* Solid. */

    sol_lerp_apply(&gl->lerp, a);

    /* Particles. */

    part_lerp_apply(a);

    /* Tilt. */

    v_lerp(gd->tilt.x, gl->tilt[PREV].x, gl->tilt[CURR].x, a);
    v_lerp(gd->tilt.z, gl->tilt[PREV].z, gl->tilt[CURR].z, a);

    gd->tilt.rx = flerp(gl->tilt[PREV].rx, gl->tilt[CURR].rx, a);
    gd->tilt.rz = flerp(gl->tilt[PREV].rz, gl->tilt[CURR].rz, a);

    /* View. */

    v_lerp(gd->view.c, gl->view[PREV].c, gl->view[CURR].c, a);
    v_lerp(gd->view.p, gl->view[PREV].p, gl->view[CURR].p, a);
    e_lerp(gd->view.e, gl->view[PREV].e, gl->view[CURR].e, a);

    /* Effects. */

    gd->goal_k = flerp(gl->goal_k[PREV], gl->goal_k[CURR], a);
    gd->jump_dt = flerp(gl->jump_dt[PREV], gl->jump_dt[CURR], a);
}

/*---------------------------------------------------------------------------*/
