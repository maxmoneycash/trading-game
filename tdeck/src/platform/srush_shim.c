/* The graphx / keypadc / fileioc slice Setup Rush needs, on a 320x240 RGB565 framebuffer (see srush_shim.h). */
#include "srush_shim.h"

SrPlatform sr;
uint16_t sr_palette[256];
uint8_t sr_vram_dummy[1];
uint8_t kb_Data[8];

#define W 320
#define H 240
static uint16_t cur;          /* the current colour, 565 */
static uint8_t transIndex = 255;

static inline uint16_t to565(uint16_t c)
{
    uint16_t r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
    return (uint16_t)((r << 11) | (((g << 1) | (g >> 4)) << 5) | b);
}

void gfx_Begin(void)
{
    for (int i = 0; i < 256; i++) sr_palette[i] = gfx_RGBTo1555(i, i, i);
    sr_palette[255] = 0x7C1F; /* the transparent index: magenta, never a game colour */
}
void gfx_End(void) {}
void gfx_SetDraw(uint8_t where) { (void)where; }
void gfx_SwapDraw(void) { sr_overlay(); if (sr.present) sr.present(sr.fb); }
uint8_t gfx_SetColor(uint8_t index) { cur = to565(sr_palette[index]); return index; }
void gfx_FillScreen(uint8_t index)
{
    uint16_t c = to565(sr_palette[index]);
    uint16_t *p = sr.fb;
    for (int i = 0; i < W * H; i++) p[i] = c;
}
void gfx_SetPixel(int x, int y) { if ((unsigned)x < W && (unsigned)y < H) sr.fb[y * W + x] = cur; }

static void hspan(int x, int y, int len)
{
    if ((unsigned)y >= H || len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > W) len = W - x;
    uint16_t *p = sr.fb + y * W + x;
    while (len-- > 0) *p++ = cur;
}
void gfx_HorizLine(int x, int y, int length) { hspan(x, y, length); }
void gfx_HorizLine_NoClip(int x, int y, int length) { hspan(x, y, length); }
void gfx_VertLine(int x, int y, int length)
{
    if ((unsigned)x >= W || length <= 0) return;
    if (y < 0) { length += y; y = 0; }
    if (y + length > H) length = H - y;
    uint16_t *p = sr.fb + y * W + x;
    while (length-- > 0) { *p = cur; p += W; }
}
void gfx_VertLine_NoClip(int x, int y, int length) { gfx_VertLine(x, y, length); }
void gfx_FillRectangle(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > H) h = H - y;
    for (int r = 0; r < h; r++) hspan(x, y + r, w);
}
void gfx_FillRectangle_NoClip(int x, int y, int w, int h) { gfx_FillRectangle(x, y, w, h); }
void gfx_Rectangle(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    hspan(x, y, w); hspan(x, y + h - 1, w);
    gfx_VertLine(x, y, h); gfx_VertLine(x + w - 1, y, h);
}
void gfx_Line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    if (dx > 2000 || -dy > 2000) return; /* nothing that long is on screen */
    for (;;) {
        gfx_SetPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void gfx_Circle(int xc, int yc, unsigned r)
{
    int x = (int)r, y = 0, d = 1 - x;
    while (x >= y) {
        gfx_SetPixel(xc + x, yc + y); gfx_SetPixel(xc + y, yc + x); gfx_SetPixel(xc - y, yc + x); gfx_SetPixel(xc - x, yc + y);
        gfx_SetPixel(xc - x, yc - y); gfx_SetPixel(xc - y, yc - x); gfx_SetPixel(xc + y, yc - x); gfx_SetPixel(xc + x, yc - y);
        y++;
        if (d < 0) d += 2 * y + 1; else { x--; d += 2 * (y - x) + 1; }
    }
}
void gfx_FillCircle(int xc, int yc, unsigned r)
{
    int rr = (int)r;
    for (int dy = -rr; dy <= rr; dy++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= rr * rr + rr) dx++;
        hspan(xc - dx, yc + dy, 2 * dx + 1);
    }
}
void gfx_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int t;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    for (int y = y0; y <= y2; y++) {
        float a = y2 != y0 ? x0 + (float)(x2 - x0) * (y - y0) / (y2 - y0) : x0, b;
        if (y < y1) b = y1 != y0 ? x0 + (float)(x1 - x0) * (y - y0) / (y1 - y0) : x0;
        else b = y2 != y1 ? x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1) : x1;
        int l = (int)(a < b ? a : b), r = (int)(a < b ? b : a);
        hspan(l, y, r - l + 1);
    }
}

gfx_sprite_t *sr_malloc_sprite(int w, int h)
{
    gfx_sprite_t *s = (gfx_sprite_t *)malloc(sizeof(gfx_sprite_t) + (size_t)w * h * 2);
    if (s) { s->width = (uint8_t)w; s->height = (uint8_t)h; }
    return s;
}
void gfx_GetSprite(gfx_sprite_t *s, int x, int y)
{
    for (int r = 0; r < s->height; r++)
        for (int c = 0; c < s->width; c++) {
            int X = x + c, Y = y + r;
            s->data[r * s->width + c] = ((unsigned)X < W && (unsigned)Y < H) ? sr.fb[Y * W + X] : to565(sr_palette[transIndex]);
        }
}
void gfx_TransparentSprite(const gfx_sprite_t *s, int x, int y)
{
    uint16_t key = to565(sr_palette[transIndex]);
    for (int r = 0; r < s->height; r++) {
        int Y = y + r; if ((unsigned)Y >= H) continue;
        for (int c = 0; c < s->width; c++) {
            int X = x + c; uint16_t v = s->data[r * s->width + c];
            if ((unsigned)X < W && v != key) sr.fb[Y * W + X] = v;
        }
    }
}
uint8_t gfx_SetTransparentColor(uint8_t index) { uint8_t o = transIndex; transIndex = index; return o; }

