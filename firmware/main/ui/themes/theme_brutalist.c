// BRUTALIST - one enormous number and nothing that apologises for it.
//
// Type is set in two sizes only: 22px for the thing you actually read across a
// room, 10px caps for everything else. Rules are 3px because 1px would be
// polite. Nothing is rounded, nothing is centred unless the grid says so, and
// the selected row is a solid inverted slab rather than a highlight.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>
#include <string.h>

static void rule(gfx_t *g, int y) { gfx_fill_rect(g, 0, y, 128, 3, PX_ON); }

static void caps(gfx_t *g, int x, int y, const char *s, px_mode_t m) {
    char up[40]; size_t i = 0;
    for (; s[i] && i < sizeof up - 1; i++)
        up[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    up[i] = 0;
    gfx_text(g, &font_bold_10, x, y, up, m);
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    int h = t < 24 ? (int)(t * 64 / 24) : 64;
    gfx_fill_rect(g, 0, 0, 128, h, PX_ON);
    if (h >= 64) {
        gfx_text(g, &font_slab_22, 6, 12, "POKET", PX_OFF);
        caps(g, 6, 40, "MP3 / BT / SD", PX_OFF);
    }
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);

    // the elapsed time is the hero element
    char a[12], b[12];
    theme_fmt_time(a, sizeof a, s->elapsed_s);
    theme_fmt_time(b, sizeof b, s->track.duration_s);
    gfx_text(g, &font_slab_22, 2, 0, a, PX_ON);
    caps(g, 2, 24, b, PX_ON);

    // state as a hard slab, right aligned
    const char *st = s->play == PLAY_PLAYING ? "PLAY" : s->play == PLAY_PAUSED ? "HOLD" : "STOP";
    int w = gfx_text_w(&font_bold_10, st) + 8;
    gfx_fill_rect(g, 128 - w, 0, w, 14, PX_ON);
    caps(g, 128 - w + 4, 2, st, PX_OFF);
    theme_draw_battery(g, 113, 18, s->batt_pct, s->charging);

    rule(g, 36);
    gfx_text_marquee(g, &font_bold_10, 2, 41, 124, s->track.title, (int)(t / 2), PX_ON);
    gfx_text_marquee(g, &font_bold_10, 2, 52, 124, s->track.artist, (int)(t / 3), PX_ON);

    uint8_t pct = s->track.duration_s ? (s->elapsed_s * 100 / s->track.duration_s) : 0;
    gfx_fill_rect(g, 0, 62, (pct * 128 + 50) / 100, 2, PX_ON);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    gfx_fill_rect(g, 0, 0, 128, 13, PX_ON);
    caps(g, 3, 2, "LIBRARY", PX_OFF);
    char n[12]; snprintf(n, sizeof n, "%u", s->browse_total);
    gfx_text(g, &font_bold_10, 125 - gfx_text_w(&font_bold_10, n), 2, n, PX_OFF);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS - 1; i++) {
        int y = 15 + i * 10;
        bool sel = (i == s->row_sel);
        if (sel) gfx_fill_rect(g, 0, y - 1, 128, 10, PX_ON);
        caps(g, 3, y, s->rows[i].name, sel ? PX_OFF : PX_ON);
    }
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    gfx_fill_rect(g, 0, 0, 128, 16, PX_ON);
    caps(g, 3, 3, "TRANSFER", PX_OFF);
    gfx_text(g, &font_slab_22, 2, 20, s->wifi_ip, PX_ON);
    rule(g, 46);
    caps(g, 2, 51, s->wifi_ssid, PX_ON);
    if (s->upload_pct > 0 && s->upload_pct < 100)
        gfx_fill_rect(g, 0, 62, (s->upload_pct * 128 + 50) / 100, 2, PX_ON);
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_fill_rect(g, 0, 16, 128, 32, PX_OFF);
    gfx_fill_rect(g, 0, 16, 128, 3, PX_ON);
    gfx_fill_rect(g, 0, 45, 128, 3, PX_ON);
    char v[10]; snprintf(v, sizeof v, s->muted ? "MUTE" : "%u", s->volume);
    gfx_text(g, &font_slab_22, 4, 20, v, PX_ON);
    int bw = ((s->muted ? 0 : s->volume) * 60 + 50) / 100;
    gfx_fill_rect(g, 64, 30, bw, 8, PX_ON);
    gfx_rect(g, 64, 30, 60, 8, PX_ON);
}

const theme_t theme_brutalist = {
    .id = "brutalist", .name = "Brutalist",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0xFF,
};
