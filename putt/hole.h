#ifndef HOLE_H
#define HOLE_H

#include "glext.h"

/*---------------------------------------------------------------------------*/

#define MAXHOL 28
#define MAXPLY 9

enum
{
    STAT_NONE = 0,
    STAT_SCORED,
    STAT_OUT
};

void  hole_init(const char *);
void  hole_free(void);
int   hole_exists(int);
int   hole_load(int, const char *);

char *hole_player(int);
char *hole_score(int, int);
char *hole_tot(int);

int  curr_hole(void);
int  curr_party(void);
int  curr_player(void);
int  curr_stroke(void);
int  curr_count(void);
int  curr_stat(int);

const char *curr_scr(void);
const char *curr_par(void);

int  hole_goto(int, int);
int  hole_next(void);
int  hole_move(void);
void hole_goal(void);
void hole_stop(void);
void hole_fall(void);

void hole_aud(void);
void hole_song(void);

GLubyte *player_color(int);
GLfloat *ball_color(int);

/*---------------------------------------------------------------------------*/

#endif
