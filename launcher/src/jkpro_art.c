/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/* JKPro's own art: logo, football, night stadium. See jkpro_art.h.
 * Plain arithmetic and sqrtf only (no libm sin/cos/pow), so host goldens
 * stay exact; every heavy step runs once into a cache. */
#include "jkpro_art.h"
#include "jk_art.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- colour helpers */

static inline int ch(uint32_t c, int s) { return (int)((c >> s) & 255); }
static inline uint32_t rgb3(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}
static inline uint32_t lerpc(uint32_t a, uint32_t b, int t /* 0..256 */)
{
    return rgb3(ch(a, 16) + ((ch(b, 16) - ch(a, 16)) * t >> 8), ch(a, 8) + ((ch(b, 8) - ch(a, 8)) * t >> 8),
                ch(a, 0) + ((ch(b, 0) - ch(a, 0)) * t >> 8));
}
static inline uint32_t *px(JkFrame *f, int x, int y) { return (uint32_t *)(f->pixels + (size_t)y * f->stride) + x; }

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

/* The glow's two ends: pitch green and floodlight gold. */
#define C_GREEN 0x14E68AU
#define C_GOLD 0xFFB82EU

/* ---------------------------------------------------------------- the football */

static double jp_sin(double x)
{
    const double PI = 3.14159265358979323846;
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    double t = x, s = x, x2 = x * x;
    for (int k = 1; k < 10; k++) { t *= -x2 / ((2 * k) * (2 * k + 1)); s += t; }
    return s;
}
static double jp_cos(double x) { return jp_sin(x + 1.57079632679489661923); }

typedef struct { float x, y, z; } V3;
static V3 v3n(float x, float y, float z)
{
    float l = sqrtf(x * x + y * y + z * z);
    V3 v = { x / l, y / l, z / l };
    return v;
}
static float dot3(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/* Truncated icosahedron: 12 pentagon centres (icosahedron vertices) and 20
 * hexagon centres (dodecahedron vertices); a direction belongs to the face
 * whose plane its ray meets first (largest dot / face distance). */
static V3 g_pent[12], g_hex[20];
static int g_faces_ok;

static void faces_init(void)
{
    if (g_faces_ok) return;
    const float P = 1.6180339887f, Q = 0.6180339887f;
    int n = 0;
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) {
            g_pent[n++] = v3n(0, (float)a, b * P);
            g_pent[n++] = v3n((float)a, b * P, 0);
            g_pent[n++] = v3n(a * P, 0, (float)b);
        }
    n = 0;
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) {
            for (int c = -1; c <= 1; c += 2) g_hex[n++] = v3n((float)a, (float)b, (float)c);
            /* the icosahedron's face centres: the dual of (0, ±1, ±P) */
            g_hex[n++] = v3n(0, a * P, b * Q);
            g_hex[n++] = v3n(a * Q, 0, b * P);
            g_hex[n++] = v3n(a * P, b * Q, 0);
        }
    g_faces_ok = 1;
}

/* Rotate v about the unit axis k by (c = cos, s = sin). */
static V3 rot(V3 v, V3 k, float c, float s)
{
    V3 x = { k.y * v.z - k.z * v.y, k.z * v.x - k.x * v.z, k.x * v.y - k.y * v.x };
    float d = dot3(k, v) * (1 - c);
    V3 r = { v.x * c + x.x * s + k.x * d, v.y * c + x.y * s + k.y * d, v.z * c + x.z * s + k.z * d };
    return r;
}

