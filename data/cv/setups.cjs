// Candle Vision setups for one window, exactly as the web game reads them (engine.js remoteReader over the window's
// candles, warm-up 40): stdin = JSON [[t,o,h,l,c,v], ...], stdout = JSON { unit, setups: [...] } with prices in real units.
const fs = require('fs');
const CV = new Function(fs.readFileSync(__dirname + '/candle-vision.js', 'utf8') + '; return CandleVision;')();
const E = require('./engine.js');
const rows = JSON.parse(fs.readFileSync(0, 'utf8'));
const n = E.normalizeBars(rows.map(r => ({ t: r[0], o: r[1], h: r[2], l: r[3], c: r[4], v: r[5] })));
// argv[2]: a Career stage's setup keys (MarketTape's `only`: one-pattern drills), else every setup with variety
const only = process.argv[2] ? JSON.parse(process.argv[2]) : null;
let out;
if (only) {
  const t = new E.MarketTape(CV, rows.map(r => ({ t: r[0], o: r[1], h: r[2], l: r[3], c: r[4], v: r[5] })), { only });
  while (!t.done) t.extend();
  out = { setups: t.setups };
} else out = E.remoteReader(CV).read({ bars: n.candles, fromK: 40, upto: n.candles.length - 1 });
const u = n.unit;
process.stdout.write(JSON.stringify({ unit: u, setups: out.setups.map(s => ({
  at: s.at, start: s.startIndex, dir: s.dir, name: s.name, conf: s.confidence, entry: s.entry * u, risk: s.risk * u,
  result: s.result, resolvedAt: s.resolvedAt, key: s.key,
})) }));
