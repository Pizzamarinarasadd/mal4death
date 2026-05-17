#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "md5.h"
#include "generated_keys.h"
#include "gif_frames.h"
#include "ui.h"
#include "difficulty.h"
#include "levels.h"
#include "validation.h"
#include "tools.h"
#include "timer.h"
#include "hints.h"
#include "audio.h"

/* ── Progress (stars / save) ───────────────────────────────────────────── */

static int  prog_completed[3] = {0, 0, 0};
static char prog_save_path[MAX_PATH];
static int  prog_has_save = 0; /* 1 if save.dat existed at launch */

static void progress_load(void) {
    char exe_dir[MAX_PATH];
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last = strrchr(exe_dir, '\\');
    if (last) *(last + 1) = '\0'; else exe_dir[0] = '\0';
    strncpy(prog_save_path, exe_dir, MAX_PATH - 9);
    prog_save_path[MAX_PATH - 9] = '\0';
    strncat(prog_save_path, "save.dat", 9);
    FILE *f = fopen(prog_save_path, "r");
    if (!f) return;
    prog_has_save = 1;
    char buf[8] = {0};
    fread(buf, 1, 3, f);
    fclose(f);
    for (int i = 0; i < 3; i++)
        prog_completed[i] = (buf[i] == '1') ? 1 : 0;
}

static void progress_save(void) {
    FILE *f = fopen(prog_save_path, "w");
    if (!f) return;
    fprintf(f, "%c%c%c",
            prog_completed[0] ? '1' : '0',
            prog_completed[1] ? '1' : '0',
            prog_completed[2] ? '1' : '0');
    fclose(f);
}

static void progress_reset(void) {
    for (int i = 0; i < 3; i++) prog_completed[i] = 0;
    remove(prog_save_path);
    prog_has_save = 0;
}

static void progress_mark_complete(Difficulty d) {
    if ((int)d >= 0 && (int)d < 3) prog_completed[(int)d] = 1;
    progress_save();
}

static int progress_stars(void) {
    return prog_completed[0] + prog_completed[2]; /* slot 1 (Standard) removed */
}

static void progress_set_all(int val) {
    for (int i = 0; i < 3; i++) prog_completed[i] = val ? 1 : 0;
}

/* ── GameState ─────────────────────────────────────────────────────────── */

typedef enum {
    MENU, DIFFICULTY_SELECT, PHASE, WIN, GAME_OVER, AUDIO_SETTINGS, DEV_OPTIONS,
    SPACE_VAULT
} GameScreen;

typedef struct {
    GameScreen       screen;
    int              current_phase;
    Difficulty       difficulty;
    DifficultyConfig diff_cfg;
    Timer            timer;
    HintPopup        hint_popup;
    int              hints_used;
    int              free_hints_left;
    int              current_hint_idx;
    char             key_input[128];
    int              key_input_len;
    int              wrong_answer;
    int              continues_left;
    /* developer options */
    int              dev_presentation;   /* 0=off, 1=on: shows [S] skip button */
    int              dev_show_keys;      /* 0=off, 1=on: shows expected key on phase screen */
    int              dev_timers[3];      /* timer override per difficulty (0 = use default) */
    int              dev_all_completed;  /* 0=off, 1=on: pretend all difficulties beaten */
    int              dev_show_vault_key; /* 0=off, 1=on: reveal decoded vault key */
    int              dev_skip_lore;      /* 0=off, 1=on: skip phase intro text */
} GameState;

static GameState G;

/* ── Helpers ───────────────────────────────────────────────────────────── */

static void enable_mouse_input(HANDLE hIn) {
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT |
                        ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);
}

static void format_time(int seconds, char *buf, int buf_size) {
    snprintf(buf, buf_size, "%02d:%02d", seconds / 60, seconds % 60);
}

static int effective_stars(void) {
    return G.dev_all_completed ? 2 : progress_stars();
}

static int dev_effective_timer(Difficulty d) {
    if (G.dev_timers[(int)d] > 0) return G.dev_timers[(int)d];
    return get_difficulty_config(d).initial_time;
}

/* ── Screen: Main Menu ─────────────────────────────────────────────────── */

static void draw_menu(void) {
    ui_clear();

    /* ── MAL  (magenta) — x=15, small figlet font, 4 rows ─── */
    ui_set_color(COLOR_MAGENTA);
    ui_print_at(15, 3, " __  __   _   _    ");
    ui_print_at(15, 4, "|  \\/  | /_\\ | |   ");
    ui_print_at(15, 5, "| |\\/| |/ _ \\| |__ ");
    ui_print_at(15, 6, "|_|  |_/_/ \\_\\____|");

    /* ── 4  (red) — x=34 ──────── */
    ui_set_color(COLOR_RED);
    ui_print_at(34, 3, " _ _  ");
    ui_print_at(34, 4, "| | | ");
    ui_print_at(34, 5, "|_  _|");
    ui_print_at(34, 6, "  |_| ");

    /* ── DEATH  (magenta) — x=40, total banner=50 cols, left=right=15 ── */
    ui_set_color(COLOR_MAGENTA);
    ui_print_at(40, 3, " ___  ___   _ _____ _  _ ");
    ui_print_at(40, 4, "|   \\| __| /_\\_   _| || |");
    ui_print_at(40, 5, "| |) | _| / _ \\| | | __ |");
    ui_print_at(40, 6, "|___/|___/_/ \\_\\_| |_||_|");

    ui_set_color(COLOR_CYAN);
    ui_print_at(25, 8, "[ Malware Analysis Challenge ]");

    /* stars */
    int stars = effective_stars();
    if (stars > 0) {
        ui_set_color(COLOR_YELLOW);
        char star_line[16] = {0};
        for (int s = 0; s < stars; s++) {
            if (s > 0) strcat(star_line, "  ");
            strcat(star_line, "[*]");
        }
        int sx = (CONSOLE_WIDTH - (stars * 3 + (stars - 1) * 2)) / 2;
        ui_print_at(sx, 10, star_line);
        ui_set_color(COLOR_DEFAULT);
    }

    int row = 13;
    ui_set_color(COLOR_DEFAULT);
    if (prog_has_save) {
        ui_print_at(28, row++, "[1]  Continue");
        ui_set_color(COLOR_RED);
        ui_print_at(28, row++, "[R]  New Game  (resets progress)");
        ui_set_color(COLOR_DEFAULT);
    } else {
        ui_print_at(28, row++, "[1]  New Game");
    }
    ui_print_at(28, row++, "[2]  Quit");
    ui_set_color(COLOR_CYAN);
    ui_print_at(28, row++, "[3]  Audio Settings");
    if (stars >= 2) {
        ui_set_color(COLOR_YELLOW);
        ui_print_at(28, row++, "[4]  Space Vault");
        ui_set_color(COLOR_DEFAULT);
    }
    ui_print_at(15, row + 1, "Chief, the planet is counting on you.");
    if (G.dev_presentation) {
        ui_set_color(COLOR_YELLOW);
        ui_print_at(25, 20, "-- PRESENTATION MODE ACTIVE --");
        ui_set_color(COLOR_DEFAULT);
    }
    ui_set_color(COLOR_DEFAULT);
    ui_print_at(1, 33, "by Pizzamarinarasadd");
    ui_print_at(60, 33, "[F10] Dev");
}