/* Colour of the ball's surface at screen normal n (y down, z to the viewer). */
static void ball_shade(V3 n, float sc, float ss, float *r, float *g, float *b)
{
    static const V3 AX = { 0.32591f, 0.93118f, 0.16296f };      /* spin axis, tilted */
    static const V3 XA = { 1, 0, 0 };
    V3 body = rot(n, AX, sc, -ss);
    body = rot(body, XA, 0.85065f, 0.52573f);                     /* a pentagon faces the viewer at spin 0 */
    const float RP = 1.0f / 2.32744f, RH = 1.0f / 2.26728f;
    float best = -2, second = -2;
    int pent = 0;
    for (int i = 0; i < 12; i++) {
        float s = dot3(body, g_pent[i]) * RP;
        if (s > best) { second = best; best = s; pent = 1; }
        else if (s > second) second = s;
    }
    for (int i = 0; i < 20; i++) {
        float s = dot3(body, g_hex[i]) * RH;
        if (s > best) { second = best; best = s; pent = 0; }
        else if (s > second) second = s;
    }
    float br, bg, bb;
    if (pent) { br = 0.07f; bg = 0.08f; bb = 0.10f; }
    else { br = 0.95f; bg = 0.96f; bb = 0.97f; }
    float seam = (best - second) / best;
    if (seam < 0.010f) {                                          /* the stitched edges */
        float t = seam / 0.010f;
        float sr = pent ? 0.30f : 0.62f;
        br = sr + (br - sr) * t; bg = sr + (bg - sr) * t; bb = sr + 0.03f + (bb - sr - 0.03f) * t;
    }
    static const V3 L = { -0.45584f, -0.60779f, 0.65003f };
    static const V3 H = { -0.24525f, -0.32700f, 0.91265f };
    float dl = dot3(n, L), dh = dot3(n, H);
    float lam = 0.26f + 0.74f * (dl > 0 ? dl : 0);
    float sp = dh > 0 ? dh : 0;
    float s2 = sp * sp, s4 = s2 * s2, s8 = s4 * s4, s16 = s8 * s8, s32 = s16 * s16;
    float spec = s32 * s8 * (pent ? 0.55f : 0.45f);
    float rim = 1 - n.z;
    rim = rim * rim * rim;
    float tg = (n.x + 1) * 0.5f;                                  /* green on the left, gold on the right */
    float rr = (0.08f + (1.00f - 0.08f) * tg), rg = (0.90f + (0.72f - 0.90f) * tg), rb = (0.54f + (0.18f - 0.54f) * tg);
    *r = br * lam + spec + rim * 0.55f * rr;
    *g = bg * lam + spec + rim * 0.55f * rg;
    *b = bb * lam + spec + rim * 0.55f * rb;
}

typedef struct { int d, spin; uint32_t *argb; } BallSprite;
#define BALL_CACHE 24
static BallSprite g_balls[BALL_CACHE];
static int g_ball_next;

static const uint32_t *ball_sprite(int d, int spin)
{
    for (int i = 0; i < BALL_CACHE; i++)
        if (g_balls[i].argb && g_balls[i].d == d && g_balls[i].spin == spin) return g_balls[i].argb;
    faces_init();
    BallSprite *b = &g_balls[g_ball_next];
    g_ball_next = (g_ball_next + 1) % BALL_CACHE;
    free(b->argb);
    b->argb = malloc((size_t)d * d * 4);
    if (!b->argb) return NULL;
    b->d = d;
    b->spin = spin;
    double a = spin * (6.28318530717958647692 / 16.0);
    float sc = (float)jp_cos(a), ss = (float)jp_sin(a);
    float R = d * 0.5f;
    for (int y = 0; y < d; y++)
        for (int x = 0; x < d; x++) {
            float r = 0, g = 0, bl = 0;
            int hit = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    float fx = (x + (sx + 0.5f) * 0.25f - R) / R, fy = (y + (sy + 0.5f) * 0.25f - R) / R;
                    float q = fx * fx + fy * fy;
                    if (q >= 1) continue;
                    hit++;
                    /* the 4x4 samples count for the edge; the colour needs only every other one */
                    if ((sx | sy) & 1) continue;
                    V3 n = { fx, fy, sqrtf(1 - q) };
                    float cr, cg, cb;
                    ball_shade(n, sc, ss, &cr, &cg, &cb);
                    r += cr; g += cg; bl += cb;
                }
            uint32_t out = 0;
            if (hit) {
                /* colour: the mean of the coloured samples that hit (re-sampled at the centre when none did) */
                int ncol = 0;
                for (int sy = 0; sy < 4; sy += 2)
                    for (int sx = 0; sx < 4; sx += 2) {
                        float fx = (x + (sx + 0.5f) * 0.25f - R) / R, fy = (y + (sy + 0.5f) * 0.25f - R) / R;
                        if (fx * fx + fy * fy < 1) ncol++;
                    }
                if (!ncol) {
                    float fx = (x + 0.5f - R) / R, fy = (y + 0.5f - R) / R;
                    float q = fx * fx + fy * fy;
                    if (q > 0.9999f) q = 0.9999f;
                    V3 n = { fx, fy, sqrtf(1 - q) };
                    ball_shade(n, sc, ss, &r, &g, &bl);
                    ncol = 1;
                }
                int A = hit * 255 / 16;
                out = (uint32_t)A << 24 | rgb3((int)(r / ncol * 255), (int)(g / ncol * 255), (int)(bl / ncol * 255));
            }
            b->argb[(size_t)y * d + x] = out;
        }
    return b->argb;
}

