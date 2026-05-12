#include "tools.h"
#include "ui.h"
#include <windows.h>
#include <stdio.h>

void tools_launch(Difficulty difficulty,
                  const char *tool_name,
                  const char *tool_path,
                  const char *sample_path) {
    if (!tool_path) return;

    switch (difficulty) {
    case NEWBIE:
        ShellExecuteA(NULL, "open", tool_path, sample_path, NULL, SW_SHOW);
        break;
    case STANDARD: {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Please open %s with file: %s",
                 tool_name ? tool_name : tool_path,
                 sample_path ? sample_path : "");
        ui_set_color(COLOR_YELLOW);
        ui_print_at(2, 30, msg);
        ui_set_color(COLOR_DEFAULT);
        break;
    }
    case HARDCORE:
        break;
    }
}
