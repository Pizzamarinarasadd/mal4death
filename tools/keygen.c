#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "../src/md5.h"

/* XOR keys - must match malus_sample_src.c */
#define XOR_KEY_DLL  0x55
#define XOR_KEY_KILL 0x33

/* Binary markers in malus_sample.exe - must match malus_sample_src.c */
#define MARKER_URL  "M4D_URL_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
#define MARKER_DLL  "M4D_DLL_ENC:"
#define MARKER_IP   "M4D_IP4_BIN:"
#define MARKER_KILL "M4D_KLL_ENC:"
#define LEN_DLL_DATA  44
#define LEN_IP_DATA    4
#define LEN_KILL_DATA 28

/* ── key generators ───────────────────────────────────────────────────── */

static int rnd(int n) { return rand() % n; }

static void gen_t1(char *out, size_t sz) {
    (void)sz;
    strcpy(out, "UPX"); /* dummy.exe is always UPX-packed */
}

static unsigned long gen_t2_val(void) {
    unsigned long base  = 0x4B3D2980UL;
    unsigned long range = 0x1DE7F700UL;
    return base + (((unsigned long)rand() << 15 | (unsigned long)rand()) % range);
}

static void gen_1a_url(char *out, size_t sz) {
    static const char *proto[] = { "http","http","http","https","https" };
    static const char *sub[]   = { "","","cdn.","update.","sync.",
                                   "api.","cmd.","dl.","gate." };
    static const char *dom[]   = { "securepatch","updateserv","syncdata",
                                   "cdnhost","msupdate","winupdate",
                                   "netservice","datacloud","sysupdate",
                                   "patchgate","botrelay","c2panel" };
    static const char *tld[]   = { ".com",".ru",".de",".it",
                                   ".cn",".io",".cc",".net" };
    static const char *path[]  = { "/payload","/gate.php","/upload","/cmd",
                                   "/sync","/update","/bot/gate",
                                   "/panel/gate.php","/drop","/callback" };
    snprintf(out, sz, "%s://%s%s%s%s",
             proto[rnd(5)], sub[rnd(9)], dom[rnd(12)], tld[rnd(8)], path[rnd(10)]);
}

static void gen_1b_dll(char *out, size_t sz) {
    static const char *dlls[] = {
        "inject32.dll","payload.dll","loader_x86.dll","malcore.dll",
        "dropper32.dll","rat_hook.dll","keylog32.dll","botmodule.dll",
        "wininject.dll","svcpatch.dll","nthook.dll","cryptload.dll",
        "shellext32.dll","patchsvc.dll","winsync.dll","netspy32.dll"
    };
    snprintf(out, sz, "%s", dlls[rnd(16)]);
}

static void gen_2a_ip(char *out, size_t sz) {
    static const int first[] = {
        5,23,31,37,45,46,62,77,80,85,91,92,94,95,103,104,
        109,141,159,162,176,178,185,188,193,194,195,212,213,217,220
    };
    snprintf(out, sz, "%d.%d.%d.%d",
             first[rnd(31)], rnd(256), rnd(256), 1 + rnd(254));
}

static void gen_vault_key(char *out) {
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 8; i++)
        out[i] = alpha[rand() % 36];
    out[8] = '\0';
}

static void gen_2b_kill(char *out, size_t sz) {
    static const char *prefix[] = {
        "KILL","STOP","HALT","ABORT","DISABLE","MALUS"
    };
    snprintf(out, sz, "%s_%04X", prefix[rnd(6)], rand() & 0xFFFF);
}

/* ── binary patching helpers ──────────────────────────────────────────── */

/* Load entire file into a malloc'd buffer; caller frees. Returns size or -1. */
static long load_file(const char *path, unsigned char **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    *out = malloc((size_t)sz);
    if (!*out) { fclose(f); return -1; }
    if (fread(*out, 1, (size_t)sz, f) != (size_t)sz) {
        free(*out); fclose(f); return -1;
    }
    fclose(f);
    return sz;
}

