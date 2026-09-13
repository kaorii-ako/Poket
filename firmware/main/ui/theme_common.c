#include "ui/theme.h"
#include "ui/fonts.h"
#include <stdio.h>

void theme_fmt_time(char *out, size_t n, uint32_t s) {
    if (s >= 3600) snprintf(out, n, "%lu:%02lu:%02lu",
                            (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60),
                            (unsigned long)(s % 60));
    else snprintf(out, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

void theme_draw_battery(gfx_t *g, int x, int y, uint8_t pct, bool charging) {
    // 13x7 cell plus a 2px nub. Reads at a glance from arm's length, which is
    // the only distance this screen is ever viewed from.
    gfx_rect(g, x, y, 13, 7, PX_ON);
    gfx_fill_rect(g, x + 13, y + 2, 2, 3, PX_ON);
    int w = (pct * 11 + 50) / 100;
    if (w > 0) gfx_fill_rect(g, x + 1, y + 1, w, 5, PX_ON);
    if (charging) {
        // lightning bolt punched out of the fill so it reads at any level
        const int bx = x + 5, by = y + 1;
        gfx_line(g, bx + 2, by,     bx,     by + 3, PX_XOR);
        gfx_line(g, bx,     by + 3, bx + 2, by + 3, PX_XOR);
        gfx_line(g, bx + 2, by + 3, bx,     by + 5, PX_XOR);
    }
}

void theme_draw_bt(gfx_t *g, int x, int y, bool connected) {
    if (!connected) return;
    const int h = 7;
    gfx_line(g, x + 2, y,     x + 2, y + h - 1, PX_ON);
    gfx_line(g, x + 2, y,     x + 4, y + 2,     PX_ON);
    gfx_line(g, x + 4, y + 2, x,     y + 4,     PX_ON);
    gfx_line(g, x + 2, y + h - 1, x + 4, y + 4, PX_ON);
    gfx_line(g, x + 4, y + 4, x,     y + 2,     PX_ON);
}

void theme_draw_progress(gfx_t *g, int x, int y, int w, int h, uint8_t pct, dither_t fill) {
    int fw = (pct * w + 50) / 100;
    if (fw > w) fw = w;
    if (h <= 1) {
        gfx_hline(g, x, y, w, PX_OFF);
        gfx_hline(g, x, y, fw, PX_ON);
    } else {
        gfx_rect(g, x, y, w, h, PX_ON);
        if (fw > 2) gfx_dither_rect(g, x + 1, y + 1, fw - 2, h - 2, fill, PX_ON);
    }
}
