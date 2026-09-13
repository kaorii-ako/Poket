// A 128x64 1-bit framebuffer and the six theme renderers, ported from the
// firmware's ui/gfx.c and ui/themes/*.c so the browser preview is not an
// artist's impression of the panel - it draws the same glyph bytes through the
// same primitives and lands on the same pixels.
import * as F from './fonts.js';

export const W = 128, H = 64;

export class Gfx {
  constructor() { this.b = new Uint8Array(W * H); this.clipReset(); }  // 1 byte per px, 0/1
  clear() { this.b.fill(0); this.clipReset(); }
  // Every write is clipped to this box (see gfx_clip in ui/gfx.c). The marquee
  // needs it: blanking its overspill with rectangles erased whatever the theme
  // had drawn beside the scrolling line.
  clip(x, y, w, h) {
    this.cx0 = Math.max(0, x); this.cy0 = Math.max(0, y);
    this.cx1 = Math.min(W, x + w); this.cy1 = Math.min(H, y + h);
  }
  clipReset() { this.cx0 = 0; this.cy0 = 0; this.cx1 = W; this.cy1 = H; }
  px(x, y, m = 1) {
    x |= 0; y |= 0;
    if (x < this.cx0 || x >= this.cx1 || y < this.cy0 || y >= this.cy1) return;
    const i = y * W + x;
    this.b[i] = m === 2 ? (this.b[i] ^ 1) : m;
  }
  hline(x, y, w, m = 1) { for (let i = 0; i < w; i++) this.px(x + i, y, m); }
  vline(x, y, h, m = 1) { for (let i = 0; i < h; i++) this.px(x, y + i, m); }
  rect(x, y, w, h, m = 1) {
    this.hline(x, y, w, m); this.hline(x, y + h - 1, w, m);
    this.vline(x, y, h, m); this.vline(x + w - 1, y, h, m);
  }
  fillRect(x, y, w, h, m = 1) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.px(x + i, y + j, m);
  }
  line(x0, y0, x1, y1, m = 1) {
    x0 |= 0; y0 |= 0; x1 |= 0; y1 |= 0;
    let dx = Math.abs(x1 - x0), dy = Math.abs(y1 - y0);
    const sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    let err = dx - dy;
    for (;;) {
      this.px(x0, y0, m);
      if (x0 === x1 && y0 === y1) break;
      const e2 = err << 1;
      if (e2 > -dy) { err -= dy; x0 += sx; }
      if (e2 < dx) { err += dx; y0 += sy; }
    }
  }
  circle(cx, cy, r, m = 1) {
    let x = r, y = 0, err = 1 - r;
    while (x >= y) {
      for (const [a, b] of [[x, y], [y, x], [-y, x], [-x, y], [-x, -y], [-y, -x], [y, -x], [x, -y]])
        this.px(cx + a, cy + b, m);
      y++;
      if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
  }
  roundRect(x, y, w, h, r, m = 1) {
    if (r * 2 > w) r = w >> 1;
    if (r * 2 > h) r = h >> 1;
    this.hline(x + r, y, w - 2 * r, m); this.hline(x + r, y + h - 1, w - 2 * r, m);
    this.vline(x, y + r, h - 2 * r, m); this.vline(x + w - 1, y + r, h - 2 * r, m);
    let cx = r, cy = 0, err = 1 - r;
    while (cx >= cy) {
      this.px(x + w - 1 - r + cx, y + r - cy, m); this.px(x + w - 1 - r + cy, y + r - cx, m);
      this.px(x + r - cy, y + r - cx, m);         this.px(x + r - cx, y + r - cy, m);
      this.px(x + r - cx, y + h - r + cy - 1, m); this.px(x + r - cy, y + h - r + cx - 1, m);
      this.px(x + w - 1 - r + cy, y + h - r + cx - 1, m);
      this.px(x + w - 1 - r + cx, y + h - r + cy - 1, m);
      cy++;
      if (err < 0) err += 2 * cy + 1; else { cx--; err += 2 * (cy - cx) + 1; }
    }
  }
  // the only "greys" a 1-bit panel has
  dither(x, y, w, h, kind = 50, m = 1) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) {
      const px = x + i, py = y + j;
      let on;
      if (kind === 25) on = (px & 1) === 0 && (py & 1) === 0;
      else if (kind === 75) on = !((px & 1) === 1 && (py & 1) === 1);
      else on = ((px + py) & 1) === 0;
      if (on) this.px(px, py, m);
    }
  }
  bitmap(x, y, w, h, bytes, m = 1) {
    const stride = (w + 7) >> 3;
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++)
      if (bytes[j * stride + (i >> 3)] & (0x80 >> (i & 7))) this.px(x + i, y + j, m);
  }
  text(f, x, y, s, m = 1) {
    const x0 = x;
    for (const ch of String(s)) {
      let code = ch.charCodeAt(0);
      if (!f.g[code]) code = 63;                       // '?'
      const [gw, bytes] = f.g[code];
      const stride = (gw + 7) >> 3;
      for (let j = 0; j < f.h; j++) for (let i = 0; i < gw; i++)
        if (bytes[j * stride + (i >> 3)] & (0x80 >> (i & 7))) this.px(x + i, y + j, m);
      x += gw + f.sp;
    }
    return x - x0;
  }
  textW(f, s) {
    let w = 0;
    for (const ch of String(s)) {
      let code = ch.charCodeAt(0);
      if (!f.g[code]) code = 63;
      w += f.g[code][0] + f.sp;
    }
    return w > 0 ? w - f.sp : 0;
  }
  textCenter(f, cx, y, s, m = 1) { return this.text(f, cx - (this.textW(f, s) >> 1), y, s, m); }
  marquee(f, x, y, maxW, s, phase, m = 1) {
    const w = this.textW(f, s);
    if (w <= maxW) return this.text(f, x, y, s, m);
    const span = w + 16, off = phase % span;
    const c = [this.cx0, this.cy0, this.cx1, this.cy1];
    this.clip(x, y, maxW, f.h);
    this.text(f, x - off, y, s, m);
    this.text(f, x - off + span, y, s, m);
    [this.cx0, this.cy0, this.cx1, this.cy1] = c;
    return maxW;
  }
}