/* graphx text fallback (only without SRFNT): 8 px per character, drawn as blocks */
static uint8_t tScale = 1, tFg;
void gfx_SetTextScale(uint8_t w, uint8_t h) { (void)h; tScale = w ? w : 1; }
uint8_t gfx_SetTextFGColor(uint8_t c) { uint8_t o = tFg; tFg = c; return o; }
uint8_t gfx_SetTextBGColor(uint8_t c) { return c; }
uint8_t gfx_SetTextTransparentColor(uint8_t c) { return c; }
void gfx_SetMonospaceFont(uint8_t spacing) { (void)spacing; }
void gfx_SetTextConfig(uint8_t c) { (void)c; }
unsigned gfx_GetStringWidth(const char *s) { return (unsigned)strlen(s) * 8u * tScale; }
void gfx_PrintStringXY(const char *s, int x, int y)
{
    gfx_SetColor(tFg);
    for (; *s; s++, x += 8 * tScale) if (*s != ' ') gfx_FillRectangle(x + tScale, y + tScale, 6 * tScale, 6 * tScale);
}

void kb_Scan(void) { memset(kb_Data, 0, sizeof kb_Data); sr_input_scan(kb_Data); }

/* ---- fileioc ---- */
typedef struct { bool used, write, save; const uint8_t *data; size_t size, pos; } Slot;
#define NSLOT 6
static Slot slots[NSLOT];
#define SAVE_CAP 512
static uint8_t saveBuf[SAVE_CAP];

static const uint8_t *pack_find(const char *name, size_t *len)
{
    /* 'SRPK', u16 n, n x (char name[8], u32 offset, u32 length) */
    const uint8_t *p = sr.pack;
    if (!p || sr.pack_len < 6 || memcmp(p, "SRPK", 4)) return NULL;
    unsigned n = p[4] | (p[5] << 8);
    for (unsigned i = 0; i < n; i++) {
        const uint8_t *e = p + 6 + i * 16;
        if (strncmp((const char *)e, name, 8) == 0) {
            uint32_t off = e[8] | (e[9] << 8) | (e[10] << 16) | ((uint32_t)e[11] << 24);
            uint32_t ln = e[12] | (e[13] << 8) | (e[14] << 16) | ((uint32_t)e[15] << 24);
            *len = ln;
            return p + off;
        }
    }
    return NULL;
}
ti_var_t ti_Open(const char *name, const char *mode)
{
    int s = 0;
    while (s < NSLOT && slots[s].used) s++;
    if (s == NSLOT) return 0;
    Slot *v = &slots[s];
    memset(v, 0, sizeof *v);
    if (!strcmp(name, "SRSAVE")) {
        v->save = true;
        if (mode[0] == 'w') { v->write = true; v->size = 0; }
        else { size_t len = 0; if (!sr.save_load || !sr.save_load(saveBuf, SAVE_CAP, &len) || !len) return 0; v->size = len; }
        v->data = saveBuf; v->used = true;
        return (ti_var_t)(s + 1);
    }
    if (mode[0] != 'r') return 0; /* the screenshot variables of the calculator's self-test: not here */
    size_t len = 0;
    const uint8_t *d = pack_find(name, &len);
    if (!d) return 0;
    v->data = d; v->size = len; v->used = true;
    return (ti_var_t)(s + 1);
}
int ti_Close(ti_var_t h)
{
    if (!h || h > NSLOT) return 0;
    Slot *v = &slots[h - 1];
    if (v->save && v->write && sr.save_store) sr.save_store(saveBuf, v->size);
    v->used = false;
    return 1;
}
size_t ti_Read(void *data, size_t size, size_t count, ti_var_t h)
{
    if (!h || h > NSLOT) return 0;
    Slot *v = &slots[h - 1];
    size_t n = 0;
    while (n < count && v->pos + size <= v->size) { memcpy((uint8_t *)data + n * size, v->data + v->pos, size); v->pos += size; n++; }
    return n;
}
size_t ti_Write(const void *data, size_t size, size_t count, ti_var_t h)
{
    if (!h || h > NSLOT) return 0;
    Slot *v = &slots[h - 1];
    if (!v->save || !v->write) return 0;
    size_t n = 0;
    while (n < count && v->pos + size <= SAVE_CAP) { memcpy(saveBuf + v->pos, (const uint8_t *)data + n * size, size); v->pos += size; n++; }
    if (v->pos > v->size) v->size = v->pos;
    return n;
}
uint16_t ti_GetSize(ti_var_t h) { return (h && h <= NSLOT) ? (uint16_t)slots[h - 1].size : 0; }
void *ti_GetDataPtr(ti_var_t h) { return (h && h <= NSLOT) ? (void *)(slots[h - 1].data + slots[h - 1].pos) : NULL; }
int ti_Rewind(ti_var_t h) { if (h && h <= NSLOT) slots[h - 1].pos = 0; return 1; }
int ti_SetArchiveStatus(bool archived, ti_var_t h) { (void)archived; (void)h; return 1; }

uint32_t sr_clock(void) { return sr.millis ? sr.millis() : 0; }
time_t sr_time(time_t *t)
{
    time_t v = sr.unix_time ? (time_t)sr.unix_time() : 0;
    if (!v) v = (time_t)1795478400; /* without a clock (no GPS fix yet): Nov 24 2026 */
    if (t) *t = v;
    return v;
}
