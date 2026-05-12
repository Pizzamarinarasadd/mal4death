#include "ui.h"
#include <stdio.h>
#include <string.h>

static HANDLE hConsole;

void ui_init(void) {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    SMALL_RECT windowSize = {0, 0, CONSOLE_WIDTH - 1, CONSOLE_HEIGHT - 1};
    COORD bufferSize = {CONSOLE_WIDTH, CONSOLE_HEIGHT};

    SetConsoleScreenBufferSize(hConsole, bufferSize);
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);

    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    SetConsoleTitleA("Mal4Death");

    /* Lock window size — prevent resize breaking layout */
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE,
                      style & ~(WS_SIZEBOX | WS_MAXIMIZEBOX));
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void ui_enforce_size(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    if (csbi.dwSize.X == CONSOLE_WIDTH && csbi.dwSize.Y == CONSOLE_HEIGHT &&
        csbi.srWindow.Right  - csbi.srWindow.Left + 1 == CONSOLE_WIDTH &&
        csbi.srWindow.Bottom - csbi.srWindow.Top  + 1 == CONSOLE_HEIGHT)
        return;
    SMALL_RECT tiny       = {0, 0, 0, 0};
    COORD      bufSize    = {CONSOLE_WIDTH, CONSOLE_HEIGHT};
    SMALL_RECT winSize    = {0, 0, CONSOLE_WIDTH - 1, CONSOLE_HEIGHT - 1};
    SetConsoleWindowInfo(hConsole, TRUE, &tiny);
    SetConsoleScreenBufferSize(hConsole, bufSize);
    SetConsoleWindowInfo(hConsole, TRUE, &winSize);
}

void ui_clear(void) {
    ui_enforce_size();
    COORD topLeft = {0, 0};
    DWORD written;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacterA(hConsole, ' ', size, topLeft, &written);
    FillConsoleOutputAttribute(hConsole, COLOR_DEFAULT, size, topLeft, &written);
    SetConsoleCursorPosition(hConsole, topLeft);
}

void ui_set_color(WORD color) {
    SetConsoleTextAttribute(hConsole, color);
}

void ui_print_at(int x, int y, const char *text) {
    if (!text) return;
    COORD pos = {(SHORT)x, (SHORT)y};
    SetConsoleCursorPosition(hConsole, pos);
    DWORD written;
    WriteConsoleA(hConsole, text, (DWORD)strlen(text), &written, NULL);
}

void ui_print_wrapped(int x, int y, int max_width, int max_rows,
                      const char *text) {
    if (!text) return;
    const char *p = text;
    int row = 0;
    while (*p && row < max_rows) {
        char line[128];
        int len = 0;
        const char *last_space = NULL;
        int last_space_len = 0;
        while (*p && *p != '\n' && len < max_width) {
            if (*p == ' ') { last_space = p; last_space_len = len; }
            line[len++] = *p++;
        }
        if (*p == '\n') {
            line[len] = '\0'; p++;
        } else if (*p && last_space) {
            len = last_space_len; p = last_space + 1;
            line[len] = '\0';
        } else {
            line[len] = '\0';
        }
        ui_print_at(x, y + row, line);
        row++;
    }
}

void ui_draw_box(int x, int y, int width, int height, const char *title) {
    char line[256];
    int i;

    /* top border */
    line[0] = 0xC9; /* ╔ */
    int title_len = title ? (int)strlen(title) : 0;
    int dash_total = width - 2 - title_len - (title_len ? 2 : 0);
    if (dash_total < 0) dash_total = 0;
    int dash_left = dash_total / 2;
    int dash_right = dash_total - dash_left;
    int pos = 1;
    for (i = 0; i < dash_left; i++) line[pos++] = 0xCD;  /* ═ */
    if (title_len) {
        line[pos++] = ' ';
        memcpy(line + pos, title, title_len); pos += title_len;
        line[pos++] = ' ';
    }
    for (i = 0; i < dash_right; i++) line[pos++] = 0xCD;
    line[pos++] = 0xBB; /* ╗ */
    line[pos] = '\0';
    ui_print_at(x, y, line);

    /* sides */
    for (i = 1; i < height - 1; i++) {
        char side[3] = {0xBA, ' ', '\0'}; /* ║ */
        ui_print_at(x, y + i, side);
        ui_fill_rect(x + 1, y + i, width - 2, 1, ' ');
        ui_print_at(x + width - 1, y + i, side);
    }

    /* bottom border */
    pos = 0;
    line[pos++] = 0xC8; /* ╚ */
    for (i = 0; i < width - 2; i++) line[pos++] = 0xCD;
    line[pos++] = 0xBC; /* ╝ */
    line[pos] = '\0';
    ui_print_at(x, y + height - 1, line);
}

void ui_fill_rect(int x, int y, int width, int height, char ch) {
    COORD pos;
    DWORD written;
    for (int row = 0; row < height; row++) {
        pos.X = (SHORT)x;
        pos.Y = (SHORT)(y + row);
        FillConsoleOutputCharacterA(hConsole, ch, width, pos, &written);
    }
}
