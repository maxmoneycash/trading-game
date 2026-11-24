/* The T-Deck's inputs as the game's keypad, and the thumb ring drawn under a finger that is riding.
 *   In a round:  hold the screen = ride long, slide down = flip short (slide back up = long again); the thumb ring
 *                sits where the finger landed. Holding the trackball click also rides long. Keyboard: W or SPACE
 *                rides long until pressed again, S rides short, ENTER lets go, DEL or Q ends the round.
 *   Elsewhere:   tap = select, swipe or roll the trackball = move, trackball click or ENTER = select,
 *                W A S D move, DEL or Q = back (out of the game from the menu).
 * A hold only counts if it began in the round, so the tap that starts a round never opens a ride. */
#include "srush_shim.h"

int srush_scene(void);
uint16_t srush_ink(int which); /* 0 gain, 1 bear, 2 text, 3 background, 4 orange; 1555 */

typedef enum { T_NONE, T_UP, T_DOWN, T_LEFT, T_RIGHT, T_ENTER, T_CLEAR } Tap;
static Tap q[8]; static uint8_t qn, gap;
static void push(Tap t) { if (qn < 8) q[qn++] = t; }

static int lastScene = -1;
static bool touchArmed, touchRide, clickArmed, clickWas, latchLong, latchShort;
static int16_t ax, ay, sx, sy;     /* where the riding finger landed; where a menu gesture began */
static bool gest;
static uint32_t lastRoll;