static int save_file(const char *path, unsigned char *buf, long sz) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(buf, 1, (size_t)sz, f);
    fclose(f);
    return 1;
}

/* Find marker bytes and return offset of first byte, or -1 */
static long find_bytes(unsigned char *buf, long sz,
                        const unsigned char *marker, int mlen) {
    for (long i = 0; i <= sz - mlen; i++)
        if (memcmp(buf + i, marker, (size_t)mlen) == 0) return i;
    return -1;
}

/* Patch PE TimeDateStamp */
static void patch_pe_timestamp(const char *path, unsigned long ts) {
    unsigned char *buf; long sz;
    if ((sz = load_file(path, &buf)) < 0) {
        fprintf(stderr, "keygen: cannot read %s\n", path); return;
    }
    unsigned long e_lfanew = 0;
    if (sz < 0x40) { free(buf); return; }
    memcpy(&e_lfanew, buf + 0x3C, 4);
    if (e_lfanew + 12 > (unsigned long)sz) { free(buf); return; }
    buf[e_lfanew + 8]  = (unsigned char)( ts        & 0xFF);
    buf[e_lfanew + 9]  = (unsigned char)((ts >>  8) & 0xFF);
    buf[e_lfanew + 10] = (unsigned char)((ts >> 16) & 0xFF);
    buf[e_lfanew + 11] = (unsigned char)((ts >> 24) & 0xFF);
    save_file(path, buf, sz);
    free(buf);
}

/* Patch plain-string placeholder (URL) */
static void patch_string(const char *path, const char *placeholder,
                          const char *value) {
    unsigned char *buf; long sz;
    if ((sz = load_file(path, &buf)) < 0) {
        fprintf(stderr, "keygen: cannot read %s\n", path); return;
    }
    int plen = (int)strlen(placeholder);
    long off = find_bytes(buf, sz, (const unsigned char *)placeholder, plen);
    if (off < 0) {
        fprintf(stderr, "keygen: placeholder not found in %s\n", path);
    } else {
        int vlen = (int)strlen(value);
        memcpy(buf + off, value, (size_t)vlen);
        memset(buf + off + vlen, 0, (size_t)(plen - vlen));
        save_file(path, buf, sz);
    }
    free(buf);
}

/* Patch XOR-encoded field: find marker, write xor(value) into data region */
static void patch_xor(const char *path, const char *marker,
                       int data_len, const char *value,
                       unsigned char xor_key) {
    unsigned char *buf; long sz;
    if ((sz = load_file(path, &buf)) < 0) {
        fprintf(stderr, "keygen: cannot read %s\n", path); return;
    }
    int mlen = (int)strlen(marker);
    long off = find_bytes(buf, sz, (const unsigned char *)marker, mlen);
    if (off < 0) {
        fprintf(stderr, "keygen: XOR marker '%s' not found in %s\n", marker, path);
    } else {
        long data_off = off + mlen;
        int vlen = (int)strlen(value);
        for (int i = 0; i < vlen && i < data_len; i++)
            buf[data_off + i] = (unsigned char)(value[i] ^ xor_key);
        /* null terminator encoded as xor_key (0x00 ^ key = key) */
        if (vlen < data_len)
            buf[data_off + vlen] = xor_key;
        save_file(path, buf, sz);
    }
    free(buf);
}

/* Patch IP as 4 raw bytes */
static void patch_ip_bytes(const char *path, const char *marker,
                            const char *ip_str) {
    unsigned char *buf; long sz;
    if ((sz = load_file(path, &buf)) < 0) {
        fprintf(stderr, "keygen: cannot read %s\n", path); return;
    }
    int mlen = (int)strlen(marker);
    long off = find_bytes(buf, sz, (const unsigned char *)marker, mlen);
    if (off < 0) {
        fprintf(stderr, "keygen: IP marker not found in %s\n", path);
    } else {
        int a, b, c, d;
        if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
            buf[off + mlen + 0] = (unsigned char)a;
            buf[off + mlen + 1] = (unsigned char)b;
            buf[off + mlen + 2] = (unsigned char)c;
            buf[off + mlen + 3] = (unsigned char)d;
            save_file(path, buf, sz);
        }
    }
    free(buf);
}

