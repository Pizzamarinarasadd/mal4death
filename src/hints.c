#include "hints.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#define HINT_BOX_W  60
#define HINT_BOX_H  10
#define HINT_BOX_X  ((CONSOLE_WIDTH  - HINT_BOX_W) / 2)
#define HINT_BOX_Y  ((CONSOLE_HEIGHT - HINT_BOX_H) / 2)

static void draw_hint_popup(HintPopup *hp) {
    if (!hp->text) return;
    ui_set_color(COLOR_YELLOW);
    ui_draw_box(HINT_BOX_X, HINT_BOX_Y, HINT_BOX_W, HINT_BOX_H, "HINT");

    ui_print_wrapped(HINT_BOX_X + 2, HINT_BOX_Y + 2, HINT_BOX_W - 4, 3, hp->text);

    char timer_line[40];
    snprintf(timer_line, sizeof(timer_line), "Auto-closes in: %ds   ", hp->countdown);
    ui_print_at(HINT_BOX_X + 2, HINT_BOX_Y + 6, timer_line);
    ui_print_at(HINT_BOX_X + 2, HINT_BOX_Y + 7, "[ESC] close  (hover to pause)    ");
    ui_set_color(COLOR_DEFAULT);
}

static void erase_hint_popup(void) {
    ui_fill_rect(HINT_BOX_X, HINT_BOX_Y, HINT_BOX_W, HINT_BOX_H, ' ');
}

static DWORD WINAPI hint_countdown_thread(LPVOID param) {
    HintPopup *hp = (HintPopup *)param;
    while (1) {
        Sleep(1000);
        EnterCriticalSection(&hp->lock);
        if (!hp->running) { LeaveCriticalSection(&hp->lock); break; }
        int should_draw = 0;
        int should_erase = 0;
        if (!hp->frozen) {
            hp->countdown--;
            should_draw = 1;
        }
        if (hp->countdown <= 0) {
            hp->active  = 0;
            hp->running = 0;
            should_erase = 1;
            should_draw  = 0;
        }
        LeaveCriticalSection(&hp->lock);
        if (should_draw)  draw_hint_popup(hp);
        if (should_erase) { erase_hint_popup(); break; }
    }
    return 0;
}

void hints_init(HintPopup *hp) {
    hp->active    = 0;
    hp->countdown = 40;
    hp->frozen    = 0;
    hp->running   = 0;
    hp->thread    = NULL;
    hp->text      = NULL;
    hp->box_x = (CONSOLE_WIDTH  - HINT_BOX_W) / 2;
    hp->box_y = (CONSOLE_HEIGHT - HINT_BOX_H) / 2;
    hp->box_w = HINT_BOX_W;
    hp->box_h = HINT_BOX_H;
    InitializeCriticalSection(&hp->lock);
}

void hints_show(HintPopup *hp, const char *text) {
    hints_dismiss(hp);
    EnterCriticalSection(&hp->lock);
    hp->active    = 1;
    hp->countdown = 40;
    hp->frozen    = 0;
    hp->running   = 1;
    hp->text      = text;
    draw_hint_popup(hp);
    LeaveCriticalSection(&hp->lock);
    hp->thread = CreateThread(NULL, 0, hint_countdown_thread, hp, 0, NULL);
}

void hints_dismiss(HintPopup *hp) {
    EnterCriticalSection(&hp->lock);
    hp->running = 0;
    LeaveCriticalSection(&hp->lock);
    if (hp->thread) {
        WaitForSingleObject(hp->thread, 1500);
        CloseHandle(hp->thread);
        hp->thread = NULL;
    }
    hp->active = 0;
    erase_hint_popup();
}

void hints_update_hover(HintPopup *hp, int mouse_x, int mouse_y) {
    if (!hp->active) return;
    EnterCriticalSection(&hp->lock);
    int inside = (mouse_x >= hp->box_x && mouse_x < hp->box_x + hp->box_w &&
                  mouse_y >= hp->box_y && mouse_y < hp->box_y + hp->box_h);
    hp->frozen = inside;
    LeaveCriticalSection(&hp->lock);
}

void hints_redraw(HintPopup *hp) {
    EnterCriticalSection(&hp->lock);
    int active = hp->active;
    LeaveCriticalSection(&hp->lock);
    if (active) draw_hint_popup(hp);
}

void hints_destroy(HintPopup *hp) {
    hints_dismiss(hp);
    DeleteCriticalSection(&hp->lock);
}
