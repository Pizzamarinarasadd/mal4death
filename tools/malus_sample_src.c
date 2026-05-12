/*
 * Mal4Death - fake malware sample for educational reverse engineering.
 * Compiled into assets/malus_sample.exe and patched by keygen at build time.
 * DO NOT USE FOR ANY MALICIOUS PURPOSE.
 *
 * Storage strategy per phase:
 *   Phase 1a (Strings)  : URL stored as plain ASCII -> Strings tool finds it
 *   Phase 1b (IDA Pro)  : DLL name XOR-encoded     -> only decode loop visible in IDA
 *   Phase 2a (Procmon)  : IP as 4 raw binary bytes -> not findable as ASCII string
 *   Phase 2b (Debugger) : kill switch XOR-encoded  -> only visible in debugger at decode
 */
#include <windows.h>

#define XOR_KEY_DLL  0x55
#define XOR_KEY_KILL 0x33

/* Phase 1a: plain ASCII - Strings tool will find this */
char m4d_url[81] = "M4D_URL_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

/*
 * Phase 1b: DLL name XOR-encoded.
 * Marker "M4D_DLL_ENC:" (12 bytes) + 44 bytes of XOR placeholder.
 * keygen patches the 44 bytes after the marker with dll_name[i] ^ XOR_KEY_DLL.
 * Strings tool cannot read this; IDA shows the decode_xor() loop.
 */
static unsigned char m4d_dll_enc[56] = {
    'M','4','D','_','D','L','L','_','E','N','C',':',
    0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,
    0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,
    0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,
    0xAA,0xAA,0xAA,0xAA
};

/*
 * Phase 2a: C2 IP as 4 raw binary bytes (not ASCII).
 * Marker "M4D_IP4_BIN:" (12 bytes) + 4 bytes patched by keygen.
 * Not visible as an IP string; requires network-level observation.
 */
static unsigned char m4d_ip_bin[16] = {
    'M','4','D','_','I','P','4','_','B','I','N',':',
    0xFF,0xFF,0xFF,0xFF
};

/*
 * Phase 2b: kill switch XOR-encoded.
 * Marker "M4D_KLL_ENC:" (12 bytes) + 28 bytes of XOR placeholder.
 * keygen patches with kill_switch[i] ^ XOR_KEY_KILL.
 * Only found via debugger watching the decode call.
 */
static unsigned char m4d_kill_enc[40] = {
    'M','4','D','_','K','L','L','_','E','N','C',':',
    0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
    0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
    0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC
};

/* Decode routine visible in IDA/debugger */
static void decode_xor(const unsigned char *enc, int off, int maxlen,
                        unsigned char key, char *out) {
    int i;
    for (i = 0; i < maxlen; i++) {
        char c = (char)(enc[off + i] ^ key);
        if (c == 0) break;
        out[i] = c;
    }
    out[i] = 0;
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int s) {
    (void)h; (void)p; (void)c; (void)s;
    /* Branch never taken - forces all data into the binary */
    if (GetTickCount() == 0) {
        char buf[64];
        OutputDebugStringA(m4d_url);
        decode_xor(m4d_dll_enc,  12, 44, XOR_KEY_DLL,  buf);
        LoadLibraryA(buf);
        decode_xor(m4d_kill_enc, 12, 28, XOR_KEY_KILL, buf);
        OutputDebugStringA(buf);
        (void)m4d_ip_bin;
    }
    return 0;
}