void sr_input_scan(uint8_t kb[8])
{
    SrRaw r; memset(&r, 0, sizeof r);
    if (sr.raw) sr.raw(&r);
    int scene = srush_scene();
    uint32_t now = sr.millis ? sr.millis() : 0;
    if (scene != lastScene) {
        /* a new screen: holds already down stay inert until released */
        touchArmed = !r.touch; touchRide = false; clickArmed = !r.click; latchLong = latchShort = false; gest = false;
        lastScene = scene;
    }
    bool play = scene == 1;

    /* keyboard */
    for (uint8_t i = 0; i < r.nkeys; i++) {
        char c = r.keys[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        bool back = c == 8 || c == 0x7f || c == 27 || c == 'q';
        if (play) {
            if (back) push(T_CLEAR);
            else if (c == 'w' || c == ' ') { latchLong = !latchLong; latchShort = false; }
            else if (c == 's') { latchShort = !latchShort; latchLong = false; }
            else if (c == '\r' || c == '\n') latchLong = latchShort = false;
        } else {
            if (back) push(T_CLEAR);
            else if (c == 'w' || c == 'k') push(T_UP);
            else if (c == 's' || c == 'j') push(T_DOWN);
            else if (c == 'a' || c == 'h') push(T_LEFT);
            else if (c == 'd' || c == 'l') push(T_RIGHT);
            else if (c == '\r' || c == '\n' || c == ' ') push(T_ENTER);
        }
    }

    /* trackball rolls move through menus, one step per 110 ms however fast the ball spins */
    if (!play && now - lastRoll >= 110) {
        int best = -1, most = 0;
        for (int d = 0; d < 4; d++) if (r.roll[d] > most) { most = r.roll[d]; best = d; }
        if (best >= 0) { push((Tap)(T_UP + best)); lastRoll = now; }
    }

    /* trackball click */
    if (!r.click) clickArmed = true;
    bool clickHold = play && clickArmed && r.click;
    if (!play && clickArmed && r.click && !clickWas) push(T_ENTER);
    clickWas = r.click;

    /* touch */
    bool L = latchLong || clickHold, D = latchShort;
    if (!r.touch) { touchArmed = true; touchRide = false; }
    if (play) {
        if (r.touch && touchArmed) {
            if (!touchRide) { touchRide = true; ax = r.tx; ay = r.ty; }
            int dy = r.ty - ay;
            if (dy > 18) { D = true; L = true; }       /* slid down: short (the game flips a long it already holds) */
            else L = true;
        }
    } else if (touchArmed || gest) {
        if (r.touch && !gest) { gest = true; sx = r.tx; sy = r.ty; }
        if (!r.touch && gest) {
            gest = false;
            int dx = sr.touch_x - sx, dy = sr.touch_y - sy, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            if (adx < 14 && ady < 14) push(T_ENTER);
            else if (ady >= adx) push(dy > 0 ? T_DOWN : T_UP);
            else push(dx > 0 ? T_RIGHT : T_LEFT);
        }
    }
    if (r.touch) { sr.touch_x = r.tx; sr.touch_y = r.ty; }
    sr.touch_down = play && touchRide;

    if (L) kb[1] |= kb_2nd;
    if (D) kb[7] |= kb_Down;
    /* queued taps: one scan down, one scan up, so the same key twice is two presses */
    if (gap) gap = 0;
    else if (qn) {
        Tap t = q[0]; memmove(q, q + 1, (size_t)(--qn) * sizeof q[0]); gap = 1;
        switch (t) {
        case T_UP: kb[7] |= kb_Up; break;
        case T_DOWN: kb[7] |= kb_Down; break;
        case T_LEFT: kb[7] |= kb_Left; break;
        case T_RIGHT: kb[7] |= kb_Right; break;
        case T_ENTER: kb[6] |= kb_Enter; break;
        case T_CLEAR: kb[6] |= kb_Clear; break;
        default: break;
        }
    }
}

/* ---- the thumb ring ---- */
static inline uint16_t c565(uint16_t c) { uint16_t r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31; return (uint16_t)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b); }
static inline void blend(int x, int y, uint16_t c, int a /* 0..256 */)
{
    if ((unsigned)x >= 320 || (unsigned)y >= 240 || a <= 0) return;
    uint16_t *p = sr.fb + y * 320 + x, d = *p;
    if (a >= 256) { *p = c; return; }
    int r = ((d >> 11) * (256 - a) + (c >> 11) * a) >> 8, g = (((d >> 5) & 63) * (256 - a) + ((c >> 5) & 63) * a) >> 8, b = ((d & 31) * (256 - a) + (c & 31) * a) >> 8;
    *p = (uint16_t)((r << 11) | (g << 5) | b);
}
/* an anti-aliased disc or ring: coverage from four sub-samples per pixel */
static void disc(int cx, int cy, int rOut, int rIn, uint16_t c, int alpha)
{
    for (int y = cy - rOut - 1; y <= cy + rOut + 1; y++)
        for (int x = cx - rOut - 1; x <= cx + rOut + 1; x++) {
            int dx = x - cx, dy = y - cy, cov = 0;
            for (int s = 0; s < 4; s++) { /* four sub-samples along the diagonal */
                float fx = dx + (s - 1.5f) * 0.25f, fy = dy + (s - 1.5f) * 0.25f, dd = fx * fx + fy * fy;
                if (dd <= (float)rOut * rOut && dd >= (float)rIn * rIn) cov++;
            }
            if (cov) blend(x, y, c, alpha * cov / 4);
        }
}
static void chevron(int cx, int cy, bool up, uint16_t c, int alpha)
{
    for (int i = 0; i < 5; i++) {
        int y = up ? cy - 2 + i : cy + 2 - i;
        for (int t = 0; t < 2; t++) { blend(cx - 4 + i, y + t, c, alpha); blend(cx + 4 - i, y + t, c, alpha); }
    }
}
void sr_overlay(void)
{
    if (!sr.touch_down) return;
    bool shortNow = sr.touch_y - ay > 18;
    uint16_t gain = c565(srush_ink(0)), bear = c565(srush_ink(1)), text = c565(srush_ink(2));
    uint16_t ink = shortNow ? bear : gain;
    int R = 30;
    disc(ax, ay, R, R - 2, text, 70);                         /* the ring where the finger landed */
    chevron(ax, ay - R + 9, true, gain, shortNow ? 90 : 256); /* long up, short down: the live one lit */
    chevron(ax, ay + R - 9, false, bear, shortNow ? 256 : 90);
    int ky = sr.touch_y; if (ky < ay - R + 12) ky = ay - R + 12; if (ky > ay + R - 12) ky = ay + R - 12;
    disc(ax, ky, 11, 0, ink, 220);                            /* the knob follows the thumb inside the ring */
    disc(ax, ky, 13, 11, ink, 80);
}
