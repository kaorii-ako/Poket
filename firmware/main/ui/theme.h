// Theme engine.
//
// A theme owns the whole look of a screen, not a colour swatch - on a 1-bit
// 128x64 panel there is no colour to vary, so a theme has to earn its
// difference through layout, type, iconography and motion. Each one draws the
// same state into the same framebuffer and they look nothing alike.
#pragma once
#include "ui/gfx.h"
#include "app/app_state.h"

#define THEME_ID_LEN 16
#define THEME_NAME_LEN 24

typedef struct theme_s {
    char id[THEME_ID_LEN];
    char name[THEME_NAME_LEN];

    // Screens. Every theme must provide all of them; there is no fallback,
    // because a half-themed UI looks broken rather than minimal.
    void (*splash)      (gfx_t *g, const app_state_t *s, uint32_t tick);
    void (*now_playing) (gfx_t *g, const app_state_t *s, uint32_t tick);
    void (*browser)     (gfx_t *g, const app_state_t *s, uint32_t tick);
    void (*menu)        (gfx_t *g, const app_state_t *s, uint32_t tick);
    void (*transfer)    (gfx_t *g, const app_state_t *s, uint32_t tick);
    void (*volume_hud)  (gfx_t *g, const app_state_t *s, uint32_t tick);

    // Panel-level: some themes read better as light-on-dark, some inverted.
    bool invert_panel;
    uint8_t contrast;          // 0..255, written to the SSD1306
} theme_t;

// Built-ins, in the order they appear in the picker.
extern const theme_t theme_minimal;
extern const theme_t theme_anime;
extern const theme_t theme_terminal;
extern const theme_t theme_cassette;
extern const theme_t theme_brutalist;
extern const theme_t theme_y2k;

#define THEME_BUILTIN_COUNT 6
extern const theme_t *const theme_builtins[THEME_BUILTIN_COUNT];

// ---- custom themes ------------------------------------------------------
// A custom theme is data, not code: the web app uploads a pack and the
// renderer below interprets it. That keeps arbitrary uploads from being
// arbitrary execution.
#define THEME_PACK_MAGIC 0x504B5431u   /* "PKT1" */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    char     name[THEME_NAME_LEN];
    uint8_t  invert;
    uint8_t  contrast;
    uint8_t  layout;           // which built-in layout to drive, 0..5
    uint8_t  accent_dither;    // dither_t for fill areas
    uint16_t art_w, art_h;     // 1-bit art blitted on the now-playing screen
    uint16_t art_x, art_y;
    uint32_t art_len;
    // followed by art_len bytes of packed 1-bit bitmap
} theme_pack_hdr_t;

esp_err_t theme_custom_load(const uint8_t *pack, size_t len);
bool      theme_custom_present(void);
void      theme_custom_clear(void);

// ---- registry -----------------------------------------------------------
int               theme_count(void);              // builtins + custom if present
const theme_t    *theme_at(int index);
const theme_t    *theme_by_id(const char *id);
const theme_t    *theme_current(void);
esp_err_t         theme_set(const char *id);      // persists to NVS
void              theme_init(void);               // restores the saved choice

// Shared furniture so themes stay consistent where consistency helps.
void theme_draw_battery(gfx_t *g, int x, int y, uint8_t pct, bool charging);
void theme_draw_bt(gfx_t *g, int x, int y, bool connected);
void theme_draw_progress(gfx_t *g, int x, int y, int w, int h, uint8_t pct, dither_t fill);
void theme_fmt_time(char *out, size_t n, uint32_t seconds);
