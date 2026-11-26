/* The T-Deck build of the game, playable on a computer or in a browser (SDL2; the web build is Emscripten).
 * The same game file and platform layer the T-Deck runs; this file stands in for the deck's screen, keys, trackball,
 * touch, speaker, clock and save.
 *   Keys:  hold W or SPACE = ride long, hold S = ride short (S while W is down flips short), let go = cash out.
 *          Menus: arrows or W A S D move, ENTER selects, ESC or BACKSPACE goes back.
 *   Mouse: the touchscreen (hold to ride long, slide down to flip short; tap menus). */
#include "srush_shim.h"
#include <SDL.h>
#include <stdio.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define W 320
#define H 240
static uint16_t fb[W * H];
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_AudioDeviceID audio;
static uint8_t *pack; static size_t pack_len;
static bool quit_req;
static uint32_t boot_unix;   /* the system clock at start (read before the shim's time() rename applies, in main) */

/* ---- input: SDL events become the deck's raw inputs ---- */
static char pend[8]; static uint8_t npend;
static uint8_t rolls[4];
static bool mouse_down; static int mx, my;
static void key_char(char c) { if (npend < sizeof pend) pend[npend++] = c; }
static void pump(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit_req = true;
        else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (e.key.keysym.sym) {
            case SDLK_UP: rolls[0]++; break;
            case SDLK_DOWN: rolls[1]++; break;
            case SDLK_LEFT: rolls[2]++; break;
            case SDLK_RIGHT: rolls[3]++; break;
            case SDLK_RETURN: case SDLK_KP_ENTER: key_char('\r'); break;
            case SDLK_BACKSPACE: key_char(8); break;
            case SDLK_ESCAPE: key_char(27); break;
            case SDLK_SPACE: key_char(' '); break;
            default:
                if (e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_z) key_char((char)e.key.keysym.sym);
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) mouse_down = true;
        else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) mouse_down = false;
        else if (e.type == SDL_FINGERDOWN) mouse_down = true;
        else if (e.type == SDL_FINGERUP) mouse_down = false;
        if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
            float lx, ly; SDL_RenderWindowToLogical(ren, e.type == SDL_MOUSEMOTION ? e.motion.x : e.button.x,
                                                    e.type == SDL_MOUSEMOTION ? e.motion.y : e.button.y, &lx, &ly);
            mx = (int)lx; my = (int)ly;
        }
        if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION) { mx = (int)(e.tfinger.x * W); my = (int)(e.tfinger.y * H); }
    }
}
static void raw(SrRaw *r)
{
    pump();
    memcpy(r->keys, pend, npend); r->nkeys = npend; npend = 0;
    memcpy(r->roll, rolls, 4); memset(rolls, 0, 4);
    r->touch = mouse_down;
    r->tx = (int16_t)(mx < 0 ? 0 : mx >= W ? W - 1 : mx); r->ty = (int16_t)(my < 0 ? 0 : my >= H ? H - 1 : my);
    const Uint8 *k = SDL_GetKeyboardState(NULL);   /* a keyboard that reports holds, like the deck's newer firmware */
    r->held = (uint8_t)(0x80 | ((k[SDL_SCANCODE_W] || k[SDL_SCANCODE_SPACE]) ? 1 : 0) | (k[SDL_SCANCODE_S] ? 2 : 0));
}