/* ── main ─────────────────────────────────────────────────────────────── */

#define NUM_KEYS 6
static const char *PHASE_LABELS[NUM_KEYS] = {
    "Tutorial 1 (UPX packing)",
    "Tutorial 2 (PE header)",
    "Phase 1a  (static URL)",
    "Phase 1b  (DLL import)",
    "Phase 2a  (C2 IP addr)",
    "Phase 2b  (kill switch)",
};
static const char *MACRO_NAMES[NUM_KEYS] = {
    "GEN_KEY_T1","GEN_KEY_T2",
    "GEN_KEY_1A","GEN_KEY_1B",
    "GEN_KEY_2A","GEN_KEY_2B",
};
/* Hardcore variants — indices 2-5 only (T1/T2 share the same dummy files) */
static const char *MACRO_NAMES_HARD[4] = {
    "GEN_KEY_H_1A","GEN_KEY_H_1B",
    "GEN_KEY_H_2A","GEN_KEY_H_2B",
};

int main(void) {
    srand((unsigned)time(NULL));

    char keys[NUM_KEYS][256];   /* story mode */
    char hkeys[4][256];         /* hardcore: indices 0-3 = phases 1a/1b/2a/2b */
    unsigned long ts_val = gen_t2_val();

    /* Story keys */
    gen_t1      (keys[0], sizeof(keys[0]));
    snprintf     (keys[1], sizeof(keys[1]), "%08lX", ts_val);
    gen_1a_url  (keys[2], sizeof(keys[2]));
    gen_1b_dll  (keys[3], sizeof(keys[3]));
    gen_2a_ip   (keys[4], sizeof(keys[4]));
    gen_2b_kill (keys[5], sizeof(keys[5]));

    /* Hardcore keys — regenerate until each differs from story */
    do { gen_1a_url (hkeys[0], sizeof(hkeys[0])); } while (strcmp(hkeys[0], keys[2]) == 0);
    do { gen_1b_dll (hkeys[1], sizeof(hkeys[1])); } while (strcmp(hkeys[1], keys[3]) == 0);
    do { gen_2a_ip  (hkeys[2], sizeof(hkeys[2])); } while (strcmp(hkeys[2], keys[4]) == 0);
    do { gen_2b_kill(hkeys[3], sizeof(hkeys[3])); } while (strcmp(hkeys[3], keys[5]) == 0);

    /* Vault key: 8-char random uppercase alphanumeric */
    char vault_plain[16];
    gen_vault_key(vault_plain);

    #define VAULT_XOR_KEY 0x5A
    unsigned char vault_enc_bytes[8];
    for (int i = 0; i < 8; i++)
        vault_enc_bytes[i] = (unsigned char)(vault_plain[i] ^ VAULT_XOR_KEY);

    uint8_t vault_md5_bytes[16];
    md5_compute(vault_plain, 8, vault_md5_bytes);

    /* Patch sample binaries.
     * Copy the freshly-built template before patching so both copies
     * start from the same clean placeholder binary. */
    patch_pe_timestamp("assets/dummy2.exe", ts_val);

    {
        unsigned char *tmpl; long sz = load_file("assets/malus_sample.exe", &tmpl);
        if (sz > 0) { save_file("assets/malus_sample_hard.exe", tmpl, sz); free(tmpl); }
        else fprintf(stderr, "keygen: cannot copy malus_sample.exe for hardcore\n");
    }

    patch_string   ("assets/malus_sample.exe", MARKER_URL,  keys[2]);
    patch_xor      ("assets/malus_sample.exe", MARKER_DLL,  LEN_DLL_DATA,  keys[3], XOR_KEY_DLL);
    patch_ip_bytes ("assets/malus_sample.exe", MARKER_IP,   keys[4]);
    patch_xor      ("assets/malus_sample.exe", MARKER_KILL, LEN_KILL_DATA, keys[5], XOR_KEY_KILL);

    patch_string   ("assets/malus_sample_hard.exe", MARKER_URL,  hkeys[0]);
    patch_xor      ("assets/malus_sample_hard.exe", MARKER_DLL,  LEN_DLL_DATA,  hkeys[1], XOR_KEY_DLL);
    patch_ip_bytes ("assets/malus_sample_hard.exe", MARKER_IP,   hkeys[2]);
    patch_xor      ("assets/malus_sample_hard.exe", MARKER_KILL, LEN_KILL_DATA, hkeys[3], XOR_KEY_KILL);

    /* Write src/generated_keys.h */
    FILE *h = fopen("src/generated_keys.h", "w");
    if (!h) { fprintf(stderr, "keygen: cannot write generated_keys.h\n"); return 1; }
    fprintf(h, "/* Auto-generated at build time -- do not edit */\n");
    fprintf(h, "#ifndef GENERATED_KEYS_H\n#define GENERATED_KEYS_H\n\n");
    fprintf(h, "/* Story Mode keys */\n");
    for (int i = 0; i < NUM_KEYS; i++)
        fprintf(h, "#define %-14s \"%s\"\n", MACRO_NAMES[i], keys[i]);

    fprintf(h, "\n/* Hardcore keys (malus_sample_hard.exe, phases 1a-2b) */\n");
    for (int i = 0; i < 4; i++)
        fprintf(h, "#define %-14s \"%s\"\n", MACRO_NAMES_HARD[i], hkeys[i]);

    /* Vault key macros */
    fprintf(h, "\n/* Vault key -- XOR-encoded; find decode_vault_key() in IDA */\n");
    fprintf(h, "#define GEN_VAULT_XOR 0x%02X\n", VAULT_XOR_KEY);
    fprintf(h, "#define GEN_VAULT_ENC {");
    for (int i = 0; i < 8; i++) {
        fprintf(h, "0x%02X", vault_enc_bytes[i]);
        if (i < 7) fprintf(h, ",");
    }
    fprintf(h, "}\n");
    fprintf(h, "#define GEN_VAULT_MD5 {");
    for (int i = 0; i < 16; i++) {
        fprintf(h, "0x%02X", vault_md5_bytes[i]);
        if (i < 15) fprintf(h, ",");
    }
    fprintf(h, "}\n");
    fprintf(h, "\n#endif\n");
    fclose(h);

    /* Write keys.txt (instructor reference) */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *f = fopen("keys.txt", "w");
    if (!f) { fprintf(stderr, "keygen: cannot write keys.txt\n"); return 1; }
    fprintf(f, "Mal4Death - Key Sheet\n");
    fprintf(f, "Compiled : %s\n\n", timestamp);

    fprintf(f, "--- STORY MODE ---\n");
    fprintf(f, "%-28s  Key\n", "Phase");
    fprintf(f, "%-28s  ----------------------------------------\n",
            "----------------------------");
    for (int i = 0; i < NUM_KEYS; i++)
        fprintf(f, "%-28s  %s\n", PHASE_LABELS[i], keys[i]);
    fprintf(f, "%-28s  %s\n", "Space Vault key", vault_plain);

    fprintf(f, "\n--- HARDCORE ---\n");
    fprintf(f, "%-28s  Key\n", "Phase");
    fprintf(f, "%-28s  ----------------------------------------\n",
            "----------------------------");
    fprintf(f, "%-28s  %s\n", PHASE_LABELS[0], keys[0]); /* T1 shared */
    fprintf(f, "%-28s  %s\n", PHASE_LABELS[1], keys[1]); /* T2 shared */
    for (int i = 0; i < 4; i++)
        fprintf(f, "%-28s  %s\n", PHASE_LABELS[2 + i], hkeys[i]);
    fprintf(f, "%-28s  %s\n", "Space Vault key", vault_plain);
    fclose(f);

    /* Single quiet summary line - no keys on stdout */
    printf("keygen: OK\n");
    return 0;
}
