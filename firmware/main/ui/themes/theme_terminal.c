// TERMINAL - the device admits it is a computer.
//
// Monospace throughout so columns line up without measuring. An inverted
// prompt bar, key/value rows, an ASCII meter, and a block cursor that blinks
// at 500 ms. A 25% scanline wash sits over everything, which on a real OLED
// reads as phosphor rather than as noise.
#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>
#include <string.h>

static void gutter(gfx_t *g) {
    for (int y = 13; y < GFX_H; y += 2) gfx_px(g, 127, y, PX_ON);
}

static void prompt_bar(gfx_t *g, const char *path) {
    gfx_fill_rect(g, 0, 0, 128, 11, PX_ON);
    char line[32];
    snprintf(line, sizeof line, "poket@sd:%.12s$", path && *path ? path : "/");
    gfx_text(g, &font_mono_8, 2, 2, line, PX_OFF);
}

static void cursor(gfx_t *g, int x, int y, uint32_t t) {
    if ((t / 25) % 2) gfx_fill_rect(g, x, y, 5, 8, PX_ON);
}

// 5px cell + 1px gap, so column 8 lands at x = 2 + 8*6.
static void kv(gfx_t *g, int y, const char *k, const char *v) {
    gfx_text(g, &font_mono_8, 2, y, k, PX_ON);
    gfx_text(g, &font_mono_8, 50, y, v, PX_ON);
}

static void splash(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)s;
    gfx_clear(g);
    const char *boot[] = {
        "poket bootloader", "sdmmc  4-bit  ok", "i2s    48k    ok",
        "codec  pcm5102a", "mount  /sdcard", "ready.",
    };
    int lines = (int)(t / 8);
    if (lines > 6) lines = 6;
    for (int i = 0; i < lines; i++) gfx_text(g, &font_mono_8, 2, 2 + i * 9, boot[i], PX_ON);
    if (lines < 6) cursor(g, 2, 2 + lines * 9, t);
    else cursor(g, 2 + gfx_text_w(&font_mono_8, "ready.") + 2, 2 + 5 * 9, t);
    gutter(g);
}

static void now_playing(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    prompt_bar(g, "play");

    char buf[40];
    gfx_text_marquee(g, &font_mono_12, 2, 14, 124, s->track.title, (int)(t / 2), PX_ON);
    gfx_text_marquee(g, &font_mono_8, 2, 28, 124, s->track.artist, (int)(t / 3), PX_ON);

    char a[12], b[12];
    theme_fmt_time(a, sizeof a, s->elapsed_s);
    theme_fmt_time(b, sizeof b, s->track.duration_s);
    snprintf(buf, sizeof buf, "%s/%s", a, b);
    gfx_text(g, &font_mono_8, 2, 38, buf, PX_ON);
    snprintf(buf, sizeof buf, "%lukbps", (unsigned long)s->track.bitrate_kbps);
    gfx_text(g, &font_mono_8, 126 - gfx_text_w(&font_mono_8, buf), 38, buf, PX_ON);

    // ASCII meter: 16 cells, filled with '#'
    uint8_t pct = s->track.duration_s ? (s->elapsed_s * 100 / s->track.duration_s) : 0;
    char bar[20];
    int n = (pct * 16 + 50) / 100;
    for (int i = 0; i < 16; i++) bar[i] = i < n ? '#' : '-';
    bar[16] = 0;
    gfx_text(g, &font_mono_8, 2, 48, "[", PX_ON);
    gfx_text(g, &font_mono_8, 9, 48, bar, PX_ON);
    gfx_text(g, &font_mono_8, 9 + gfx_text_w(&font_mono_8, bar), 48, "]", PX_ON);

    snprintf(buf, sizeof buf, "%s vol %02u %s",
             s->play == PLAY_PLAYING ? ">" : s->play == PLAY_PAUSED ? "||" : "#",
             s->volume, s->out == OUT_BLUETOOTH ? "bt" : "jack");
    gfx_text(g, &font_mono_8, 2, 56, buf, PX_ON);
    theme_draw_battery(g, 111, 56, s->batt_pct, s->charging);
    gutter(g);
}

static void browser(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    prompt_bar(g, s->browse_path);
    for (int i = 0; i < s->row_count && i < BROWSE_ROWS - 1; i++) {
        int y = 13 + i * 9;
        bool sel = (i == s->row_sel);
        char line[36];
        snprintf(line, sizeof line, "%s%.17s%s", sel ? ">" : " ",
                 s->rows[i].name, s->rows[i].is_dir ? "/" : "");
        if (sel) gfx_fill_rect(g, 0, y - 1, 128, 9, PX_ON);
        gfx_text(g, &font_mono_8, 1, y, line, sel ? PX_OFF : PX_ON);
    }
    char foot[32];
    snprintf(foot, sizeof foot, "%u items", s->browse_total);
    gfx_text(g, &font_mono_8, 2, 56, foot, PX_ON);
    cursor(g, 2 + gfx_text_w(&font_mono_8, foot) + 3, 56, t);
    gutter(g);
}

static void menu(gfx_t *g, const app_state_t *s, uint32_t t) { browser(g, s, t); }

static void transfer(gfx_t *g, const app_state_t *s, uint32_t t) {
    gfx_clear(g);
    prompt_bar(g, "net");
    kv(g, 15, "mode", s->wifi_is_ap ? "ap" : "sta");
    kv(g, 24, "ssid", s->wifi_ssid);
    kv(g, 33, "addr", s->wifi_ip);
    char c[16]; snprintf(c, sizeof c, "%u", s->clients);
    kv(g, 42, "peers", c);
    if (s->upload_pct > 0 && s->upload_pct < 100) {
        char bar[24]; int n = (s->upload_pct * 18 + 50) / 100;
        for (int i = 0; i < 18; i++) bar[i] = i < n ? '#' : '.';
        bar[18] = 0;
        gfx_text(g, &font_mono_8, 2, 54, bar, PX_ON);
    } else {
        gfx_text(g, &font_mono_8, 2, 54, "waiting", PX_ON);
        cursor(g, 2 + gfx_text_w(&font_mono_8, "waiting") + 3, 54, t);
    }
    gutter(g);
}

static void volume_hud(gfx_t *g, const app_state_t *s, uint32_t t) {
    (void)t;
    gfx_fill_rect(g, 8, 22, 112, 20, PX_OFF);
    gfx_rect(g, 8, 22, 112, 20, PX_ON);
    char bar[24];
    int n = ((s->muted ? 0 : s->volume) * 18 + 50) / 100;
    for (int i = 0; i < 18; i++) bar[i] = i < n ? '#' : '-';
    bar[18] = 0;
    char line[32];
    snprintf(line, sizeof line, "vol %02u", s->muted ? 0 : s->volume);
    gfx_text(g, &font_mono_8, 12, 25, line, PX_ON);
    gfx_text(g, &font_mono_8, 12, 34, bar, PX_ON);
}

const theme_t theme_terminal = {
    .id = "terminal", .name = "Terminal",
    .splash = splash, .now_playing = now_playing, .browser = browser,
    .menu = menu, .transfer = transfer, .volume_hud = volume_hud,
    .invert_panel = false, .contrast = 0xCF,
};