/* ---- the clock, the save ---- */
static uint32_t ms(void) { return SDL_GetTicks(); }
static uint32_t unix_now(void) { return (uint32_t)SDL_GetTicks() / 1000u + boot_unix; }   /* the shim renames time() */
#ifdef __EMSCRIPTEN__
EM_JS(int, ls_load, (uint8_t *buf, int cap), {
    try { const s = localStorage.getItem('trading-game-save'); if (!s) return 0;
          const b = atob(s), n = Math.min(b.length, cap); for (let i = 0; i < n; i++) HEAPU8[buf + i] = b.charCodeAt(i); return n; }
    catch (e) { return 0; }
});
EM_JS(void, ls_store, (const uint8_t *buf, int len), {
    try { let s = ''; for (let i = 0; i < len; i++) s += String.fromCharCode(HEAPU8[buf + i]); localStorage.setItem('trading-game-save', btoa(s)); }
    catch (e) {}
});
static bool sload(uint8_t *b, size_t cap, size_t *len) { int n = ls_load(b, (int)cap); *len = (size_t)n; return n > 0; }
static bool sstore(const uint8_t *b, size_t len) { ls_store(b, (int)len); return true; }
#else
static char save_path[1024];
static bool sload(uint8_t *b, size_t cap, size_t *len)
{
    FILE *f = fopen(save_path, "rb"); if (!f) return false;
    *len = fread(b, 1, cap, f); fclose(f); return *len > 0;
}
static bool sstore(const uint8_t *b, size_t len)
{
    FILE *f = fopen(save_path, "wb"); if (!f) return false;
    fwrite(b, 1, len, f); fclose(f); return true;
}
#endif

/* ---- a finished frame: to the window, plus this frame's sound, then wait out the frame (about 30 fps, as the deck) ---- */
static void present(const uint16_t *f)
{
    SDL_UpdateTexture(tex, NULL, f, W * 2);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);
    if (audio) { /* keep about 100 ms of sound queued */
        int16_t buf[1024];
        while (SDL_GetQueuedAudioSize(audio) < SR_AUDIO_RATE / 10 * 2) { sr_audio_render(buf, 1024); SDL_QueueAudio(audio, buf, sizeof buf); }
    }
    static uint32_t last; uint32_t now = SDL_GetTicks(), wait = now - last < 33 ? 33 - (now - last) : 0;
#ifdef __EMSCRIPTEN__
    emscripten_sleep(wait ? wait : 1);   /* hands the browser its turn (ASYNCIFY) */
#else
    if (wait) SDL_Delay(wait);
#endif
    last = SDL_GetTicks();
    if (quit_req) { SDL_Quit(); exit(0); }
}

static void start_audio(void)
{
    SDL_AudioSpec want = { 0 }, got;
    want.freq = SR_AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 512;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);
}

static bool load_pack(const char *path)
{
    FILE *p = fopen(path, "rb"); if (!p) return false;
    fseek(p, 0, SEEK_END); pack_len = (size_t)ftell(p); fseek(p, 0, SEEK_SET);
    pack = malloc(pack_len); if (fread(pack, 1, pack_len, p) != pack_len) { fclose(p); return false; }
    fclose(p); return true;
}

#undef time
int main(int argc, char **argv)
{
    boot_unix = (uint32_t)time(NULL);
#ifdef __EMSCRIPTEN__
    const char *pk = "/srpack.bin"; (void)argc; (void)argv;
#else
    const char *pk = argc > 1 ? argv[1] : "data/srpack.bin";
    const char *home = getenv("HOME"); snprintf(save_path, sizeof save_path, "%s/.trading-game.sav", home ? home : ".");
#endif
    if (!load_pack(pk)) { fprintf(stderr, "no chart pack at %s (run from tdeck/, or pass its path)\n", pk); return 1; }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    win = SDL_CreateWindow("trading-game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W * 3, H * 3, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, W, H);
#ifndef __EMSCRIPTEN__
    SDL_RenderSetIntegerScale(ren, SDL_TRUE);   /* the page scales the canvas itself */
#endif
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H);
    start_audio();
    sr.fb = fb; sr.present = present; sr.raw = raw; sr.millis = ms; sr.unix_time = unix_now;
    sr.pack = pack; sr.pack_len = pack_len; sr.save_load = sload; sr.save_store = sstore;
    for (;;) { (void)srush_main(); if (quit_req) break; }   /* back out of the intro: it starts over, as on the deck */
    SDL_Quit();
    return 0;
}
