/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERPUTT is  free software; you can redistribute  it and/or modify
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

#include "solid_sim.h"
#include "solid_all.h"

#include "common_draw.h"

/*---------------------------------------------------------------------------*/

static void game_draw_items(struct s_rend *rend, const struct s_vary *vary, const float *bill_M, float t)
{
    int hi;

    for (hi = 0; hi < vary->hc; hi++)
    {
        struct v_item *hp = &vary->hv[hi];

        float item_p[3], item_e[4], u[3], a;

        /* Skip picked up items. */

        if (hp->t == ITEM_NONE)
            continue;

        sol_entity_p(item_p, vary, hp->mi, hp->mj);
        sol_entity_e(item_e, vary, hp->mi, hp->mj);

        q_as_axisangle(item_e, u, &a);

        /* Draw model. */

        glPushMatrix();
        {
            glTranslatef(item_p[0], item_p[1], item_p[2]);
            glRotatef(V_DEG(a), u[0], u[1], u[2]);
            glTranslatef(hp->p[0], hp->p[1], hp->p[2]);
            item_draw(rend, hp, bill_M, t);
        }
        glPopMatrix();
    }
}

/*---------------------------------------------------------------------------*/

static void game_refl_all(struct s_rend *rend, const struct s_draw *fp, struct renderer *instance)
{
    glPushMatrix();
    {
        if (instance->rotate_tilt)
            instance->rotate_tilt(1);

        /* Draw the floor. */

        sol_refl(fp, rend);
    }
    glPopMatrix();
}

/*---------------------------------------------------------------------------*/

static void game_draw_light(int d, float t)
{
    GLfloat p[4];

    /* Configure the lighting. */

    light_conf();

    /* Overrride light 2 position. */

    p[0] = cosf(t);
    p[1] = 0.0f;
    p[2] = sinf(t);
    p[3] = 0.0f;

    glLightfv(GL_LIGHT2, GL_POSITION, p);

    /* Enable scene lights. */

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
}

static void game_clip_refl(int d)
{
    /* Fudge to eliminate the floor from reflection. */

    glClipPlane4f_(GL_CLIP_PLANE0, 0, 1, 0, -0.00001);
}

static void game_clip_ball(float *view_p, int d, const float *p)
{
    GLfloat r, c[3], pz[4], nz[4];

    /* Compute the plane giving the front of the ball, as seen from view.p. */

    c[0] = p[0];
    c[1] = p[1] * d;
    c[2] = p[2];

    pz[0] = view_p[0] - c[0];
    pz[1] = view_p[1] - c[1];
    pz[2] = view_p[2] - c[2];

    r = sqrt(pz[0] * pz[0] + pz[1] * pz[1] + pz[2] * pz[2]);

    pz[0] /= r;
    pz[1] /= r;
    pz[2] /= r;
    pz[3] = -(pz[0] * c[0] +
              pz[1] * c[1] +
              pz[2] * c[2]);

    /* Find the plane giving the back of the ball, as seen from view.p. */

    nz[0] = -pz[0];
    nz[1] = -pz[1];
    nz[2] = -pz[2];
    nz[3] = -pz[3];

    /* Reflect these planes as necessary, and store them in the GL state. */

    pz[1] *= d;
    nz[1] *= d;

    glClipPlane4f_(GL_CLIP_PLANE1, nz[0], nz[1], nz[2], nz[3]);
    glClipPlane4f_(GL_CLIP_PLANE2, pz[0], pz[1], pz[2], pz[3]);
}

static void game_draw_fore(struct s_rend *rend,
                           struct s_draw *draw, float *view_p,
                           int pose, float *M,
                           int d, float t,
                           struct renderer *instance)
{
    const float *ball_p = draw->vary->uv[0].p;

    glPushMatrix();
    {
        /* Rotate the environment about the position of the ball. */

        if (instance->rotate_tilt)
            instance->rotate_tilt(d);

        /* Compute clipping planes for reflection and ball facing. */

        game_clip_refl(d);
        game_clip_ball(view_p, d, ball_p);

        if (d < 0)
            glEnable(GL_CLIP_PLANE0);

        switch (pose)
        {
        case POSE_LEVEL:
            sol_draw(draw, rend, 0, 1);
            break;

        case POSE_BALL:
            if (curr_tex_env == &tex_env_pose)
            {
                /*
                 * We need the check above because otherwise the
                 * active texture env is set up in a way that makes
                 * level geometry visible, and we don't want that.
                 */

                glDepthMask(GL_FALSE);
                sol_draw(draw, rend, 0, 1);
                glDepthMask(GL_TRUE);
            }
            instance->draw_balls(rend, draw->vary, M, d, t);
            break;

        case POSE_NONE:
            /* Draw the coins. */

            game_draw_items(rend, draw->vary, M, t);

            /* Draw the floor. */

            sol_draw(draw, rend, 0, 1);

            /* Draw the ball. */

            instance->draw_balls(rend, draw->vary, M, d, t);

            break;
        }


        glDepthMask(GL_FALSE);
        {
            /* Draw the billboards, entity beams, and coin particles. */

            sol_bill(draw, rend, M, t);
            instance->draw_beams(rend, draw->vary);
            part_draw_coin(draw, rend, M, t);

            /* Draw the entity particles using only the sparkle light. */

            glDisable(GL_LIGHT0);
            glDisable(GL_LIGHT1);
            glEnable (GL_LIGHT2);
            {
                if (instance->draw_goals) instance->draw_goals(rend, draw->vary, t);
                if (instance->draw_jumps) instance->draw_jumps(rend, draw->vary, t);
            }
            glDisable(GL_LIGHT2);
            glEnable (GL_LIGHT1);
            glEnable (GL_LIGHT0);
        }
        glDepthMask(GL_TRUE);

        if (d < 0)
            glDisable(GL_CLIP_PLANE0);
    }
    glPopMatrix();
}