static GameScreen screen_menu(void) {
    draw_menu();
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    enable_mouse_input(hIn);
    while (1) {
        INPUT_RECORD rec;
        DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) { draw_menu(); continue; }
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
            char ch = rec.Event.KeyEvent.uChar.AsciiChar;
            if (ch == '1') return DIFFICULTY_SELECT;
            if ((ch == 'r' || ch == 'R') && prog_has_save) {
                progress_reset();
                draw_menu();
                continue;
            }
            if (ch == '2') ExitProcess(0);
            if (ch == '3') return AUDIO_SETTINGS;
            if (ch == '4' && effective_stars() >= 2) return SPACE_VAULT;
            if (vk == VK_F10) return DEV_OPTIONS;
        }
    }
}

/* ── Screen: Difficulty Select ─────────────────────────────────────────── */

static int hardcore_unlocked(void) {
    return prog_completed[0] || G.dev_all_completed;
}

static void draw_difficulty(void) {
    ui_clear();
    ui_set_color(COLOR_CYAN);

    int hc = hardcore_unlocked();
    char tbuf[8];
    char line[72];

    if (hc) {
        ui_draw_box(5, 2, 70, 22, "SELECT DIFFICULTY");

        ui_set_color(COLOR_GREEN);
        ui_print_at(8, 7, "[1]  STORY MODE");
        ui_set_color(COLOR_DEFAULT);
        format_time(dev_effective_timer(NEWBIE), tbuf, sizeof(tbuf));
        snprintf(line, sizeof(line),
                 "     Time: %s  |  Hints: 3  |  Wrong answer: -5 min", tbuf);
        ui_print_at(8, 8, line);

        ui_set_color(COLOR_RED);
        ui_print_at(8, 15, "[2]  HARDCORE");
        ui_set_color(COLOR_DEFAULT);
        format_time(dev_effective_timer(HARDCORE), tbuf, sizeof(tbuf));
        snprintf(line, sizeof(line),
                 "     Time: %s  |  Hints: 0  |  Wrong answer: -5 min", tbuf);
        ui_print_at(8, 16, line);
        ui_set_color(COLOR_YELLOW);
        ui_print_at(8, 17, "     No dialogue. You are on your own.");
        ui_set_color(COLOR_DEFAULT);
    } else {
        ui_draw_box(5, 2, 70, 10, "SELECT DIFFICULTY");

        ui_set_color(COLOR_GREEN);
        ui_print_at(8, 5, "[1]  STORY MODE");
        ui_set_color(COLOR_DEFAULT);
        format_time(dev_effective_timer(NEWBIE), tbuf, sizeof(tbuf));
        snprintf(line, sizeof(line),
                 "     Time: %s  |  Hints: 3  |  Wrong answer: -5 min", tbuf);
        ui_print_at(8, 6, line);
    }
}

static GameScreen screen_difficulty(void) {
    draw_difficulty();
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);
    while (1) {
        INPUT_RECORD rec;
        DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) { draw_difficulty(); continue; }
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
            char ch = rec.Event.KeyEvent.uChar.AsciiChar;
            if (vk == VK_ESCAPE) return MENU;
            Difficulty d;
            if (ch == '1') d = NEWBIE;
            else if (ch == '2' && hardcore_unlocked()) d = HARDCORE;
            else continue;

            G.difficulty      = d;
            G.diff_cfg        = get_difficulty_config(d);
            G.free_hints_left = G.diff_cfg.free_hints;
            G.continues_left  = (d == NEWBIE) ? 1 : 0;
            int t = dev_effective_timer(d);
            timer_init(&G.timer, t);
            return PHASE;
        }
    }
}

/* ── Phase Text ────────────────────────────────────────────────────────── */

static const char *PHASE_TEXTS[TOTAL_PHASES] = {
    /* Phase 0 — wake-up intro */
    "Chief! Finally, you're awake. We barely made the jump to this planet. "
    "You looking at me a bit weird... you okay? You remember us, right?\n\n"
    "I'm Edwin, your Co-Chief, and you're the one in charge! We're scouting "
    "for a new place to live because Malus trashed our last home. We didn't "
    "quite wipe him out, and now he's out for blood. Let's get you up to speed!\n\n"
    "Quick controls: [?] Request hint  |  [ESC] Dismiss popup\n"
    "On Story Mode tools open automatically. "
    "On Hardcore you're on your own and there is no dialogue.",

    /* Phase 1 — UPX */
    "First things first, Chief -- we need to figure out what Malus is "
    "actually made of. We managed to snag an instance of an older Malus "
    "version. It's incredibly similar to the real thing, making it the "
    "perfect test subject.\n\n"
    "Open up UPX. We need to check if Malus is using some sort of shield; "
    "if it's just a common packer, it'll be easy to break through. For "
    "practice, take a look at the dummy.exe file in the folder. Can you "
    "find out which packing algorithm was used?",

    /* Phase 2 — PEviewer */
    "Spot on, Chief! You haven't lost your touch. Now, let's move forward "
    "with PEviewer. Think of it as an X-ray tool -- it gives us the "
    "'headers' of a file, showing us imports, exports, compile timestamps, "
    "and the target platform.\n\n"
    "Open dummy2.exe with PEviewer and see what you can find! Remember, "
    "this is just a warm-up for the real fight. You'll need to master "
    "advanced tools like IDA Pro for what comes next.",

    /* Phase 3 — Strings (malus transmission shown instead) */
    NULL,

    /* Phase 4 — IDA Pro */
    "Now we dive deeper, Chief. IDA Pro lets us look at the actual "
    "instructions inside the binary -- even when strings are hidden or "
    "XOR-encoded.\n\n"
    "Load malus_sample.exe into IDA Pro and trace the decode loop. "
    "The DLL name is obfuscated, but the decode routine is visible in "
    "the disassembly. Can you find out which DLL Malus is trying to load?",

    /* Phase 5 — Procmon / dynamic analysis */
    "Next, we need to watch Malus while it runs. We'll use Procmon to "
    "observe exactly what it does to our system in real time.\n\n"
    "Be careful: the subject might try to hide inside other processes. "
    "Monitor the network calls -- the C2 IP address isn't stored as plain "
    "text, so you'll need to catch it live. Let's give it a shot!",

    /* Phase 6 — ApateDNS / kill switch */
    "Almost there, Chief! Malus might try to phone home. We'll use ApateDNS "
    "to hijack its DNS calls so we can see exactly where it's reaching out.\n\n"
    "Once we find the kill switch, we can shut Malus down for good. "
    "One last push and we save this planet!",
};

