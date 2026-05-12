/* ============================================================
   malus_sample.exe — Safe fake malware for educational use.
   This executable is BENIGN. It simulates malware artifacts
   that the player discovers with real analysis tools.

   Artifacts embedded:
     Phase 1a — URL visible in strings output
     Phase 1b — imports malus_inject.dll (visible in IDA Pro)
     Phase 2a — TCP connect attempt to C2 IP (visible in Procmon)
     Phase 2b — DNS query (visible in ApateDNS) + kill switch
                in strcmp (visible in debugger)
   ============================================================ */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* Phase 1a: this string is plaintext in the binary.
   strings.exe / Strings (Sysinternals) will show it. */
static const char C2_URL[]      = "http://malus-command.evil/payload";

/* Phase 2a: hardcoded C2 server.
   Procmon logs the TCP Connect attempt to this IP. */
static const char C2_IP[]       = "192.168.13.37";
static const int  C2_PORT       = 4444;

/* Phase 2b: kill switch password.
   Debugger breakpoint on strcmp reveals this value. */
static const char KILL_SWITCH[] = "MALUS_STOP_99";

/* Phase 1b: imported from malus_inject.dll.
   IDA Pro's Imports window shows this DLL. */
__declspec(dllimport) void inject_payload(void);

/* ── Phase 2b: DNS beacon (captured by ApateDNS) ─────────── */
static void dns_beacon(void) {
    struct addrinfo hints, *res = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    /* ApateDNS intercepts this DNS query */
    getaddrinfo("malus-command.evil", "80", &hints, &res);
    if (res) freeaddrinfo(res);
}

/* ── Phase 2a: TCP beacon (captured by Procmon) ─────────── */
static void tcp_beacon(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return; }

    struct sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((u_short)C2_PORT);
    addr.sin_addr.s_addr = inet_addr(C2_IP);

    /* Connection will fail (no real C2), but Procmon logs it */
    connect(s, (struct sockaddr *)&addr, sizeof(addr));

    closesocket(s);
    WSACleanup();
}

/* ── Phase 2b: kill switch check (set breakpoint here) ─── */
static int check_kill_switch(const char *input) {
    return strcmp(input, KILL_SWITCH) == 0;
}

/* ── Main malware routine ──────────────────────────────── */
int main(void) {
    /* Step 1: phone home via DNS (ApateDNS captures this) */
    dns_beacon();

    /* Step 2: connect to C2 IP (Procmon captures this) */
    tcp_beacon();

    /* Step 3: check environment for kill switch */
    char env_key[128];
    ZeroMemory(env_key, sizeof(env_key));
    DWORD len = GetEnvironmentVariableA("MALUS_KEY", env_key, sizeof(env_key));
    if (len > 0 && check_kill_switch(env_key)) {
        /* Kill switch activated — malware stands down */
        return 0;
    }

    /* Step 4: call injected payload from malus_inject.dll */
    inject_payload();

    /* Simulate persistence (writes nothing real to registry) */
    (void)C2_URL; /* ensure string is not optimised away */

    return 1;
}
