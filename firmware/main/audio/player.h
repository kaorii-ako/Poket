#pragma once
#include "esp_err.h"
#include "app/app_state.h"

#define PLAYER_QUEUE_MAX 256

esp_err_t player_init(void);
esp_err_t player_play_index(uint16_t idx);     // index into the library
// Plays a list of library indices - a playlist, or any ad-hoc selection.
esp_err_t player_play_queue(const char *name, const uint16_t *idx, int n, int start);
const char *player_queue_name(void);           // "" when playing the library
uint16_t    player_queue_len(void);
void      player_toggle(void);
void      player_stop(void);
void      player_next(void);
void      player_prev(void);
void      player_seek(uint32_t seconds);
void      player_set_volume(uint8_t v);        // 0..100
uint8_t   player_volume(void);
void      player_set_repeat(repeat_t r);
void      player_set_shuffle(bool on);
void      player_set_output(audio_out_t o);
audio_out_t player_output(void);
void      player_publish(app_state_t *s);      // copy player fields into a snapshot