/*---------------------------------------------------------------------------*/

static void game_shadow_conf(int pose, int enable)
{
    if (enable && config_get_d(CONFIG_SHADOW))
    {
        switch (pose)
        {
        case POSE_LEVEL:
            /* No shadow. */
            tex_env_active(&tex_env_default);
            break;

        case POSE_BALL:
            /* Shadow only. */
            tex_env_select(&tex_env_pose,
                           &tex_env_default,
                           NULL);
            break;

        default:
            /* Regular shadow. */
            tex_env_select(&tex_env_shadow_clip,
                           &tex_env_shadow,
                           &tex_env_default,
                           NULL);
            break;
        }
    }
    else
    {
        tex_env_active(&tex_env_default);
    }
}

void common_draw(int pose, float t, float fov, struct s_draw *draw,
                 float view_c[3], float view_p[3], float view_e[3][3],
                 struct renderer *instance)
{
    struct s_rend rend;

    draw->shadow_ui = 0;

    game_shadow_conf(pose, 1);
    r_draw_enable(&rend);

    video_push_persp(fov, 0.1f, FAR_DIST);
    glPushMatrix();
    {
        float T[16], U[16], M[16], v[3];

        /* Compute direct and reflected view bases. */

        v[0] = +view_p[0];
        v[1] = -view_p[1];
        v[2] = +view_p[2];

        video_calc_view(T, view_c, view_p, view_e[1]);
        video_calc_view(U, view_c, v,      view_e[1]);
        
        m_xps(M, T);

        /* Apply the current view. */

        v_sub(v, view_c, view_p);

        glTranslatef(0.f, 0.f, -v_len(v));
        glMultMatrixf(M);
        glTranslatef(-view_c[0], -view_c[1], -view_c[2]);

        /* Draw the background. */

        instance->draw_back(&rend, pose, +1, t);

        /* Draw the reflection. */

        if (draw->reflective && config_get_d(CONFIG_REFLECTION))
        {
            glEnable(GL_STENCIL_TEST);
            {
                /* Draw the mirrors only into the stencil buffer. */

                glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
                glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glDepthMask(GL_FALSE);

                game_refl_all(&rend, draw, instance);

                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);

                /* Draw the scene reflected into color and depth buffers. */

                glFrontFace(GL_CW);
                glPushMatrix();
                {
                    glScalef(+1.0f, -1.0f, +1.0f);

                    game_draw_light(-1, t);

                    instance->draw_back(&rend, pose, -1, t);
                    game_draw_fore(&rend, draw, view_p, pose, U, -1, t, instance);
                }
                glPopMatrix();
                glFrontFace(GL_CCW);

                glStencilFunc(GL_ALWAYS, 0, 0xFFFFFFFF);
            }
            glDisable(GL_STENCIL_TEST);
        }

        /* Ready the lights for foreground rendering. */

        game_draw_light(1, t);

        /* When reflection is disabled, mirrors must be rendered opaque  */
        /* to prevent the background from showing.                       */

        if (draw->reflective && !config_get_d(CONFIG_REFLECTION))
        {
            r_color_mtrl(&rend, 1);
            {
                glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
                game_refl_all(&rend, draw, instance);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            }
            r_color_mtrl(&rend, 0);
        }

        /* Draw the mirrors and the rest of the foreground. */

        game_refl_all (&rend, draw, instance);
        game_draw_fore(&rend, draw, view_p, pose, T, +1, t, instance);
    }
    glPopMatrix();
    video_pop_matrix();
    
    r_draw_disable(&rend);
    game_shadow_conf(pose, 0);
}