static void show_phase_text(int phase) {
    if (phase == 3) {
        audio_play_track(0);
        ui_clear();
        ui_set_color(COLOR_RED);
        ui_draw_box(9, 8, 62, 17, "!!! INCOMING TRANSMISSION !!!");
        ui_set_color(COLOR_YELLOW);
        ui_print_at(28, 10, "Chief!  Bad News!");
        ui_set_color(COLOR_WHITE);
        ui_print_wrapped(12, 13, 56, 5,
            "Malus is attacking our planet! "
            "We need to help this planet ASAP!");
        ui_set_color(COLOR_DEFAULT);
        ui_print_at(21, 21, "Press any key to engage...");
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
        FlushConsoleInputBuffer(hIn);
        INPUT_RECORD rec; DWORD nread;
        do {
            ReadConsoleInputA(hIn, &rec, 1, &nread);
        } while (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown);
        return;
    }

    const char *text = (phase >= 0 && phase < TOTAL_PHASES) ? PHASE_TEXTS[phase] : NULL;
    if (!text) return;

    ui_clear();
    ui_set_color(COLOR_CYAN);
    ui_draw_box(0, 0, 80, 35, "TEXT");
    ui_set_color(COLOR_YELLOW);
    ui_print_at(2, 1, "Edwin:");
    ui_set_color(COLOR_DEFAULT);
    ui_print_wrapped(2, 3, 76, 22, text);
    ui_set_color(COLOR_GREEN);
    ui_print_at(40, 28, "[->] / [ENTER]  Continue");
    ui_set_color(COLOR_DEFAULT);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec; DWORD nread;
    do {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_RETURN || vk == VK_RIGHT) break;
    } while (1);
}

/* ── Screen: Phase ─────────────────────────────────────────────────────── */

static void draw_phase_screen(void) {
    const Level *l = get_level(G.current_phase);
    if (!l) return;
    char time_buf[16];
    format_time(timer_get_remaining(&G.timer), time_buf, sizeof(time_buf));

    ui_clear();

    /* Lore panel (0,0) 80x5 */
    ui_set_color(COLOR_CYAN);
    ui_draw_box(0, 0, 80, 5, "LORE");
    ui_set_color(COLOR_DEFAULT);
    ui_print_wrapped(2, 1, 76, 3, l->lore_text);

    /* Mission panel (0,5) 52x20 */
    ui_set_color(COLOR_GREEN);
    ui_draw_box(0, 5, 52, 20, "MISSION");
    ui_set_color(COLOR_WHITE);
    ui_print_at(2, 7, l->phase_name);
    ui_set_color(COLOR_DEFAULT);

    if (l->theory_standard)
        ui_print_wrapped(2, 9, 48, 9, l->theory_standard);

    if (l->tool_name) {
        char tool_line[52];
        snprintf(tool_line, sizeof(tool_line), "Tool:  %s", l->tool_name);
        ui_print_at(2, 21, tool_line);
    }
    {
        const char *sp = (G.difficulty == HARDCORE && l->sample_path_hard)
                         ? l->sample_path_hard : l->sample_path;
        if (sp) {
            char file_line[52];
            snprintf(file_line, sizeof(file_line), "File:  %s", sp);
            ui_print_at(2, 22, file_line);
        }
    }

    /* Status panel (52,5) 28x10 */
    ui_set_color(COLOR_CYAN);
    ui_draw_box(52, 5, 28, 10, "STATUS");
    ui_set_color(COLOR_DEFAULT);
    char buf_time[32], buf_diff[32], buf_hints[32];
    snprintf(buf_time,  sizeof(buf_time),  "Time:  %s", time_buf);
    snprintf(buf_diff,  sizeof(buf_diff),  "Mode:  %s",
             G.difficulty == NEWBIE ? "Story" : "Hardcore");
    snprintf(buf_hints, sizeof(buf_hints), "Hints: %d free left",
             G.free_hints_left);
    ui_print_at(54, 7,  buf_time);
    ui_print_at(54, 8,  buf_diff);
    ui_print_at(54, 9,  buf_hints);
    if (G.difficulty == NEWBIE) {
        char buf_cont[32];
        if (G.continues_left > 0)
            snprintf(buf_cont, sizeof(buf_cont), "Cont:  %d left   ", G.continues_left);
        else
            snprintf(buf_cont, sizeof(buf_cont), "Cont:  none      ");
        ui_print_at(54, 10, buf_cont);
    }
    if (G.current_phase > 0) {
        if (G.current_hint_idx < l->hint_count) {
            if (G.free_hints_left > 0) {
                ui_set_color(COLOR_YELLOW);
                ui_print_at(54, 11, "[?] Request Hint  ");
            } else {
                ui_set_color(COLOR_MAGENTA);
                ui_print_at(54, 11, "[?] Hint  (-5 min)");
            }
        }
        ui_set_color(COLOR_CYAN);
        ui_print_at(54, 13, "[i] Tool Info");
        ui_set_color(COLOR_DEFAULT);
    }

    /* Key entry panel — hidden in presentation mode during active phases */
    if (G.dev_presentation && G.current_phase > 0) {
        ui_set_color(COLOR_YELLOW);
        ui_draw_box(52, 15, 28, 10, "PRESENTATION");
        ui_print_at(54, 19, "[S] Skip Phase");
        {
            const char *dk = (G.difficulty == HARDCORE && l->expected_key_hard)
                             ? l->expected_key_hard : l->expected_key;
            if (G.dev_show_keys && dk) {
                ui_set_color(COLOR_RED);
                char kline[28];
                snprintf(kline, sizeof(kline), "Key: %-20.20s", dk);
                ui_print_at(54, 21, kline);
            }
        }
        ui_set_color(COLOR_DEFAULT);
    } else {
        ui_set_color(COLOR_WHITE);
        ui_draw_box(52, 15, 28, 10, "KEY ENTRY");
        ui_set_color(COLOR_DEFAULT);

        if (G.current_phase == 0) {
            ui_set_color(COLOR_GREEN);
            ui_print_at(54, 17, "Press ENTER to");
            ui_print_at(54, 18, "accept & start.");
            ui_set_color(COLOR_DEFAULT);
        } else {
            ui_print_at(54, 17, "Enter the key:");
            int show_start = G.key_input_len > 22 ? G.key_input_len - 22 : 0;
            char shown[25];
            snprintf(shown, sizeof(shown), "%s_", G.key_input + show_start);
            char input_display[32];
            snprintf(input_display, sizeof(input_display), "> %-23s", shown);
            ui_print_at(54, 18, input_display);

            if (G.wrong_answer) {
                ui_set_color(COLOR_RED);
                ui_print_at(54, 20, G.difficulty == NEWBIE
                    ? "Wrong! Speed x1.25   "
                    : "Wrong! Speed x2.5     ");
            }
            {
                const char *dk = (G.difficulty == HARDCORE && l->expected_key_hard)
                                 ? l->expected_key_hard : l->expected_key;
                if (G.dev_show_keys && dk) {
                    ui_set_color(COLOR_RED);
                    char kline[28];
                    snprintf(kline, sizeof(kline), "Key: %-20s", dk);
                    ui_print_at(54, 21, kline);
                }
            }
            ui_set_color(COLOR_RED);
            ui_print_at(54, 23, "[Q] Surrender");
            ui_set_color(COLOR_DEFAULT);
        }
    }
}

