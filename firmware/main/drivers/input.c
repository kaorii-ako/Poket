// Encoder + buttons.
//
// The encoder is read with a 4-state Gray decoder in an ISR rather than a
// timer poll: EC11s emit their transitions faster than a 10 ms poll can see,
// and a missed edge shows up as the knob "sticking", which feels broken in a
// way users notice immediately.
#include "drivers/input.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static QueueHandle_t s_q;

// Gray-code transition table, index = (prev<<2)|now. +1 CW, -1 CCW, 0 invalid.
static const int8_t kStep[16] = { 0,-1, 1, 0, 1, 0, 0,-1,-1, 0, 0, 1, 0, 1,-1, 0 };
static volatile uint8_t  s_prev;
static volatile int8_t   s_accum;
static volatile int64_t  s_btn_down[4];

static void IRAM_ATTR enc_isr(void *arg) {
    (void)arg;
    uint8_t now = (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) | gpio_get_level(PIN_ENC_B));
    int8_t step = kStep[(s_prev << 2) | now];
    s_prev = now;
    s_accum += step;
    // EC11s are detented every 4 transitions; only emit on a full detent so a
    // single click is exactly one event.
    if (s_accum >= 4 || s_accum <= -4) {
        input_ev_t e = s_accum > 0 ? INPUT_ENC_CW : INPUT_ENC_CCW;
        s_accum = 0;
        BaseType_t hp = pdFALSE;
        xQueueSendFromISR(s_q, &e, &hp);
        if (hp) portYIELD_FROM_ISR();
    }
}

typedef struct { gpio_num_t pin; input_ev_t press, longpress; int idx; } btn_t;
static const btn_t kButtons[] = {
    { PIN_ENC_SW,   INPUT_ENC_PRESS, INPUT_ENC_LONG,  0 },
    { PIN_BTN_PREV, INPUT_PREV,      INPUT_PREV,      1 },
    { PIN_BTN_PLAY, INPUT_PLAY,      INPUT_PLAY_LONG, 2 },
    { PIN_BTN_NEXT, INPUT_NEXT,      INPUT_NEXT,      3 },
};

static void btn_task(void *arg) {
    (void)arg;
    bool was[4] = { false, false, false, false };
    for (;;) {
        int64_t now = esp_timer_get_time();
        for (int i = 0; i < 4; i++) {
            bool down = gpio_get_level(kButtons[i].pin) == 0;   // active low
            if (down && !was[i]) {
                s_btn_down[i] = now;
            } else if (!down && was[i]) {
                int64_t held = now - s_btn_down[i];
                if (held > 12000) {          // 12 ms debounce
                    input_ev_t e = held > 600000 ? kButtons[i].longpress : kButtons[i].press;
                    xQueueSend(s_q, &e, 0);
                }
            }
            was[i] = down;
        }
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

void input_init(void) {
    s_q = xQueueCreate(16, sizeof(input_ev_t));

    gpio_config_t enc = {
        .pin_bit_mask = (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,          // 10k externals too
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&enc);

    uint64_t mask = 0;
    for (int i = 0; i < 4; i++) mask |= 1ULL << kButtons[i].pin;
    gpio_config_t btn = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn);

    s_prev = (uint8_t)((gpio_get_level(PIN_ENC_A) << 1) | gpio_get_level(PIN_ENC_B));
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(PIN_ENC_A, enc_isr, NULL);
    gpio_isr_handler_add(PIN_ENC_B, enc_isr, NULL);
    xTaskCreate(btn_task, "btn", 2560, NULL, 6, NULL);
}

input_ev_t input_get(uint32_t wait_ms) {
    input_ev_t e = INPUT_NONE;
    xQueueReceive(s_q, &e, pdMS_TO_TICKS(wait_ms));
    return e;
}
