// Poket device web app.
//
// Talks to the firmware over /api/*. When those endpoints aren't there - the
// file opened straight from disk, or a review build - it falls back to the
// demo library so the page is still a working thing to look at rather than a
// wall of dashes.
import { THEMES, render } from '../lib/oled.js';
import { makeClock } from '../lib/clock.js';
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
  bt: { up: false, scanning: false, connected: false, peer: '',
        saved: false, savedName: '', devices: [] },
  btBusy: false,
  // Playback position is kept as "elapsed E at wall-clock T" and interpolated,
  // never counted in animation frames. The old code advanced a second every 25
  // rAF ticks, which at 60 Hz ran the clock 2.4x too fast.
  playlists: [],
  openPl: null,          // playlist being viewed, null = whole library
  plTracks: [],
  query: '',
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

// send() swallows errors because a dropped volume tick does not matter. The
// Bluetooth actions do: "switch to headphones" failing silently is exactly the
// bug this panel exists to avoid.
async function post(path, body) {
  const r = await fetch(path, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  let data = {};
  try { data = await r.json(); } catch (e) {}
  if (!r.ok || data.ok === false) {
    throw new Error(data.error || `HTTP ${r.status}`);
  }
  return data;
}

function btNote(msg, isError) {
  const el = $('#btNote');
  el.textContent = msg;
  el.toggleAttribute('data-err', !!isError);
}

// ---- bluetooth ------------------------------------------------------------
// Poket is the A2DP source, so pairing runs the other way round from a phone:
// it scans, filters to devices that can actually play audio, and connects out.
const DEMO_SINKS = [
  { addr: '38:18:4C:0A:11:92', name: 'WH-1000XM4',      rssi: -47 },
  { addr: 'F4:4E:FD:22:07:3B', name: 'JBL Flip 5',      rssi: -68 },
  { addr: '00:1B:66:31:9A:C4', name: 'Sennheiser HD1',  rssi: -81 },
];

async function btRefresh() {
  if (!state.online) return;
  try {
    Object.assign(state.bt, await api('/api/bt'));
    paintBt();
  } catch (e) { /* device went away; poll() will notice */ }
}

async function btScan() {
  if (state.btBusy) return;
  state.btBusy = true;
  state.bt.devices = [];
  state.bt.scanning = true;
  btNote('Scanning\u2026 make sure your headphones are in pairing mode.');
  paintBt();

  if (!state.online) {                       // demo: show the real flow
    let i = 0;
    const iv = setInterval(() => {
      state.bt.devices.push(DEMO_SINKS[i++]);
      paintBt();
      if (i >= DEMO_SINKS.length) {
        clearInterval(iv);
        state.bt.scanning = false;
        state.btBusy = false;
        btNote(`Found ${DEMO_SINKS.length} device(s). Tap one to link it.`);
        paintBt();
      }
    }, 700);
    return;
  }

  try {
    await post('/api/bt/scan');
  } catch (e) {
    state.bt.scanning = false;
    state.btBusy = false;
    btNote('Could not start the scan: ' + e.message, true);
    paintBt();
    return;
  }
  // the device scans for ~8 s; follow it until it says it has stopped
  const until = Date.now() + 15000;
  const tick = setInterval(async () => {
    await btRefresh();
    if (!state.bt.scanning || Date.now() > until) {
      clearInterval(tick);
      state.btBusy = false;
      btNote(state.bt.devices.length
        ? `Found ${state.bt.devices.length} device(s). Tap one to link it.`
        : 'No headphones found. Put them in pairing mode and scan again.',
        !state.bt.devices.length);
      paintBt();
    }
  }, 1200);
}

async function btConnect(i) {
  const d = state.bt.devices[i];
  if (!d) return;
  btNote(`Linking to ${d.name}\u2026`);
  if (!state.online) {
    setTimeout(() => {
      Object.assign(state.bt, { connected: true, peer: d.name,
                                saved: true, savedName: d.name });
      state.now.out = 'bt';
      btNote(`Linked. Audio now goes to ${d.name}.`);
      paintBt(); paintChrome();
    }, 900);
    return;
  }
  try {
    await post('/api/bt/connect', { index: i });
    await btRefresh();
    btNote(state.bt.connected ? `Linked to ${state.bt.peer}.`
                              : 'Asked to link \u2014 waiting for the headset\u2026');
  } catch (e) {
    btNote('Could not link: ' + e.message, true);
  }
  paintBt();
}

async function btForget() {
  if (state.online) {
    try { await post('/api/bt/forget'); } catch (e) { btNote(e.message, true); }
    await btRefresh();
  } else {
    Object.assign(state.bt, { connected: false, peer: '', saved: false, savedName: '' });
    state.now.out = 'jack';
    paintChrome();
  }
  btNote('Forgotten. Scan again to link a different pair.');
  paintBt();
}

function bars(rssi) {
  // -50 or better is right next to you; -90 is about to drop out
  const n = rssi >= -55 ? 4 : rssi >= -70 ? 3 : rssi >= -82 ? 2 : 1;
  return `<span class="bars" title="${rssi} dBm">` +
    [1, 2, 3, 4].map(k => `<i ${k <= n ? 'data-on' : ''}></i>`).join('') + '</span>';
}

function paintBt() {
  const b = state.bt;
  $('#btState').textContent = b.connected ? 'LINKED'
                            : b.scanning ? 'SCANNING'
                            : b.saved ? 'REMEMBERED' : 'NOT LINKED';
  $('#btLink').hidden = !(b.connected || b.saved);
  if (b.connected || b.saved) {
    $('#btPeer').textContent = b.peer || b.savedName || 'headphones';
    $('#btPeerSub').textContent = b.connected
      ? 'Connected \u2014 audio is going here'
      : 'Remembered \u2014 will reconnect automatically';
    $('#btDisconnect').hidden = !b.connected;
  }
  $('#btScanLbl').textContent = b.scanning ? 'Scanning\u2026' : 'Scan for headphones';
  $('#btScan').disabled = !!b.scanning;

  const list = $('#btList');
  list.innerHTML = b.devices.map((d, i) => `<li>
      <span class="nm">${esc(d.name)}<small>${esc(d.addr || '')}</small></span>
      ${bars(d.rssi ?? -80)}
      <button type="button" data-bt="${i}" ${b.connected && b.peer === d.name ? 'disabled' : ''}>
        ${b.connected && b.peer === d.name ? 'Linked' : 'Link'}</button>
    </li>`).join('')
    + (b.scanning ? '<li class="scanning"><span class="dotpulse"></span>listening for devices\u2026</li>' : '');
}

async function poll() {
  try {
    const s = await api('/api/state');
    const wasPlaying = state.now.state;
    Object.assign(state.now, s.now || {});
    // The device reports once a second; snapping the clock to every reply would
    // make the readout stutter. Only resync on real drift or a state change.
    if (Math.abs(state.now.elapsed - elapsedNow()) > 1.2 || state.now.state !== wasPlaying)
      setClock(state.now.elapsed);
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
// ---- transport clock ------------------------------------------------------
// The arithmetic lives in lib/clock.js so it can be unit-tested without a
// browser; see lib/clock.test.mjs.
const clock = makeClock();
function setClock(elapsed) {
  clock.set(elapsed, state.now.state === 'playing', state.now.duration);
}
function elapsedNow() { return clock.get(); }

// Called every frame: the bar moves smoothly, the readout only when the whole
// second changes, so the text is not repainted 60 times a second.
let lastShownSecond = -1;
function paintClock() {
  const n = state.now;
  const e = elapsedNow();
  const pct = n.duration ? Math.min(100, e / n.duration * 100) : 0;
  $('#trackFill').style.width = pct.toFixed(2) + '%';
  const sec = Math.floor(e);
  if (sec !== lastShownSecond) {
    lastShownSecond = sec;
    $('#npElapsed').textContent = fmt(sec);
    $('#track').setAttribute('aria-valuenow', Math.round(pct));
  }
  // offline the track has to roll over by itself
  if (!state.online && clock.ended()) localStep(1);
}

function paintChrome() {
  const n = state.now, d = state.device;
  $('#npTitle').textContent = n.title || 'Nothing queued';
  $('#npArtist').textContent = n.artist || '';
  $('#npDur').textContent = fmt(n.duration);
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

function visibleTracks() {
  const src = state.openPl ? state.plTracks : state.tracks;
  const q = state.query.trim().toLowerCase();
  if (!q) return src;
  return src.filter(t => (t.title + ' ' + (t.artist || '')).toLowerCase().includes(q));
}

function paintLibrary() {
  const all = state.openPl ? state.plTracks : state.tracks;
  const t = visibleTracks();
  $('#libTitle').textContent = state.openPl ? state.openPl : '/Music';
  $('#libCount').textContent = state.query && t.length !== all.length
    ? `${t.length} of ${all.length}` : (all.length ? `${all.length} items` : '');

  $('#rows').innerHTML = t.length ? t.map((x) => {
    const i = all.indexOf(x);
    return `<tr data-i="${i}" ${x.title === state.now.title ? 'data-playing' : ''}>
    <td class="num">${String(x.n ?? i + 1).padStart(2, '0')}</td>
    <td>${esc(x.title)}<span class="by">${esc(x.artist || '')}</span></td>
    <td class="num">${fmt(x.dur)}</td>
    <td class="num">${x.kbps ? x.kbps + 'k' : ''}</td>
    <td class="num"><span class="rowacts">${state.openPl
        ? `<button data-act="plremove" data-i="${i}" title="Remove from this playlist">&minus;</button>`
        : `<button data-act="plopen" data-i="${i}" title="Add to a playlist">+</button>`}
      <button class="del" data-act="del" data-i="${i}" title="Delete from the card">\u00d7</button>
    </span></td></tr>`;
  }).join('')
    : `<tr><td colspan="5" style="color:var(--ink-3);padding:22px;text-align:center">${
        state.query ? 'Nothing matches &ldquo;' + esc(state.query) + '&rdquo;'
        : state.openPl ? 'This playlist is empty &mdash; add tracks from /Music'
        : 'Card is empty &mdash; drop some MP3s below'}</td></tr>`;

  requestAnimationFrame(() => {
    const w = $('.libwrap');
    w.toggleAttribute('data-more', w.scrollHeight > w.clientHeight + 2);
  });
}

// ---- playlists ------------------------------------------------------------
async function loadPlaylists() {
  if (state.online) {
    try { state.playlists = (await api('/api/playlists')).playlists || []; }
    catch (e) { /* keep what we have */ }
  }
  paintPlaylists();
}

function paintPlaylists() {
  $('#plCount').textContent = state.playlists.length
    ? `${state.playlists.length}` : '';
  const host = $('#plList');
  host.innerHTML = state.playlists.length ? state.playlists.map(p => `
    <li data-pl="${esc(p.name)}" ${state.openPl === p.name ? 'aria-current="true"' : ''}>
      <span class="pn">${esc(p.name)}</span>
      <span class="pc">${p.count} track${p.count === 1 ? '' : 's'}</span>
      <button data-plact="play" data-pl="${esc(p.name)}">Play</button>
      <button data-plact="del"  data-pl="${esc(p.name)}">Delete</button>
    </li>`).join('')
    : '<li class="plEmpty">No playlists yet. Name one above and hit Create.</li>';
}

async function openPlaylist(name) {
  if (state.openPl === name) { state.openPl = null; state.plTracks = []; }
  else {
    state.openPl = name;
    if (state.online) {
      try { state.plTracks = (await api('/api/playlist?name=' + encodeURIComponent(name))).tracks || []; }
      catch (e) { state.plTracks = []; }
    } else {
      const pl = state.playlists.find(p => p.name === name);
      state.plTracks = (pl && pl.tracks) || [];
    }
  }
  paintPlaylists(); paintLibrary();
}

async function plAction(action, body) {
  if (!state.online) return true;                 // demo mode mutates locally
  try { await post('/api/playlist', { action, ...body }); return true; }
  catch (e) { btNote('Playlist: ' + e.message, true); return false; }
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
  if (!state.seeking) {
    state.now.elapsed = elapsedNow();     // the OLED preview draws from this
    paintClock();
  }
  const cs = getComputedStyle(document.documentElement);
  render($('#screen'), state.theme, state.now, state.tick, {
    scale: state.scale,
    on: cs.getPropertyValue('--screen-on').trim() || '#e9f2ec',
    off: cs.getPropertyValue('--screen-off').trim() || '#04110c',
    grid: false,
  });
  requestAnimationFrame(frame);
}

/* ---- transport -------------------------------------------------------- */
function localStep(d) {
  const i = state.tracks.findIndex(t => t.title === state.now.title);
  const t = state.tracks[(i + d + state.tracks.length) % state.tracks.length];
  if (!t) return;
  Object.assign(state.now, { title: t.title, artist: t.artist, duration: t.dur,
                             bitrate: t.kbps, elapsed: 0, pos: t.n });
  setClock(0);
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
  $('[data-toggle=out]').onclick = async () => {
    const want = state.now.out === 'bt' ? 'jack' : 'bt';
    if (!state.online) {
      if (want === 'bt' && !state.bt.connected) {
        btNote('No headphones linked yet \u2014 scan and link a pair first.', true);
        return;
      }
      state.now.out = want; paintChrome(); return;
    }
    try {
      await post('/api/out', { out: want });
      state.now.out = want;
      paintChrome();
    } catch (e) {
      // the firmware refuses with a reason when nothing is linked
      btNote(e.message, true);
    }
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
    setClock(state.now.elapsed);
    paintClock();
    send('/api/transport', { action: 'seek', value: state.now.elapsed });
  };
  track.addEventListener('pointerdown', e => { state.seeking = true; seekTo(e.clientX); });
  addEventListener('pointermove', e => { if (state.seeking) seekTo(e.clientX); });
  addEventListener('pointerup', () => { state.seeking = false; });
  track.addEventListener('keydown', e => {
    const d = e.key === 'ArrowRight' ? 5 : e.key === 'ArrowLeft' ? -5 : 0;
    if (!d) return;
    e.preventDefault();
    state.now.elapsed = Math.max(0, Math.min(state.now.duration, elapsedNow() + d));
    setClock(state.now.elapsed);
    paintClock();
    send('/api/transport', { action: 'seek', value: state.now.elapsed });
  });

  $('#rows').addEventListener('click', async e => {
    const act = e.target.closest('button[data-act]');
    if (act) {
      e.stopPropagation();
      const i = +act.dataset.i;
      const src = state.openPl ? state.plTracks : state.tracks;
      const t = src[i];
      if (!t) return;
      if (act.dataset.act === 'del') {
        if (!confirm(`Delete "${t.title}" from the card? This cannot be undone.`)) return;
        if (state.online) {
          try { await post('/api/delete', { path: t.path }); } catch (err) { btNote(err.message, true); return; }
          await loadLibrary();
        } else {
          state.tracks = state.tracks.filter(x => x !== t);
        }
        paintLibrary();
      } else if (act.dataset.act === 'plopen') {
        if (!state.playlists.length) { btNote('Create a playlist first.', true); return; }
        const name = prompt('Add to which playlist?\n\n' +
                            state.playlists.map(p => '\u2022 ' + p.name).join('\n'),
                            state.playlists[0].name);
        if (!name) return;
        if (await plAction('add', { name, path: t.path || t.title })) {
          const pl = state.playlists.find(p => p.name === name);
          if (pl) { pl.count++; (pl.tracks = pl.tracks || []).push(t); }
          paintPlaylists();
          if (state.openPl === name) openPlaylist(name), openPlaylist(name);
        }
      } else if (act.dataset.act === 'plremove') {
        if (await plAction('removeAt', { name: state.openPl, index: i })) {
          state.plTracks.splice(i, 1);
          const pl = state.playlists.find(p => p.name === state.openPl);
          if (pl) pl.count = state.plTracks.length;
          paintPlaylists(); paintLibrary();
        }
      }
      return;
    }
    const tr = e.target.closest('tr[data-i]');
    if (!tr) return;
    const src = state.openPl ? state.plTracks : state.tracks;
    const t = src[+tr.dataset.i];
    if (!t) return;
    Object.assign(state.now, { title: t.title, artist: t.artist, duration: t.dur,
                               bitrate: t.kbps, elapsed: 0, state: 'playing', pos: t.n });
    setClock(0);
    paintChrome(); paintLibrary();
    if (state.openPl) {
      send('/api/playlist', { action: 'play', name: state.openPl, start: +tr.dataset.i });
    } else {
      send('/api/transport', { action: 'play', path: t.path || t.title });
    }
  });

  // search
  $('#search').addEventListener('input', e => {
    state.query = e.target.value;
    paintLibrary();
  });

  // shuffle / repeat
  $('#bShuffle').onclick = () => {
    state.now.shuffle = !state.now.shuffle;
    $('#bShuffle').setAttribute('aria-pressed', String(state.now.shuffle));
    send('/api/mode', { shuffle: state.now.shuffle });
  };
  const REPEATS = ['off', 'all', 'one'];
  $('#bRepeat').onclick = () => {
    const next = REPEATS[(REPEATS.indexOf(state.now.repeat || 'off') + 1) % 3];
    state.now.repeat = next;
    $('#bRepeat').dataset.mode = next;
    $('#bRepeat').setAttribute('aria-pressed', String(next !== 'off'));
    $('#repeatLbl').textContent = next === 'one' ? 'One' : next === 'all' ? 'All' : 'Off';
    send('/api/mode', { repeat: next });
  };

  // playlists
  $('#plCreate').onclick = async () => {
    const name = $('#plNew').value.trim();
    if (!name) return;
    if (state.playlists.some(p => p.name.toLowerCase() === name.toLowerCase())) {
      btNote('A playlist called that already exists.', true); return;
    }
    if (await plAction('create', { name })) {
      state.playlists.push({ name, count: 0, tracks: [] });
      $('#plNew').value = '';
      paintPlaylists();
    }
  };
  $('#plNew').addEventListener('keydown', e => { if (e.key === 'Enter') $('#plCreate').click(); });
  $('#plList').addEventListener('click', async e => {
    const btn = e.target.closest('button[data-plact]');
    const name = (btn || e.target.closest('li[data-pl]'))?.dataset.pl;
    if (!name) return;
    if (!btn) return openPlaylist(name);
    if (btn.dataset.plact === 'play') {
      if (await plAction('play', { name, start: 0 })) {
        const pl = state.playlists.find(p => p.name === name);
        const first = (pl && pl.tracks && pl.tracks[0]) || null;
        if (first) {
          Object.assign(state.now, { title: first.title, artist: first.artist,
                                     duration: first.dur, elapsed: 0, state: 'playing' });
          setClock(0); paintChrome(); paintLibrary();
        }
        btNote(`Playing "${name}".`);
      }
    } else if (btn.dataset.plact === 'del') {
      if (!confirm(`Delete the playlist "${name}"? The tracks stay on the card.`)) return;
      if (await plAction('delete', { name })) {
        state.playlists = state.playlists.filter(p => p.name !== name);
        if (state.openPl === name) { state.openPl = null; state.plTracks = []; }
        paintPlaylists(); paintLibrary();
      }
    }
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
  $('#btScan').onclick = btScan;
  $('#btForget').onclick = btForget;
  $('#btDisconnect').onclick = async () => {
    if (state.online) { try { await post('/api/bt/forget'); } catch (e) {} await btRefresh(); }
    else { state.bt.connected = false; state.now.out = 'jack'; paintChrome(); }
    paintBt();
  };
  $('#btList').addEventListener('click', e => {
    const btn = e.target.closest('button[data-bt]');
    if (btn) btConnect(+btn.dataset.bt);
  });
  $('#zIn').onclick = () => setZoom(state.scale + 1);
  $('#zOut').onclick = () => setZoom(state.scale - 1);
  addEventListener('resize', () => setZoom(state.wantScale, false));

  setClock(state.now.elapsed);
  paintChrome(); paintLibrary(); paintBt();
  poll(); loadLibrary(); btRefresh(); loadPlaylists();
  setInterval(poll, 1000);
  frame();
}
boot();
