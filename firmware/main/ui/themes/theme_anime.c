// ANIME - soft, round, and it looks back at you.
//
// The trick on a 1-bit panel is that "cute" comes from curves and motion, not
// detail. So: rounded type (Cantarell), a 25% dither wash instead of hard
// fills, a chibi face drawn from curves that blinks on its own timer and
// bobs with the audio level, and sparkles that drift. Nothing here is square except the panel.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>
#include <string.h>

// The face is drawn, not stored: a 24x24 blob was 72 bytes that only ever
// rendered one expression, and the blink variant differed by two rows. Curves
// from gfx primitives cost less and can be re-posed.
static void face(gfx_t *g, int x, int y, bool blink) {
    gfx_circle(g, x + 12, y + 12, 11, PX_ON);          // head
    gfx_line(g, x + 6, y + 4, x + 12, y + 1, PX_ON);   // hair swoosh
    gfx_line(g, x + 12, y + 1, x + 18, y + 4, PX_ON);
    if (blink) {
        gfx_hline(g, x + 6, y + 12, 4, PX_ON);
        gfx_hline(g, x + 15, y + 12, 4, PX_ON);
    } else {
        gfx_fill_rect(g, x + 6, y + 10, 3, 5, PX_ON);  // eyes
        gfx_fill_rect(g, x + 16, y + 10, 3, 5, PX_ON);
        gfx_px(g, x + 8, y + 11, PX_OFF);              // catchlight
        gfx_px(g, x + 18, y + 11, PX_OFF);
    }
    gfx_px(g, x + 3, y + 16, PX_ON); gfx_px(g, x + 5, y + 17, PX_ON);   // blush
    gfx_px(g, x + 19, y + 17, PX_ON); gfx_px(g, x + 21, y + 16, PX_ON);
    gfx_line(g, x + 10, y + 17, x + 12, y + 19, PX_ON);                 // smile
    gfx_line(g, x + 12, y + 19, x + 14, y + 17, PX_ON);
}

static void sparkle(gfx_t *g, int x, int y, int size) {
    gfx_px(g, x, y, PX_ON);
    if (size > 0) {
        gfx_px(g, x - 1, y, PX_ON); gfx_px(g, x + 1, y, PX_ON);
        gfx_px(g, x, y - 1, PX_ON); gfx_px(g, x, y + 1, PX_ON);
    }
    if (size > 1) {
        gfx_px(g, x - 2, y, PX_ON); gfx_px(g, x + 2, y, PX_ON);
        gfx_px(g, x, y - 2, PX_ON); gfx_px(g, x, y + 2, PX_ON);
    }
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    gfx_dither_rect(g, 0, 0, 128, 64, DITHER_25, PX_ON);
    int bob = ((t / 6) % 4 < 2) ? 0 : 1;
    gfx_fill_rect(g, 50, 12 + bob, 28, 28, PX_OFF);
    face(g, 52, 14 + bob, (t / 40) % 5 == 0);
    gfx_fill_rect(g, 30, 44, 68, 14, PX_OFF);
    gfx_text_center(g, &font_round_13, 64, 45, "Poket", PX_ON);
    sparkle(g, 26, 18, (t / 5) % 3);
    sparkle(g, 104, 30, (t / 5 + 1) % 3);
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    gfx_dither_rect(g, 0, 0, 128, 64, DITHER_25, PX_ON);

    // face bobs a little with the level so it feels alive while music plays
    int bob = (s->play == PLAY_PLAYING) ? (s->vu_left > 8 ? 1 : 0) : 0;
    bool blink = ((t / 35) % 6) == 0;
    gfx_fill_rect(g, 2, 16 + bob, 28, 28, PX_OFF);
    face(g, 4, 18 + bob, blink);

    // title sits on a cleared rounded plate so it stays legible over the wash
    gfx_fill_rect(g, 33, 14, 95, 32, PX_OFF);
    gfx_round_rect(g, 33, 14, 95, 32, 6, PX_ON);
    gfx_text_marquee(g, &font_round_13, 38, 18, 85, s->track.title, (int)(t / 3), PX_ON);
    gfx_text_marquee(g, &font_round_9, 38, 33, 85, s->track.artist, (int)(t / 4), PX_ON);

    gfx_fill_rect(g, 0, 0, 128, 12, PX_OFF);
    gfx_text(g, &font_round_9, 2, 1, s->play == PLAY_PLAYING ? "> now playing"
                                  : s->play == PLAY_PAUSED  ? "|| paused" : "stopped", PX_ON);
    theme_draw_battery(g, 111, 2, s->batt_pct, s->charging);

    uint8_t pct = s->track.duration_s ? (s->elapsed_s * 100 / s->track.duration_s) : 0;
    gfx_fill_rect(g, 0, 50, 128, 14, PX_OFF);
    gfx_round_rect(g, 4, 52, 120, 9, 4, PX_ON);
    int fw = (pct * 116 + 50) / 100;
    if (fw > 2) gfx_fill_rect(g, 6, 54, fw - 2 > 116 ? 116 : fw - 2, 5, PX_ON);
    if (s->play == PLAY_PLAYING) sparkle(g, 6 + (fw > 4 ? fw - 4 : 0), 57, (t / 4) % 3);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    gfx_dither_rect(g, 0, 0, 128, 64, DITHER_25, PX_ON);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS; i++) {
        int y = i * 10 + 2;
        bool sel = (i == s->row_sel);
        gfx_fill_rect(g, 2, y - 1, 124, 10, PX_OFF);
        if (sel) {
            gfx_round_rect(g, 2, y - 1, 124, 10, 4, PX_ON);
            sparkle(g, 120, y + 4, (t / 6) % 3);
        }
        gfx_text(g, &font_round_9, 8, y, s->rows[i].name, PX_ON);
    }
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    gfx_dither_rect(g, 0, 0, 128, 64, DITHER_25, PX_ON);
    gfx_fill_rect(g, 6, 6, 116, 52, PX_OFF);
    gfx_round_rect(g, 6, 6, 116, 52, 8, PX_ON);
    gfx_text_center(g, &font_round_13, 64, 10, "send me songs", PX_ON);
    gfx_text_center(g, &font_round_9, 64, 26, s->wifi_ssid, PX_ON);
    gfx_text_center(g, &font_round_13, 64, 37, s->wifi_ip, PX_ON);
    if (s->upload_pct > 0 && s->upload_pct < 100) {
        gfx_round_rect(g, 14, 52, 100, 7, 3, PX_ON);
        int fw = (s->upload_pct * 96 + 50) / 100;
        if (fw > 2) gfx_fill_rect(g, 16, 54, fw - 2, 3, PX_ON);
    }
    sparkle(g, 16, 14, (t / 5) % 3);
    sparkle(g, 112, 50, (t / 5 + 2) % 3);
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_fill_rect(g, 12, 20, 104, 24, PX_OFF);
    gfx_round_rect(g, 12, 20, 104, 24, 10, PX_ON);
    char v[10];
    snprintf(v, sizeof v, s->muted ? "shh" : "%u", s->volume);
    gfx_text_center(g, &font_round_9, 64, 22, v, PX_ON);
    gfx_round_rect(g, 22, 33, 84, 7, 3, PX_ON);
    int fw = ((s->muted ? 0 : s->volume) * 80 + 50) / 100;
    if (fw > 2) gfx_fill_rect(g, 24, 35, fw - 2, 3, PX_ON);
}

const theme_t theme_anime = {
    .id = "anime", .name = "Anime",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0x9F,
};
