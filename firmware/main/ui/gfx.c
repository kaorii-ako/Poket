#include "ui/gfx.h"
#include <string.h>

static inline void set_px(gfx_t *g, int x, int y, px_mode_t m) {
    if ((unsigned)x >= GFX_W || (unsigned)y >= GFX_H) return;
    // A zero right/bottom edge means "unset", so a gfx_t that was only
    // zero-initialised still draws the whole panel.
    int cx1 = g->clip_x1 ? g->clip_x1 : GFX_W;
    int cy1 = g->clip_y1 ? g->clip_y1 : GFX_H;
    if (x < g->clip_x0 || x >= cx1 || y < g->clip_y0 || y >= cy1) return;
    uint8_t *p = &g->buf[(y >> 3) * GFX_W + x];
    uint8_t bit = 1u << (y & 7);
    switch (m) {
        case PX_ON:  *p |= bit;  break;
        case PX_OFF: *p &= ~bit; break;
        case PX_XOR: *p ^= bit;  break;
    }
}

void gfx_px(gfx_t *g, int x, int y, px_mode_t m) { set_px(g, x, y, m); g->dirty = true; }
void gfx_clip(gfx_t *g, int x, int y, int w, int h) {
    g->clip_x0 = x < 0 ? 0 : (int16_t)x;
    g->clip_y0 = y < 0 ? 0 : (int16_t)y;
    g->clip_x1 = (x + w) > GFX_W ? GFX_W : (int16_t)(x + w);
    g->clip_y1 = (y + h) > GFX_H ? GFX_H : (int16_t)(y + h);
}
void gfx_clip_reset(gfx_t *g) {
    g->clip_x0 = 0; g->clip_y0 = 0; g->clip_x1 = GFX_W; g->clip_y1 = GFX_H;
}
void gfx_clear(gfx_t *g) {
    memset(g->buf, 0, GFX_BUF_LEN);
    gfx_clip_reset(g);
    g->dirty = true;
}
void gfx_fill(gfx_t *g, px_mode_t m) {
    memset(g->buf, m == PX_ON ? 0xFF : 0x00, GFX_BUF_LEN);
    g->dirty = true;
}

void gfx_hline(gfx_t *g, int x, int y, int w, px_mode_t m) {
    for (int i = 0; i < w; i++) set_px(g, x + i, y, m);
    g->dirty = true;
}
void gfx_vline(gfx_t *g, int x, int y, int h, px_mode_t m) {
    for (int i = 0; i < h; i++) set_px(g, x, y + i, m);
    g->dirty = true;
}
void gfx_rect(gfx_t *g, int x, int y, int w, int h, px_mode_t m) {
    if (w <= 0 || h <= 0) return;
    gfx_hline(g, x, y, w, m);
    gfx_hline(g, x, y + h - 1, w, m);
    gfx_vline(g, x, y, h, m);
    gfx_vline(g, x + w - 1, y, h, m);
}
void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, px_mode_t m) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) set_px(g, x + i, y + j, m);
    g->dirty = true;
}

void gfx_line(gfx_t *g, int x0, int y0, int x1, int y1, px_mode_t m) {
    int dx = x1 - x0, dy = y1 - y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx;  dy = dy < 0 ? -dy : dy;
    int err = dx - dy;
    for (;;) {
        set_px(g, x0, y0, m);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
    g->dirty = true;
}

void gfx_circle(gfx_t *g, int cx, int cy, int r, px_mode_t m) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        set_px(g, cx + x, cy + y, m); set_px(g, cx + y, cy + x, m);
        set_px(g, cx - y, cy + x, m); set_px(g, cx - x, cy + y, m);
        set_px(g, cx - x, cy - y, m); set_px(g, cx - y, cy - x, m);
        set_px(g, cx + y, cy - x, m); set_px(g, cx + x, cy - y, m);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
    g->dirty = true;
}

void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, px_mode_t m) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) set_px(g, cx + x, cy + y, m);
    g->dirty = true;
}

