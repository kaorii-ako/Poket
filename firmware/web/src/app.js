// Poket device web app.
//
// Talks to the firmware over /api/*. When those endpoints aren't there - the
// file opened straight from disk, or a review build - it falls back to the
// demo library so the page is still a working thing to look at rather than a
// wall of dashes.
import { THEMES, render } from '../lib/oled.js';
import { TRACKS as DEMO_TRACKS, NOW as DEMO_NOW, DEVICE as DEMO_DEVICE, fmt } from '../lib/data.js';

const $ = s => document.querySelector(s);
const $$ = s => [...document.querySelectorAll(s)];

// J-Card reseeds its accent from the screen theme; Fab stays copper, because a
// fab drawing that changed ink colour per theme would just look mis-printed.
const THEME_INK = {
  minimal:  '#3a3733',
  anime:    '#c8447e',
  terminal: '#2f7a43',
  cassette: '#b4620b',
  brutalist:'#bb2b1c',
  y2k:      '#3a56c8',
};

const state = {
  online: null,          // null until the first poll answers
  skin: 'fab',
  scale: 4, wantScale: 5,
  tick: 0,
  seeking: false,
  theme: 'minimal',
  now: { ...DEMO_NOW },
  device: { ...DEMO_DEVICE },
  tracks: DEMO_TRACKS.slice(),
};

/* ---- api ------------------------------------------------------------- */
async function api(path, opts) {
  const r = await fetch(path, { cache: 'no-store', ...opts });
  if (!r.ok) throw new Error(`${path} -> ${r.status}`);
  return r.headers.get('content-type')?.includes('json') ? r.json() : r.text();
}
function setOnline(on, why) {
  if (state.online === on) return;
  state.online = on;
  $('#connDot').toggleAttribute('data-off', !on);
  $('#connTxt').textContent = on ? `connected to ${state.device.ip || 'device'}`
                                 : `offline — ${why || 'demo library'}`;
}
// Fire and forget, but never let a dropped request leave the UI lying.
async function send(path, body) {
  if (!state.online) return;
  try {
    await api(path, { method: 'POST', headers: { 'content-type': 'application/json' },
                      body: JSON.stringify(body) });
  } catch (e) { setOnline(false, 'lost the device'); }
}

async function poll() {
  try {
    const s = await api('/api/state');
    Object.assign(state.now, s.now || {});
    Object.assign(state.device, s.device || {});
    if (s.theme && THEMES[s.theme] && s.theme !== state.theme) selectTheme(s.theme, false);
    setOnline(true);
  } catch (e) {
    setOnline(false);
  }
  paintChrome();
}

async function loadLibrary() {
  try {
    const l = await api('/api/library');
    state.tracks = l.tracks || [];
    setOnline(true);
  } catch (e) { /* demo tracks stay */ }
  paintLibrary();
}

/* ---- chrome ----------------------------------------------------------- */
function paintChrome() {
  const n = state.now, d = state.device;
  $('#npTitle').textContent = n.title || 'Nothing queued';
  $('#npArtist').textContent = n.artist || '';
  $('#npElapsed').textContent = fmt(n.elapsed);
  $('#npDur').textContent = fmt(n.duration);
  const pct = n.duration ? Math.min(100, n.elapsed / n.duration * 100) : 0;
  $('#trackFill').style.width = pct + '%';
  $('#track').setAttribute('aria-valuenow', Math.round(pct));
  $('#stateTxt').textContent = (n.state || 'stopped').toUpperCase();
  $('#ppLbl').textContent = n.state === 'playing' ? 'Pause' : 'Play';
  $('[data-toggle=out]').setAttribute('aria-pressed', String(n.out === 'bt'));
  $('#iName').textContent = d.name || 'Poket';
  $('#iIp').textContent = d.ip || '—';
  $('#iSsid').textContent = d.ssid || '—';
  $('#iCard').textContent = d.cardTotal ? `${(+d.cardUsed).toFixed(1)} / ${d.cardTotal} GB` : '—';
  $('#iBatt').textContent = n.batt != null ? `${n.batt}%${n.charging ? ' ⚡' : ''}` : '—';
  $('#iOut').textContent = n.out === 'bt' ? (d.bt || 'bluetooth') : '3.5 mm jack';
  $('#iFw').textContent = d.fw || '—';
  $('#uptime').textContent = d.uptime ? `up ${d.uptime}` : '';
  $('#cardFree').textContent = d.cardTotal
    ? `${(d.cardTotal - d.cardUsed).toFixed(1)} GB free` : '';
  $('#spineTxt').textContent = `${d.ssid || 'Poket'} · ${state.tracks.length} tracks`;
  if (!state.seeking) $('#vol').value = n.volume ?? 60;
  $('#volOut').value = n.volume ?? 60;
}