void common_draw_balls(struct s_rend *rend, float *bill_M, float t, struct v_ball ball, GLfloat *color)
{
    float ball_M[16];
    float pend_M[16];

    m_basis(ball_M, ball.e[0], ball.e[1], ball.e[2]);
    m_basis(pend_M, ball.E[0], ball.E[1], ball.E[2]);

    glPushMatrix();
    {
        glTranslatef(ball.p[0],
                     ball.p[1] + BALL_FUDGE,
                     ball.p[2]);
        glScalef(ball.r,
                 ball.r,
                 ball.r);

        glColor4f(color[0], color[1], color[2], color[3]);
        ball_draw(rend, ball_M, pend_M, bill_M, t);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    glPopMatrix();
}

void common_draw_beams(struct s_rend *rend, struct s_vary *vary, int jump_e,
                       int goal_e, GLfloat goal_k, GLfloat *(*goal_c)(int))
{
    static const GLfloat jump_c[2][4]    =  {{ 0.7f, 0.5f, 1.0f, 0.5f },
                                             { 0.7f, 0.5f, 1.0f, 0.8f }};
    static const GLfloat swch_c[2][2][4] = {{{ 1.0f, 0.0f, 0.0f, 0.5f },
                                             { 1.0f, 0.0f, 0.0f, 0.8f }},
                                            {{ 0.0f, 1.0f, 0.0f, 0.5f },
                                             { 0.0f, 1.0f, 0.0f, 0.8f }}};

    const struct s_base *base = vary->base;
    float beam_p[3], beam_e[4], u[3], a;
    int i;

    /* Goal beams */

    if (goal_e)
        for (i = 0; i < base->zc; i++)
        {
            sol_entity_p(beam_p, vary, vary->zv[i].mi, vary->zv[i].mj);
            sol_entity_e(beam_e, vary, vary->zv[i].mi, vary->zv[i].mj);

            q_as_axisangle(beam_e, u, &a);

            glPushMatrix();
            {
                glTranslatef(beam_p[0], beam_p[1], beam_p[2]);
                glRotatef(V_DEG(a), u[0], u[1], u[2]);
                beam_draw(rend, base->zv[i].p, goal_c(i), base->zv[i].r, goal_k * 3.0f);
            }
            glPopMatrix();
        }

    /* Jump beams */

    for (i = 0; i < base->jc; i++)
    {
        sol_entity_p(beam_p, vary, vary->jv[i].mi, vary->jv[i].mj);
        sol_entity_e(beam_e, vary, vary->jv[i].mi, vary->jv[i].mj);

        q_as_axisangle(beam_e, u, &a);

        glPushMatrix();
        {
            glTranslatef(beam_p[0], beam_p[1], beam_p[2]);
            glRotatef(V_DEG(a), u[0], u[1], u[2]);
            beam_draw(rend, base->jv[i].p, jump_c[jump_e ? 0 : 1], base->jv[i].r, 2.0f);
        }
        glPopMatrix();
    }

    /* Switch beams */

    for (i = 0; i < base->xc; i++)
        if (!vary->xv[i].base->i)
        {
            sol_entity_p(beam_p, vary, vary->xv[i].mi, vary->xv[i].mj);
            sol_entity_e(beam_e, vary, vary->xv[i].mi, vary->xv[i].mj);

            q_as_axisangle(beam_e, u, &a);

            glPushMatrix();
            {
                glTranslatef(beam_p[0], beam_p[1], beam_p[2]);
                glRotatef(V_DEG(a), u[0], u[1], u[2]);
                beam_draw(rend, base->xv[i].p, swch_c[vary->xv[i].f][vary->xv[i].e], base->xv[i].r, 2.0f);
            }
            glPopMatrix();
        }
}

void common_draw_goals(struct s_rend *rend, struct s_vary *vary, float t,
                       int e, GLfloat goal_k, GLfloat *(*goal_c)(int))
{
    const struct s_base *base = vary->base;

    float goal_p[3], goal_e[4], u[3], a;
    int i;

    if (e)
    {
        if (goal_c) r_color_mtrl(rend, 1);
        for (i = 0; i < base->zc; i++)
        {
            sol_entity_p(goal_p, vary, vary->zv[i].mi, vary->zv[i].mj);
            sol_entity_e(goal_e, vary, vary->zv[i].mi, vary->zv[i].mj);

            q_as_axisangle(goal_e, u, &a);

            glPushMatrix();
            {
                glTranslatef(goal_p[0], goal_p[1], goal_p[2]);
                glRotatef(V_DEG(a), u[0], u[1], u[2]);

                if (goal_c) 
                {
                    GLfloat *c = goal_c(i);
                    glColor4f(c[0], c[1], c[2], c[3]);
                }

                goal_draw(rend, base->zv[i].p, base->zv[i].r, goal_k, t);
                if (goal_c) glColor4f(1.f, 1.f, 1.f, 1.f);
            }
            glPopMatrix();
        }
        if (goal_c) r_color_mtrl(rend, 0);
    }
}

void common_draw_jumps(struct s_rend *rend, struct s_vary *vary, float t)
{
    const struct s_base *base = vary->base;

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