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

#include <SDL.h>
#include <math.h>

#include "glext.h"
#include "game.h"
#include "vec3.h"
#include "geom.h"
#include "ball.h"
#include "hole.h"
#include "hud.h"
#include "hmd.h"
#include "image.h"
#include "audio.h"
#include "config.h"
#include "video.h"

#include "common_draw.h"
#include "solid_draw.h"
#include "solid_sim.h"
#include "solid_all.h"

/*---------------------------------------------------------------------------*/

static struct s_full files[MAXPLY];
static struct s_full file;

static int scored[MAXPLY];
static int state;

static float view_a;                    /* Ideal view rotation about Y axis  */
static float view_m;
static float view_ry;                   /* Angular velocity about Y axis     */
static float view_dy;                   /* Ideal view distance above ball    */
static float view_dz;                   /* Ideal view distance behind ball   */

static float view_c[3];                 /* Current view center               */
static float view_v[3];                 /* Current view vector               */
static float view_p[3];                 /* Current view position             */
static float view_e[3][3];              /* Current view orientation          */

static float jump_e = 1;                /* Jumping enabled flag              */
static float jump_b = 0;                /* Jump-in-progress flag             */
static float jump_dt;                   /* Jump duration                     */
static float jump_p[3];                 /* Jump destination                  */

static float idle_t;                    /* Idling timeout                    */

/*---------------------------------------------------------------------------*/

static void view_init(void)
{
    view_a  = 0.f;
    view_m  = 0.f;
    view_ry = 0.f;
    view_dy = 3.f;
    view_dz = 5.f;

    view_c[0] = 0.f;
    view_c[1] = 0.f;
    view_c[2] = 0.f;

    view_p[0] =     0.f;
    view_p[1] = view_dy;
    view_p[2] = view_dz;

    view_e[0][0] = 1.f;
    view_e[0][1] = 0.f;
    view_e[0][2] = 0.f;
    view_e[1][0] = 0.f;
    view_e[1][1] = 1.f;
    view_e[1][2] = 0.f;
    view_e[2][0] = 0.f;
    view_e[2][1] = 0.f;
    view_e[2][2] = 1.f;
}

int game_init(const char *s)
{
    int i, ui;

    jump_e = 1;
    jump_b = 0;

    idle_t = 1.0f;

    view_init();

    for (ui = 0; ui < MAXPLY; ui++)
    {
        
    /* 
     * Don't allocate unneeded memory, but still initialize all references.
     * This fixes a segfault when not playing with max players.
     */
    if (ui > curr_party())
    {
        files[ui] = files[0];
        continue;
    }

    /* Load a level instance for each player. */
    if (!(state = sol_load_full(files + ui, s, config_get_d(CONFIG_SHADOW))))
        return 0;

    sol_init_sim(&files[ui].vary);
    file = files[ui];

    /* Apply starting position for each player. */
    if (ui)
    {
        v_cpy(file.vary.uv[0].p,    file.vary.uv[(ui - 1) % 4 + 1].p);
        v_cpy(file.vary.uv[0].e[0], file.vary.uv[(ui - 1) % 4 + 1].e[0]);
        v_cpy(file.vary.uv[0].e[1], file.vary.uv[(ui - 1) % 4 + 1].e[1]);
        v_cpy(file.vary.uv[0].e[2], file.vary.uv[(ui - 1) % 4 + 1].e[2]);
    }
    
    for (i = 0; i < file.base.dc; i++)
    {
        const char *k = file.base.av + file.base.dv[i].ai;
        const char *v = file.base.av + file.base.dv[i].aj;

        if (strcmp(k, "idle") == 0)
        {
            sscanf(v, "%f", &idle_t);

            if (idle_t < 1.0f)
                idle_t = 1.0f;
        }
    }
    }
    
    return 1;
}

void game_free(void)
{
    sol_quit_sim();

    for (int ui = curr_party(); ui >= 0; ui--) sol_free_full(files + ui);
    for (int ui = MAXPLY - 1; ui >= 0; ui--) scored[ui] = 0;

    state = 0;
}

/*---------------------------------------------------------------------------*/

static int save_goal_id(struct b_goal *goal)
{
    if (!goal)
        return 0;

    scored[curr_player()] = file.base.zv - goal;
    return 1;
}

static GLfloat *goal_color(int goal_id)
{
    static GLfloat goal_c[4] = {1.f, 1.f, 1.f, 1.f};
    GLfloat *ball_colors;
    
    int ui, i;
    float tot = 0;

    for (ui = curr_party(); ui > 0; ui--)
        if (curr_stat(ui) == STAT_SCORED && scored[ui] == goal_id) tot++;

    /* Blend ball colors. */

    goal_c[0] = tot ? 0.f : 1.f;
    goal_c[1] = tot ? 0.f : 1.f;
    goal_c[2] = tot ? 0.f : 1.f;

    for (ui = curr_party(); ui > 0; ui--)
        if (curr_stat(ui) == STAT_SCORED && scored[ui] == goal_id)
        {
            ball_colors = ball_color(ui);
            for (i = 0; i < 3; i++)
                goal_c[i] += ball_colors[i] / tot;
        }

    return goal_c;
}

