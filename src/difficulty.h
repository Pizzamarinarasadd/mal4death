#ifndef DIFFICULTY_H
#define DIFFICULTY_H

typedef enum { NEWBIE = 0, STANDARD = 1, HARDCORE = 2 } Difficulty;

typedef struct {
    Difficulty  mode;
    int         initial_time;   /* seconds */
    int         free_hints;
    float       speedup_mult;
    int         show_theory;    /* 0=none, 1=summary, 2=full */
} DifficultyConfig;

DifficultyConfig get_difficulty_config(Difficulty mode);

#endif
