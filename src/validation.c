#include "validation.h"
#include "levels.h"
#include <string.h>
#include <ctype.h>

static int strcasecmp_portable(const char *a, const char *b) {
    if (!a || !b) return 1;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 1;
        a++; b++;
    }
    return (*a != '\0' || *b != '\0') ? 1 : 0;
}

int validation_check(int phase, const char *input, int is_hardcore) {
    const Level *l = get_level(phase);
    if (!l || !input || input[0] == '\0') return 0;
    const char *key = (is_hardcore && l->expected_key_hard)
                      ? l->expected_key_hard : l->expected_key;
    if (!key) return 0;
    return strcasecmp_portable(key, input) == 0 ? 1 : 0;
}
