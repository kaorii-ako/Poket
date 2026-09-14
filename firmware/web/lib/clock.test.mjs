import { makeClock } from './clock.js';
let t = 0;                          // a fake wall clock, in ms
const c = makeClock(() => t);
let fails = 0;
const ok = (name, cond, got) => {
  if (!cond) { fails++; console.log(`FAIL ${name}  got ${got}`); }
  else console.log(`ok   ${name}${got !== undefined ? '  -> ' + got : ''}`);
};

c.set(0, true, 457);
t += 10_000;
ok('10 s of wall time advances the clock 10 s', Math.abs(c.get() - 10) < 1e-9, c.get().toFixed(3));

t += 50_000;
ok('60 s total', Math.abs(c.get() - 60) < 1e-9, c.get().toFixed(3));

c.setRunning(false);                // pause
const held = c.get();
t += 30_000;
ok('paused clock does not advance', c.get() === held, c.get().toFixed(3));

c.setRunning(true);                 // resume
t += 5_000;
ok('resumes from where it stopped', Math.abs(c.get() - (held + 5)) < 1e-9, c.get().toFixed(3));

c.set(450, true, 457);
t += 20_000;
ok('never runs past the duration', c.get() === 457, c.get());
ok('reports ended', c.ended() === true, c.ended());

c.set(0, true, 457);
ok('not ended at the start', c.ended() === false, c.ended());

// the bug this replaced: 1 s per 25 rAF ticks at 60 Hz = 2.4x fast
const frames = 60, perSec = frames / 25;
ok('old frame-counted approach was 2.4x fast (regression note)',
   Math.abs(perSec - 2.4) < 1e-9, perSec + 'x');

console.log(fails ? `\n${fails} FAILED` : '\nall clock tests pass');
process.exit(fails ? 1 : 0);
