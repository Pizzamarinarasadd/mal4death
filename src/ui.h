#ifndef UI_H
#define UI_H

#include <windows.h>

#define COLOR_DEFAULT  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define COLOR_GREEN    (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_RED      (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_YELLOW   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_CYAN     (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_WHITE    (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define COLOR_MAGENTA  (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)

#define CONSOLE_WIDTH  80
#define CONSOLE_HEIGHT 35

void ui_init(void);
void ui_enforce_size(void);
void ui_clear(void);
void ui_set_color(WORD color);
void ui_print_at(int x, int y, const char *text);
void ui_print_wrapped(int x, int y, int max_width, int max_rows, const char *text);
void ui_draw_box(int x, int y, int width, int height, const char *title);
void ui_fill_rect(int x, int y, int width, int height, char ch);

#endif