void gfx_round_rect(gfx_t *g, int x, int y, int w, int h, int r, px_mode_t m) {
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    gfx_hline(g, x + r, y, w - 2 * r, m);
    gfx_hline(g, x + r, y + h - 1, w - 2 * r, m);
    gfx_vline(g, x, y + r, h - 2 * r, m);
    gfx_vline(g, x + w - 1, y + r, h - 2 * r, m);
    int cx = r, cy = 0, err = 1 - r;
    while (cx >= cy) {
        set_px(g, x + w - 1 - r + cx, y + r - cy, m);
        set_px(g, x + w - 1 - r + cy, y + r - cx, m);
        set_px(g, x + r - cy, y + r - cx, m);
        set_px(g, x + r - cx, y + r - cy, m);
        set_px(g, x + r - cx, y + h - 1 - r + cy, m);
        set_px(g, x + r - cy, y + h - 1 - r + cx, m);
        set_px(g, x + w - 1 - r + cy, y + h - 1 - r + cx, m);
        set_px(g, x + w - 1 - r + cx, y + h - 1 - r + cy, m);
        cy++;
        if (err < 0) err += 2 * cy + 1;
        else { cx--; err += 2 * (cy - cx) + 1; }
    }
    g->dirty = true;
}

void gfx_dither_rect(gfx_t *g, int x, int y, int w, int h, dither_t d, px_mode_t m) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j;
            bool on;
            switch (d) {
                case DITHER_25: on = ((px & 1) == 0) && ((py & 1) == 0); break;
                case DITHER_75: on = !(((px & 1) == 1) && ((py & 1) == 1)); break;
                default:        on = ((px + py) & 1) == 0; break;   // 50%
            }
            if (on) set_px(g, px, py, m);
        }
    }
    g->dirty = true;
}

void gfx_bitmap(gfx_t *g, int x, int y, int w, int h, const uint8_t *bmp, px_mode_t m) {
    int stride = (w + 7) / 8;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (bmp[j * stride + (i >> 3)] & (0x80u >> (i & 7)))
                set_px(g, x + i, y + j, m);
    g->dirty = true;
}

static const uint8_t *glyph(const font_t *f, char c, uint8_t *w_out) {
    if ((uint8_t)c < f->first || (uint8_t)c > f->last) c = '?';
    int idx = (uint8_t)c - f->first;
    *w_out = f->width[idx];
    return &f->data[f->offset[idx]];
}

int gfx_text(gfx_t *g, const font_t *f, int x, int y, const char *s, px_mode_t m) {
    int x0 = x;
    for (; *s; s++) {
        uint8_t gw;
        const uint8_t *bm = glyph(f, *s, &gw);
        int stride = (gw + 7) / 8;
        for (int j = 0; j < f->height; j++)
            for (int i = 0; i < gw; i++)
                if (bm[j * stride + (i >> 3)] & (0x80u >> (i & 7)))
                    set_px(g, x + i, y + j, m);
        x += gw + f->spacing;
    }
    g->dirty = true;
    return x - x0;
}

int gfx_text_w(const font_t *f, const char *s) {
    int w = 0;
    for (; *s; s++) {
        uint8_t gw;
        glyph(f, *s, &gw);
        w += gw + f->spacing;
    }
    return w > 0 ? w - f->spacing : 0;
}

int gfx_text_center(gfx_t *g, const font_t *f, int cx, int y, const char *s, px_mode_t m) {
    return gfx_text(g, f, cx - gfx_text_w(f, s) / 2, y, s, m);
}

int gfx_text_marquee(gfx_t *g, const font_t *f, int x, int y, int max_w,
                     const char *s, int phase, px_mode_t m) {
    int w = gfx_text_w(f, s);
    if (w <= max_w) return gfx_text(g, f, x, y, s, m);

    // Scroll two copies so the loop has no gap at the wrap, clipped to the
    // box. Blanking the overspill with rectangles instead used to erase
    // whatever the theme had drawn to the left of the line.
    const int gap = 16;
    int span = w + gap;
    int off = phase % span;
    int cx0 = g->clip_x0, cy0 = g->clip_y0, cx1 = g->clip_x1, cy1 = g->clip_y1;
    gfx_clip(g, x, y, max_w, f->height);
    gfx_text(g, f, x - off, y, s, m);
    gfx_text(g, f, x - off + span, y, s, m);
    g->clip_x0 = (int16_t)cx0; g->clip_y0 = (int16_t)cy0;
    g->clip_x1 = (int16_t)cx1; g->clip_y1 = (int16_t)cy1;
    return max_w;
}