function paintLibrary() {
  const t = state.tracks;
  $('#libCount').textContent = t.length ? `${t.length} items` : '';
  // a half-visible row at the fold reads as a clipping bug; fade it instead
  requestAnimationFrame(() => {
    const w = $('.libwrap');
    w.toggleAttribute('data-more', w.scrollHeight > w.clientHeight + 2);
  });
  $('#rows').innerHTML = t.length ? t.map((x, i) => `<tr data-i="${i}" ${
      x.title === state.now.title ? 'data-playing' : ''}>
    <td class="num">${String(x.n ?? i + 1).padStart(2, '0')}</td>
    <td>${esc(x.title)}<span class="by">${esc(x.artist || '')}</span></td>
    <td class="num">${fmt(x.dur)}</td>
    <td class="num">${x.kbps ? x.kbps + 'k' : ''}</td>
    <td class="num">${x.size != null ? (+x.size).toFixed(1) + ' MB' : ''}</td></tr>`).join('')
    : `<tr><td colspan="5" style="color:var(--ink-3);padding:22px;text-align:center">
       Card is empty — drop some MP3s below</td></tr>`;
}
const esc = s => String(s ?? '').replace(/[<>&]/g, c => ({ '<': '&lt;', '>': '&gt;', '&': '&amp;' }[c]));

/* ---- skins ------------------------------------------------------------ */
function setSkin(skin, persist = true) {
  state.skin = skin;
  document.documentElement.dataset.skin = skin;
  $$('[data-skin-btn]').forEach(b =>
    b.setAttribute('aria-pressed', String(b.dataset.skinBtn === skin)));
  $('#revTxt').textContent = skin === 'jcard' ? 'C-90' : 'Rev A';
  $('#hdrMeta').textContent = skin === 'jcard'
    ? 'Dolby B · normal bias · recorded at home'
    : '80.00 × 54.00 mm · 4 layer';
  $('#infoTitle').textContent = skin === 'jcard' ? '' : 'Title block';
  reseedAccent();
  if (persist) { try { localStorage.setItem('poket.skin', skin); } catch (e) {} }
}
function reseedAccent() {
  const el = document.documentElement;
  if (state.skin === 'jcard') el.style.setProperty('--accent', THEME_INK[state.theme] || '#b4620b');
  else el.style.removeProperty('--accent');
}

/* ---- themes ----------------------------------------------------------- */
function buildThemes() {
  const host = $('#themes');
  host.innerHTML = '';
  Object.entries(THEMES).forEach(([id, t]) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.dataset.theme = id;
    b.innerHTML = `<span>${t.name}</span><span class="hint">set</span>`;
    b.onclick = () => selectTheme(id, true);
    host.append(b);
  });
  markTheme();
}
function markTheme() {
  $$('#themes button').forEach(b =>
    b.setAttribute('aria-pressed', String(b.dataset.theme === state.theme)));
}
function selectTheme(id, push) {
  state.theme = id;
  markTheme();
  reseedAccent();
  try { localStorage.setItem('poket.theme', id); } catch (e) {}
  if (push) send('/api/theme', { id });
}

