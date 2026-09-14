// The shell: one task owns the panel.
//
// It reads input events, moves the screen state machine, renders whichever
// theme is selected into the framebuffer and flushes. Nothing else touches
// the SSD1306, so there is no locking around the display at all.
#include "app/app_state.h"
#include "audio/player.h"
#include "drivers/input.h"
#include "drivers/ssd1306.h"
#include "drivers/battery.h"
#include "storage/library.h"
#include "storage/sdcard.h"
#include "net/wifi.h"
#include "ui/gfx.h"
#include "ui/theme.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui";

#define FRAME_MS        40      // 25 fps; the I2C flush is ~9 ms at 400 kHz
#define SPLASH_MS     1600
#define VOLUME_HUD_MS 1200
#define BATT_EVERY_MS 5000

static gfx_t s_g;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// ---- state pokes --------------------------------------------------------

typedef struct { screen_t screen; } screen_ctx_t;
static void set_screen(app_state_t *s, void *ctx) { s->screen = ((screen_ctx_t *)ctx)->screen; }
static void go(screen_t sc) { screen_ctx_t c = { sc }; app_state_update(set_screen, &c); }

static void bump_volume_hud(app_state_t *s, void *ctx) {
    (void)ctx;
    s->volume_hud_until_ms = now_ms() + VOLUME_HUD_MS;
}

typedef struct { int delta; } sel_ctx_t;
static void move_sel(app_state_t *s, void *ctx) {
    int d = ((sel_ctx_t *)ctx)->delta;
    if (s->row_count == 0) return;
    int sel = (int)s->row_sel + d;
    if (sel < 0) sel = 0;
    if (sel > s->row_count - 1) sel = s->row_count - 1;
    s->row_sel = (uint8_t)sel;
}

static void publish_player(app_state_t *s, void *ctx) { (void)ctx; player_publish(s); }

static void publish_battery(app_state_t *s, void *ctx) {
    const batt_t *b = ctx;
    s->batt_pct = b->pct;
    s->batt_mv = b->mv;
    s->batt_ma = b->ma;
    s->charging = b->charging;
    s->card_present = sdcard_mounted();
}

// Fills the six browser rows from the library around the current selection.
static void publish_rows(app_state_t *s, void *ctx) {
    (void)ctx;
    uint16_t total = library_count();
    s->browse_total = total;
    if (s->browse_offset > total) s->browse_offset = 0;
    uint8_t n = 0;
    for (uint16_t i = s->browse_offset; i < total && n < BROWSE_ROWS; i++, n++) {
        const lib_entry_t *e = library_get(i);
        snprintf(s->rows[n].name, sizeof s->rows[n].name, "%s", e ? e->title : "?");
        s->rows[n].is_dir = false;
    }
    s->row_count = n;
    if (s->row_sel >= n) s->row_sel = n ? n - 1 : 0;
}

// ---- input --------------------------------------------------------------

static void handle(input_ev_t ev, const app_state_t *s) {
    switch (ev) {
        case INPUT_ENC_CW:
        case INPUT_ENC_CCW: {
            int d = (ev == INPUT_ENC_CW) ? 1 : -1;
            if (s->screen == SCREEN_BROWSER || s->screen == SCREEN_MENU) {
                sel_ctx_t c = { d };
                app_state_update(move_sel, &c);
            } else {
                int v = (int)player_volume() + d * 4;
                player_set_volume(v < 0 ? 0 : v > 100 ? 100 : (uint8_t)v);
                app_state_update(bump_volume_hud, NULL);
            }
            break;
        }
        case INPUT_ENC_PRESS:
            if (s->screen == SCREEN_BROWSER)
                player_play_index(s->browse_offset + s->row_sel);
            go(s->screen == SCREEN_BROWSER ? SCREEN_NOW_PLAYING : SCREEN_BROWSER);
            break;

        case INPUT_ENC_LONG:
            // Transfer mode is modal on purpose - see wifi.c.
            if (wifi_transfer_active()) { wifi_transfer_stop(); go(SCREEN_NOW_PLAYING); }
            else if (wifi_transfer_start() == ESP_OK) go(SCREEN_TRANSFER);
            break;

        case INPUT_PLAY:      player_toggle(); break;
        case INPUT_NEXT:      player_next();   break;
        case INPUT_PREV:      player_prev();   break;
        case INPUT_PLAY_LONG: player_set_output(player_output() == OUT_JACK
                                                ? OUT_BLUETOOTH : OUT_JACK); break;
        default: break;
    }
}

// ---- render -------------------------------------------------------------

static void render(const app_state_t *s, const theme_t *t, uint32_t tick) {
    gfx_clear(&s_g);
    switch (s->screen) {
        case SCREEN_SPLASH:      t->splash(&s_g, s, tick); break;
        case SCREEN_BROWSER:     t->browser(&s_g, s, tick); break;
        case SCREEN_MENU:        t->menu(&s_g, s, tick); break;
        case SCREEN_TRANSFER:    t->transfer(&s_g, s, tick); break;
        case SCREEN_NOW_PLAYING:
        default:                 t->now_playing(&s_g, s, tick); break;
    }
    // The volume HUD is an overlay, not a screen: it must not lose the user's
    // place in the browser just because they turned the knob.
    if (s->volume_hud_until_ms > now_ms()) t->volume_hud(&s_g, s, tick);
}

static void ui_task(void *arg) {
    (void)arg;
    const theme_t *applied = NULL;
    uint32_t tick = 0, next_batt = 0;
    const uint32_t boot_ms = now_ms();

    for (;;) {
        const uint32_t frame_start = now_ms();

        // Input is drained rather than waited on, so the frame rate stays
        // even whether the user is spinning the encoder or not.
        app_state_t s;
        for (input_ev_t ev; (ev = input_get(0)) != INPUT_NONE; ) {
            app_state_snapshot(&s);
            handle(ev, &s);
        }

        if (frame_start >= next_batt) {
            batt_t b;
            battery_read(&b);
            app_state_update(publish_battery, &b);
            next_batt = frame_start + BATT_EVERY_MS;
        }

        app_state_update(publish_player, NULL);
        app_state_update(publish_rows, NULL);
        app_state_snapshot(&s);

        if (s.screen == SCREEN_SPLASH && frame_start - boot_ms > SPLASH_MS)
            go(SCREEN_NOW_PLAYING);

        const theme_t *t = theme_current();
        if (t != applied) {          // panel-level settings follow the theme
            ssd1306_contrast(t->contrast);
            ssd1306_invert(t->invert_panel);
            applied = t;
            ESP_LOGI(TAG, "theme: %s", t->name);
        }

        render(&s, t, tick++);
        ssd1306_flush(&s_g);

        const uint32_t spent = now_ms() - frame_start;
        vTaskDelay(pdMS_TO_TICKS(spent >= FRAME_MS ? 1 : FRAME_MS - spent));
    }
}

void ui_task_start(void) {
    // Renders into a 1 kB framebuffer and calls into theme code; 4 kB is
    // comfortable and this task must never be the one that runs out.
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 4, NULL, 0);
}
