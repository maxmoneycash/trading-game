import re
"""Pack real Hyperliquid 5m candles from setup-rush main for the TI-84 CE, with each candle's intrabar path.

Windows: LEAD candles shown before the clock + PLAY candles to play (the web game's tape runs 1.1-3.4+ bars/s).
Picked like engine.js pickStart: the swingiest stretch (swingScore over the play bars, 2.5 ATR reversals), inside the
range the pack's recorder covered (path-10..47), no gaps.

Each candle's 32-step path is engine.js barPath for pv 3: recordedPath from the candle's own recorded trades
(path-NN.json), else walkPath (pv 2's seeded tick walk), seeded by the bar's index on the window's tape.

Appvars SRD0..SRDB (PER windows each): 'SRD3', u8 count, u8 lead, u16 bars, then per window:
  char sym[8], u32 start_unix, i32 lo, u32 span, u8 dec, u8 recorded_pct,
  bars * 4 u16 (O,H,L,C scaled 0..65535 over [lo, lo+span], raw = price*10^dec),
  bars * 31 u8 (path steps 1..31 over the candle's own [low, high]; step 0 = open, step 32 = close),
  bars * u16 volume (scaled to the window's largest bar = 65535; VWAP weights),
  u8 setups, then MAXS records of SREC bytes: u16 at, u16 start, i8 dir, u8 confidence %, u16 entry, u16 risk (price
  units), u8 result (1 win), u16 resolvedAt (0xffff none), char name[22] -- Candle Vision's own setups for the window,
  from the web engine (cv/setups.cjs: engine.js remoteReader + vendor/candle-vision.js from main).
Usage: make_pack.py OUTDIR
"""
import base64, gzip, json, math, os, struct, subprocess, sys
MAXS, SREC = 40, 35

REPO = os.environ.get('SETUP_RUSH_REPO') or sys.exit('set SETUP_RUSH_REPO to a setup-rush checkout (the recorded charts come from its main)')
PACK = 'setup-rush-app/public/history/v2/20261117'
LEAD, PLAY = 100, 300   # LEAD: indicator warm-up + the chart shown before the clock
N = LEAD + PLAY
PER_MARKET, PER = 2, 3
PATH_N2 = 32
M32 = 0xFFFFFFFF


def show(p):
    return subprocess.run(['git', '-C', REPO, 'show', f'origin/main:{p}'], capture_output=True, check=True).stdout


def mulberry32(seed):
    a = seed & M32
    def r():
        nonlocal a
        a = (a + 0x6D2B79F5) & M32
        t = ((a ^ (a >> 15)) * (1 | a)) & M32
        t = ((t + (((t ^ (t >> 7)) * (61 | t)) & M32)) & M32) ^ t
        return ((t ^ (t >> 14)) & M32) / 4294967296
    return r


def walk_path(o, h, l, cl, seed):
    n = PATH_N2
    p = [0.0] * (n + 1)
    rng = h - l
    if not rng > 0:
        p = [o] * (n + 1); p[n] = cl; return p
    r = mulberry32(seed)
    up = cl >= o
    counter_first = r() < 0.62
    first = (l if counter_first else h) if up else (h if counter_first else l)
    second = h if first == l else l
    i1 = 1 + math.floor(r() * (n * 2 / 3 - 1))
    i2 = i1 + 2 + math.floor(r() * (n - 2 - i1))
    def fold(x):
        k = 0
        while k < 4 and (x > h or x < l):
            x = 2 * h - x if x > h else 2 * l - x; k += 1
        return h if x > h else l if x < l else x
    def leg(A, B, a, b):
        m = B - A; W = [0.0]
        for s in range(1, m + 1):
            W.append(W[s - 1] + (r() + r() + r() - 1.5) * (2.4 if r() < 0.2 else 0.8))
        for s in range(1, m):
            p[A + s] = fold(a + (b - a) * (s / m) + (W[s] - (s / m) * W[m]) * rng * 0.11)
    p[0] = o; p[i1] = first; p[i2] = second; p[n] = cl
    leg(0, i1, o, first); leg(i1, i2, first, second); leg(i2, n, second, cl)
    return p