static void game_draw_back(struct s_rend *rend, int pose, int d, float t)
{
    if (pose == POSE_BALL)
        return;

    /* Center the skybox about the position of the camera. */

    glPushMatrix();
    {
        glTranslatef(view_p[0], view_p[1], view_p[2]);
        back_draw(rend);
    }
    glPopMatrix();
}

static void game_draw_vect(struct s_rend *rend, struct v_ball uv_ball)
{
    if (view_m > 0.f)
    {
        glPushMatrix();
        {
            glTranslatef(uv_ball.p[0],
                         uv_ball.p[1],
                         uv_ball.p[2]);
            glRotatef(view_a, 0.0f, 1.0f, 0.0f);
            glScalef(uv_ball.r,
                     uv_ball.r * 0.1f, view_m);

            vect_draw(rend);
        }
        glPopMatrix();
    }
}

static void game_draw_balls(struct s_rend *rend,
                            struct s_vary *fp,
                            float *bill_M, int d, float t)
{
    GLfloat *color;

    int ui;
    struct v_ball ui_ball;

    r_color_mtrl(rend, 1);

    for (ui = curr_party(); ui > 0; ui--)
    {
        if (curr_stat(ui) == STAT_SCORED)
            continue;

        ui_ball = files[ui].draw.vary->uv[0];
        color = ball_color(ui);
        
        if (ui == curr_player())
        {
            common_draw_balls(rend, bill_M, t, ui_ball, color);

            /* Draw the aim polygon for the current player. */

            if (d != -1)
                game_draw_vect(rend, ui_ball);
        }
        else
        {
            glPushMatrix();
            {
                glTranslatef(ui_ball.p[0],
                             ui_ball.p[1] - ui_ball.r + BALL_FUDGE,
                             ui_ball.p[2]);
                glScalef(ui_ball.r,
                         ui_ball.r,
                         ui_ball.r);

                glColor4f(color[0],
                          color[1],
                          color[2], 0.5f);

                mark_draw(rend);
            }
            glPopMatrix();
        }
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    r_color_mtrl(rend, 0);
}

static void game_draw_flags(struct s_rend *rend, const struct s_base *fp)
{
    int zi;
    float *flag_p;

    for (zi = 0; zi < fp->zc; zi++)
    {
        flag_p = fp->zv[zi].p;
        glPushMatrix();
        {
            /* Rotate goal flags to always face the camera. */

            glTranslatef(flag_p[0], flag_p[1], flag_p[2]);
            glRotatef(view_a, 0, 1, 0);
            glTranslatef(-flag_p[0], -flag_p[1], -flag_p[2]);
            flag_draw(rend, flag_p);
        }
        glPopMatrix();
    }
}

static void game_draw_beams(struct s_rend *rend, struct s_vary *vary)
{
    game_draw_flags(rend, vary->base);
    common_draw_beams(rend, vary, jump_e, 1, 0.25f, goal_color);
}

static void game_draw_goals(struct s_rend *rend, struct s_vary *vary, float t)
{
    common_draw_goals(rend, vary, t, 1, 0.25f, goal_color);
}

/*---------------------------------------------------------------------------*/

struct renderer r_instance = {
    game_draw_back,
    game_draw_balls,
    game_draw_beams,
    game_draw_goals,
    common_draw_jumps,
    NULL
};

void game_draw(int pose, float t)
{
    if (!state)
        return;

    float fov = FOV;
    if (jump_b) fov *= 2.0f * fabsf(jump_dt - 0.5f);

    /* In VR, move the view center up to keep the viewer level. */

    float c[3];
    v_cpy(c, view_c);

    if (hmd_stat())
        c[1] += view_dy;

    common_draw(pose, t, fov, &file.draw, c, view_p, view_e, &r_instance);
}

/*---------------------------------------------------------------------------*/

void game_update_view(float dt)
{
    const float y[3] = { 0.f, 1.f, 0.f };

    float dy;
    float dz;
    float k;
    float e[3];
    float d[3];
    float s = 2.f * dt;

    if (!state)
        return;

    /* Center the view about the ball. */

    v_cpy(view_c, file.vary.uv[0].p);
    v_inv(view_v, file.vary.uv[0].v);

    switch (config_get_d(CONFIG_CAMERA))
    {
    case 2:
        /* Camera 2: View vector is given by view angle. */

        view_e[2][0] = fsinf(V_RAD(view_a));
        view_e[2][1] = 0.f;
        view_e[2][2] = fcosf(V_RAD(view_a));

        s = 1.f;
        break;

    default:
        /* View vector approaches the ball velocity vector. */

        v_mad(e, view_v, y, v_dot(view_v, y));
        v_inv(e, e);

        k = v_dot(view_v, view_v);

        v_sub(view_e[2], view_p, view_c);
        v_mad(view_e[2], view_e[2], view_v, k * dt * 0.1f);
    }

    /* Orthonormalize the basis of the view in its new position. */

    v_crs(view_e[0], view_e[1], view_e[2]);
    v_crs(view_e[2], view_e[0], view_e[1]);
    v_nrm(view_e[0], view_e[0]);
    v_nrm(view_e[2], view_e[2]);

    /* The current view (dy, dz) approaches the ideal (view_dy, view_dz). */

    v_sub(d, view_p, view_c);

    dy = v_dot(view_e[1], d);
    dz = v_dot(view_e[2], d);

    dy += (view_dy - dy) * s;
    dz += (view_dz - dz) * s;

    /* Compute the new view position. */

    view_p[0] = view_p[1] = view_p[2] = 0.f;

    v_mad(view_p, view_c, view_e[1], dy);
    v_mad(view_p, view_p, view_e[2], dz);

    view_a = V_DEG(fatan2f(view_e[2][0], view_e[2][2]));
}

static int game_update_state(float dt)
{
    static float t = 0.f;

    struct s_vary *fp = &file.vary;
    float p[3];

    if (dt > 0.f)
        t += dt;
    else
        t = 0.f;

    /* Test for a switch. */

    if (sol_swch_test(fp, NULL, 0) == SWCH_INSIDE)
        audio_play(AUD_SWITCH, 1.f);

    /* Test for a jump. */

    if (jump_e == 1 && jump_b == 0 && (sol_jump_test(fp, jump_p, 0) ==
                                       JUMP_INSIDE))
    {
        jump_b  = 1;
        jump_e  = 0;
        jump_dt = 0.f;

        audio_play(AUD_JUMP, 1.f);
    }
    if (jump_e == 0 && jump_b == 0 && (sol_jump_test(fp, jump_p, 0) ==
                                       JUMP_OUTSIDE))
    {
        jump_e = 1;
    }

    /* Test for fall-out. */

    if (file.base.vc == 0 || fp->uv[0].p[1] < file.base.vv[0].p[1])
        return GAME_FALL;

    /* Test for a goal or stop. */

    if (t > 1.f && save_goal_id(sol_goal_test(fp, p, 0)))
    {
        t = 0.f;
        return GAME_GOAL;
    }

    if (t > idle_t)
    {
        t = 0.f;
        return GAME_STOP;
    }

    return GAME_NONE;
}

/*
 * On  most  hardware, rendering  requires  much  more  computing power  than
 * physics.  Since  physics takes less time  than graphics, it  make sense to
 * detach  the physics update  time step  from the  graphics frame  rate.  By
 * performing multiple physics updates for  each graphics update, we get away
 * with higher quality physics with little impact on overall performance.
 *
 * Toward this  end, we establish a  baseline maximum physics  time step.  If
 * the measured  frame time  exceeds this  maximum, we cut  the time  step in
 * half, and  do two updates.  If THIS  time step exceeds the  maximum, we do
 * four updates.  And  so on.  In this way, the physics  system is allowed to
 * seek an optimal update rate independent of, yet in integral sync with, the
 * graphics frame rate.
 */

int game_step(const float g[3], float dt)
{
    if (!state)
        return GAME_NONE;

    struct s_vary *fp = &file.vary;

    static float s = 0.f;
    static float t = 0.f;

    float d = 0.f;
    float b = 0.f;
    float st = 0.f;
    int i, n = 1, m = 0;

    s = (7.f * s + dt) / 8.f;
    t = s;

    if (jump_b)
    {
        jump_dt += dt;

        /* Handle a jump. */

        if (0.5f < jump_dt)
        {
            fp->uv[0].p[0] = jump_p[0];
            fp->uv[0].p[1] = jump_p[1];
            fp->uv[0].p[2] = jump_p[2];
        }
        if (1.f < jump_dt)
            jump_b = 0;
    }
    else
    {
        /* Run the sim. */

        while (t > MAX_DT && n < MAX_DN)
        {
            t /= 2;
            n *= 2;
        }

        for (i = 0; i < n; i++)
        {
            d = sol_step(fp, NULL, g, t, 0, &m);

            if (b < d)
                b = d;
            if (m)
                st += t;
        }

        /* Mix the sound of a ball bounce. */

        if (b > 0.5f)
            audio_play(AUD_BUMP, (b - 0.5f) * 2.0f);
    }

    game_update_view(dt);
    return game_update_state(st);
}

void game_putt(void)
{
    /*
     * HACK: The BALL_FUDGE here  guarantees that a putt doesn't drive
     * the ball  too directly down  toward a lump,  triggering rolling
     * friction too early and stopping the ball prematurely.
     */

    file.vary.uv[0].v[0] = -4.f * view_e[2][0] * view_m;
    file.vary.uv[0].v[1] = -4.f * view_e[2][1] * view_m + BALL_FUDGE;
    file.vary.uv[0].v[2] = -4.f * view_e[2][2] * view_m;

    view_m = 0.f;
}

/*---------------------------------------------------------------------------*/

void game_set_rot(float d)
{
    view_a += (float) (30.f * d) / config_get_d(CONFIG_MOUSE_SENSE);
}

void game_clr_mag(void)
{
    view_m = 1.f;
}

void game_set_mag(float d)
{
    view_m -= (float) (1.f * d) / config_get_d(CONFIG_MOUSE_SENSE);

    if (view_m < 0.25f)
        view_m = 0.25f;
}

void game_set_fly(float k)
{
    if (!state)
        return;

    struct s_vary *fp = &file.vary;

    float  x[3] = { 1.f, 0.f, 0.f };
    float  y[3] = { 0.f, 1.f, 0.f };
    float  z[3] = { 0.f, 0.f, 1.f };
    float c0[3] = { 0.f, 0.f, 0.f };
    float p0[3] = { 0.f, 0.f, 0.f };
    float c1[3] = { 0.f, 0.f, 0.f };
    float p1[3] = { 0.f, 0.f, 0.f };
    float  v[3];

    v_cpy(view_e[0], x);
    v_cpy(view_e[1], y);

    if (fp->base->zc > 0)
        v_sub(view_e[2], fp->uv[0].p, fp->base->zv[0].p);
    else
        v_cpy(view_e[2], z);

    if (fabs(v_dot(view_e[1], view_e[2])) > 0.999)
        v_cpy(view_e[2], z);

    v_crs(view_e[0], view_e[1], view_e[2]);
    v_crs(view_e[2], view_e[0], view_e[1]);

    v_nrm(view_e[0], view_e[0]);
    v_nrm(view_e[2], view_e[2]);

    /* k = 0.0 view is at the ball. */

    if (fp->uc > 0)
    {
        v_cpy(c0, fp->uv[0].p);
        v_cpy(p0, fp->uv[0].p);
    }

    v_mad(p0, p0, view_e[1], view_dy);
    v_mad(p0, p0, view_e[2], view_dz);

    /* k = +1.0 view is s_view 0 */

    if (k >= 0 && fp->base->wc > 0)
    {
        v_cpy(p1, fp->base->wv[0].p);
        v_cpy(c1, fp->base->wv[0].q);
    }

    /* k = -1.0 view is s_view 1 */

    if (k <= 0 && fp->base->wc > 1)
    {
        v_cpy(p1, fp->base->wv[1].p);
        v_cpy(c1, fp->base->wv[1].q);
    }

    /* Interpolate the views. */

    v_sub(v, p1, p0);
    v_mad(view_p, p0, v, k * k);

    v_sub(v, c1, c0);
    v_mad(view_c, c0, v, k * k);

    /* Orthonormalize the view basis. */

    v_sub(view_e[2], view_p, view_c);
    v_crs(view_e[0], view_e[1], view_e[2]);
    v_crs(view_e[2], view_e[0], view_e[1]);
    v_nrm(view_e[0], view_e[0]);
    v_nrm(view_e[2], view_e[2]);

    view_a = V_DEG(fatan2f(view_e[2][0], view_e[2][2]));
}

void game_ball(int i)
{
    int ui;

    file = files[i];

    jump_e = 1;
    jump_b = 0;

    for (ui = 0; ui < file.vary.uc; ui++)
    {
        file.vary.uv[ui].v[0] = 0.f;
        file.vary.uv[ui].v[1] = 0.f;
        file.vary.uv[ui].v[2] = 0.f;

        file.vary.uv[ui].w[0] = 0.f;
        file.vary.uv[ui].w[1] = 0.f;
        file.vary.uv[ui].w[2] = 0.f;
    }
}

void game_get_pos(float p[3], float e[3][3])
{
    v_cpy(p,    file.vary.uv[0].p);
    v_cpy(e[0], file.vary.uv[0].e[0]);
    v_cpy(e[1], file.vary.uv[0].e[1]);
    v_cpy(e[2], file.vary.uv[0].e[2]);
}

void game_set_pos(float p[3], float e[3][3])
{
    v_cpy(file.vary.uv[0].p,    p);
    v_cpy(file.vary.uv[0].e[0], e[0]);
    v_cpy(file.vary.uv[0].e[1], e[1]);
    v_cpy(file.vary.uv[0].e[2], e[2]);
}

/*---------------------------------------------------------------------------*/

