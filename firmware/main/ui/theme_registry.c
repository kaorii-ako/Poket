// Theme registry, persistence, and the custom-theme renderer.
//
// A custom theme uploaded from the web app is DATA, never code. The pack
// carries a name, panel flags, one of the built-in layouts to drive, and a
// 1-bit bitmap. That is a deliberate limit: letting a browser upload something
// that executes on the device would be a far worse idea than a slightly less
// flexible theme format.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "theme";
#define NVS_NS  "poket"
#define NVS_KEY "theme"

const theme_t *const theme_builtins[THEME_BUILTIN_COUNT] = {
    &theme_minimal, &theme_anime, &theme_terminal,
    &theme_cassette, &theme_brutalist, &theme_y2k,
};

static const theme_t *s_current = &theme_minimal;

// ---- custom theme -------------------------------------------------------
static theme_pack_hdr_t s_pack;
static uint8_t *s_art;                 // in PSRAM
static bool     s_have_custom;
static theme_t  s_custom;

static const theme_t *layout_base(void) {
    int i = s_pack.layout < THEME_BUILTIN_COUNT ? s_pack.layout : 0;
    return theme_builtins[i];
}

// The custom screens defer to the chosen layout, then stamp the uploaded art
// over the now-playing screen. Everything else is inherited, which is why a
// custom theme always looks finished rather than half-drawn.
static void custom_now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    layout_base()->now_playing(g, s, t);
    if (s_art && s_pack.art_w && s_pack.art_h) {
        gfx_fill_rect(g, s_pack.art_x - 1, s_pack.art_y - 1,
                      s_pack.art_w + 2, s_pack.art_h + 2, PX_OFF);
        gfx_bitmap(g, s_pack.art_x, s_pack.art_y, s_pack.art_w, s_pack.art_h, s_art, PX_ON);
    }
}
static void custom_splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    if (s_art && s_pack.art_w) {
        int x = (GFX_W - s_pack.art_w) / 2, y = (GFX_H - s_pack.art_h) / 2 - 6;
        gfx_bitmap(g, x, y, s_pack.art_w, s_pack.art_h, s_art, PX_ON);
    }
    gfx_text_center(g, &font_sans_8, 64, 54, s_pack.name, PX_ON);
    (void)s; (void)t;
}
static void custom_browser (gfx_t *g, const app_state_t *s, uint32_t t) { layout_base()->browser(g, s, t); }
static void custom_menu    (gfx_t *g, const app_state_t *s, uint32_t t) { layout_base()->menu(g, s, t); }
static void custom_transfer(gfx_t *g, const app_state_t *s, uint32_t t) { layout_base()->transfer(g, s, t); }
static void custom_volume  (gfx_t *g, const app_state_t *s, uint32_t t) { layout_base()->volume_hud(g, s, t); }

esp_err_t theme_custom_load(const uint8_t *pack, size_t len) {
    if (len < sizeof(theme_pack_hdr_t)) return ESP_ERR_INVALID_SIZE;
    theme_pack_hdr_t h;
    memcpy(&h, pack, sizeof h);
    if (h.magic != THEME_PACK_MAGIC) return ESP_ERR_INVALID_ARG;
    if (h.art_w > GFX_W || h.art_h > GFX_H) return ESP_ERR_INVALID_SIZE;
    size_t need = (size_t)((h.art_w + 7) / 8) * h.art_h;
    if (h.art_len != need || len < sizeof h + need) return ESP_ERR_INVALID_SIZE;

    uint8_t *art = heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!art) art = malloc(need);
    if (!art) return ESP_ERR_NO_MEM;
    memcpy(art, pack + sizeof h, need);

    free(s_art);
    s_art = art;
    s_pack = h;
    s_pack.name[THEME_NAME_LEN - 1] = 0;

    memset(&s_custom, 0, sizeof s_custom);
    strlcpy(s_custom.id, "custom", THEME_ID_LEN);
    strlcpy(s_custom.name, s_pack.name[0] ? s_pack.name : "Custom", THEME_NAME_LEN);
    s_custom.splash = custom_splash;   s_custom.now_playing = custom_now_playing;
    s_custom.browser = custom_browser; s_custom.menu = custom_menu;
    s_custom.transfer = custom_transfer; s_custom.volume_hud = custom_volume;
    s_custom.invert_panel = h.invert != 0;
    s_custom.contrast = h.contrast ? h.contrast : 0x8F;
    s_have_custom = true;
    ESP_LOGI(TAG, "custom theme '%s' loaded, %ux%u art", s_custom.name, h.art_w, h.art_h);
    return ESP_OK;
}

bool theme_custom_present(void) { return s_have_custom; }

void theme_custom_clear(void) {
    if (s_current == &s_custom) s_current = &theme_minimal;
    free(s_art); s_art = NULL;
    s_have_custom = false;
}

// ---- registry -----------------------------------------------------------
int theme_count(void) { return THEME_BUILTIN_COUNT + (s_have_custom ? 1 : 0); }

const theme_t *theme_at(int i) {
    if (i < 0) return NULL;
    if (i < THEME_BUILTIN_COUNT) return theme_builtins[i];
    return (s_have_custom && i == THEME_BUILTIN_COUNT) ? &s_custom : NULL;
}

const theme_t *theme_by_id(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < THEME_BUILTIN_COUNT; i++)
        if (!strcmp(theme_builtins[i]->id, id)) return theme_builtins[i];
    if (s_have_custom && !strcmp(s_custom.id, id)) return &s_custom;
    return NULL;
}

const theme_t *theme_current(void) { return s_current; }

esp_err_t theme_set(const char *id) {
    const theme_t *t = theme_by_id(id);
    if (!t) return ESP_ERR_NOT_FOUND;
    s_current = t;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY, id);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "theme -> %s", t->name);
    return ESP_OK;
}

void theme_init(void) {
    nvs_handle_t h;
    char id[THEME_ID_LEN] = {0};
    size_t n = sizeof id;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_str(h, NVS_KEY, id, &n) == ESP_OK) {
            const theme_t *t = theme_by_id(id);
            if (t) s_current = t;
        }
        nvs_close(h);
    }
    ESP_LOGI(TAG, "active theme: %s", s_current->name);
}
