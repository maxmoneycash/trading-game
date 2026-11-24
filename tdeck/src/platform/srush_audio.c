/* Setup Rush's sounds on the T-Deck speaker: a port of the web game's synth (setup-rush-app/src/game/fx.js).
 * Voices are oscillators with an exponential pitch ramp and an exponential attack/decay; hiss is white noise through a
 * biquad whose cutoff ramps the same way; both feed fx.js's bus: a short damped room delay into a compressor.
 * The game calls sr_sfx(); the firmware pulls samples with sr_audio_render() as its I2S buffer has room. */
#include "srush_shim.h"
#include <math.h>

#define RATE SR_AUDIO_RATE
#define NV 28
#define NH 10

typedef struct { bool on; uint8_t type; uint32_t start, len, att; float ph, f, fmul, g, gpeak, gmul, gup; } Voice;
typedef struct { bool on; uint8_t type; uint32_t start, len, att; float f, fmul, q, g, gpeak, gmul, gup, b0, b1, b2, a1, a2, x1, x2, y1, y2; } Hiss;
enum { SINE, TRI, SQUARE, SAW };
enum { BP, LP, HP };

static Voice v[NV];
static Hiss h[NH];
static uint32_t now;            /* samples rendered so far */
static uint32_t rng = 0x9e3779b9u;
static float room[2048]; static int roomI;           /* 85 ms at 22.05 kHz = 1874 samples */
static float damp, env = 0;
static uint32_t lastTick, lastClink;

static float st(float n) { return powf(2.0f, n / 12.0f); }
static uint32_t ms(float s) { return (uint32_t)(s * RATE); }

static void voice(float f, float to, int type, float dur, float vol, float at, float attack)
{
    for (int i = 0; i < NV; i++) if (!v[i].on) {
        Voice *o = &v[i];
        o->on = true; o->type = (uint8_t)type; o->start = now + ms(at); o->len = ms(dur); o->att = ms(attack); if (o->att < 1) o->att = 1;
        if (o->len <= o->att + 1) o->len = o->att + 2;
        o->ph = 0; o->f = f; o->fmul = to > 0 ? powf((to < 20 ? 20 : to) / f, 1.0f / o->len) : 1.0f;
        o->gpeak = vol; o->g = 0.0001f; o->gup = powf(vol / 0.0001f, 1.0f / o->att); o->gmul = powf(0.0001f / vol, 1.0f / (o->len - o->att));
        return;
    }
}
static void hiss(float dur, float vol, float at, int type, float f, float to, float q)
{
    for (int i = 0; i < NH; i++) if (!h[i].on) {
        Hiss *o = &h[i];
        memset(o, 0, sizeof *o);
        o->on = true; o->type = (uint8_t)type; o->start = now + ms(at); o->len = ms(dur); o->att = ms(0.004f); if (o->att < 1) o->att = 1;
        if (o->len <= o->att + 1) o->len = o->att + 2;
        o->f = f; o->fmul = to > 0 ? powf(to / f, 1.0f / o->len) : 1.0f; o->q = q;
        o->gpeak = vol; o->g = 0.0001f; o->gup = powf(vol / 0.0001f, 1.0f / o->att); o->gmul = powf(0.0001f / vol, 1.0f / (o->len - o->att));
        return;
    }
}
#define V(f, to, type, dur, vol, at) voice(f, to, type, dur, vol, at, 0.004f)
#define HS(dur, vol, at, type, f, to, q) hiss(dur, vol, at, type, f, to, q)

static void win(int combo, int size)
{
    float up = st((float)(combo - 1 < 0 ? 0 : combo - 1 > 12 ? 12 : combo - 1));
    static const int small[4] = { 0, 4, 7, 12 }, big[6] = { 0, 4, 7, 12, 16, 19 };
    const int *notes = size > 1 ? big : small; int n = size > 1 ? 6 : 4;
    for (int i = 0; i < n; i++) { V(523.25f * st((float)notes[i]) * up, 0, TRI, 0.18f, 0.07f, i * 0.055f); V(1046.5f * st((float)notes[i]) * up, 0, SINE, 0.12f, 0.025f, i * 0.055f); }
    float end = n * 0.055f;
    V(2637 * up, 0, TRI, 0.09f, 0.04f, end);
    V(3520 * up, 0, TRI, 0.22f, 0.035f, end + 0.05f);
    HS(0.12f, 0.03f, end, HP, 6000, 0, 0.9f);
    if (size > 1) { V(65, 45, SINE, 0.4f, 0.18f, 0); HS(0.5f, 0.03f, end + 0.1f, HP, 7000, 0, 0.9f); }
}

