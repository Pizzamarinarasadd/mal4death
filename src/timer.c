#include "timer.h"

static DWORD WINAPI timer_thread(LPVOID param) {
    Timer *t = (Timer *)param;
    float  accumulator = 0.0f;

    while (1) {
        Sleep(100);

        EnterCriticalSection(&t->lock);
        if (!t->running) {
            LeaveCriticalSection(&t->lock);
            break;
        }
        accumulator += t->speedup_mult * 0.1f;
        if (accumulator >= 1.0f) {
            int full_seconds = (int)accumulator;
            accumulator -= (float)full_seconds;
            t->time_remaining -= full_seconds;
            if (t->time_remaining <= 0) {
                t->time_remaining = 0;
                t->game_over = 1;
                t->running   = 0;
                LeaveCriticalSection(&t->lock);
                break;
            }
        }
        LeaveCriticalSection(&t->lock);
    }
    return 0;
}

void timer_init(Timer *t, int initial_seconds) {
    t->time_remaining = initial_seconds;
    t->speedup_mult   = 1.0f;
    t->running        = 0;
    t->game_over      = 0;
    t->initialized    = 1;
    InitializeCriticalSection(&t->lock);
    t->thread = NULL;
}

void timer_start(Timer *t) {
    if (t->thread != NULL) return;
    EnterCriticalSection(&t->lock);
    t->running = 1;
    LeaveCriticalSection(&t->lock);
    t->thread = CreateThread(NULL, 0, timer_thread, t, 0, NULL);
}

void timer_stop(Timer *t) {
    EnterCriticalSection(&t->lock);
    t->running = 0;
    LeaveCriticalSection(&t->lock);
    if (t->thread) {
        WaitForSingleObject(t->thread, 1000);
        CloseHandle(t->thread);
        t->thread = NULL;
    }
}

int timer_get_remaining(Timer *t) {
    EnterCriticalSection(&t->lock);
    int r = t->time_remaining;
    LeaveCriticalSection(&t->lock);
    return r;
}

void timer_apply_speedup(Timer *t, float mult) {
    EnterCriticalSection(&t->lock);
    t->speedup_mult *= mult;
    LeaveCriticalSection(&t->lock);
}

void timer_penalize(Timer *t, int seconds) {
    EnterCriticalSection(&t->lock);
    t->time_remaining -= seconds;
    if (t->time_remaining <= 0) {
        t->time_remaining = 0;
        t->game_over      = 1;
        t->running        = 0;
    }
    LeaveCriticalSection(&t->lock);
}

void timer_destroy(Timer *t) {
    if (!t->initialized) return;
    timer_stop(t);
    DeleteCriticalSection(&t->lock);
    t->initialized = 0;
}