void jkpro_ball(JkFrame *f, int cx, int cy, int d, int spin)
{
    if (!f || d < 8) return;
    if (d > 400) d = 400;
    spin &= 15;
    const uint32_t *s = ball_sprite(d, spin);
    if (!s) return;
    int x0 = cx - d / 2, y0 = cy - d / 2;
    for (int y = 0; y < d; y++) {
        int fy = y0 + y;
        if (fy < 0 || fy >= (int)f->height) continue;
        for (int x = 0; x < d; x++) {
            int fx = x0 + x;
            if (fx < 0 || fx >= (int)f->width) continue;
            uint32_t c = s[(size_t)y * d + x];
            int A = (int)(c >> 24);
            if (!A) continue;
            uint32_t *p = px(f, fx, fy);
            *p = A == 255 ? (c & 0xFFFFFF) : lerpc(*p, c & 0xFFFFFF, A * 256 / 255);
        }
    }
}

/* ---------------------------------------------------------------- the logo */

#define PAD 76
typedef struct {
    int ok, W, H, cap, cap_top, text_w, ball_cx, ball_cy, ball_d;
    uint32_t *glow;   /* additive 0x00RRGGBB */
    uint8_t *core;    /* the letters' coverage */
} Logo;
static Logo g_logo;

typedef struct { int cap, W, H; uint32_t *glow; uint8_t *core; } LogoScaled;
#define LOGO_SIZES 4
static LogoScaled g_ls[LOGO_SIZES];
static int g_ls_next;

/* Three box passes approximate a Gaussian. a: W*H ints, tmp the same size. */
static void blur(int *a, int *tmp, int W, int H, int r)
{
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < H; y++) {
            int *row = a + (size_t)y * W, *out = tmp + (size_t)y * W;
            long sum = 0;
            for (int x = -r; x <= r; x++) sum += row[x < 0 ? 0 : x >= W ? W - 1 : x];
            for (int x = 0; x < W; x++) {
                out[x] = (int)(sum / (2 * r + 1));
                int xo = x - r, xi = x + r + 1;
                sum += row[xi >= W ? W - 1 : xi] - row[xo < 0 ? 0 : xo];
            }
        }
        for (int x = 0; x < W; x++) {
            long sum = 0;
            for (int y = -r; y <= r; y++) sum += tmp[(size_t)(y < 0 ? 0 : y >= H ? H - 1 : y) * W + x];
            for (int y = 0; y < H; y++) {
                a[(size_t)y * W + x] = (int)(sum / (2 * r + 1));
                int yo = y - r, yi = y + r + 1;
                sum += tmp[(size_t)(yi >= H ? H - 1 : yi) * W + x] - tmp[(size_t)(yo < 0 ? 0 : yo) * W + x];
            }
        }
    }
}

