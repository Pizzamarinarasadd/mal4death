#include "levels.h"
#include "generated_keys.h"
#include <stddef.h>

#define TOOL_PATH_UPX       "tools\\upx.exe"
#define TOOL_PATH_PEVIEWER  "tools\\PEview.exe"
#define TOOL_PATH_STRINGS   "tools\\strings.exe"
#define TOOL_PATH_IDA       "C:\\Program Files\\IDA Pro\\idat.exe"
#define TOOL_PATH_REGSHOT   "tools\\Regshot.exe"
#define TOOL_PATH_PROCMON   "tools\\Procmon.exe"
#define TOOL_PATH_APATEDNS  "tools\\ApateDNS.exe"

#define SAMPLE_DUMMY        "assets\\dummy.exe"
#define SAMPLE_DUMMY2       "assets\\dummy2.exe"
#define SAMPLE_MALUS        "assets\\malus_sample.exe"
#define SAMPLE_MALUS_HARD   "assets\\malus_sample_hard.exe"

static const Level levels[TOTAL_PHASES] = {
    {
        .phase_name      = "Phase 0: The Infection",
        .lore_text       = "Edwin: \"Chief! Finally, you're awake. We barely made the jump to this planet. Malus trashed our last home and he's out for blood. Let's get you up to speed!\"",
        .theory_newbie   = "Welcome, Chief. Before facing Malus, we need to restore your skills. You will go through two training exercises using dummy files.",
        .theory_standard = "Two warm-up exercises with dummy files before the real fight.",
        .tool_name       = NULL,
        .tool_path       = NULL,
        .sample_path     = NULL,
        .expected_key    = NULL,
        .hints           = { NULL, NULL, NULL },
        .hint_count      = 0
    },
    {
        .phase_name      = "Tutorial 1: Packing Detection",
        .lore_text       = "Edwin: \"Open up UPX. We need to check if Malus is using some sort of shield. For practice, take a look at dummy.exe. Can you find the packing algorithm?\"",
        .theory_newbie   = "UPX (Ultimate Packer for eXecutables) is a free executable compressor that shrinks binaries by 50-70% while keeping them fully runnable. Malware authors use it to hide code from static analysis tools. Analysts use it in reverse to detect and unpack compressed samples.",
        .theory_standard = "Use UPX on dummy.exe. The packing format name is your key.",
        .tool_name       = "UPX",
        .tool_path       = TOOL_PATH_UPX,
        .sample_path     = SAMPLE_DUMMY,
        .expected_key    = GEN_KEY_T1,
        .hints = {
            "Run UPX with the -l (list) flag on the file.",
            "Look at the 'Format' column in the UPX output.",
            "The answer is a three-letter acronym."
        },
        .hint_count = 3
    },
    {
        .phase_name      = "Tutorial 2: PE Headers",
        .lore_text       = "Edwin: \"Now, let's move forward with PEviewer. Think of it as an X-ray tool. Open dummy2.exe with PEviewer and see what you can find!\"",
        .theory_newbie   = "PEview is a lightweight viewer for Windows Portable Executable (PE) files. It exposes the internal structure of an executable -- headers, sections, imports, exports, and timestamps -- without running it. Analysts use it to quickly fingerprint a binary during static analysis.",
        .theory_standard = "Open dummy2.exe in PEviewer. Look in the NT headers for the hidden value.",
        .tool_name       = "PEview",
        .tool_path       = TOOL_PATH_PEVIEWER,
        .sample_path     = SAMPLE_DUMMY2,
        .expected_key    = GEN_KEY_T2,
        .hints = {
            "Open PEview and load dummy2.exe via File > Open.",
            "Expand IMAGE_NT_HEADERS > IMAGE_FILE_HEADER.",
            "The key is the TimeDateStamp value in hex (e.g. 5F3E2A1B)."
        },
        .hint_count = 3
    },
    {
        .phase_name      = "Phase 1a: Static Basic Analysis",
        .lore_text       = "Edwin: \"The warm-up is over, Chief. This is the real Malus instance. Let's start with basic static analysis -- find that hidden URL!\"",
        .theory_newbie   = "Strings (Sysinternals) scans a binary for readable ASCII and Unicode text -- URLs, IP addresses, file paths, and other hardcoded data -- without executing the file. Malware often embeds command-and-control addresses in plaintext, making this a fast first step in static analysis.",
        .theory_standard = "Run strings on malus_sample.exe and grep for http://.",
        .tool_name       = "Strings",
        .tool_path       = TOOL_PATH_STRINGS,
        .sample_path      = SAMPLE_MALUS,
        .expected_key     = GEN_KEY_1A,
        .expected_key_hard = GEN_KEY_H_1A,
        .sample_path_hard  = SAMPLE_MALUS_HARD,
        .hints = {
            "Use a strings extraction tool on malus_sample.exe.",
            "Filter the output for lines starting with 'http'.",
            "The URL is embedded in plaintext in the .data section."
        },
        .hint_count = 3
    },
    {
        .phase_name      = "Phase 1b: Static Advanced Analysis",
        .lore_text       = "Edwin: \"Good work, Chief! Now sharpen your skills -- open IDA Pro. We need to identify which malicious DLL Malus calls to extract its payload.\"",
        .theory_newbie   = "IDA Pro is the industry-standard interactive disassembler and debugger. It converts compiled machine code back into assembly and pseudo-code, letting analysts trace program logic even when strings are obfuscated or XOR-encoded. It supports virtually all processor architectures and is the primary tool for deep malware reverse engineering.",
        .theory_standard = "Check the import table in IDA Pro. Find the non-standard DLL.",
        .tool_name       = "IDA Pro",
        .tool_path       = TOOL_PATH_IDA,
        .sample_path      = SAMPLE_MALUS,
        .expected_key     = GEN_KEY_1B,
        .expected_key_hard = GEN_KEY_H_1B,
        .sample_path_hard  = SAMPLE_MALUS_HARD,
        .hints = {
            "Open the Imports window in IDA Pro (View > Open Subviews > Imports).",
            "Sort imports by library name and look for one that isn't a standard Windows DLL.",
            "Standard DLLs include kernel32.dll, user32.dll, ntdll.dll -- yours is different."
        },
        .hint_count = 3
    },
    {
        .phase_name      = "Phase 2a: Dynamic Basic Analysis",
        .lore_text       = "Edwin: \"Chief -- Malus is alive and moving! Use Regshot and Procmon. Take a snapshot, run the sample, then find where it's phoning home!\"",
        .theory_newbie   = "Process Monitor (Procmon) is a Sysinternals tool that records file system, registry, and network activity on Windows in real time. It captures every action a running process takes, making it essential for dynamic malware analysis -- you can watch a sample phone home, drop files, or modify registry keys as it happens.",
        .theory_standard = "Regshot: take before/after snapshots. Procmon: filter for network connections.",
        .tool_name       = "Regshot + Procmon",
        .tool_path       = TOOL_PATH_REGSHOT,
        .sample_path      = SAMPLE_MALUS,
        .expected_key     = GEN_KEY_2A,
        .expected_key_hard = GEN_KEY_H_2A,
        .sample_path_hard  = SAMPLE_MALUS_HARD,
        .hints = {
            "In Procmon, add a filter: Operation is TCP Connect.",
            "Look for an outbound connection to a non-local IP.",
            "The IP will appear in the Path column of Procmon's network events."
        },
        .hint_count = 3
    },
    {
        .phase_name      = "Phase 2b: Dynamic Advanced -- Kill Switch",
        .lore_text       = "Edwin: \"This is it, Chief! Use ApateDNS to hijack Malus's comms, then find the kill switch. One last push and we save this planet!\"",
        .theory_newbie   = "ApateDNS is a fake DNS server that intercepts DNS queries from a running process and returns spoofed responses, typically redirecting to localhost. Malware analysts use it to prevent a sample from reaching real C2 servers while still observing which domains it tries to contact -- exposing the malware's communication infrastructure without risk.",
        .theory_standard = "ApateDNS intercepts DNS. Use a debugger to find the kill switch string.",
        .tool_name       = "ApateDNS + Debugger",
        .tool_path       = TOOL_PATH_APATEDNS,
        .sample_path      = SAMPLE_MALUS,
        .expected_key     = GEN_KEY_2B,
        .expected_key_hard = GEN_KEY_H_2B,
        .sample_path_hard  = SAMPLE_MALUS_HARD,
        .hints = {
            "Start ApateDNS before running the sample.",
            "Set a breakpoint on common string comparison functions (strcmp, lstrcmpA).",
            "The kill switch is a plaintext string passed to the comparison function."
        },
        .hint_count = 3
    }
};

const Level *get_level(int phase) {
    if (phase < 0 || phase >= TOTAL_PHASES) return NULL;
    return &levels[phase];
}