/* ---- preview ---------------------------------------------------------- */
// Integer scale, smoothing off: one panel pixel is exactly scale x scale
// device pixels. A fractional zoom would blur the 1px rules the themes are
// built out of, which would make the preview a lie.
function fitScale() {
  // 128 * scale px is the real width of the preview, so the zoom ceiling is
  // whatever the bay can show without the panel overflowing its column.
  const bay = $('.bay');
  const inner = bay.clientWidth - 16;
  return Math.max(2, Math.min(9, Math.floor(inner / 128) || 2));
}
function setZoom(v, persist = true) {
  state.wantScale = Math.max(2, Math.min(9, Math.round(v)));
  state.scale = Math.min(state.wantScale, fitScale());
  $('#zLbl').textContent = state.scale + '×';
  $('#zIn').disabled = state.scale >= fitScale();
  $('#zOut').disabled = state.scale <= 2;
  if (persist) { try { localStorage.setItem('poket.scale', state.wantScale); } catch (e) {} }
}
function frame() {
  state.tick++;
  if (!state.online && state.now.state === 'playing' && !state.seeking && state.tick % 25 === 0)
    state.now.elapsed = (state.now.elapsed + 1) % (state.now.duration || 1);
  const cs = getComputedStyle(document.documentElement);
  render($('#screen'), state.theme, state.now, state.tick, {
    scale: state.scale,
    on: cs.getPropertyValue('--screen-on').trim() || '#e9f2ec',
    off: cs.getPropertyValue('--screen-off').trim() || '#04110c',
    grid: false,
  });
  if (!state.online && state.tick % 25 === 0) paintChrome();
  requestAnimationFrame(frame);
}

/* ---- transport -------------------------------------------------------- */
function localStep(d) {
  const i = state.tracks.findIndex(t => t.title === state.now.title);
  const t = state.tracks[(i + d + state.tracks.length) % state.tracks.length];
  if (!t) return;
  Object.assign(state.now, { title: t.title, artist: t.artist, duration: t.dur,
                             bitrate: t.kbps, elapsed: 0, pos: t.n });
  paintChrome(); paintLibrary();
}
function wireTransport() {
  $('[data-act=play]').onclick = () => {
    state.now.state = state.now.state === 'playing' ? 'paused' : 'playing';
    paintChrome();
    send('/api/transport', { action: state.now.state === 'playing' ? 'play' : 'pause' });
  };
  $('[data-act=next]').onclick = () => { localStep(1); send('/api/transport', { action: 'next' }); };
  $('[data-act=prev]').onclick = () => { localStep(-1); send('/api/transport', { action: 'prev' }); };
  $('[data-toggle=out]').onclick = () => {
    state.now.out = state.now.out === 'bt' ? 'jack' : 'bt';
    paintChrome();
    send('/api/out', { out: state.now.out });
  };

  const vol = $('#vol');
  vol.addEventListener('input', () => {
    state.now.volume = +vol.value; $('#volOut').value = vol.value;
  });
  vol.addEventListener('change', () => send('/api/volume', { value: +vol.value }));

  const track = $('#track');
  const seekTo = clientX => {
    const r = track.getBoundingClientRect();
    const f = Math.max(0, Math.min(1, (clientX - r.left) / r.width));
    state.now.elapsed = Math.round(f * state.now.duration);
    paintChrome();
    send('/api/transport', { action: 'seek', value: state.now.elapsed });
  };
  track.addEventListener('pointerdown', e => { state.seeking = true; seekTo(e.clientX); });
  addEventListener('pointermove', e => { if (state.seeking) seekTo(e.clientX); });
  addEventListener('pointerup', () => { state.seeking = false; });
  track.addEventListener('keydown', e => {
    const d = e.key === 'ArrowRight' ? 5 : e.key === 'ArrowLeft' ? -5 : 0;
    if (!d) return;
    e.preventDefault();
    state.now.elapsed = Math.max(0, Math.min(state.now.duration, state.now.elapsed + d));
    paintChrome();
    send('/api/transport', { action: 'seek', value: state.now.elapsed });
  });

  $('#rows').addEventListener('click', e => {
    const tr = e.target.closest('tr[data-i]');
    if (!tr) return;
    const t = state.tracks[+tr.dataset.i];
    Object.assign(state.now, { title: t.title, artist: t.artist, duration: t.dur,
                               bitrate: t.kbps, elapsed: 0, state: 'playing', pos: t.n });
    paintChrome(); paintLibrary();
    send('/api/transport', { action: 'play', path: t.path || t.title });
  });
}

