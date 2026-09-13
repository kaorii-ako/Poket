#pragma once
#include "esp_err.h"
#include "app/app_state.h"

esp_err_t player_init(void);
esp_err_t player_play_index(uint16_t idx);     // index into the library
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