const fmtTime = s => {
  s = Math.max(0, s | 0);
  const m = (s / 60) | 0;
  return `${m}:${String(s % 60).padStart(2, '0')}`;
};

function battery(g, x, y, pct, charging) {
  g.rect(x, y, 13, 7); g.fillRect(x + 13, y + 2, 2, 3);
  const w = Math.round(pct * 11 / 100);
  if (w > 0) g.fillRect(x + 1, y + 1, w, 5);
  if (charging) {
    const bx = x + 5, by = y + 1;
    g.line(bx + 2, by, bx, by + 3, 2); g.line(bx, by + 3, bx + 2, by + 3, 2);
    g.line(bx + 2, by + 3, bx, by + 5, 2);
  }
}

// drawn, not stored - mirrors face() in themes/theme_anime.c
function face(g, x, y, blink) {
  g.circle(x + 12, y + 12, 11);
  g.line(x + 6, y + 4, x + 12, y + 1);
  g.line(x + 12, y + 1, x + 18, y + 4);
  if (blink) { g.hline(x + 6, y + 12, 4); g.hline(x + 15, y + 12, 4); }
  else {
    g.fillRect(x + 6, y + 10, 3, 5); g.fillRect(x + 16, y + 10, 3, 5);
    g.px(x + 8, y + 11, 0); g.px(x + 18, y + 11, 0);
  }
  g.px(x + 3, y + 16); g.px(x + 5, y + 17);
  g.px(x + 19, y + 17); g.px(x + 21, y + 16);
  g.line(x + 10, y + 17, x + 12, y + 19);
  g.line(x + 12, y + 19, x + 14, y + 17);
}

function sparkle(g, x, y, size) {
  g.px(x, y);
  if (size > 0) { g.px(x-1,y); g.px(x+1,y); g.px(x,y-1); g.px(x,y+1); }
  if (size > 1) { g.px(x-2,y); g.px(x+2,y); g.px(x,y-2); g.px(x,y+2); }
}

