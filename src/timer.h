#ifndef TIMER_H
#define TIMER_H

#include <windows.h>

typedef struct {
    volatile int    time_remaining;
    volatile float  speedup_mult;
    volatile int    running;
    volatile int    game_over;
    int             initialized;
    CRITICAL_SECTION lock;
    HANDLE           thread;
} Timer;

void  timer_init(Timer *t, int initial_seconds);
void  timer_start(Timer *t);
void  timer_stop(Timer *t);
int   timer_get_remaining(Timer *t);
void  timer_apply_speedup(Timer *t, float mult);
void  timer_set_speedup(Timer *t, float mult);
void  timer_penalize(Timer *t, int seconds);
void  timer_continue(Timer *t, int seconds);
void  timer_destroy(Timer *t);

#endif
