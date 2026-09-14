// The one snapshot everything else renders from.
//
// Readers never block writers for long: a snapshot is a single memcpy under a
// short mutex, so a slow OLED flush can't tear against a decoder callback or
// an HTTP request.
#include "app/app_state.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static app_state_t     s_state;
static SemaphoreHandle_t s_lock;

void app_state_init(void) {
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof s_state);
    s_state.play    = PLAY_STOPPED;
    s_state.volume  = 60;
    s_state.out     = OUT_JACK;
    s_state.screen  = SCREEN_SPLASH;
    s_state.repeat  = REPEAT_ALL;
    snprintf(s_state.browse_path, sizeof s_state.browse_path, "/Music");
}

void app_state_snapshot(app_state_t *out) {
    if (!out) return;
    if (!s_lock) { memset(out, 0, sizeof *out); return; }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(out, &s_state, sizeof s_state);
    xSemaphoreGive(s_lock);
}

void app_state_update(app_state_fn fn, void *ctx) {
    if (!fn || !s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fn(&s_state, ctx);
    xSemaphoreGive(s_lock);
}

// ---- small setters ------------------------------------------------------

typedef struct { const char *name; uint8_t pct; } upload_ctx_t;

static void set_upload(app_state_t *s, void *ctx) {
    const upload_ctx_t *u = ctx;
    snprintf(s->upload_name, sizeof s->upload_name, "%s", u->name ? u->name : "");
    s->upload_pct = u->pct;
}
void app_state_set_upload(const char *name, uint8_t pct) {
    upload_ctx_t u = { name, pct };
    app_state_update(set_upload, &u);
}

static void set_clients(app_state_t *s, void *ctx) { s->clients = *(uint8_t *)ctx; }
void app_state_set_clients(uint8_t n) { app_state_update(set_clients, &n); }