static void redraw_input(void) {
    int show_start = G.key_input_len > 22 ? G.key_input_len - 22 : 0;
    char shown[25];
    snprintf(shown, sizeof(shown), "%s_", G.key_input + show_start);
    char line[32];
    snprintf(line, sizeof(line), "> %-23s", shown);
    ui_set_color(COLOR_DEFAULT);
    ui_print_at(54, 18, line);
    ui_print_at(54, 20, G.wrong_answer
        ? (G.difficulty == NEWBIE ? "Wrong! Speed x1.25   " : "Wrong! Speed x2.5     ")
        : "                      ");
}

static void phase_request_hint(void) {
    const Level *l = get_level(G.current_phase);
    if (!l || l->hint_count == 0) return;
    if (G.hint_popup.active) return;
    if (G.current_hint_idx >= l->hint_count) return;

    if (G.free_hints_left > 0) {
        G.free_hints_left--;
    } else {
        timer_penalize(&G.timer, 300);
    }
    draw_phase_screen();
    hints_show(&G.hint_popup, l->hints[G.current_hint_idx]);
    G.current_hint_idx++;
    G.hints_used++;
}

static void phase_show_info(void) {
    const Level *l = get_level(G.current_phase);
    if (!l || !l->theory_newbie) return;
    ui_set_color(COLOR_CYAN);
    ui_draw_box(2, 4, 76, 25, "TOOL INFO");
    ui_set_color(COLOR_DEFAULT);
    ui_print_wrapped(4, 6, 72, 20, l->theory_newbie);
    ui_set_color(COLOR_CYAN);
    ui_print_at(28, 30, "[ESC] / [ENTER]  Close");
    ui_set_color(COLOR_DEFAULT);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec; DWORD nread;
    do {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_ESCAPE || vk == VK_RETURN) break;
    } while (1);
    draw_phase_screen();
}

static int phase_confirm_surrender(void) {
    ui_set_color(COLOR_RED);
    ui_draw_box(20, 14, 40, 7, "SURRENDER?");
    ui_set_color(COLOR_WHITE);
    ui_print_at(23, 16, "Give up and return to menu?");
    ui_set_color(COLOR_YELLOW);
    ui_print_at(27, 18, "[Y] Yes     [N] No");
    ui_set_color(COLOR_DEFAULT);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec; DWORD nread;
    while (1) {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        if (ch == 'y' || ch == 'Y') {
            progress_save();
            prog_has_save = 1;
            return 1;
        }
        if (ch == 'n' || ch == 'N' ||
            rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) {
            draw_phase_screen();
            return 0;
        }
    }
}

static int phase_show_continue(void) {
    ui_set_color(COLOR_YELLOW);
    ui_draw_box(15, 10, 50, 11, "CONTINUE?");
    ui_set_color(COLOR_WHITE);
    ui_print_at(18, 12, "The timer ran out, Chief!");
    ui_print_at(18, 14, "Use your continue?  (+10 min, speed resets)");
    ui_set_color(COLOR_CYAN);
    ui_print_at(18, 15, "(1 continue remaining)");
    ui_set_color(COLOR_YELLOW);
    ui_print_at(23, 17, "[Y] Yes     [N] No");
    ui_set_color(COLOR_DEFAULT);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec; DWORD nread;
    while (1) {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        if (ch == 'y' || ch == 'Y') return 1;
        if (ch == 'n' || ch == 'N' ||
            rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) return 0;
    }
}

static GameScreen screen_phase(void) {
    const Level *l = get_level(G.current_phase);
    if (!l) return GAME_OVER;

    G.key_input[0]     = '\0';
    G.key_input_len    = 0;
    G.wrong_answer     = 0;
    G.current_hint_idx = 0;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    enable_mouse_input(hIn);

    if (!G.dev_skip_lore && G.difficulty != HARDCORE)
        show_phase_text(G.current_phase);
    enable_mouse_input(hIn);

    if (G.current_phase == 0) {
        timer_start(&G.timer);
        G.current_phase++;
        return PHASE;
    }

    draw_phase_screen();

    while (1) {
        if (G.timer.game_over) {
            if (G.difficulty == NEWBIE && G.continues_left > 0) {
                hints_dismiss(&G.hint_popup);
                if (phase_show_continue()) {
                    G.continues_left--;
                    timer_continue(&G.timer, 600);
                    enable_mouse_input(hIn);
                    draw_phase_screen();
                    continue;
                }
            }
            return GAME_OVER;
        }

        DWORD events;
        GetNumberOfConsoleInputEvents(hIn, &events);
        if (events == 0) {
            Sleep(100);
            char time_buf[16];
            format_time(timer_get_remaining(&G.timer), time_buf, sizeof(time_buf));
            char buf_time[32];
            snprintf(buf_time, sizeof(buf_time), "Time:  %s", time_buf);
            ui_set_color(COLOR_DEFAULT);
            ui_print_at(54, 7, buf_time);
            continue;
        }

        INPUT_RECORD rec;
        DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            draw_phase_screen();
            hints_redraw(&G.hint_popup);
            continue;
        }

        if (rec.EventType == MOUSE_EVENT) {
            hints_update_hover(&G.hint_popup,
                               rec.Event.MouseEvent.dwMousePosition.X,
                               rec.Event.MouseEvent.dwMousePosition.Y);
            continue;
        }

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;

        if (vk == VK_ESCAPE) {
            hints_dismiss(&G.hint_popup);
            draw_phase_screen();
            continue;
        }

        if (ch == '?' && G.current_phase > 0) {
            phase_request_hint();
            continue;
        }

        if ((ch == 'i' || ch == 'I') && G.current_phase > 0) {
            phase_show_info();
            enable_mouse_input(hIn);
            hints_redraw(&G.hint_popup);
            continue;
        }

        if ((ch == 'q' || ch == 'Q') && G.current_phase > 0) {
            if (phase_confirm_surrender()) return GAME_OVER;
            enable_mouse_input(hIn);
            hints_redraw(&G.hint_popup);
            continue;
        }


        if ((ch == 's' || ch == 'S') && G.dev_presentation && G.current_phase > 0) {
            hints_dismiss(&G.hint_popup);
            G.current_phase++;
            G.wrong_answer = 0;
            if (G.current_phase >= TOTAL_PHASES) return WIN;
            return PHASE;
        }

        if (G.dev_presentation && G.current_phase > 0) continue;

        if (vk == VK_BACK && G.key_input_len > 0) {
            G.key_input[--G.key_input_len] = '\0';
            G.wrong_answer = 0;
            redraw_input();
            continue;
        }

        if (vk == VK_RETURN) {
            hints_dismiss(&G.hint_popup);
            if (validation_check(G.current_phase, G.key_input, G.difficulty == HARDCORE)) {
                G.current_phase++;
                G.wrong_answer = 0;
                if (G.current_phase >= TOTAL_PHASES) return WIN;
                return PHASE;
            } else {
                G.wrong_answer = 1;
                timer_apply_speedup(&G.timer, G.difficulty == NEWBIE ? 1.25f : 2.5f);
                draw_phase_screen();
            }
            continue;
        }

        if (ch >= 0x20 && ch < 0x7F && G.key_input_len < 127) {
            G.key_input[G.key_input_len++] = ch;
            G.key_input[G.key_input_len]   = '\0';
            G.wrong_answer = 0;
            redraw_input();
        }
    }
}