static int logo_build(void)
{
    if (g_logo.ok) return 0;
    Logo *L = &g_logo;
    const char *text = "JKPR";
    int lh = jk_font_line_height(JK_FONT_LOGO), asc = jk_font_ascent(JK_FONT_LOGO);
    int tw = jk_text_width(JK_FONT_LOGO, text);
    /* the letters, white on black, to find the cap height */
    int TW = tw + 2 * PAD + 40, TH = lh + 2 * PAD;
    JkFrame t = { calloc((size_t)TW * TH, 4), (uint32_t)TW, (uint32_t)TH, (uint32_t)TW * 4 };
    if (!t.pixels) return -1;
    jk_text(&t, JK_FONT_LOGO, PAD, PAD, text, 0xFFFFFF);
    int base = PAD + asc, top = base;
    for (int y = 0; y < TH && top == base; y++)
        for (int x = 0; x < TW; x++)
            if (ch(*px(&t, x, y), 16)) { top = y; break; }
    int cap = base - top;
    int D = cap * 104 / 100, gap = cap * 8 / 100;
    L->cap = cap;
    L->cap_top = top;
    L->text_w = tw;
    L->ball_d = D;
    L->ball_cx = PAD + tw + gap + D / 2;
    L->ball_cy = base - D / 2 + cap / 60;
    L->W = PAD + tw + gap + D + PAD;
    L->H = TH;
    size_t n = (size_t)L->W * L->H;
    int *a = calloc(n, sizeof *a), *b = calloc(n, sizeof *b), *tmp = calloc(n, sizeof *tmp);
    L->glow = calloc(n, 4);
    L->core = calloc(n, 1);
    if (!a || !b || !tmp || !L->glow || !L->core) { free(a); free(b); free(tmp); free(t.pixels); return -1; }
    for (int y = 0; y < L->H; y++)
        for (int x = 0; x < L->W && x < TW; x++) {
            int v = ch(*px(&t, x, y), 16);
            L->core[(size_t)y * L->W + x] = (uint8_t)v;
            a[(size_t)y * L->W + x] = v * 16;
        }
    free(t.pixels);
    /* the ball's own halo moves with the ball (jkpro_ball_halo), so only the letters glow here */
    memcpy(b, a, n * sizeof *a);
    blur(a, tmp, L->W, L->H, cap / 14 + 2);     /* tight glow */
    blur(b, tmp, L->W, L->H, cap / 5 + 4);      /* wide haze */
    int span = L->W - 2 * PAD;
    for (int y = 0; y < L->H; y++)
        for (int x = 0; x < L->W; x++) {
            size_t i = (size_t)y * L->W + x;
            int t2 = (x - PAD) * 256 / (span > 0 ? span : 1);
            t2 = t2 < 0 ? 0 : t2 > 256 ? 256 : t2;
            uint32_t c = lerpc(C_GREEN, C_GOLD, t2);
            int g = a[i] * 15 / 160 + b[i] * 9 / 160;       /* /16 scale back, then the mix */
            if (g > 255) g = 255;
            L->glow[i] = rgb3(ch(c, 16) * g / 255, ch(c, 8) * g / 255, ch(c, 0) * g / 255);
        }
    free(a); free(b); free(tmp);
    L->ok = 1;
    return 0;
}

