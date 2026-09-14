// Playback position, kept as "elapsed E as of wall-clock T" and interpolated.
//
// The first version counted animation frames - one second every 25 rAF ticks -
// which assumes a 25 Hz display. At 60 Hz the clock ran 2.4x fast. Position is
// time, so it is measured against a clock, never against frames.
//
// Split out of app.js so the arithmetic can be tested without a browser: under
// headless virtual time rAF is throttled, so a repaint-based test measures the
// repaint cadence rather than the clock.

export function makeClock(now = () => performance.now()) {
  let base = 0;          // elapsed seconds at the last stamp
  let at = now();        // wall time of that stamp
  let running = false;
  let duration = 0;

  return {
    /** Stamp the position. Call on seek, track change, play and pause. */
    set(elapsed, isRunning = running, dur = duration) {
      base = Math.max(0, Number(elapsed) || 0);
      at = now();
      running = !!isRunning;
      duration = Math.max(0, Number(dur) || 0);
    },
    setRunning(isRunning) {
      if (!!isRunning === running) return;
      this.set(this.get(), isRunning);      // freeze where we are, then flip
    },
    setDuration(dur) { duration = Math.max(0, Number(dur) || 0); },
    /** Position in seconds, interpolated. Never runs past the track. */
    get() {
      if (!running) return base;
      const t = base + (now() - at) / 1000;
      return duration ? Math.min(t, duration) : t;
    },
    /** True once the track has played out. */
    ended() { return duration > 0 && this.get() >= duration - 0.05; },
    get running() { return running; },
    get duration() { return duration; },
  };
}