/* ── Vault key (XOR-encoded — find it in IDA, like the game taught you) ── */

/*
 * Plaintext key: open mal4death.exe in IDA Pro and trace decode_vault_key().
 * XOR key and encoded bytes are in generated_keys.h, produced at build time.
 */
static const unsigned char vault_enc[8]  = GEN_VAULT_ENC;
static const uint8_t       vault_md5[16] = GEN_VAULT_MD5;

static void decode_vault_key(char *out) {
    for (int i = 0; i < 8; i++)
        out[i] = (char)(vault_enc[i] ^ GEN_VAULT_XOR);
    out[8] = '\0';
}

static int vault_key_correct(const char *input) {
    uint8_t hash[16];
    md5_compute(input, strlen(input), hash);
    return memcmp(hash, vault_md5, 16) == 0;
}

/* ── BSOD ──────────────────────────────────────────────────────────────── */

#define BSOD_COLOR (BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | \
                    FOREGROUND_BLUE | FOREGROUND_INTENSITY)

static void show_bsod(void) {
    audio_stop();
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HWND   hwnd = GetConsoleWindow();

    /* hide cursor, remove scrollbar, go fullscreen */
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(hOut, &ci);

    /* maximize the window — covers the whole screen */
    ShowWindow(hwnd, SW_MAXIMIZE);
    Sleep(200); /* let the OS resize the window */

    /* query real dimensions after maximise */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    SHORT w = (SHORT)(csbi.srWindow.Right  - csbi.srWindow.Left + 1);
    SHORT h = (SHORT)(csbi.srWindow.Bottom - csbi.srWindow.Top  + 1);

    /* shrink buffer to exactly match window — removes scrollbar */
    COORD bufSize = {w, h};
    SetConsoleScreenBufferSize(hOut, bufSize);

    /* flood-fill entire buffer with blue */
    COORD  origin = {0, 0};
    DWORD  total  = (DWORD)(w * h);
    DWORD  written;
    FillConsoleOutputAttribute(hOut,  BSOD_COLOR, total, origin, &written);
    FillConsoleOutputCharacterA(hOut, ' ',        total, origin, &written);
    SetConsoleTextAttribute(hOut, BSOD_COLOR);
    SetConsoleCursorPosition(hOut, origin);

    /* position content proportionally to actual window size */
    int lm = w / 16;   /* left margin  */
    int tm = h / 6;    /* top margin   */

    ui_print_at(lm, tm,     ":(");
    ui_print_at(lm, tm + 3, "Your PC ran into a problem and needs to restart.");
    ui_print_at(lm, tm + 4, "We're just collecting some error info, and then");
    ui_print_at(lm, tm + 5, "we'll restart for you.");
    ui_print_at(lm, tm + 9, "100% complete");
    ui_print_at(lm, tm +13, "For more information about this issue and possible fixes, visit");
    ui_print_at(lm, tm +14, "https://www.windows.com/stopcode");
    ui_print_at(lm, tm +18, "If you call a support person, give them this info:");
    ui_print_at(lm, tm +19, "Stop code:  UNAUTHORIZED_VAULT_ACCESS");
    Sleep(6000);
    ExitProcess(0);
}

/* ── Vault lore sequence ───────────────────────────────────────────────── */

