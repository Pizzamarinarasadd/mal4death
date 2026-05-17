#ifndef HINTS_H
#define HINTS_H

#include <windows.h>

typedef struct {
    volatile int    active;
    volatile int    countdown;
    volatile int    frozen;
    volatile int    running;
    HANDLE          thread;
    CRITICAL_SECTION lock;
    int  box_x, box_y, box_w, box_h;
    const char *text;
} HintPopup;

void hints_init(HintPopup *hp);
void hints_show(HintPopup *hp, const char *text);
void hints_dismiss(HintPopup *hp);
void hints_update_hover(HintPopup *hp, int mouse_x, int mouse_y);
void hints_redraw(HintPopup *hp);
void hints_destroy(HintPopup *hp);

#endif
