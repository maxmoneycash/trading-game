/* Setup Rush on the T-Deck: the TI-84 Plus CE game (game/main.c) runs unchanged on top of this.
 * It provides the slice of the CE toolchain's graphx, keypadc, fileioc and sys/rtc the game calls, over a 320x240
 * RGB565 framebuffer, the T-Deck's inputs and an embedded data pack. Palette entries stay 1555 (the game writes them
 * with gfx_RGBTo1555); a colour is resolved to 565 when it is set, so a palette change applies to what is drawn next. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t uint24_t;

#include "srush_platform.h"

/* ---- graphx ---- */
#define GFX_LCD_WIDTH 320
#define GFX_LCD_HEIGHT 240
extern uint16_t sr_palette[256];
#define gfx_palette sr_palette
#define gfx_RGBTo1555(r, g, b) ((uint16_t)(((((r) & 255) >> 3) << 10) | ((((g) & 255) >> 3) << 5) | (((b) & 255) >> 3)))
typedef struct { uint8_t width, height; uint16_t data[]; } gfx_sprite_t;
#define gfx_MallocSprite(w, h) ((gfx_sprite_t *)sr_malloc_sprite((w), (h)))
gfx_sprite_t *sr_malloc_sprite(int w, int h);
/* screenshot path in the game (never taken here): keep it compiling */
extern uint8_t sr_vram_dummy[1];
#define gfx_vram (sr_vram_dummy)
enum { gfx_screen = 0, gfx_buffer = 1 };
enum { gfx_text_clip = 1, gfx_text_noclip = 2 };

void gfx_Begin(void);
void gfx_End(void);
void gfx_SetDraw(uint8_t where);
#define gfx_SetDrawBuffer() gfx_SetDraw(gfx_buffer)
void gfx_SwapDraw(void);
uint8_t gfx_SetColor(uint8_t index);
void gfx_FillScreen(uint8_t index);
void gfx_SetPixel(int x, int y);
/* T-Deck only: one of the app's emoji candle sprites (set 0..5, up or down), its top-left at x, y, blended in */
void sr_emoji(int set, int up, int x, int y);
int sr_emoji_sets(void);
const char *sr_emoji_name(int set);
void gfx_HorizLine(int x, int y, int length);
void gfx_HorizLine_NoClip(int x, int y, int length);
void gfx_VertLine(int x, int y, int length);
void gfx_VertLine_NoClip(int x, int y, int length);
void gfx_Line(int x0, int y0, int x1, int y1);
void gfx_Rectangle(int x, int y, int w, int h);
void gfx_FillRectangle(int x, int y, int w, int h);
void gfx_FillRectangle_NoClip(int x, int y, int w, int h);
void gfx_Circle(int x, int y, unsigned r);
void gfx_FillCircle(int x, int y, unsigned r);
void gfx_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2);
void gfx_GetSprite(gfx_sprite_t *s, int x, int y);
void gfx_TransparentSprite(const gfx_sprite_t *s, int x, int y);
#define gfx_TransparentSprite_NoClip gfx_TransparentSprite
uint8_t gfx_SetTransparentColor(uint8_t index);
/* graphx's own 8x8 text: the game only falls back to it without SRFNT */
void gfx_SetTextScale(uint8_t w, uint8_t h);
uint8_t gfx_SetTextFGColor(uint8_t c);
uint8_t gfx_SetTextBGColor(uint8_t c);
uint8_t gfx_SetTextTransparentColor(uint8_t c);
void gfx_SetMonospaceFont(uint8_t spacing);
void gfx_PrintStringXY(const char *s, int x, int y);
unsigned gfx_GetStringWidth(const char *s);
void gfx_SetTextConfig(uint8_t c);

/* ---- keypadc ---- */
extern uint8_t kb_Data[8];
void kb_Scan(void);
#define kb_2nd (1 << 5)
#define kb_Enter (1 << 0)
#define kb_Clear (1 << 6)
#define kb_Down (1 << 0)
#define kb_Left (1 << 1)
#define kb_Right (1 << 2)
#define kb_Up (1 << 3)

/* ---- fileioc: named variables over the data pack (read-only) and SRSAVE (the saved game) ---- */
typedef uint8_t ti_var_t;
ti_var_t ti_Open(const char *name, const char *mode);
int ti_Close(ti_var_t v);
size_t ti_Read(void *data, size_t size, size_t count, ti_var_t v);
size_t ti_Write(const void *data, size_t size, size_t count, ti_var_t v);
uint16_t ti_GetSize(ti_var_t v);
void *ti_GetDataPtr(ti_var_t v);
int ti_Rewind(ti_var_t v);
int ti_SetArchiveStatus(bool archived, ti_var_t v);

/* ---- sys/rtc, debug, time ---- */
#define rtc_Time() (sr.millis ? sr.millis() * 2654435761u : 1u)
#define dbg_printf(...) ((void)0)
uint32_t sr_clock(void);
time_t sr_time(time_t *t);
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000UL
#define clock sr_clock
#define time sr_time

#ifdef __cplusplus
}
#endif
