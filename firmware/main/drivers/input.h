#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum {
    INPUT_NONE = 0,
    INPUT_ENC_CW, INPUT_ENC_CCW, INPUT_ENC_PRESS, INPUT_ENC_LONG,
    INPUT_PREV, INPUT_PLAY, INPUT_NEXT, INPUT_PLAY_LONG,
} input_ev_t;
void       input_init(void);
input_ev_t input_get(uint32_t wait_ms);   // blocks on the event queue