def recorded_path(o, h, l, cl, rp, span=300000):
    n = PATH_N2
    pts = sorted([(t / span) * n, x] for t, x in rp if 0 <= t < span)
    if len(pts) < 2: return None
    if not h - l > 0:
        p = [o] * (n + 1); p[n] = cl; return p
    p = [0.0] * (n + 1); j = 0
    for s in range(n + 1):
        while j < len(pts) - 2 and pts[j + 1][0] <= s: j += 1
        xa, pa = pts[j]; xb, pb = pts[j + 1]
        p[s] = pa if s <= xa else pb if s >= xb else pa + (pb - pa) * ((s - xa) / (xb - xa))
    for s in range(1, n): p[s] = h if p[s] > h else l if p[s] < l else p[s]
    xH, vH, xL, vL = pts[0][0], -math.inf, pts[0][0], math.inf
    for x, v in pts:
        if v > vH: vH, xH = v, x
        if v < vL: vL, xL = v, x
    step = lambda x: max(1, min(n - 1, round(x)))
    sH, sL = step(xH), step(xL)
    if sH == sL:
        if xH <= xL: sL = sH + 1 if sH < n - 1 else sH - 1
        else: sH = sL + 1 if sL < n - 1 else sL - 1
    p[sH] = h; p[sL] = l; p[0] = o; p[n] = cl
    return p


def swing_score(bars, start, n, mult):
    end = min(len(bars), start + n)
    if end - start < 60: return 0
    atr = 0
    for i in range(start + 1, min(start + 15, end)):
        atr += max(bars[i][2] - bars[i][3], abs(bars[i][2] - bars[i - 1][4]), abs(bars[i][3] - bars[i - 1][4]))
    atr /= 14
    if not atr > 0: return 0
    th = atr * mult; d = 0; ext = bars[start][4]; swings = 0; amp = 0
    for i in range(start + 1, end):
        c = bars[i][4]
        if d == 0:
            if abs(c - ext) >= th: d = 1 if c > ext else -1; ext = c
            continue
        if d > 0:
            if c > ext: ext = c
            elif ext - c >= th: swings += 1; amp += (ext - c) / th; d = -1; ext = c
        else:
            if c < ext: ext = c
            elif c - ext >= th: swings += 1; amp += (c - ext) / th; d = 1; ext = c
    return swings + min(swings, amp * 0.1)


# Career (core.js CAREER): each stage drills its own setups on one chart. The phone seeds a window of its whole history;
# here each stage takes, in a seeded order, the first window of these markets where its setups print often enough.
CAREER = [('hammer', ['hammer']), ('engulf', ['engulfing']), ('star', ['star', 'piercing']), ('crows', ['soldiers', 'engulfing']),
          ('double', ['doubleBottom']), ('flags', ['flag']), ('fog', None), ('hs', ['headShoulders']), ('wedge', ['wedge']),
          ('turbo', None), ('blind', None), ('survivor', None)]
def hash_seed(t):
    h = 2166136261
    for ch in t: h ^= ord(ch); h = (h * 16777619) & 0xFFFFFFFF
    return h % 1000000000 + 1
