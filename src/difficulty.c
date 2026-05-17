#include "difficulty.h"

DifficultyConfig get_difficulty_config(Difficulty mode) {
    DifficultyConfig configs[3] = {
        { NEWBIE,    2400, 3, 1.2f, 2 },
        { STANDARD,  1800, 1, 1.5f, 1 },
        { HARDCORE,   600, 0, 2.5f, 0 },
    };
    if ((int)mode < 0 || (int)mode > 2) return configs[0];
    return configs[mode];
}