static void show_vault_lore(void) {
    static const struct {
        const char *speaker; /* NULL = stage direction */
        const char *text;
        int open_gif;
    } lines[] = {
        { "Doctor", "HE'S AWAKE, HE'S AWAKE!",                          0 },
        { "Marcus", "Where am I?",                                        0 },
        { "Doctor", "You've been in a coma for 2 long years, Marcus.",   0 },
        { NULL,     "*You look outside*",                                 0 },
        { "Marcus", "What's happening outside?",                         0 },
        { "Doctor", "Marcus, I need to tell you something...",           0 },
        { "Doctor", "We are being attacked by unknown enemies.",         0 },
        { "Doctor", "We call them Alienware.",                           0 },
        { "Marcus", "Ah shit, here we go again.",                        1 },
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));

    audio_stop();

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);

    for (int i = 0; i < n; i++) {
        ui_clear();
        ui_set_color(COLOR_CYAN);
        ui_draw_box(5, 2, 70, 26, "CLASSIFIED TRANSMISSION");

        for (int j = 0; j <= i; j++) {
            int y = 4 + j * 2;
            if (lines[j].speaker == NULL) {
                ui_set_color(COLOR_WHITE);
                ui_print_at(8, y, lines[j].text);
            } else {
                ui_set_color(strcmp(lines[j].speaker, "Doctor") == 0
                             ? COLOR_GREEN : COLOR_YELLOW);
                char buf[128];
                snprintf(buf, sizeof(buf), "%s: %s",
                         lines[j].speaker, lines[j].text);
                ui_print_at(8, y, buf);
            }
        }

        if (lines[i].open_gif) {
            audio_play_sfx("assets\\ahshit.wav");
            /* Play ASCII animation — loops until any key is pressed */
            SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
            FlushConsoleInputBuffer(hIn);
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            /* hide cursor to prevent flicker */
            CONSOLE_CURSOR_INFO ci = {1, FALSE};
            SetConsoleCursorInfo(hOut, &ci);
            /* draw static elements once */
            ui_clear();
            ui_set_color(COLOR_YELLOW);
            ui_print_at(20, GIF_FRAME_H + 3, "Ah shit, here we go again.");
            ui_set_color(COLOR_DEFAULT);
            ui_print_at(22, GIF_FRAME_H + 5, "Press any key to continue.");
            int frame = 0;
            int playing = 1;
            while (playing) {
                /* overwrite frame in-place — no clear, no flicker */
                for (int r = 0; r < GIF_FRAME_H; r++) {
                    COORD pos = {2, (SHORT)(r + 1)};
                    SetConsoleCursorPosition(hOut, pos);
                    DWORD written;
                    WriteConsoleA(hOut, gif_frames[frame][r], GIF_FRAME_W, &written, NULL);
                }
                frame = (frame + 1) % GIF_FRAME_COUNT;
                Sleep(120);
                DWORD events;
                GetNumberOfConsoleInputEvents(hIn, &events);
                if (events > 0) {
                    INPUT_RECORD rec2; DWORD nr;
                    ReadConsoleInputA(hIn, &rec2, 1, &nr);
                    if (rec2.EventType == KEY_EVENT && rec2.Event.KeyEvent.bKeyDown)
                        playing = 0;
                }
            }
            ci.bVisible = TRUE;
            SetConsoleCursorInfo(hOut, &ci);
            return; /* skip the normal keypress wait below */
        }

        ui_set_color(COLOR_DEFAULT);
        ui_print_at(18, 29, i < n - 1 ? "Press any key..."
                                       : "Press any key to return to menu.");

        INPUT_RECORD rec; DWORD nread;
        do {
            ReadConsoleInputA(hIn, &rec, 1, &nread);
        } while (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown);
    }
}

/* ── Screen: Space Vault ───────────────────────────────────────────────── */

static void draw_space_vault_bg(void) {
    ui_clear();
    ui_set_color(COLOR_YELLOW);
    ui_draw_box(10, 2, 60, 30, "SPACE VAULT");

    ui_set_color(COLOR_CYAN);
    ui_print_at(26,  5, " _____");
    ui_print_at(26,  6, "|     |");
    ui_print_at(26,  7, "| [*] |   CLASSIFIED");
    ui_print_at(26,  8, "|     |");
    ui_print_at(26,  9, "|_____|");

    ui_set_color(COLOR_WHITE);
    ui_print_at(13, 12, "You have proven yourself, Chief.");
    ui_print_at(13, 13, "Behind this door lies what Malus was really after.");
    ui_print_at(13, 15, "Enter the vault access code to proceed.");
    ui_print_at(13, 16, "One wrong attempt will trigger a lockdown protocol.");

    ui_set_color(COLOR_YELLOW);
    ui_print_at(13, 19, "Access code: ");
    ui_set_color(COLOR_DEFAULT);
}

static GameScreen screen_space_vault(void) {
    draw_space_vault_bg();

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);
    FlushConsoleInputBuffer(hIn);

    char input[32] = {0};
    int  len = 0;

    while (1) {
        /* draw input field */
        char display[32];
        snprintf(display, sizeof(display), "%-20s", input);
        ui_set_color(COLOR_WHITE);
        ui_print_at(26, 19, display);
        ui_print_at(26 + len, 19, "_");

        INPUT_RECORD rec; DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            draw_space_vault_bg();
            continue;
        }

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;

        if (vk == VK_RETURN) {
            if (vault_key_correct(input)) {
                show_vault_lore();
                audio_play();
                return MENU;
            } else {
                show_bsod(); /* never returns */
            }
        }

        if (vk == VK_ESCAPE) return MENU;

        if (vk == VK_BACK && len > 0) {
            input[--len] = '\0';
            ui_print_at(26 + len, 19, "_ ");
        } else if (ch >= 0x20 && ch < 0x7F && len < 20) {
            input[len++] = (char)toupper((unsigned char)ch);
            input[len]   = '\0';
        }
    }
}

/* ── Screen: Audio Settings ────────────────────────────────────────────── */

static void draw_audio_settings(void) {
    ui_clear();
    ui_set_color(COLOR_CYAN);
    ui_draw_box(20, 6, 40, 16, "AUDIO SETTINGS");
    ui_set_color(COLOR_DEFAULT);

    int vol = audio_get_volume();
    int bars = vol / 10;
    char bar[11];
    for (int i = 0; i < 10; i++) bar[i] = (i < bars) ? '#' : '-';
    bar[10] = '\0';
    char vol_line[40];
    snprintf(vol_line, sizeof(vol_line), "Volume: [%s] %3d%%", bar, vol);
    ui_print_at(24, 10, vol_line);

    ui_set_color(COLOR_YELLOW);
    ui_print_at(24, 13, "[+]  Volume Up   (+10%)");
    ui_print_at(24, 14, "[-]  Volume Down (-10%)");
    ui_print_at(24, 16, audio_is_muted() ? "[M]  Unmute             "
                                         : "[M]  Mute               ");
    ui_set_color(COLOR_DEFAULT);
    ui_print_at(24, 19, "[ESC]  Back to Menu");
}

static GameScreen screen_audio_settings(void) {
    draw_audio_settings();
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);
    while (1) {
        INPUT_RECORD rec;
        DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) { draw_audio_settings(); continue; }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        if (vk == VK_ESCAPE) return MENU;
        if (ch == '+' || ch == '=') {
            audio_set_volume(audio_get_volume() + 10);
            draw_audio_settings();
        } else if (ch == '-') {
            audio_set_volume(audio_get_volume() - 10);
            draw_audio_settings();
        } else if (ch == 'm' || ch == 'M') {
            audio_set_muted(!audio_is_muted());
            draw_audio_settings();
        }
    }
}

/* ── Screen: Developer Options ─────────────────────────────────────────── */

/* Maps dev-options display slot (0=Story, 1=Hardcore) to dev_timers/config index */
static const int DEV_DIFF_IDX[2]       = {0, 2};
static const int DEV_DEFAULT_TIMERS[3] = {2400, 1800, 600};
static const char *DEV_DIFF_NAMES[2]   = {"STORY   ", "HARDCORE"};

