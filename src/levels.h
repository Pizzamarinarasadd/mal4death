#ifndef LEVELS_H
#define LEVELS_H

#define MAX_HINTS     3
#define TOTAL_PHASES  7

typedef struct {
    const char *phase_name;
    const char *lore_text;
    const char *theory_newbie;
    const char *theory_standard;
    const char *tool_name;
    const char *tool_path;
    const char *sample_path;
    const char *expected_key;
    const char *expected_key_hard; /* NULL = same as expected_key */
    const char *sample_path_hard;  /* NULL = same as sample_path  */
    const char *hints[MAX_HINTS];
    int         hint_count;
} Level;

const Level *get_level(int phase);

#endif