def cv_setups(w, only=None):
    args = ['node', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cv', 'setups.cjs')] + ([json.dumps(only)] if only else [])
    return json.loads(subprocess.run(args, input=json.dumps(w), capture_output=True, text=True, check=True).stdout)['setups']

def main(outdir):
    man = json.loads(show(f'{PACK}/manifest.json'))
    wins = []; allrows = {}
    for s in man['sets']:
        rows = []
        for c in s['chunks']: rows += json.loads(show(f"{PACK}/{c['file']}"))['rows']
        rows.sort(key=lambda r: r[0])
        allrows[s['id']] = (s, rows)
        rec = s['rec']; block = rec['block']; ks = sorted(p['k'] for p in rec['paths'])
        lo_i, hi_i = ks[0] * block, (ks[-1] + 1) * block
        cands = []
        for i in range(lo_i, min(hi_i, len(rows)) - N, 10):
            w = rows[i:i + N]
            if any(w[j + 1][0] - w[j][0] != 300 for j in range(N - 1)): continue
            if sum(1 for r in w if r[2] == r[3]) > N * 0.08: continue  # dead tape
            cands.append((swing_score(rows, i + LEAD, PLAY, 2.5), i))
        cands.sort(reverse=True); picked = []
        for sc, i in cands:
            if all(abs(i - j) >= N for j in picked): picked.append(i)
            if len(picked) == PER_MARKET: break
        files = {p['k']: p['file'] for p in rec['paths']}
        for i in picked:
            rp = {}
            for k in range(i // block, (i + N - 1) // block + 1):
                d = json.loads(show(f'{PACK}/{files[k]}'))
                for t, pts in json.loads(gzip.decompress(base64.b64decode(d['data'])))['rows']: rp[t] = pts
            wins.append((s, rows[i:i + N], rp, None))
            print(f"{s['coin']:10} start={rows[i][0]} score={dict((b, a) for a, b in cands)[i]:.1f}", file=sys.stderr)
    # Career windows, after the 36 practice windows
    for sid, only in CAREER:
        r = mulberry32(hash_seed('career:' + sid)); best = None
        sets = list(allrows.values())
        for tries in range(30 if only else 8):
            s, rows = sets[int(r() * len(sets))]
            lo_i = sorted(p['k'] for p in s['rec']['paths'])[0] * s['rec']['block']
            i = lo_i + int(r() * (len(rows) - N - lo_i)); w = rows[i:i + N]
            if any(w[j + 1][0] - w[j][0] != 300 for j in range(N - 1)): continue
            ss = cv_setups(w, only)
            n_in = sum(1 for q in ss if LEAD <= q['at'] < LEAD + 220)
            if not best or n_in > best[0]: best = (n_in, s, i)
            if n_in >= (5 if only else 8): break
        n_in, s, i = best
        files = {p['k']: p['file'] for p in s['rec']['paths']}; block = s['rec']['block']; rp = {}
        for kb in range(i // block, (i + N - 1) // block + 1):
            if kb in files:
                d = json.loads(show(f'{PACK}/{files[kb]}'))
                for t, pts in json.loads(gzip.decompress(base64.b64decode(d['data'])))['rows']: rp[t] = pts
        wins.append((s, allrows[s['id']][1][i:i + N], rp, only))
        print(f"career {sid:9} {s['coin']:10} setups in play {n_in}", file=sys.stderr)
    os.makedirs(outdir, exist_ok=True)
    for a in range(0, len(wins), PER):
        out = bytearray()
        chunk = wins[a:a + PER]
        for s, w, rp, only in chunk:
            hi = max(r[2] for r in w); lo = min(r[3] for r in w)
            dec = max(0, min(6, 5 - len(str(int(hi)))))
            loi = math.floor(lo * 10 ** dec); span = max(1, math.ceil(hi * 10 ** dec) - loi)
            paths = []; nrec = 0
            for i, (t, o, h, l, c, v) in enumerate(w):
                h = max(h, o, c); l = min(l, o, c)
                p = recorded_path(o, h, l, c, rp[t]) if t in rp else None
                if p: nrec += 1
                else: p = walk_path(o, h, l, c, ((((i + 1) * 2654435761) & M32) ^ 0x5BD1E995) & M32)
                paths.append(bytes(0 if h == l else max(0, min(255, round((x - l) / (h - l) * 255))) for x in p[1:PATH_N2]))
            sym = s['coin'].split(':')[-1][:8].encode().ljust(8, b'\0')
            out += sym + struct.pack('<IiIBB', w[0][0], loi, span, dec, round(100 * nrec / N))
            for r in w:
                o, h, l, c = r[1], max(r[2], r[1], r[4]), min(r[3], r[1], r[4]), r[4]
                for x in (o, h, l, c): out += struct.pack('<H', max(0, min(65535, round((x * 10 ** dec - loi) / span * 65535))))
            for p in paths: out += p
            vmax = max(r[5] for r in w) or 1
            for r in w: out += struct.pack('<H', round(r[5] / vmax * 65535))
            ss = cv_setups(w, only)[:MAXS]
            vv = lambda price: max(0, min(65535, round((price * 10 ** dec - loi) / span * 65535)))
            out += struct.pack('<B', len(ss))
            for z in range(MAXS):
                if z < len(ss):
                    q = ss[z]
                    out += struct.pack('<HHbBHHBH', q['at'], q['start'], q['dir'], round(q['conf'] * 100), vv(q['entry']),
                                       min(65535, round(q['risk'] * 10 ** dec / span * 65535)), 1 if q['result'] == 'win' else 0,
                                       q['resolvedAt'] if q['resolvedAt'] >= 0 else 0xFFFF) + short_name(q['name']).encode()[:21].ljust(22, b'\0')
                else: out += bytes(SREC)
            print(f"  {s['coin']:10} setups {len(ss)}", file=sys.stderr)
            print(f"  {s['coin']:10} recorded {nrec}/{N}", file=sys.stderr)
        body = b'SRD4' + struct.pack('<BBH', len(chunk), LEAD, N) + out
        name = '0123456789ABCDEF'[a // PER]
        open(f'{outdir}/srd{name}.bin', 'wb').write(body)
        print(f'srd{name}.bin {len(body)} bytes', file=sys.stderr)


def short_name(nm):
    # The caption says BULLISH or BEARISH on its own line, so the name drops it: 'Bullish Three-Line Strike' fits in 21.
    return re.sub(r'^(Bullish|Bearish) ', '', nm)


if __name__ == '__main__':
    main(sys.argv[1])