static void draw_dev_options(int sel) {
    ui_clear();
    ui_set_color(COLOR_YELLOW);
    ui_draw_box(15, 3, 50, 23, "DEVELOPER OPTIONS");
    ui_set_color(COLOR_DEFAULT);

    /* Toggles */
    char pmode[50];
    snprintf(pmode, sizeof(pmode), "[P]  Presentation Mode:  [ %-8s]",
             G.dev_presentation ? "ENABLED" : "OFF");
    ui_set_color(G.dev_presentation ? COLOR_GREEN : COLOR_DEFAULT);
    ui_print_at(18, 6, pmode);

    char kmode[50];
    snprintf(kmode, sizeof(kmode), "[K]  Show Keys:          [ %-8s]",
             G.dev_show_keys ? "ENABLED" : "OFF");
    ui_set_color(G.dev_show_keys ? COLOR_GREEN : COLOR_DEFAULT);
    ui_print_at(18, 7, kmode);

    char cmode[50];
    snprintf(cmode, sizeof(cmode), "[C]  Completed Game:     [ %-8s]",
             G.dev_all_completed ? "ENABLED" : "OFF");
    ui_set_color(G.dev_all_completed ? COLOR_GREEN : COLOR_DEFAULT);
    ui_print_at(18, 8, cmode);

    char vmode[64];
    char vkey[16]; decode_vault_key(vkey);
    snprintf(vmode, sizeof(vmode), "[V]  Vault Key:          [ %-8s]",
             G.dev_show_vault_key ? vkey : "HIDDEN");
    ui_set_color(G.dev_show_vault_key ? COLOR_RED : COLOR_DEFAULT);
    ui_print_at(18, 9, vmode);

    char lmode[50];
    snprintf(lmode, sizeof(lmode), "[L]  Skip Lore:          [ %-8s]",
             G.dev_skip_lore ? "ENABLED" : "OFF");
    ui_set_color(G.dev_skip_lore ? COLOR_GREEN : COLOR_DEFAULT);
    ui_print_at(18, 10, lmode);
    ui_set_color(COLOR_DEFAULT);

    /* Timer overrides (Story + Hardcore only) */
    ui_print_at(18, 12, "Timer Overrides:");
    ui_print_at(18, 13, "Select [1/2], adjust with [+/-] (5 min steps)");

    for (int i = 0; i < 2; i++) {
        int idx  = DEV_DIFF_IDX[i];
        int secs = G.dev_timers[idx] > 0 ? G.dev_timers[idx] : DEV_DEFAULT_TIMERS[idx];
        char tbuf[8];
        format_time(secs, tbuf, sizeof(tbuf));
        char row[52];
        snprintf(row, sizeof(row), "[%d]  %s   %s  %s",
                 i + 1, DEV_DIFF_NAMES[i], tbuf,
                 G.dev_timers[idx] == 0 ? "(default)" : "         ");
        if (i == sel) {
            ui_set_color(COLOR_YELLOW);
            ui_print_at(17, 15 + i * 2, ">");
        } else {
            ui_print_at(17, 15 + i * 2, " ");
        }
        ui_set_color(i == sel ? COLOR_YELLOW : COLOR_DEFAULT);
        ui_print_at(18, 15 + i * 2, row);
        ui_set_color(COLOR_DEFAULT);
    }

    ui_print_at(18, 20, "[R]  Reset timers to defaults");
    ui_print_at(18, 22, "[ESC]  Back to Menu");
}

static GameScreen screen_dev_options(void) {
    int sel = 0;
    draw_dev_options(sel);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT);
    while (1) {
        INPUT_RECORD rec;
        DWORD nread;
        ReadConsoleInputA(hIn, &rec, 1, &nread);
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) { draw_dev_options(sel); continue; }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        char ch = rec.Event.KeyEvent.uChar.AsciiChar;

        if (vk == VK_ESCAPE) return MENU;

        if (ch == 'p' || ch == 'P') {
            G.dev_presentation = !G.dev_presentation;
            draw_dev_options(sel);
            continue;
        }

        if (ch == 'k' || ch == 'K') {
            G.dev_show_keys = !G.dev_show_keys;
            draw_dev_options(sel);
            continue;
        }

        if (ch == 'c' || ch == 'C') {
            G.dev_all_completed = !G.dev_all_completed;
            progress_set_all(G.dev_all_completed);
            draw_dev_options(sel);
            continue;
        }

        if (ch == 'v' || ch == 'V') {
            G.dev_show_vault_key = !G.dev_show_vault_key;
            draw_dev_options(sel);
            continue;
        }

        if (ch == 'l' || ch == 'L') {
            G.dev_skip_lore = !G.dev_skip_lore;
            draw_dev_options(sel);
            continue;
        }

        if (ch == 'r' || ch == 'R') {
            for (int i = 0; i < 3; i++) G.dev_timers[i] = 0;
            draw_dev_options(sel);
            continue;
        }

        if (ch >= '1' && ch <= '2') {
            sel = ch - '1';
            draw_dev_options(sel);
            continue;
        }

        if (ch == '+' || ch == '=') {
            int idx = DEV_DIFF_IDX[sel];
            int cur = G.dev_timers[idx] > 0 ? G.dev_timers[idx] : DEV_DEFAULT_TIMERS[idx];
            G.dev_timers[idx] = cur + 300;
            draw_dev_options(sel);
            continue;
        }

        if (ch == '-') {
            int idx  = DEV_DIFF_IDX[sel];
            int cur  = G.dev_timers[idx] > 0 ? G.dev_timers[idx] : DEV_DEFAULT_TIMERS[idx];
            int next = cur - 300;
            G.dev_timers[idx] = next < 60 ? 60 : next;
            draw_dev_options(sel);
            continue;
        }
    }
}

/* ── Screen: Win ───────────────────────────────────────────────────────── */

