// MINIMAL - the one that gets out of the way.
//
// Rule: one thing is loud, everything else is quiet. The title is the only
// element at full size; artist, times and status all sit at 8px. No boxes, no
// borders, no chrome. The progress bar is a single hairline because a 1px rule
// at the very bottom edge reads as part of the panel, not as a widget.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>
#include <string.h>

static void status_bar(gfx_t *g, const app_state_t *s) {
    theme_draw_battery(g, 111, 0, s->batt_pct, s->charging);
    if (s->out == OUT_BLUETOOTH) theme_draw_bt(g, 100, 0, s->bt_connected);
    char pos[16];
    snprintf(pos, sizeof pos, "%u/%u", s->queue_pos + 1, s->queue_len);
    gfx_text(g, &font_sans_8, 0, 0, pos, PX_ON);
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    gfx_text_center(g, &font_sans_16, 64, 22, "Poket", PX_ON);
    // hairline that draws itself in over ~1s, then sits still
    int w = t < 30 ? (int)(t * 62 / 30) : 62;
    gfx_hline(g, 64 - w / 2, 44, w, PX_ON);
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    status_bar(g, s);

    gfx_text_marquee(g, &font_sans_16, 0, 16, 128, s->track.title, (int)(t / 2), PX_ON);
    gfx_text(g, &font_sans_8, 0, 35, s->track.artist, PX_ON);

    char a[12], b[12];
    theme_fmt_time(a, sizeof a, s->elapsed_s);
    theme_fmt_time(b, sizeof b, s->track.duration_s);
    gfx_text(g, &font_sans_8, 0, 50, a, PX_ON);
    gfx_text(g, &font_sans_8, 128 - gfx_text_w(&font_sans_8, b), 50, b, PX_ON);

    if (s->play == PLAY_PAUSED) gfx_text_center(g, &font_sans_8, 64, 50, "paused", PX_ON);

    uint8_t pct = s->track.duration_s ? (s->elapsed_s * 100 / s->track.duration_s) : 0;
    theme_draw_progress(g, 0, 63, 128, 1, pct, DITHER_50);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS; i++) {
        int y = i * 10 + 2;
        bool sel = (i == s->row_sel);
        if (sel) gfx_fill_rect(g, 0, y - 1, 128, 10, PX_ON);
        const char *nm = s->rows[i].name;
        gfx_text(g, &font_sans_8, 4, y, nm, sel ? PX_OFF : PX_ON);
        if (s->rows[i].is_dir)
            gfx_text(g, &font_sans_8, 120, y, ">", sel ? PX_OFF : PX_ON);
    }
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    gfx_text_center(g, &font_sans_11, 64, 4, s->wifi_is_ap ? "Hotspot" : "Wi-Fi", PX_ON);
    gfx_text_center(g, &font_sans_8, 64, 20, s->wifi_ssid, PX_ON);
    gfx_text_center(g, &font_sans_11, 64, 32, s->wifi_ip, PX_ON);
    if (s->upload_pct > 0 && s->upload_pct < 100) {
        gfx_text(g, &font_sans_8, 0, 50, s->upload_name, PX_ON);
        theme_draw_progress(g, 0, 62, 128, 2, s->upload_pct, DITHER_50);
    } else {
        gfx_text_center(g, &font_sans_8, 64, 52, "open in a browser", PX_ON);
    }
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_fill_rect(g, 14, 22, 100, 20, PX_OFF);
    gfx_rect(g, 14, 22, 100, 20, PX_ON);
    char v[8];
    snprintf(v, sizeof v, s->muted ? "muted" : "%u", s->volume);
    gfx_text_center(g, &font_sans_8, 64, 25, v, PX_ON);
    theme_draw_progress(g, 20, 36, 88, 1, s->muted ? 0 : s->volume, DITHER_50);
}

const theme_t theme_minimal = {
    .id = "minimal", .name = "Minimal",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0x7F,
};