void sr_sfx(int id, int a)
{
    switch (id) {
    case SFX_TAP: HS(0.025f, 0.05f, 0, HP, 3500, 0, 0.9f); V(1900, 0, TRI, 0.03f, 0.02f, 0); break;
    case SFX_CANDLE: if (now - lastTick < ms(0.07f)) return; lastTick = now; HS(0.018f, 0.022f, 0, HP, 5200, 0, 0.9f); break;
    case SFX_LOCK:
        HS(0.16f, 0.045f, 0, BP, 900, 6500, 2.5f); V(620, 1240, SINE, 0.12f, 0.06f, 0);
        V(1760, 0, TRI, 0.22f, 0.06f, 0.1f); V(2637, 0, SINE, 0.16f, 0.025f, 0.12f); break;
    case SFX_OPEN:
        HS(0.2f, 0.06f, 0, LP, 300, 2600, 0.7f); V(a > 0 ? 160 : 140, 70, SINE, 0.16f, 0.16f, 0);
        V(a > 0 ? 660 : 494, a > 0 ? 990 : 330, TRI, 0.12f, 0.05f, 0.02f); break;
    case SFX_FLIP: V(700, 360, SQUARE, 0.08f, 0.035f, 0); HS(0.06f, 0.03f, 0, BP, 1800, 0, 0.9f); break;
    case SFX_WIN: win(a < 1 ? 1 : a, 1); break;
    case SFX_BIG: win(a < 1 ? 1 : a, 2); break;
    case SFX_LOSS: V(180, 72, SINE, 0.28f, 0.14f, 0); HS(0.18f, 0.04f, 0, LP, 800, 200, 0.9f); V(233, 196, TRI, 0.22f, 0.04f, 0.04f); break;
    case SFX_LIQ:
        V(110, 32, SAW, 0.7f, 0.12f, 0); V(55, 30, SINE, 0.8f, 0.2f, 0);
        HS(0.6f, 0.09f, 0, LP, 2400, 120, 0.6f); HS(0.08f, 0.06f, 0.02f, HP, 3000, 0, 0.9f); break;
    case SFX_RIGHT: { float up = st((float)(a - 1 < 0 ? 0 : a - 1 > 12 ? 12 : a - 1));
        V(988 * up, 0, TRI, 0.09f, 0.08f, 0); V(1480 * up, 0, TRI, 0.16f, 0.07f, 0.07f); HS(0.05f, 0.03f, 0.07f, HP, 5000, 0, 0.9f); break; }
    case SFX_WRONG: V(260, 200, SQUARE, 0.12f, 0.05f, 0); V(180, 120, SQUARE, 0.25f, 0.05f, 0.1f); HS(0.2f, 0.04f, 0.08f, LP, 900, 0, 0.9f); break;
    case SFX_TICK: V(1200, 0, SQUARE, 0.03f, 0.02f, 0); break;
    case SFX_DING: { float up = st((float)(a * 2 > 12 ? 12 : a * 2)); V(1568 * up, 0, TRI, 0.14f, 0.05f, 0); V(3136 * up, 0, SINE, 0.1f, 0.02f, 0); break; }
    case SFX_COUNT: V(a == 0 ? 1320 : 880, 0, SQUARE, a == 0 ? 0.25f : 0.07f, 0.04f, 0); break;
    case SFX_COMBO: { float up = st((float)(a > 12 ? 12 : a)); HS(0.25f, 0.05f, 0, BP, 400, 5000, 1.6f); V(440 * up, 880 * up, SAW, 0.22f, 0.03f, 0); break; }
    case SFX_CLINK: { if (now - lastClink < ms(0.026f)) return; lastClink = now; float up = st(a * 0.75f > 19 ? 19 : a * 0.75f);
        V(1975 * up, 0, TRI, 0.05f, 0.032f, 0); V(3951 * up, 0, SINE, 0.03f, 0.012f, 0); break; }
    case SFX_STAMP: V(a ? 120 : 84, 40, SINE, 0.26f, 0.2f, 0); HS(0.12f, 0.07f, 0, LP, 2400, 240, 0.9f); V(a ? 1320 : 520, 0, TRI, 0.05f, 0.035f, 0.004f); break;
    case SFX_COLLECT: for (int i = 0; i < 3; i++) V(1046.5f * st((float)(i == 0 ? 0 : i == 1 ? 7 : 12)), 0, TRI, 0.13f, 0.05f, i * 0.06f); HS(0.1f, 0.025f, 0.14f, HP, 6500, 0, 0.9f); break;
    default: break;
    }
}