static const LogoScaled *logo_at(int cap)
{
    if (logo_build() != 0) return NULL;
    Logo *L = &g_logo;
    if (cap > L->cap) cap = L->cap;
    for (int i = 0; i < LOGO_SIZES; i++)
        if (g_ls[i].glow && g_ls[i].cap == cap) return &g_ls[i];
    LogoScaled *s = &g_ls[g_ls_next];
    g_ls_next = (g_ls_next + 1) % LOGO_SIZES;
    free(s->glow); free(s->core);
    s->cap = cap;
    s->W = (int)((long)L->W * cap / L->cap);
    s->H = (int)((long)L->H * cap / L->cap);
    if (s->W < 1) s->W = 1;
    if (s->H < 1) s->H = 1;
    s->glow = malloc((size_t)s->W * s->H * 4);
    s->core = malloc((size_t)s->W * s->H);
    if (!s->glow || !s->core) { free(s->glow); free(s->core); s->glow = NULL; s->core = NULL; return NULL; }
    for (int ty = 0; ty < s->H; ty++) {
        int y0 = (int)((long)ty * L->H / s->H), y1 = (int)((long)(ty + 1) * L->H / s->H);
        if (y1 <= y0) y1 = y0 + 1;
        for (int tx = 0; tx < s->W; tx++) {
            int x0 = (int)((long)tx * L->W / s->W), x1 = (int)((long)(tx + 1) * L->W / s->W);
            if (x1 <= x0) x1 = x0 + 1;
            long r = 0, g = 0, b = 0, c = 0, k = 0;
            for (int y = y0; y < y1 && y < L->H; y++)
                for (int x = x0; x < x1 && x < L->W; x++) {
                    uint32_t v = L->glow[(size_t)y * L->W + x];
                    r += ch(v, 16); g += ch(v, 8); b += ch(v, 0);
                    c += L->core[(size_t)y * L->W + x];
                    k++;
                }
            if (!k) k = 1;
            s->glow[(size_t)ty * s->W + tx] = rgb3((int)(r / k), (int)(g / k), (int)(b / k));
            s->core[(size_t)ty * s->W + tx] = (uint8_t)(c / k);
        }
    }
    return s;
}

int jkpro_logo_width(int cap)
{
    if (logo_build() != 0) return 0;
    if (cap > g_logo.cap) cap = g_logo.cap;
    return (int)((long)(g_logo.W - 2 * PAD) * cap / g_logo.cap);
}

JkProLogoBox jkpro_logo(JkFrame *f, int x, int y, int cap, int with_ball, int glow)
{
    JkProLogoBox box = { x, y, 0, cap, 0, 0, 0 };
    const LogoScaled *s = logo_at(cap);
    if (!s || !f) return box;
    Logo *L = &g_logo;
    cap = s->cap;
    int ox = x - (int)((long)PAD * cap / L->cap), oy = y - (int)((long)L->cap_top * cap / L->cap);
    for (int ty = 0; ty < s->H; ty++) {
        int fy = oy + ty;
        if (fy < 0 || fy >= (int)f->height) continue;
        for (int tx = 0; tx < s->W; tx++) {
            int fx = ox + tx;
            if (fx < 0 || fx >= (int)f->width) continue;
            uint32_t *p = px(f, fx, fy), d = *p, g = s->glow[(size_t)ty * s->W + tx];
            if (g && glow > 0)
                d = rgb3(ch(d, 16) + ch(g, 16) * glow / 256, ch(d, 8) + ch(g, 8) * glow / 256, ch(d, 0) + ch(g, 0) * glow / 256);
            int c = s->core[(size_t)ty * s->W + tx];
            if (c) d = lerpc(d, 0xFFFFFF, c * 256 / 255);
            *p = d;
        }
    }
    box.w = jkpro_logo_width(cap);
    box.h = cap;
    box.ball_d = (int)((long)L->ball_d * cap / L->cap);
    box.ball_cx = x + (int)((long)(L->ball_cx - PAD) * cap / L->cap);
    box.ball_cy = y + (int)((long)(L->ball_cy - L->cap_top) * cap / L->cap);
    if (with_ball) {
        jkpro_ball_halo(f, box.ball_cx, box.ball_cy, box.ball_d, glow);
        jkpro_ball(f, box.ball_cx, box.ball_cy, box.ball_d, 0);
    }
    return box;
}

void jkpro_ball_halo(JkFrame *f, int cx, int cy, int d, int glow)
{
    if (!f || d < 8 || glow <= 0) return;
    jk_glow(f, cx, cy, d * 84 / 100, d * 84 / 100, C_GOLD, 150 * glow / 256);
    jk_glow(f, cx, cy, d * 64 / 100, d * 64 / 100, 0xFFE2A0, 130 * glow / 256);
}

/* ---------------------------------------------------------------- the stadium */

static uint8_t *g_stadium;