static GameScreen screen_win(void) {
    timer_stop(&G.timer);
    int remaining = timer_get_remaining(&G.timer);
    int elapsed   = dev_effective_timer(G.difficulty) - remaining;
    char time_rem[16], time_ela[16];
    format_time(remaining, time_rem, sizeof(time_rem));
    format_time(elapsed,   time_ela, sizeof(time_ela));

    audio_stop();
    audio_play_sfx("assets\\win.wav");

    ui_clear();

    /* Firecrackers */
    ui_set_color(COLOR_YELLOW);
    ui_print_at( 2, 0, "  *    *        *   *");
    ui_print_at(20, 0, "        *    *       *    *");
    ui_print_at(55, 0, "  *        *    *       *  ");
    ui_set_color(COLOR_RED);
    ui_print_at( 2, 1, " *|* . *|* .  *|*  . *|*");
    ui_print_at(22, 1, " *|*  . *|*  . *|*");
    ui_print_at(55, 1, " *|* . *|*  . *|*  . *|*");
    ui_set_color(COLOR_YELLOW);
    ui_print_at( 3, 2, "  |       |      |      |");
    ui_print_at(23, 2, "  |      |       |");
    ui_print_at(56, 2, "  |      |       |       |");
    ui_set_color(COLOR_WHITE);
    ui_print_at( 3, 3, " \\|/     \\|/    \\|/    \\|/");
    ui_print_at(23, 3, " \\|/    \\|/     \\|/");
    ui_print_at(56, 3, " \\|/    \\|/     \\|/    \\|/");

    /* Title */
    ui_set_color(COLOR_GREEN);
    ui_print_at(22, 5, "__   _____ ___ _____ ___  _____   __");
    ui_print_at(22, 6, "\\ \\ / /_ _/ __|_   _/ _ \\| _ \\ \\ / /");
    ui_print_at(22, 7, " \\ V / | | (__  | || (_) |   /\\ V / ");
    ui_print_at(22, 8, "  \\_/ |___\\___| |_| \\___/|_|_\\ |_|  ");

    /* Quote */
    ui_set_color(COLOR_CYAN);
    ui_print_at(10, 10, "Edwin: \"Chief! You did it! The kill switch worked.");
    ui_print_at(10, 11, "       Malus is neutralised. The planet is safe.\"");

    /* Stats box */
    ui_set_color(COLOR_GREEN);
    ui_draw_box(20, 13, 40, 12, "MISSION DEBRIEF");
    ui_set_color(COLOR_DEFAULT);

    const char *diff_name = G.difficulty == NEWBIE ? "Story Mode" : "Hardcore";
    char line[48];
    snprintf(line, sizeof(line), "Difficulty  :  %s", diff_name);
    ui_print_at(23, 15, line);
    snprintf(line, sizeof(line), "Time used   :  %s", time_ela);
    ui_print_at(23, 16, line);
    snprintf(line, sizeof(line), "Time left   :  %s", time_rem);
    ui_print_at(23, 17, line);
    snprintf(line, sizeof(line), "Hints used  :  %d", G.hints_used);
    ui_print_at(23, 18, line);
    snprintf(line, sizeof(line), "Phases done :  %d", TOTAL_PHASES);
    ui_print_at(23, 19, line);

    ui_set_color(COLOR_DEFAULT);
    ui_print_at(22, 26, "Press any key to return to menu.");

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec;
    DWORD nread;
    do {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
    } while (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown);
    progress_mark_complete(G.difficulty);
    audio_stop();
    audio_play();
    return MENU;
}

/* ── Screen: Game Over ─────────────────────────────────────────────────── */

static GameScreen screen_game_over(void) {
    timer_stop(&G.timer);

    int elapsed = dev_effective_timer(G.difficulty) - timer_get_remaining(&G.timer);
    char time_ela[16];
    format_time(elapsed, time_ela, sizeof(time_ela));

    ui_clear();

    /* Skull art */
    ui_set_color(COLOR_RED);
    ui_print_at(30,  2, "  _____ ");
    ui_print_at(30,  3, " /     \\");
    ui_print_at(30,  4, "| () () |");
    ui_print_at(30,  5, " \\  ^  /");
    ui_print_at(30,  6, "  |||||");
    ui_print_at(30,  7, "  |||||");

    /* Title */
    ui_set_color(COLOR_RED);
    ui_print_at(12,  9, " ___ _  _ ___ ___ ___ _____  ___ ___  _  _");
    ui_print_at(12, 10, "|_ _| \\| | __| __/ __|_   _||_ _/ _ \\| \\| |");
    ui_print_at(12, 11, " | || .` | _|| _| (__  | |  | | (_) | .` |");
    ui_print_at(12, 12, "|___|_|\\_|_| |___\\___| |_| |___\\___/|_|\\_|");
    ui_set_color(COLOR_WHITE);
    ui_print_at(22, 13, "C O M P L E T E");

    /* Quote */
    ui_set_color(COLOR_DEFAULT);
    ui_print_at(10, 15, "Edwin: \"Chief... the infection spread too fast.");
    ui_print_at(10, 16, "       We'll need to find another planet.\"");

    /* Stats box */
    ui_set_color(COLOR_RED);
    ui_draw_box(20, 18, 40, 10, "POST-MORTEM");
    ui_set_color(COLOR_DEFAULT);

    const char *diff_name = G.difficulty == NEWBIE ? "Story Mode" : "Hardcore";
    char line[48];
    snprintf(line, sizeof(line), "Difficulty  :  %s", diff_name);
    ui_print_at(23, 20, line);
    snprintf(line, sizeof(line), "Time used   :  %s", time_ela);
    ui_print_at(23, 21, line);
    snprintf(line, sizeof(line), "Hints used  :  %d", G.hints_used);
    ui_print_at(23, 22, line);
    snprintf(line, sizeof(line), "Phases done :  %d / %d", G.current_phase - 1, TOTAL_PHASES);
    ui_print_at(23, 23, line);

    ui_set_color(COLOR_DEFAULT);
    ui_print_at(22, 29, "Press any key to return to menu.");

    audio_stop();
    audio_play_sfx("assets\\gameover.wav");
    Sleep(2500); /* let the death sound play; also flushes any buffered keys */

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec;
    DWORD nread;
    do {
        ReadConsoleInputA(hIn, &rec, 1, &nread);
    } while (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown);
    progress_save();
    prog_has_save = 1;
    audio_play();
    return MENU;
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(void) {
    ui_init();
    audio_init();
    audio_play();
    progress_load();
    hints_init(&G.hint_popup);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);

    G.screen           = MENU;
    G.current_phase    = 0;
    G.dev_presentation = 0;
    G.dev_show_keys    = 0;
    G.dev_timers[0]      = 0;
    G.dev_timers[1]      = 0;
    G.dev_timers[2]      = 0;
    G.dev_all_completed  = 0;
    G.dev_show_vault_key = 0;
    G.dev_skip_lore      = 0;

    while (1) {
        switch (G.screen) {
        case MENU:              G.screen = screen_menu();             break;
        case DIFFICULTY_SELECT: G.screen = screen_difficulty();       break;
        case PHASE:             G.screen = screen_phase();            break;
        case WIN:               G.screen = screen_win();              break;
        case GAME_OVER:         G.screen = screen_game_over();        break;
        case AUDIO_SETTINGS:    G.screen = screen_audio_settings();   break;
        case DEV_OPTIONS:       G.screen = screen_dev_options();      break;
        case SPACE_VAULT:       G.screen = screen_space_vault();      break;
        }

        if (G.screen == MENU) {
            timer_destroy(&G.timer);
            hints_destroy(&G.hint_popup);
            hints_init(&G.hint_popup);
            G.current_phase      = 0;
            G.hints_used         = 0;
            G.free_hints_left    = 0;
            G.continues_left     = 0;
            G.key_input[0]       = '\0';
            G.key_input_len      = 0;
            G.wrong_answer       = 0;
        }
    }
    return 0;
}
