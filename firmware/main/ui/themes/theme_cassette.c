// CASSETTE - the reels actually turn.
//
// Both hubs rotate at a rate derived from elapsed time, and the tape pack
// radius shifts between them as the track plays, so the left reel empties
// while the right fills. It is the one piece of motion on this device that
// encodes real state rather than decoration.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>
#include <math.h>

static void reel(gfx_t *g, int cx, int cy, int pack_r, float angle) {
    if (pack_r > 6) gfx_dither_rect(g, cx - pack_r, cy - pack_r,
                                    pack_r * 2, pack_r * 2, DITHER_50, PX_OFF);
    if (pack_r > 6) gfx_circle(g, cx, cy, pack_r, PX_ON);
    gfx_circle(g, cx, cy, 6, PX_ON);
    for (int i = 0; i < 3; i++) {
        float a = angle + (float)i * 2.0944f;      // 120 deg apart
        gfx_line(g, cx, cy, cx + (int)(5.0f * cosf(a)), cy + (int)(5.0f * sinf(a)), PX_ON);
    }
    gfx_px(g, cx, cy, PX_ON);
}

static void shell(gfx_t *g) {
    gfx_round_rect(g, 2, 12, 124, 50, 4, PX_ON);
    gfx_round_rect(g, 8, 18, 112, 24, 2, PX_ON);     // window
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    shell(g);
    float a = (float)t * 0.12f;
    reel(g, 34, 30, 10, a);
    reel(g, 94, 30, 10, -a);
    gfx_fill_rect(g, 44, 44, 40, 12, PX_OFF);
    gfx_text_center(g, &font_round_13, 64, 45, "Poket", PX_ON);
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    shell(g);

    float frac = s->track.duration_s ? (float)s->elapsed_s / (float)s->track.duration_s : 0.f;
    if (frac > 1.f) frac = 1.f;
    // the window is 24px tall, so a pack radius over 11 would draw the reel
    // straight through its own frame
    int lr = 10 - (int)(4.f * frac);       // supply reel shrinks
    int rr = 6 + (int)(4.f * frac);        // take-up reel grows
    float a = (s->play == PLAY_PLAYING) ? (float)t * 0.16f : 0.f;
    reel(g, 34, 30, lr, a);
    reel(g, 94, 30, rr, -a);
    gfx_hline(g, 34, 30 - lr, 60, PX_ON);  // tape path across the window

    // label strip below the window
    gfx_fill_rect(g, 8, 44, 112, 15, PX_OFF);
    gfx_round_rect(g, 8, 44, 112, 15, 2, PX_ON);
    gfx_text_marquee(g, &font_round_9, 12, 46, 104, s->track.title, (int)(t / 3), PX_ON);

    // counter in the top bar, mono so the digits do not jitter
    char c[16];
    snprintf(c, sizeof c, "%04lu", (unsigned long)(s->elapsed_s % 10000));
    gfx_fill_rect(g, 0, 0, 128, 11, PX_OFF);
    gfx_rect(g, 44, 0, 40, 11, PX_ON);
    gfx_text_center(g, &font_mono_8, 64, 2, c, PX_ON);
    gfx_text(g, &font_round_9, 2, 1, s->play == PLAY_PLAYING ? "PLAY"
                                   : s->play == PLAY_PAUSED  ? "PAUSE" : "STOP", PX_ON);
    theme_draw_battery(g, 111, 2, s->batt_pct, s->charging);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_clear(g);
    gfx_fill_rect(g, 0, 0, 128, 11, PX_ON);
    gfx_text(g, &font_round_9, 3, 1, "SIDE A", PX_OFF);
    char n[16]; snprintf(n, sizeof n, "%u", s->browse_total);
    gfx_text(g, &font_round_9, 128 - gfx_text_w(&font_round_9, n) - 3, 1, n, PX_OFF);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS - 1; i++) {
        int y = 14 + i * 10;
        bool sel = (i == s->row_sel);
        if (sel) gfx_fill_rect(g, 0, y - 1, 128, 10, PX_ON);
        char line[40];
        snprintf(line, sizeof line, "%2d  %.18s", i + 1 + s->browse_offset, s->rows[i].name);
        gfx_text(g, &font_round_9, 3, y, line, sel ? PX_OFF : PX_ON);
    }
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    shell(g);
    float a = (float)t * 0.10f;
    reel(g, 34, 30, 11, a);
    reel(g, 94, 30, 11, -a);
    gfx_fill_rect(g, 8, 44, 112, 15, PX_OFF);
    gfx_round_rect(g, 8, 44, 112, 15, 2, PX_ON);
    gfx_text_center(g, &font_round_9, 64, 46, s->wifi_ip, PX_ON);
    gfx_fill_rect(g, 0, 0, 128, 11, PX_OFF);
    gfx_text_center(g, &font_round_9, 64, 1, "DUBBING", PX_ON);
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_fill_rect(g, 14, 22, 100, 20, PX_OFF);
    gfx_round_rect(g, 14, 22, 100, 20, 3, PX_ON);
    char v[10]; snprintf(v, sizeof v, s->muted ? "MUTE" : "VOL %u", s->volume);
    gfx_text_center(g, &font_round_9, 64, 24, v, PX_ON);
    // level as tape-counter ticks
    int n = ((s->muted ? 0 : s->volume) * 20 + 50) / 100;
    for (int i = 0; i < 20; i++)
        gfx_vline(g, 22 + i * 4, 35, i < n ? 5 : 2, PX_ON);
}

const theme_t theme_cassette = {
    .id = "cassette", .name = "Cassette",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0x8F,
};
