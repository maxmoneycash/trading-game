/* Setup Rush's platform interface: what the firmware (or the desktop harness) hands the game. Safe to include from
 * C++; srush_shim.h, which the game itself includes, adds the calculator-library names on top. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SrRaw {
    bool touch; int16_t tx, ty;   /* a finger on the screen, in 320x240 landscape pixels */
    bool click;                   /* the trackball pressed in */
    uint8_t roll[4];              /* trackball steps since the last read: up, down, left, right */
    char keys[8]; uint8_t nkeys;  /* keyboard characters since the last read */
    uint8_t held;                 /* keys held right now, when the keyboard can say: bit 0 a long key (W, SPACE),
                                   * bit 1 a short key (S); bit 7 set means it can (its raw mode answers) */
} SrRaw;
void sr_input_scan(uint8_t kb[8]);
void sr_overlay(void);

/* ---- sound: the web game's effects (fx.js), by name; the game calls SFX(id, arg) ---- */
enum { SFX_TAP, SFX_CANDLE, SFX_LOCK, SFX_OPEN, SFX_FLIP, SFX_WIN, SFX_BIG, SFX_LOSS, SFX_LIQ, SFX_RIGHT, SFX_WRONG,
       SFX_TICK, SFX_DING, SFX_COUNT, SFX_COMBO, SFX_CLINK, SFX_STAMP, SFX_COLLECT };
#define SR_AUDIO_RATE 22050
#ifndef SR_AUDIO_GAIN
#define SR_AUDIO_GAIN 7.0f   /* the speaker is small: fx.js's levels, lifted well up (a soft knee rounds the peaks) */
#endif
void sr_sfx(int id, int arg);
void sr_audio_render(int16_t *out, int n);  /* mono, SR_AUDIO_RATE */

/* ---- the platform: set by the firmware (or the desktop harness) before srush_main() ---- */
typedef struct {
    uint16_t *fb;                                  /* 320 x 240 RGB565, row-major */
    void (*present)(const uint16_t *fb);           /* show a finished frame */
    void (*raw)(struct SrRaw *r);                  /* read the T-Deck's inputs (srush_input.c turns them into keys) */
    uint32_t (*millis)(void);
    uint32_t (*unix_time)(void);                   /* 0 when unknown */
    const uint8_t *pack; size_t pack_len;          /* srpack.bin: the chart windows and the fonts */
    bool (*save_load)(uint8_t *buf, size_t cap, size_t *len);
    bool (*save_store)(const uint8_t *buf, size_t len);
    /* set by srush_input.c: a finger riding in a round, and where it is */
    bool touch_down; int16_t touch_x, touch_y;
    /* a tap on the menu or results, for the game to hit-test against its own layout (it clears tapped) */
    bool tapped; int16_t tap_x, tap_y;
} SrPlatform;
extern SrPlatform sr;
int srush_main(void);

#ifdef __cplusplus
}
#endif
