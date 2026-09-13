// 1-bit framebuffer for the 128x64 SSD1306.
//
// The panel is paged: 8 rows of 128 bytes, each byte a vertical run of 8
// pixels. Everything here writes into that layout directly so a flush is a
// straight memcpy out to I2C with no repacking.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GFX_W 128
#define GFX_H 64
#define GFX_PAGES (GFX_H / 8)
#define GFX_BUF_LEN (GFX_W * GFX_PAGES)

typedef enum { PX_OFF = 0, PX_ON = 1, PX_XOR = 2 } px_mode_t;

typedef struct {
    uint8_t buf[GFX_BUF_LEN];
    bool dirty;
    // Every write is clipped to this box. It exists for the marquee: blanking
    // the overspill with two wide rectangles used to erase whatever the theme
    // had already drawn beside the scrolling line.
    int16_t clip_x0, clip_y0, clip_x1, clip_y1;   // x1/y1 exclusive
} gfx_t;

typedef struct {
    uint8_t  first, last;       // ASCII range covered
    uint8_t  height;            // rows
    uint8_t  spacing;           // px between glyphs
    const uint8_t  *data;       // column-major bitmaps, 1 bit per px
    const uint16_t *offset;     // index into data per glyph
    const uint8_t  *width;      // advance per glyph
} font_t;

void gfx_clear(gfx_t *g);
// Restrict drawing to a box; gfx_clip_reset() opens it back up to the panel.
// gfx_clear() resets it too, so a theme never inherits one, and a gfx_t that
// was only zero-initialised is unclipped.
void gfx_clip(gfx_t *g, int x, int y, int w, int h);
void gfx_clip_reset(gfx_t *g);
void gfx_fill(gfx_t *g, px_mode_t m);
void gfx_px(gfx_t *g, int x, int y, px_mode_t m);
void gfx_hline(gfx_t *g, int x, int y, int w, px_mode_t m);
void gfx_vline(gfx_t *g, int x, int y, int h, px_mode_t m);
void gfx_rect(gfx_t *g, int x, int y, int w, int h, px_mode_t m);
void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, px_mode_t m);
void gfx_round_rect(gfx_t *g, int x, int y, int w, int h, int r, px_mode_t m);
void gfx_circle(gfx_t *g, int cx, int cy, int r, px_mode_t m);
void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, px_mode_t m);
void gfx_line(gfx_t *g, int x0, int y0, int x1, int y1, px_mode_t m);

// 25%, 50% and 75% ordered dithers - the only "greys" a 1-bit panel has.
typedef enum { DITHER_25, DITHER_50, DITHER_75 } dither_t;
void gfx_dither_rect(gfx_t *g, int x, int y, int w, int h, dither_t d, px_mode_t m);

// blit a packed 1-bit bitmap, row-major, MSB first, stride = (w+7)/8
void gfx_bitmap(gfx_t *g, int x, int y, int w, int h, const uint8_t *bmp, px_mode_t m);

int  gfx_text(gfx_t *g, const font_t *f, int x, int y, const char *s, px_mode_t m);
int  gfx_text_w(const font_t *f, const char *s);
int  gfx_text_center(gfx_t *g, const font_t *f, int cx, int y, const char *s, px_mode_t m);
// draws s clipped to max_w, scrolling it horizontally by phase px when it
// overflows - used for long track titles
int  gfx_text_marquee(gfx_t *g, const font_t *f, int x, int y, int max_w,
                      const char *s, int phase, px_mode_t m);