static float sinT[256]; static bool sinReady;   /* sinf per sample per voice is too slow on the ESP32 */
static float osc(int type, float ph)
{
    switch (type) {
    case SINE: { float x = ph * 256; int i = (int)x; float fr = x - i; return sinT[i & 255] + (sinT[(i + 1) & 255] - sinT[i & 255]) * fr; }
    case TRI: return ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4;
    case SQUARE: return ph < 0.5f ? 0.6f : -0.6f;   /* square and saw are loud for their rms; fx.js's mix leans on the browser's band-limiting */
    default: return (ph * 2 - 1) * 0.7f;
    }
}
static void coefs(Hiss *o)
{
    float f = o->f; if (f > RATE * 0.45f) f = RATE * 0.45f; if (f < 20) f = 20;
    float w = 6.2831853f * f / RATE, cw = cosf(w), sw = sinf(w), al = sw / (2 * o->q), a0 = 1 + al;
    if (o->type == BP) { o->b0 = al / a0; o->b1 = 0; o->b2 = -al / a0; }
    else if (o->type == LP) { o->b0 = (1 - cw) / 2 / a0; o->b1 = (1 - cw) / a0; o->b2 = o->b0; }
    else { o->b0 = (1 + cw) / 2 / a0; o->b1 = -(1 + cw) / a0; o->b2 = o->b0; }
    o->a1 = -2 * cw / a0; o->a2 = (1 - al) / a0;
}

void sr_audio_render(int16_t *out, int n)
{
    if (!sinReady) { for (int i = 0; i < 256; i++) sinT[i] = sinf(i * 6.2831853f / 256); sinReady = true; }
    for (int s = 0; s < n; s++, now++) {
        float x = 0;
        for (int i = 0; i < NV; i++) {
            Voice *o = &v[i];
            if (!o->on || now < o->start) continue;
            uint32_t t = now - o->start;
            if (t >= o->len) { o->on = false; continue; }
            x += osc(o->type, o->ph) * o->g;
            o->ph += o->f / RATE; if (o->ph >= 1) o->ph -= 1;
            o->f *= o->fmul;
            o->g *= t < o->att ? o->gup : o->gmul;
        }
        for (int i = 0; i < NH; i++) {
            Hiss *o = &h[i];
            if (!o->on || now < o->start) continue;
            uint32_t t = now - o->start;
            if (t >= o->len) { o->on = false; continue; }
            if ((t & 15) == 0) coefs(o);
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            float w = (float)(int32_t)rng / 2147483648.0f;
            float y = o->b0 * w + o->b1 * o->x1 + o->b2 * o->x2 - o->a1 * o->y1 - o->a2 * o->y2;
            o->x2 = o->x1; o->x1 = w; o->y2 = o->y1; o->y1 = y;
            x += y * o->g;
            o->f *= o->fmul;
            o->g *= t < o->att ? o->gup : o->gmul;
        }
        /* the room: 85 ms, damped at 3.8 kHz, 22% feedback, 18% wet */
        int len = (int)(0.085f * RATE);
        float d = room[(roomI + 2048 - len) & 2047];
        damp += (d - damp) * 0.66f;
        room[roomI] = x + damp * 0.22f; roomI = (roomI + 1) & 2047;
        x += damp * 0.18f;
        /* the compressor: -16 dB threshold, 4:1, fast attack, 200 ms release */
        float a = x < 0 ? -x : x;
        env = a > env ? env + (a - env) * 0.4f : env * 0.99977f;
        float thr = 0.158f, gain = env > thr ? (thr + (env - thr) / 4) / env : 1;
        x *= gain * 0.9f * SR_AUDIO_GAIN;
        if (x > 1) x = 1; else if (x < -1) x = -1;
        out[s] = (int16_t)(x * 32000);
    }
}
