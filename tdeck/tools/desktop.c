/* Run the T-Deck build of Setup Rush on a desktop: a deterministic 30 fps clock, scripted keys, frames to PPM.
 * cc -O2 -Isrc/setup_rush tools/setup_rush/desktop.c src/setup_rush/srush_*.c -o .pio/srush/srush
 * .pio/srush/srush OUTDIR SCRIPT   where SCRIPT is lines "t_ms mask shot": the mask is hex, bit0 trackball click held,
 * bits 1-4 a trackball roll up/down/left/right, 5 ENTER, 6 DEL, 8 W, 9 S (rolls and keys fire as the bit turns on),
 * 7 a finger down; "1" saves that frame. A "touch x y" line sets where the finger is for the lines after it. */
#include "srush_shim.h"
#include <stdio.h>

static uint16_t fb[320 * 240];
static uint32_t now_ms;
static const char *outdir;
static struct { uint32_t t; unsigned keys; int shot; int tx, ty; } ev[512];
static int nev, frame_no, cur_ev = -1;
static uint8_t *pack; static size_t pack_len;
static uint8_t save[512]; static size_t save_len;

static uint32_t ms(void) { return now_ms; }
static uint32_t unix_now(void) { return 1795478400u + 3600u * 15; }
static bool sload(uint8_t *b, size_t cap, size_t *len) { if (!save_len) return false; memcpy(b, save, save_len < cap ? save_len : cap); *len = save_len; return true; }
static bool sstore(const uint8_t *b, size_t len) { memcpy(save, b, len); save_len = len; return true; }

static void raw(SrRaw *r)
{
    static unsigned prev;
    while (cur_ev + 1 < nev && ev[cur_ev + 1].t <= now_ms) cur_ev++;
    unsigned k = cur_ev >= 0 ? ev[cur_ev].keys : 0, rise = k & ~prev;
    prev = k;
    r->click = k & 1;
    for (int d = 0; d < 4; d++) if (rise & (2u << d)) r->roll[d] = 3;
    if (rise & 32) r->keys[r->nkeys++] = '\r';
    if (rise & 64) r->keys[r->nkeys++] = 8;
    if (rise & 256) r->keys[r->nkeys++] = 'w';
    if (rise & 512) r->keys[r->nkeys++] = 's';
    r->touch = (k & 128) != 0;
    if (cur_ev >= 0) { r->tx = (int16_t)ev[cur_ev].tx; r->ty = (int16_t)ev[cur_ev].ty; }
}
static void present(const uint16_t *f)
{
    static int shot_done[512];
    for (int i = 0; i < nev; i++) {
        if (ev[i].shot && !shot_done[i] && now_ms >= ev[i].t) {
            shot_done[i] = 1;
            char path[512]; snprintf(path, sizeof path, "%s/f%06u.ppm", outdir, (unsigned)ev[i].t);
            FILE *o = fopen(path, "wb"); fprintf(o, "P6\n320 240\n255\n");
            for (int p = 0; p < 320 * 240; p++) { uint16_t c = f[p]; unsigned char rgb[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3) }; fwrite(rgb, 1, 3, o); }
            fclose(o);
        }
    }
    frame_no++; if (nev && now_ms > ev[nev - 1].t + 500) { printf("done after %d frames\n", frame_no); exit(0); }
    now_ms += 33;
}

int main(int argc, char **argv)
{
    outdir = argv[1];
    FILE *s = fopen(argv[2], "r"); char line[128];
    int tx = 160, ty = 140;
    while (fgets(line, sizeof line, s)) {
        if (!strncmp(line, "touch", 5)) { sscanf(line + 5, "%d %d", &tx, &ty); continue; }
        unsigned t, k; int sh = 0;
        if (sscanf(line, "%u %x %d", &t, &k, &sh) >= 2) { ev[nev].t = t; ev[nev].keys = k; ev[nev].shot = sh; ev[nev].tx = tx; ev[nev].ty = ty; nev++; }
    }
    FILE *p = fopen("data/setup_rush/srpack.bin", "rb"); fseek(p, 0, SEEK_END); pack_len = (size_t)ftell(p); fseek(p, 0, SEEK_SET);
    pack = malloc(pack_len); fread(pack, 1, pack_len, p); fclose(p);
    sr.fb = fb; sr.present = present; sr.raw = raw; sr.millis = ms; sr.unix_time = unix_now; sr.pack = pack; sr.pack_len = pack_len;
    sr.save_load = sload; sr.save_store = sstore;
    int r = srush_main();
    printf("exit %d after %d frames, %u ms\n", r, frame_no, (unsigned)now_ms);
    return 0;
}
