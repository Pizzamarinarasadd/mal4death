#include "audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUM_TRACKS 4
static const char *TRACK_NAMES[NUM_TRACKS] = {
    "assets\\music_01.wav",
    "assets\\music_02.wav",
    "assets\\music_03.wav",
    "assets\\music_04.wav",
};

static int    volume   = 70;
static int    muted    = 0;
static int    loop_run = 0;
static HANDLE music_thread = NULL;
static char   exe_dir[MAX_PATH];
static char   track_paths[NUM_TRACKS][MAX_PATH];
static int    track_polls[NUM_TRACKS]; /* max 100ms ticks per track */
static int    shuffle_order[NUM_TRACKS];
static int    current_track = 0;

static void apply_volume(void) {
    int v = muted ? 0 : volume;
    DWORD vol = (DWORD)(v * 0xFFFF / 100);
    waveOutSetVolume(0, vol | (vol << 16));
}

/* Derive duration from WAV header (mono 16-bit 22050Hz assumed as fallback). */
static int wav_duration_polls(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 2500; /* fallback ~250s */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    unsigned int sample_rate = 22050;
    unsigned short channels = 1, bits = 16;
    fseek(f, 22, SEEK_SET); fread(&channels,    2, 1, f);
    fseek(f, 24, SEEK_SET); fread(&sample_rate, 4, 1, f);
    fseek(f, 34, SEEK_SET); fread(&bits,        2, 1, f);
    fclose(f);
    long bytes_per_sec = sample_rate * channels * (bits / 8);
    if (bytes_per_sec <= 0 || fsize <= 44) return 2500;
    /* polls = duration_ms / 100, +20 ticks (2s) safety margin */
    return (int)((fsize - 44) * 10 / bytes_per_sec) + 20;
}

static void shuffle_playlist(void) {
    for (int i = 0; i < NUM_TRACKS; i++) shuffle_order[i] = i;
    for (int i = NUM_TRACKS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = shuffle_order[i];
        shuffle_order[i] = shuffle_order[j];
        shuffle_order[j] = t;
    }
    current_track = 0;
}

static DWORD WINAPI music_loop(LPVOID param) {
    (void)param;
    srand(GetTickCount());
    shuffle_playlist();
    while (loop_run) {
        int idx = shuffle_order[current_track];
        /* SND_ASYNC returns immediately; poll loop_run every 100ms
         * so audio_stop can cancel within one tick — no SND_SYNC hang. */
        PlaySoundA(track_paths[idx], NULL, SND_FILENAME | SND_ASYNC);
        for (int i = 0; i < track_polls[idx] && loop_run; i++)
            Sleep(100);
        if (!loop_run) break;
        current_track++;
        if (current_track >= NUM_TRACKS) shuffle_playlist();
    }
    PlaySoundA(NULL, NULL, 0); /* stop the async sound on exit */
    return 0;
}

void audio_init(void) {
    volume   = 70;
    muted    = 0;
    loop_run = 0;
    music_thread = NULL;

    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *last = strrchr(exe_dir, '\\');
    if (last) *(last + 1) = '\0';

    for (int i = 0; i < NUM_TRACKS; i++) {
        strncpy(track_paths[i], exe_dir, MAX_PATH - 1);
        track_paths[i][MAX_PATH - 1] = '\0';
        strncat(track_paths[i], TRACK_NAMES[i],
                MAX_PATH - strlen(track_paths[i]) - 1);
        track_polls[i] = wav_duration_polls(track_paths[i]);
    }
}

void audio_play(void) {
    audio_stop();
    apply_volume();
    loop_run = 1;
    music_thread = CreateThread(NULL, 0, music_loop, NULL, 0, NULL);
}

void audio_play_track(int track_index) {
    if (track_index < 0 || track_index >= NUM_TRACKS) return;
    audio_stop();
    apply_volume();
    /* Place the requested track first in the shuffle order */
    shuffle_playlist();
    shuffle_order[0] = track_index;
    current_track    = 0;
    loop_run = 1;
    music_thread = CreateThread(NULL, 0, music_loop, NULL, 0, NULL);
}

void audio_stop(void) {
    if (!music_thread) return;
    loop_run = 0;
    /* Thread polls loop_run every 100ms and calls PlaySoundA(NULL) on exit;
     * wait up to 500ms for it to finish cleanly. */
    WaitForSingleObject(music_thread, 500);
    CloseHandle(music_thread);
    music_thread = NULL;
}

void audio_play_sfx(const char *filename) {
    char path[MAX_PATH];
    strncpy(path, exe_dir, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';
    strncat(path, filename, MAX_PATH - strlen(path) - 1);
    PlaySoundA(path, NULL, SND_FILENAME | SND_ASYNC);
}

void audio_play_sfx_sync(const char *filename) {
    char path[MAX_PATH];
    strncpy(path, exe_dir, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';
    strncat(path, filename, MAX_PATH - strlen(path) - 1);
    PlaySoundA(path, NULL, SND_FILENAME | SND_SYNC);
}

void audio_set_volume(int vol) {
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;
    volume = vol;
    if (muted) return;
    apply_volume();
}

int audio_get_volume(void) { return volume; }
int audio_is_muted(void)   { return muted; }

void audio_set_muted(int m) {
    muted = m;
    apply_volume();
}