/* An additive line from (x0,y0) to (x1,y1), w px thick, brightest at its head (x1,y1). */
static void streak(JkFrame *f, int x0, int y0, int x1, int y1, int w, uint32_t rgb, int alpha)
{
    int dx = x1 - x0, dy = y1 - y0;
    int len = dx > 0 ? dx : -dx;
    if (len < 1) return;
    for (int i = 0; i <= len; i++) {
        int x = x0 + (dx > 0 ? i : -i);
        int yc = y0 * 16 + dy * 16 * i / len;          /* 1/16 px */
        int a = alpha * i / len;                         /* fades in toward the head */
        for (int k = -w * 16; k <= w * 16; k += 16) {
            int yy = (yc + k) >> 4, frac = (yc + k) & 15;
            int fall = 256 - (k < 0 ? -k : k) * 256 / (w * 16 + 16);
            int aa = a * fall / 256;
            if (yy >= 0 && yy + 1 < (int)f->height && x >= 0 && x < (int)f->width) {
                jk_fill_add(f, x, yy, 1, 1, rgb, aa * (16 - frac) / 16);
                jk_fill_add(f, x, yy + 1, 1, 1, rgb, aa * frac / 16);
            }
        }
    }
}

/* A soft ring (an ellipse outline), additive. */
static void ring_ellipse(JkFrame *f, int cx, int cy, int rx, int ry, uint32_t rgb, int alpha, int th)
{
    for (int y = -ry - th; y <= ry + th; y++)
        for (int x = -rx - th; x <= rx + th; x++) {
            /* distance to the ellipse, in units of the minor axis */
            long q = (long)x * x * ry * ry + (long)y * y * rx * rx, r2 = (long)rx * rx * ry * ry;
            float d = (sqrtf((float)q) - sqrtf((float)r2)) / (float)rx;   /* ~ px along x */
            if (d < 0) d = -d;
            if (d > th) continue;
            int a = (int)(alpha * (1.0f - d / th));
            int fx = cx + x, fy = cy + y;
            if (fx >= 0 && fy >= 0 && fx < (int)f->width && fy < (int)f->height) jk_fill_add(f, fx, fy, 1, 1, rgb, a);
        }
}