// ---- the six themes, now-playing screen ---------------------------------
export const THEMES = {
  minimal: { name: 'Minimal', draw(g, s, t) {
    g.text(F.font_sans_8, 0, 0, `${s.pos}/${s.total}`);
    battery(g, 111, 0, s.batt, s.charging);
    g.marquee(F.font_sans_16, 0, 16, 128, s.title, (t / 2) | 0);
    g.text(F.font_sans_8, 0, 35, s.artist);
    g.text(F.font_sans_8, 0, 50, fmtTime(s.elapsed));
    const d = fmtTime(s.duration);
    g.text(F.font_sans_8, 128 - g.textW(F.font_sans_8, d), 50, d);
    if (s.state === 'paused') g.textCenter(F.font_sans_8, 64, 50, 'paused');
    g.hline(0, 63, Math.round(s.elapsed / s.duration * 128));
  }},

  anime: { name: 'Anime', draw(g, s, t) {
    g.dither(0, 0, 128, 64, 25);
    const bob = s.state === 'playing' && ((t / 18) | 0) % 2 ? 1 : 0;
    const blink = ((t / 35) | 0) % 6 === 0;
    g.fillRect(2, 16 + bob, 28, 28, 0);
    face(g, 4, 18 + bob, blink);
    g.fillRect(33, 14, 95, 32, 0); g.roundRect(33, 14, 95, 32, 6);
    g.marquee(F.font_round_13, 38, 18, 85, s.title, (t / 3) | 0);
    g.marquee(F.font_round_9, 38, 33, 85, s.artist, (t / 4) | 0);
    g.fillRect(0, 0, 128, 12, 0);
    g.text(F.font_round_9, 2, 1, s.state === 'playing' ? '> now playing'
                              : s.state === 'paused' ? '|| paused' : 'stopped');
    battery(g, 111, 2, s.batt, s.charging);
    g.fillRect(0, 50, 128, 14, 0); g.roundRect(4, 52, 120, 9, 4);
    const fw = Math.round(s.elapsed / s.duration * 116);
    if (fw > 2) g.fillRect(6, 54, Math.min(fw - 2, 116), 5);
    if (s.state === 'playing') sparkle(g, 6 + Math.max(fw - 4, 0), 57, ((t / 4) | 0) % 3);
  }},

  terminal: { name: 'Terminal', draw(g, s, t) {
    g.fillRect(0, 0, 128, 11); g.text(F.font_mono_8, 2, 2, 'poket@sd:play$', 0);
    g.marquee(F.font_mono_12, 2, 14, 124, s.title, (t / 2) | 0);
    g.marquee(F.font_mono_8, 2, 28, 124, s.artist, (t / 3) | 0);
    g.text(F.font_mono_8, 2, 38, `${fmtTime(s.elapsed)}/${fmtTime(s.duration)}`);
    const kb = `${s.bitrate}kbps`;
    g.text(F.font_mono_8, 126 - g.textW(F.font_mono_8, kb), 38, kb);
    const n = Math.round(s.elapsed / s.duration * 16);
    const bar = '#'.repeat(n) + '-'.repeat(16 - n);
    g.text(F.font_mono_8, 2, 48, '[');
    g.text(F.font_mono_8, 9, 48, bar);
    g.text(F.font_mono_8, 9 + g.textW(F.font_mono_8, bar), 48, ']');
    g.text(F.font_mono_8, 2, 56,
      `${s.state === 'playing' ? '>' : s.state === 'paused' ? '||' : '#'} vol ${String(s.volume).padStart(2, '0')} ${s.out}`);
    battery(g, 111, 56, s.batt, s.charging);
    // no scanline wash - inverting whole rows on a 1-bit panel eats the text;
    // a dotted gutter carries the terminal feel instead (matches theme_terminal.c)
    for (let y = 13; y < 64; y += 2) g.px(127, y);
  }},

  cassette: { name: 'Cassette', draw(g, s, t) {
    g.roundRect(2, 12, 124, 50, 4); g.roundRect(8, 18, 112, 24, 2);
    const frac = Math.min(s.elapsed / s.duration, 1);
    // the tape window is 24px tall - a pack radius over 11 draws through its frame
    const lr = 10 - Math.round(4 * frac), rr = 6 + Math.round(4 * frac);
    const a = s.state === 'playing' ? t * 0.16 : 0;
    const reel = (cx, cy, pr, ang) => {
      if (pr > 6) { g.dither(cx - pr, cy - pr, pr * 2, pr * 2, 50, 0); g.circle(cx, cy, pr); }
      g.circle(cx, cy, 6);
      for (let i = 0; i < 3; i++) {
        const th = ang + i * 2.0944;
        g.line(cx, cy, cx + 5 * Math.cos(th), cy + 5 * Math.sin(th));
      }
      g.px(cx, cy);
    };
    reel(34, 30, lr, a); reel(94, 30, rr, -a);
    g.hline(34, 30 - lr, 60);
    g.fillRect(8, 44, 112, 15, 0); g.roundRect(8, 44, 112, 15, 2);
    g.marquee(F.font_round_9, 12, 46, 104, s.title, (t / 3) | 0);
    g.fillRect(0, 0, 128, 11, 0); g.rect(44, 0, 40, 11);
    g.textCenter(F.font_mono_8, 64, 2, String(s.elapsed % 10000).padStart(4, '0'));
    g.text(F.font_round_9, 2, 1, s.state === 'playing' ? 'PLAY' : s.state === 'paused' ? 'PAUSE' : 'STOP');
    battery(g, 111, 2, s.batt, s.charging);
  }},

  brutalist: { name: 'Brutalist', draw(g, s, t) {
    const caps = x => String(x).toUpperCase();
    g.text(F.font_slab_22, 2, 0, fmtTime(s.elapsed));
    g.text(F.font_bold_10, 2, 24, caps(fmtTime(s.duration)));
    const st = s.state === 'playing' ? 'PLAY' : s.state === 'paused' ? 'HOLD' : 'STOP';
    const w = g.textW(F.font_bold_10, st) + 8;
    g.fillRect(128 - w, 0, w, 14);
    g.text(F.font_bold_10, 128 - w + 4, 2, st, 0);
    battery(g, 113, 18, s.batt, s.charging);
    g.fillRect(0, 36, 128, 3);
    g.marquee(F.font_bold_10, 2, 41, 124, caps(s.title), (t / 2) | 0);
    g.marquee(F.font_bold_10, 2, 52, 124, caps(s.artist), (t / 3) | 0);
    g.fillRect(0, 62, Math.round(s.elapsed / s.duration * 128), 2);
  }},

  y2k: { name: 'Y2K Chrome', draw(g, s, t) {
    const bevel = (x, y, w, h, r) => {
      g.fillRect(x, y, w, h, 0); g.roundRect(x, y, w, h, r);
      g.roundRect(x + 2, y + 2, w - 4, h - 4, Math.max(r - 2, 1));
      g.dither(x + 3, y + 3, w - 6, (h - 6) >> 1, 25);
      g.dither(x + 3, y + 3 + ((h - 6) >> 1), w - 6, (h - 6) >> 1, 75);
    };
    const star = (x, y, r) => {
      g.line(x - r, y, x + r, y); g.line(x, y - r, x, y + r);
      const d = (r * 2 / 3) | 0;
      g.line(x - d, y - d, x + d, y + d); g.line(x - d, y + d, x + d, y - d);
    };
    bevel(0, 0, 128, 13, 5); g.fillRect(4, 2, 120, 9, 0);
    g.text(F.font_sans_8, 6, 2, s.state === 'playing' ? 'NOW PLAYING' : 'PAUSED');
    battery(g, 110, 3, s.batt, s.charging);
    bevel(0, 15, 128, 30, 8); g.fillRect(5, 19, 118, 22, 0);
    g.marquee(F.font_sans_11, 8, 20, 112, s.title, (t / 2) | 0);
    g.marquee(F.font_sans_8, 8, 32, 112, s.artist, (t / 3) | 0);
    bevel(0, 48, 128, 16, 8);
    const fw = Math.round(s.elapsed / s.duration * 116);
    if (fw > 0) {
      g.dither(6, 52, fw, 4, 75); g.dither(6, 56, fw, 4, 25);
      star(6 + fw, 54, 3);
    }
    // the meter under it is dithered, so the readout gets its own plate
    const et = fmtTime(s.elapsed), tw = g.textW(F.font_sans_8, et);
    g.fillRect(121 - tw - 3, 51, tw + 5, 10, 0);
    g.rect(121 - tw - 3, 51, tw + 5, 10);
    g.text(F.font_sans_8, 121 - tw, 52, et);
  }},
};