/* ---- upload ----------------------------------------------------------- */
function wireUpload() {
  const drop = $('#drop'), picker = $('#picker');
  ['dragenter', 'dragover'].forEach(ev =>
    drop.addEventListener(ev, e => { e.preventDefault(); drop.setAttribute('data-over', ''); }));
  ['dragleave', 'drop'].forEach(ev =>
    drop.addEventListener(ev, e => { e.preventDefault(); drop.removeAttribute('data-over'); }));
  drop.addEventListener('drop', e => queue([...(e.dataTransfer?.files || [])]));
  drop.addEventListener('click', () => picker.click());
  picker.addEventListener('change', () => { queue([...picker.files]); picker.value = ''; });
}
function queue(files) {
  for (const f of files) {
    if (!/\.mp3$/i.test(f.name)) { job(f.name, 0).fail('not an MP3'); continue; }
    upload(f);
  }
}
function job(name, size) {
  const el = document.createElement('div');
  el.className = 'job';
  el.innerHTML = `<span>${esc(name)}</span><span class="pc">0%</span><span class="bar"><i></i></span>`;
  $('#jobs').prepend(el);
  const pc = el.querySelector('.pc'), bar = el.querySelector('.bar i');
  return {
    set(p) { pc.textContent = Math.round(p) + '%'; bar.style.width = p + '%'; },
    done() { pc.textContent = 'written'; bar.style.width = '100%';
             setTimeout(() => el.remove(), 2200); loadLibrary(); },
    fail(msg) { el.setAttribute('data-err', ''); pc.textContent = msg; },
  };
}
function upload(file) {
  const j = job(file.name, file.size);
  if (!state.online) {            // demo mode: show the flow, touch nothing
    let p = 0;
    const iv = setInterval(() => {
      p = Math.min(100, p + 9); j.set(p);
      if (p >= 100) { clearInterval(iv); j.done(); }
    }, 130);
    return;
  }
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/upload?name=' + encodeURIComponent(file.name));
  xhr.upload.onprogress = e => { if (e.lengthComputable) j.set(e.loaded / e.total * 100); };
  xhr.onload = () => (xhr.status < 300 ? j.done() : j.fail('HTTP ' + xhr.status));
  xhr.onerror = () => j.fail('upload failed');
  xhr.setRequestHeader('content-type', 'application/octet-stream');
  xhr.send(file);
}

/* ---- boot ------------------------------------------------------------- */
function boot() {
  let saved = {};
  try {
    saved = { skin: localStorage.getItem('poket.skin'),
              theme: localStorage.getItem('poket.theme'),
              scale: +localStorage.getItem('poket.scale') };
  } catch (e) {}
  if (saved.theme && THEMES[saved.theme]) state.theme = saved.theme;
  const q = new URLSearchParams(location.search);
  if (q.get('theme') && THEMES[q.get('theme')]) state.theme = q.get('theme');
  const skin = q.get('skin') || saved.skin;
  setSkin(skin === 'jcard' ? 'jcard' : 'fab', !q.has('skin'));
  setZoom(saved.scale || 5, false);

  buildThemes();
  wireTransport();
  wireUpload();
  $$('[data-skin-btn]').forEach(b => b.onclick = () => setSkin(b.dataset.skinBtn));
  $('#zIn').onclick = () => setZoom(state.scale + 1);
  $('#zOut').onclick = () => setZoom(state.scale - 1);
  addEventListener('resize', () => setZoom(state.wantScale, false));

  paintChrome(); paintLibrary();
  poll(); loadLibrary();
  setInterval(poll, 1000);
  frame();
}
boot();
