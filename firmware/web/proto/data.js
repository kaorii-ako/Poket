// Demo data for the prototypes. Real builds read this from /api/*.
export const TRACKS = [
  { n: 1, title: 'Kiara',              artist: 'Bonobo',            dur: 241, kbps: 320, size: 9.6 },
  { n: 2, title: 'Ghost Town',         artist: 'Kaytranada',        dur: 187, kbps: 256, size: 6.0 },
  { n: 3, title: 'Nightcall',          artist: 'Kavinsky',          dur: 257, kbps: 320, size: 10.3 },
  { n: 4, title: 'Teardrop',           artist: 'Massive Attack',    dur: 330, kbps: 320, size: 13.2 },
  { n: 5, title: 'Sunset',             artist: 'The Midnight',      dur: 294, kbps: 256, size: 9.4 },
  { n: 6, title: 'A Real Hero',        artist: 'College',           dur: 264, kbps: 320, size: 10.6 },
  { n: 7, title: 'Innerbloom',         artist: 'RÜFÜS DU SOL',      dur: 597, kbps: 320, size: 23.9 },
  { n: 8, title: 'Gosh',               artist: 'Jamie xx',          dur: 232, kbps: 256, size: 7.4 },
  { n: 9, title: 'Open Eye Signal',    artist: 'Jon Hopkins',       dur: 457, kbps: 320, size: 18.3 },
  { n: 10, title: 'Lost in the World', artist: 'Kanye West',        dur: 254, kbps: 320, size: 10.2 },
];
export const NOW = {
  title: 'Open Eye Signal', artist: 'Jon Hopkins',
  elapsed: 137, duration: 457, pos: 9, total: 10,
  batt: 72, charging: false, volume: 68, out: 'jack',
  bitrate: 320, state: 'playing',
};
export const DEVICE = {
  name: 'Poket', fw: '0.1.0', ssid: 'Poket-A4F2', ip: '192.168.4.1',
  cardTotal: 29.7, cardUsed: 4.1, uptime: '00:12:41', bt: 'not paired',
};
export const fmt = s => `${(s / 60) | 0}:${String((s % 60) | 0).padStart(2, '0')}`;