// ---- canvas presentation ------------------------------------------------
// Scale is an INTEGER and smoothing is off, so one panel pixel is exactly
// scale x scale device pixels with no resampling. A fractional scale would
// blur the 1px rules that half these themes are built out of.
export function paint(canvas, gfx, opts = {}) {
  const scale = Math.max(1, Math.round(opts.scale || 4));
  const on = opts.on || '#e8f4ff';
  const off = opts.off || '#0b0f14';
  const grid = opts.grid !== false && scale >= 5;
  canvas.width = W * scale;
  canvas.height = H * scale;
  canvas.style.width = (W * scale) + 'px';
  canvas.style.height = (H * scale) + 'px';
  const c = canvas.getContext('2d');
  c.imageSmoothingEnabled = false;
  c.fillStyle = off;
  c.fillRect(0, 0, canvas.width, canvas.height);
  c.fillStyle = on;
  const inset = grid ? 1 : 0;
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++)
      if (gfx.b[y * W + x])
        c.fillRect(x * scale, y * scale, scale - inset, scale - inset);
}

export function render(canvas, themeId, state, tick, opts) {
  const th = THEMES[themeId] || THEMES.minimal;
  const g = new Gfx();
  g.clear();
  th.draw(g, state, tick);
  paint(canvas, g, opts);
  return g;
}
