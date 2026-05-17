#ifndef AUDIO_H
#define AUDIO_H

void audio_init(void);
void audio_play(void);
void audio_stop(void);
void audio_play_track(int track_index);         /* immediately switch to track */
void audio_play_sfx(const char *filename);      /* async: non-blocking */
void audio_play_sfx_sync(const char *filename); /* sync: blocks until done */
void audio_set_volume(int vol);  /* 0-100 */
int  audio_get_volume(void);
int  audio_is_muted(void);
void audio_set_muted(int muted);

#endif
