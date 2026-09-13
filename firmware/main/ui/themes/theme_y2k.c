// Y2K - chrome, bevels and starbursts, rendered with the only tools a 1-bit
// panel has: double outlines and ordered dither. The "gradient" on the bevels
// is a 75% dither on top and 25% on the bottom, which at this pixel pitch
// genuinely reads as a curved chrome edge.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>

static void bevel(gfx_t *g, int x, int y, int w, int h, int r) {
    gfx_fill_rect(g, x, y, w, h, PX_OFF);
    gfx_round_rect(g, x, y, w, h, r, PX_ON);
    gfx_round_rect(g, x + 2, y + 2, w - 4, h - 4, r > 2 ? r - 2 : 1, PX_ON);
    gfx_dither_rect(g, x + 3, y + 3, w - 6, (h - 6) / 2, DITHER_25, PX_ON);
    gfx_dither_rect(g, x + 3, y + 3 + (h - 6) / 2, w - 6, (h - 6) / 2, DITHER_75, PX_ON);
}

static void star(gfx_t *g, int x, int y, int r) {
    gfx_line(g, x - r, y, x + r, y, PX_ON);
    gfx_line(g, x, y - r, x, y + r, PX_ON);
    int d = r * 2 / 3;
    gfx_line(g, x - d, y - d, x + d, y + d, PX_ON);
    gfx_line(g, x - d, y + d, x + d, y - d, PX_ON);
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    bevel(g, 8, 16, 112, 32, 10);
    gfx_fill_rect(g, 20, 22, 88, 20, PX_OFF);
    gfx_text_center(g, &font_sans_16, 64, 23, "Poket", PX_ON);
    star(g, 16, 10, 4 + (int)((t / 6) % 2));
    star(g, 112, 54, 3 + (int)((t / 6 + 1) % 2));
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    bevel(g, 0, 0, 128, 13, 5);
    gfx_fill_rect(g, 4, 2, 120, 9, PX_OFF);
    gfx_text(g, &font_sans_8, 6, 2, s->play == PLAY_PLAYING ? "NOW PLAYING" : "PAUSED", PX_ON);
    theme_draw_battery(g, 110, 3, s->batt_pct, s->charging);

    bevel(g, 0, 15, 128, 30, 8);
    gfx_fill_rect(g, 5, 19, 118, 22, PX_OFF);
    gfx_text_marquee(g, &font_sans_11, 8, 20, 112, s->track.title, (int)(t / 2), PX_ON);
    gfx_text_marquee(g, &font_sans_8, 8, 32, 112, s->track.artist, (int)(t / 3), PX_ON);

    // capsule progress with a chrome fill
    uint8_t pct = s->track.duration_s ? (s->elapsed_s * 100 / s->track.duration_s) : 0;
    bevel(g, 0, 48, 128, 16, 8);
    int fw = (pct * 116 + 50) / 100;
    if (fw > 0) {
        gfx_dither_rect(g, 6, 52, fw, 4, DITHER_75, PX_ON);
        gfx_dither_rect(g, 6, 56, fw, 4, DITHER_25, PX_ON);
        star(g, 6 + fw, 54, 3);
    }
    char a[12]; theme_fmt_time(a, sizeof a, s->elapsed_s);
    // the meter under it is 75%/25% dither, so the readout needs its own plate
    int tw = gfx_text_w(&font_sans_8, a);
    gfx_fill_rect(g, 121 - tw - 3, 51, tw + 5, 10, PX_OFF);
    gfx_rect(g, 121 - tw - 3, 51, tw + 5, 10, PX_ON);
    gfx_text(g, &font_sans_8, 121 - tw, 52, a, PX_ON);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    bevel(g, 0, 0, 128, 12, 5);
    gfx_fill_rect(g, 4, 2, 120, 8, PX_OFF);
    gfx_text(g, &font_sans_8, 6, 1, "MY MUSIC", PX_ON);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS - 1; i++) {
        int y = 14 + i * 10;
        bool sel = (i == s->row_sel);
        if (sel) { bevel(g, 0, y - 1, 128, 10, 4); gfx_fill_rect(g, 3, y, 122, 8, PX_OFF); }
        gfx_text(g, &font_sans_8, 6, y, s->rows[i].name, PX_ON);
        if (sel) star(g, 121, y + 4, 3);
    }
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    bevel(g, 4, 6, 120, 52, 10);
    gfx_fill_rect(g, 9, 11, 110, 42, PX_OFF);
    gfx_text_center(g, &font_sans_11, 64, 12, "CONNECT", PX_ON);
    gfx_text_center(g, &font_sans_8, 64, 26, s->wifi_ssid, PX_ON);
    gfx_text_center(g, &font_sans_11, 64, 36, s->wifi_ip, PX_ON);
    if (s->upload_pct > 0 && s->upload_pct < 100) {
        int fw = (s->upload_pct * 100 + 50) / 100;
        gfx_dither_rect(g, 14, 50, fw, 3, DITHER_75, PX_ON);
    }
    star(g, 12, 12, 3 + (int)((t / 7) % 2));
    star(g, 116, 52, 3 + (int)((t / 7 + 1) % 2));
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    bevel(g, 10, 20, 108, 24, 11);
    gfx_fill_rect(g, 15, 24, 98, 16, PX_OFF);
    char v[10]; snprintf(v, sizeof v, s->muted ? "MUTE" : "%u", s->volume);
    gfx_text(g, &font_sans_11, 18, 26, v, PX_ON);
    int fw = ((s->muted ? 0 : s->volume) * 60 + 50) / 100;
    gfx_dither_rect(g, 48, 30, fw, 4, DITHER_75, PX_ON);
    gfx_dither_rect(g, 48, 34, fw, 4, DITHER_25, PX_ON);
    gfx_rect(g, 48, 30, 60, 8, PX_ON);
}

const theme_t theme_y2k = {
    .id = "y2k", .name = "Y2K Chrome",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0xAF,
};
