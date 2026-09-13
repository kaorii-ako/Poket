// A sink swallows interleaved 16-bit stereo PCM. Two exist: the PCM5102A over
// I2S, and A2DP to a Bluetooth headset. The player does not care which.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    const char *name;
    esp_err_t (*start)(uint32_t sample_rate, uint8_t channels);
    size_t    (*write)(const int16_t *pcm, size_t frames);   // returns frames taken
    void      (*stop)(void);
    void      (*set_volume)(uint8_t pct);
} audio_sink_t;

extern const audio_sink_t sink_i2s;
extern const audio_sink_t sink_a2dp;