static void build_stadium(JkFrame *f)
{
    const int HZ = 650;                                   /* the horizon: stands above, pitch below */
    jk_gradient_v(f, 0, 0, JK_W, HZ, 0x050913, 0x0C1830);
    jk_gradient_v(f, 0, HZ, JK_W, JK_H - HZ, 0x061A13, 0x0B3221);
    /* the stands: a dark band with a few crowd lights */
    for (int y = 520; y < HZ; y++) {
        int a = (y - 520) * 200 / (HZ - 520);
        jk_fill_alpha(f, 0, y, JK_W, 1, 0x03050B, a);
    }
    for (int i = 0; i < 900; i++) {
        uint32_t h = hash32((uint32_t)i * 2654435761U + 17);
        int x = (int)(h % JK_W), y = 560 + (int)((h >> 12) % 84);
        int a = 30 + (int)((h >> 20) % 70);
        jk_fill_add(f, x, y, 2, 2, (h & 1) ? 0xFFE7B0 : 0xA8D8FF, a);
    }
    /* floodlights: two towers with a hot core, a haze and a cone onto the pitch */
    static const int TX[2] = { 250, 1670 };
    for (int t = 0; t < 2; t++) {
        jk_glow(f, TX[t], 150, 760, 520, 0xFFD58A, 34);
        jk_glow(f, TX[t], 150, 260, 190, 0xFFE9C4, 70);
        jk_glow(f, TX[t], 150, 70, 46, 0xFFFFFF, 220);
        for (int r = 0; r < 2; r++)                     /* the lamp bank: 2 x 4 lamps */
            for (int k = 0; k < 4; k++) jk_glow(f, TX[t] - 39 + k * 26, 140 + r * 20, 11, 9, 0xFFFFFF, 255);
        /* the cone: widening toward the centre of the pitch */
        for (int y = 170; y < JK_H; y++) {
            int span = (y - 150) * 9 / 10;
            int cx = TX[t] + (960 - TX[t]) * (y - 150) / (JK_H - 150);
            int a = 26 - (y - 170) * 22 / (JK_H - 170);
            if (a > 0) jk_fill_add(f, cx - span / 2, y, span, 1, 0xFFE2A6, a);
        }
    }
    /* the pitch: mowing stripes, touchlines and the centre circle in perspective */
    for (int i = 0; i < 9; i++) {
        int y0 = HZ + (JK_H - HZ) * i * i / 81, y1 = HZ + (JK_H - HZ) * (i + 1) * (i + 1) / 81;
        if (i & 1) jk_fill_add(f, 0, y0, JK_W, y1 - y0, 0x0E4A30, 22);
    }
    for (int y = HZ; y < JK_H; y++) {
        int k = (y - HZ) * 1000 / (JK_H - HZ);
        int xl = 360 - 560 * k / 1000, xr = 1560 + 560 * k / 1000;
        jk_fill_add(f, xl - 1, y, 3, 1, 0x4FA77A, 70);
        jk_fill_add(f, xr - 1, y, 3, 1, 0x4FA77A, 70);
        jk_fill_add(f, 959, y, 3, 1, 0x4FA77A, 60);
    }
    jk_fill_add(f, 360, HZ, 1200, 2, 0x4FA77A, 60);
    ring_ellipse(f, 960, 868, 430, 118, 0x4FA77A, 80, 3);
    jk_glow(f, 960, 868, 16, 6, 0x9BE3BC, 120);
    /* JkFlow-style light streaks across the sky */
    streak(f, -40, 300, 1180, 236, 1, C_GREEN, 150);
    streak(f, 1060, 168, 1960, 118, 1, C_GOLD, 120);
    streak(f, -40, 520, 820, 470, 1, 0x7FE3C0, 90);
    streak(f, 1180, 470, 1960, 420, 1, C_GOLD, 80);
    /* vignette and grain */
    for (int y = 0; y < JK_H; y++)
        for (int x = 0; x < JK_W; x++) {
            int dx = (x - 960) * 1000 / 960, dy = (y - 520) * 1000 / 620;
            long d2 = (long)dx * dx + (long)dy * dy;           /* 0 .. ~2e6 */
            int v = d2 > 600000 ? (int)((d2 - 600000) / 9000) : 0;
            if (v > 150) v = 150;
            uint32_t *p = px(f, x, y);
            int n = (int)(hash32((uint32_t)(y * JK_W + x)) & 7) - 3;
            *p = rgb3(ch(*p, 16) * (256 - v) / 256 + n, ch(*p, 8) * (256 - v) / 256 + n, ch(*p, 0) * (256 - v) / 256 + n);
        }
}

void jkpro_stadium(JkFrame *f)
{
    size_t n = (size_t)JK_W * JK_H * 4;
    if (!g_stadium) {
        g_stadium = malloc(n);
        if (g_stadium) {
            JkFrame c = { g_stadium, JK_W, JK_H, JK_W * 4 };
            build_stadium(&c);
        }
    }
    if (!g_stadium) { build_stadium(f); return; }
    uint32_t rows = f->height < JK_H ? f->height : JK_H, cols = f->width < JK_W ? f->width : JK_W;
    for (uint32_t y = 0; y < rows; y++)
        memcpy(f->pixels + (size_t)y * f->stride, g_stadium + (size_t)y * JK_W * 4, (size_t)cols * 4);
}

void jkpro_art_release(void)
{
    for (int i = 0; i < BALL_CACHE; i++) { free(g_balls[i].argb); g_balls[i].argb = NULL; }
    for (int i = 0; i < LOGO_SIZES; i++) { free(g_ls[i].glow); free(g_ls[i].core); g_ls[i].glow = NULL; g_ls[i].core = NULL; }
    free(g_logo.glow); free(g_logo.core);
    memset(&g_logo, 0, sizeof g_logo);
    free(g_stadium);
    g_stadium = NULL;
}
