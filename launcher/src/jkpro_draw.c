/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * The JKPro screens as pure drawing from the model (design/mockups rev 2).
 * Own art only: a stadium at night with pitch lines, flat fills, a solid amber
 * focus. The background is built once into a cache and copied. Integer maths
 * only, so host goldens are exact.
 */
#include "jkpro_ui.h"
#include "jkpro_kitcfg.h"
#include "jkpro_fservfix.h"

/* Paths and UI texts are cut to their buffers on purpose (a path longer than
 * PATH_MAX_JP is refused by the file calls that follow); GCC's truncation
 * guesses are not errors here. */
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "jk_art.h"
#include "jkpro_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* palette */
enum {
    C_NIGHT = 0x0D1A24, C_NIGHT2 = 0x09131B, C_STAND = 0x152634, C_STAND2 = 0x1D3344, C_LINE = 0x2B4559,
    C_PITCH = 0x1B3040, C_CHALK = 0xEEF3F6, C_DIM = 0xA3B6C4, C_FLOOD = 0xF3B640, C_INK = 0x1A1405,
    C_OK = 0x4CC38A, C_BAD = 0xFF7B70, C_OKBG = 0x173A2C, C_WARNBG = 0x3A3016, C_MUTEBG = 0x22384A, C_BADBG = 0x3C1D1A
};

enum { X0 = 150, X1 = 1770, HINT_Y = 972, HINT_LINE = 950, TOP_Y = 70, GLYPH = 40 };

/* ---------------------------------------------------------------- background */

static uint8_t *bg_cache;

static int isqrt(long long v)
{
    if (v <= 0) return 0;
    long long r = 0, bit = 1ll << 40;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int)r;
}

/* A ring (outer radius ro, thickness th) as horizontal spans. */
static void ring(JkFrame *f, int cx, int cy, int ro, int th, uint32_t rgb)
{
    int ri = ro - th;
    for (int dy = -ro; dy <= ro; dy++) {
        int xo = isqrt((long long)ro * ro - (long long)dy * dy);
        if (dy > -ri && dy < ri) {
            int xi = isqrt((long long)ri * ri - (long long)dy * dy);
            jk_fill(f, cx - xo, cy + dy, xo - xi, 1, rgb);
            jk_fill(f, cx + xi, cy + dy, xo - xi, 1, rgb);
        } else {
            jk_fill(f, cx - xo, cy + dy, 2 * xo, 1, rgb);
        }
    }
}

static void build_bg(JkFrame *f)
{
    jk_gradient_v(f, 0, 0, JK_W, JK_H, C_NIGHT, C_NIGHT2);
    /* floodlight haze from the top, drawn as a few flat bands */
    for (int i = 0; i < 6; i++) jk_fill_alpha(f, 0, 0, JK_W, 40 + i * 30, 0x1F3A4E, 10);
    /* the pitch: halfway line and centre circle, right side */
    ring(f, 1700, 560, 380, 4, C_PITCH);
    jk_fill(f, 1698, 0, 4, JK_H, C_PITCH);
    ring(f, 1700, 560, 12, 12, C_PITCH);
    /* the penalty box arc at the bottom left */
    jk_fill(f, 0, 1042, 520, 4, C_PITCH);
    jk_fill(f, 516, 1042, 4, 38, C_PITCH);
}

static void background(JkFrame *f)
{
    size_t n = (size_t)JK_W * JK_H * 4;
    if (!bg_cache) {
        bg_cache = malloc(n);
        if (bg_cache) {
            JkFrame c = { bg_cache, JK_W, JK_H, JK_W * 4 };
            build_bg(&c);
        }
    }
    if (!bg_cache) { build_bg(f); return; }
    uint32_t rows = f->height < JK_H ? f->height : JK_H, cols = f->width < JK_W ? f->width : JK_W;
    for (uint32_t y = 0; y < rows; y++) memcpy(f->pixels + (size_t)y * f->stride, bg_cache + (size_t)y * JK_W * 4, cols * 4);
}

void jkpro_draw_release(void)
{
    free(bg_cache);
    jkpro_art_release();
    bg_cache = NULL;
}

/* ---------------------------------------------------------------- pieces */

static void frame_rect(JkFrame *f, int x, int y, int w, int h, int t, uint32_t rgb)
{
    jk_fill(f, x, y, w, t, rgb);
    jk_fill(f, x, y + h - t, w, t, rgb);
    jk_fill(f, x, y, t, h, rgb);
    jk_fill(f, x + w - t, y, t, h, rgb);
}

static void panel(JkFrame *f, int x, int y, int w, int h)
{
    jk_fill(f, x, y, w, h, C_STAND);
    frame_rect(f, x, y, w, h, 2, C_LINE);
}

static int text_right(JkFrame *f, JkFont font, int xr, int y, const char *s, uint32_t rgb)
{
    int w = jk_text_width(font, s);
    jk_text(f, font, xr - w, y, s, rgb);
    return w;
}

/* Draws s, shortened with "…" to fit max_w. */
static void text_fit(JkFrame *f, JkFont font, int x, int y, const char *s, uint32_t rgb, int max_w)
{
    if (jk_text_width(font, s) <= max_w) { jk_text(f, font, x, y, s, rgb); return; }
    char b[256];
    snprintf(b, sizeof b, "%s", s);
    size_t n = strlen(b);
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)b[n] & 0xC0) == 0x80) n--;
        b[n] = 0;
        char t[264];
        snprintf(t, sizeof t, "%s…", b);
        if (jk_text_width(font, t) <= max_w) { jk_text(f, font, x, y, t, rgb); return; }
    }
}

/* Word-wrapped text; returns the height used. */
static int wrap(JkFrame *f, JkFont font, int x, int y, int max_w, const char *s, uint32_t rgb)
{
    int lh = jk_font_line_height(font) + 6, yy = y;
    char line[512] = "";
    const char *p = s;
    while (*p) {
        const char *sp = strchr(p, ' ');
        size_t wl = sp ? (size_t)(sp - p) : strlen(p);
        char word[256];
        if (wl >= sizeof word) wl = sizeof word - 1;
        memcpy(word, p, wl);
        word[wl] = 0;
        char trial[1024];
        snprintf(trial, sizeof trial, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && jk_text_width(font, trial) > max_w) {
            jk_text(f, font, x, yy, line, rgb);
            yy += lh;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", trial);
        }
        p += wl;
        while (*p == ' ') p++;
    }
    if (line[0]) { jk_text(f, font, x, yy, line, rgb); yy += lh; }
    return yy - y;
}

static void chip(JkFrame *f, int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    int w = jk_text_width(JK_FONT_HINT, s) + 32, h = jk_font_line_height(JK_FONT_HINT) + 8;
    jk_fill(f, x, y, w, h, bg);
    jk_text(f, JK_FONT_HINT, x + 16, y + 4, s, fg);
}

static int chip_w(const char *s) { return jk_text_width(JK_FONT_HINT, s) + 32; }

/* The JKPRO logo (jkpro_art: glowing letters, the O a football), small. */
static void wordmark(JkFrame *f)
{
    jkpro_logo(f, X0, TOP_Y - 8, 46, 1, 230);
}

static void heading(JkFrame *f, const char *title, const char *sub)
{
    int w = jk_text(f, JK_FONT_HEAD, X0, TOP_Y - 10, title, C_CHALK);
    if (sub && sub[0]) jk_text(f, JK_FONT_BODY, X0 + w + 24, TOP_Y - 4, sub, C_DIM);
}

typedef struct { JkButton b; const char *label; } Hint;

/* The hint bar, right-aligned; a NULL-label entry ends the list. b < 0: text only. */
static void hints(JkFrame *f, const Hint *h, int n)
{
    jk_fill(f, X0, HINT_LINE, X1 - X0, 2, C_LINE);
    int x = X1;
    for (int i = n - 1; i >= 0; i--) {
        int w = jk_text_width(JK_FONT_HINT, h[i].label);
        x -= w;
        jk_text(f, JK_FONT_HINT, x, HINT_Y, h[i].label, C_DIM);
        if ((int)h[i].b >= 0) {
            x -= GLYPH + 12;
            jk_button_glyph(f, x, HINT_Y - 4, GLYPH, h[i].b);
        }
        x -= 48;
    }
}

static void hint_text_left(JkFrame *f, const char *s)
{
    jk_text(f, JK_FONT_HINT, X0, HINT_Y, s, C_DIM);
}

/* "2026-11-25_1802" -> "2026-11-25 18:02"; anything else as it is. */
static void nice_stamp(const char *s, char *out, size_t cap)
{
    if (strlen(s) >= 15 && s[10] == '_')
        snprintf(out, cap, "%.10s %.2s:%.2s%s", s, s + 11, s + 13, s + 15);
    else
        snprintf(out, cap, "%s", s);
}

static const char *game_title(const JpModel *m)
{
    return m->game_tile >= 0 ? m->tiles[m->game_tile].title : "PES 2013";
}
static const char *game_folder(const JpModel *m)
{
    return m->game_tile >= 0 ? m->tiles[m->game_tile].folder : "";
}

/* ---------------------------------------------------------------- shelf */

enum { TILE_W = 300, TILE_H = 400, TILE_FW = 360, TILE_FH = 480, TILE_GAP = 40, TILE_BOTTOM = 760 };

static void tile(JkFrame *f, const JpTile *t, int x, int focus)
{
    int w = focus ? TILE_FW : TILE_W, h = focus ? TILE_FH : TILE_H, y = TILE_BOTTOM - h;
    jk_fill(f, x, y, w, h, C_STAND);
    frame_rect(f, x, y, w, h, focus ? 6 : 2, focus ? C_FLOOD : C_LINE);
    /* our pitch motif: centre circle and halfway line */
    int r = w / 5, cyc = y + h / 2;
    ring(f, x + w / 2, cyc, r, 3, 0x2F4A5E);
    jk_fill(f, x + w / 2 - 1, cyc - r - 20, 3, 2 * r + 40, 0x2F4A5E);
    JkFont tf = focus ? JK_FONT_MENU : JK_FONT_HEAD;
    if (jk_text_width(tf, t->title) > w - 48) tf = JK_FONT_HEAD;
    text_fit(f, tf, x + 24, y + h - jk_font_line_height(tf) - 64, t->title, C_CHALK, w - 48);
    /* owner (rc8): always one short state line under the name, with a dot */
    uint32_t sc = t->state == JP_ST_READY ? C_OK : (t->state == JP_ST_NEEDS_PATCH || t->state == JP_ST_PATCH_AGAIN) ? C_FLOOD
                                                                                                                  : C_BAD;
    jk_fill(f, x + 26, y + h - 40, 14, 14, sc);
    jk_text(f, JK_FONT_HINT, x + 50, y + h - 52, jkpro_state_text(t->state), sc);

}

/* No PES 2013 folder yet: where to put it. */
static void draw_empty(JkFrame *f, const JpModel *m)
{
    int x = X0, y = 280, w = 1100, h = 400;
    panel(f, x, y, w, h);
    jk_text(f, JK_FONT_MENU, x + 48, y + 40, "PES 2013 not found", C_CHALK);
    char t[400];
    snprintf(t, sizeof t, "Copy your whole PES 2013 folder (with pes2013.exe) over FTP to %s/, for example %s. Then close "
             "JKPro and open it again: the game shows up here.", m->root_path, m->game_path);
    wrap(f, JK_FONT_BODY, x + 48, y + 150, w - 96, t, C_DIM);
    Hint hb[] = { { JK_BTN_SQUARE, "Help" } };
    hints(f, hb, 1);
}

static void draw_shelf(JkFrame *f, const JpModel *m, int dimmed)
{
    wordmark(f);
    if (m->ntiles == 0) { draw_empty(f, m); return; }
    /* 1.0 has one tile; the row scrolls when later releases add games */
    int first = m->tile_focus > 3 ? m->tile_focus - 3 : 0, x = X0;
    for (int i = first; i < m->ntiles && x < X1; i++) {
        tile(f, &m->tiles[i], x, i == m->tile_focus);
        x += (i == m->tile_focus ? TILE_FW : TILE_W) + TILE_GAP;
    }
    if (dimmed) { jk_fill_alpha(f, 0, 0, JK_W, JK_H, C_NIGHT2, 170); return; }
    const JpTile *t = &m->tiles[m->tile_focus];
    const char *play = t->state == JP_ST_READY ? "Play" : (t->state == JP_ST_NEEDS_PATCH || t->state == JP_ST_PATCH_AGAIN)
                                                            ? "Patch & play" : "Why?";
    Hint h[] = { { JK_BTN_CROSS, play }, { JK_BTN_TRIANGLE, "Patch & options" }, { JK_BTN_SQUARE, "Help" } };
    hints(f, h, 3);
}

/* ---------------------------------------------------------------- menu */

static const char *MENU_TEXT[JP_MENU_COUNT] = { "PLAY", "PATCH", "SETTINGS", "SAVE GAMES", "DATA PACKS" };

static void menu_item(JkFrame *f, int x, int y, int w, const char *s, const char *note, int focus)
{
    int h = jk_font_line_height(JK_FONT_MENU) + 12;
    if (focus) jk_fill(f, x, y, w, h, C_FLOOD);
    jk_text(f, JK_FONT_MENU, x + 28, y + 6, s, focus ? C_INK : C_DIM);
    if (note && note[0]) text_right(f, JK_FONT_HINT, x + w - 28, y + h / 2 - 16, note, focus ? 0x4A3A10 : C_DIM);
}

static void draw_menu(JkFrame *f, const JpModel *m)
{
    draw_shelf(f, m, 1);
    int rows = m->dp_on ? JP_MENU_COUNT : JP_MENU_DATAPACKS;
    int x = 640, w = 640, h = 96 + rows * (jk_font_line_height(JK_FONT_MENU) + 28) + 16, y = (JK_H - h) / 2 - 40;
    panel(f, x, y, w, h);
    char head[128];
    snprintf(head, sizeof head, "%s  ·  %s", game_title(m), game_folder(m));
    text_fit(f, JK_FONT_BODY, x + 28, y + 24, head, C_DIM, w - 56);
    const JpTile *g = m->game_tile >= 0 ? &m->tiles[m->game_tile] : NULL;
    const char *pnote = !g ? "" : g->state == JP_ST_READY ? "done" : g->state == JP_ST_PATCH_AGAIN ? "again" : "needed";
    int step = jk_font_line_height(JK_FONT_MENU) + 28;
    for (int i = 0; i < rows; i++)
        menu_item(f, x + 24, y + 96 + i * step, w - 48, MENU_TEXT[i], i == JP_MENU_PATCH ? pnote : "", i == m->menu_focus);
    Hint hb[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, "Select" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, hb, 3);
}

/* ---------------------------------------------------------------- patch */

static const char *STEP_TEXT[JP_STEP_COUNT] = {
    "Check game folder", "Prepare Windows layer", "Install JKPro files", "Import registry file", "Write settings",
    "Check and record"
};

static void draw_patch(JkFrame *f, const JpModel *m)
{
    int done = 0, shown = 0, now = 0;
    for (int i = 0; i < JP_STEP_COUNT; i++) {
        if (m->steps[i] == JP_SS_HIDDEN) continue;
        shown++;
        done += m->steps[i] == JP_SS_OK;
        if (i <= m->step_now) now = shown;
    }
    char st[96];
    snprintf(st, sizeof st, "%s  ·  Step %d of %d", game_folder(m), m->phase == JP_PH_DONE ? shown : now, shown);
    heading(f, "PATCH", st);
    int y = 190, rowh = shown > 6 ? 74 : 84;
    for (int i = 0; i < JP_STEP_COUNT; i++) {
        if (m->steps[i] == JP_SS_HIDDEN) continue;
        JkIcon ic = m->steps[i] == JP_SS_OK ? JK_ICON_OK : m->steps[i] == JP_SS_FAIL ? JK_ICON_BAD
                  : m->steps[i] == JP_SS_NOW ? JK_ICON_ACTIVE : JK_ICON_PENDING;
        jk_status_icon(f, X0, y + 4, 40, ic);
        uint32_t c = m->steps[i] == JP_SS_WAIT ? 0x5D7486 : m->steps[i] == JP_SS_OK ? C_DIM : C_CHALK;
        jk_text(f, JK_FONT_BODY, X0 + 64, y, STEP_TEXT[i], c);
        char detail[160] = "";
        if (i == JP_STEP_CHECK && m->steps[i] == JP_SS_OK)
            snprintf(detail, sizeof detail, "%s%s%s", game_title(m), m->version[0] ? " v" : "", m->version);
        else if (i == JP_STEP_FILES && m->steps[i] != JP_SS_WAIT) snprintf(detail, sizeof detail, "d3d9.dll");
        else if (i == JP_STEP_REGISTRY && m->reg_used[0]) snprintf(detail, sizeof detail, "%s", m->reg_used);
        else if (i == JP_STEP_SETTINGS && m->steps[i] == JP_SS_OK)
            snprintf(detail, sizeof detail, "%s · %s", jkpro_res_text(m->res), jkpro_quality_text(m->quality));
        if (detail[0]) text_right(f, JK_FONT_HINT, 1500, y + 4, detail, C_DIM);
        y += rowh;
    }
    /* the bar: whole steps, plus the bytes of the running copy */
    int bx = X0, by = 740, bw = 1150, bh = 18;
    jk_fill(f, bx, by, bw, bh, C_STAND2);
    long long part = 0;
    if (m->phase == JP_PH_RUNNING && m->bytes_total > 0) part = m->bytes_done * 1000 / m->bytes_total;
    int fillw = (int)((long long)bw * (done * 1000 + part) / ((shown ? shown : 1) * 1000));
    jk_fill(f, bx, by, fillw, bh, m->phase == JP_PH_FAILED ? C_BAD : C_FLOOD);
    char mb[64] = "";
    if (m->bytes_total > 0)
        snprintf(mb, sizeof mb, "%lld of %lld KB", (m->bytes_done + 1023) / 1024, (m->bytes_total + 1023) / 1024);
    if (mb[0]) text_right(f, JK_FONT_HINT, bx + bw, by + 32, mb, C_DIM);
    const char *line = m->phase == JP_PH_DONE ? "PES 2013 is ready. Your game files were not deleted; anything replaced was backed up."
                     : m->phase == JP_PH_FAILED ? "PATCH stopped. Fix what the message said, then choose Patch again."
                     : m->phase == JP_PH_RUNNING ? "Your game files are not deleted. Anything replaced is backed up."
                     : "PATCH prepares this game folder for the PS4. It is safe to run again.";
    text_fit(f, JK_FONT_BODY, bx, by + 70, line, m->phase == JP_PH_DONE ? C_OK : C_DIM, X1 - X0);
    if (m->phase == JP_PH_RUNNING) { hint_text_left(f, "Don't turn off the PS4"); jk_fill(f, X0, HINT_LINE, X1 - X0, 2, C_LINE); return; }
    if (m->phase == JP_PH_DONE) {
        Hint h[] = { { JK_BTN_CROSS, "Play" }, { JK_BTN_CIRCLE, "Back" } };
        hints(f, h, 2);
    } else {
        Hint h[] = { { JK_BTN_CROSS, "Patch" }, { JK_BTN_SQUARE, "Help" }, { JK_BTN_CIRCLE, "Back" } };
        hints(f, h, 3);
    }
}

/* ---------------------------------------------------------------- settings */

static const char *SET_LABEL[JP_SET_COUNT] = {
    "Resolution", "Picture quality", "Display", "Frame rate", "Controller", "Show FPS", "Kitserver", "Kitserver face fix",
    "Online", "Logs & support", "About"
};

static void setting_value(const JpModel *m, int row, char *out, size_t cap, const char **lock)
{
    *lock = NULL;
    switch (row) {
    case JP_SET_RESOLUTION: snprintf(out, cap, "%s", jkpro_res_text(m->res)); *lock = "fixed"; break;
    case JP_SET_QUALITY: snprintf(out, cap, "%s", jkpro_quality_text(m->quality)); *lock = "fixed"; break;
    case JP_SET_DISPLAY: snprintf(out, cap, "Full screen"); *lock = "fixed on PS4"; break;
    case JP_SET_FRAMERATE: snprintf(out, cap, "60 FPS"); *lock = "game speed follows it"; break;
    case JP_SET_CONTROLLER: snprintf(out, cap, "PS4 pad (XInput)"); *lock = "fixed"; break;
    case JP_SET_FPS: snprintf(out, cap, "%s", m->show_fps ? "On" : "Off"); break;
    case JP_SET_KITSERVER: snprintf(out, cap, "%s", jkpro_kit_text((JkKitMode)m->kit_mode)); break;
    case JP_SET_FACEFIX:
        snprintf(out, cap, "%s", m->face_state == JKPRO_FSERVFIX_PATCHED ? "On"
                               : m->face_state == JKPRO_FSERVFIX_ALREADY ? "Off" : "Not for this fserv.dll");
        break;
    case JP_SET_ONLINE: snprintf(out, cap, "%s  ›", m->online ? "ThirteenServer" : "Off"); break;
    case JP_SET_LOGS: snprintf(out, cap, "%s  ›", m->logs ? "Logs on" : "Logs off"); break;
    default: snprintf(out, cap, "›"); break;
    }
}

static void draw_settings(JkFrame *f, const JpModel *m)
{
    heading(f, "SETTINGS", game_folder(m));
    /* the Kitserver rows only with their files; the line under a Kitserver row only while it is focused */
    int nrows = JP_SET_COUNT - !m->kit_avail - !m->face_avail;
    int y = 180, w = 1360;
    int rh = nrows >= 11 ? 56 : nrows == 10 ? 60 : nrows == 9 ? 68 : 76, gap = nrows >= 10 ? 6 : 8, ty = (rh - 48) / 2;
    for (int i = 0; i < JP_SET_COUNT; i++, y += rh + gap) {
        if ((i == JP_SET_KITSERVER && !m->kit_avail) || (i == JP_SET_FACEFIX && !m->face_avail)) { y -= rh + gap; continue; }
        int locked = i == JP_SET_RESOLUTION || i == JP_SET_QUALITY || i == JP_SET_DISPLAY || i == JP_SET_FRAMERATE ||
                     i == JP_SET_CONTROLLER;
        int focus = i == m->set_focus;
        if (focus) { jk_fill(f, X0, y, w, rh, C_STAND2); frame_rect(f, X0, y, w, rh, 4, C_FLOOD); }
        jk_text(f, JK_FONT_BODY, X0 + 32, y + ty, SET_LABEL[i], locked ? C_DIM : C_CHALK);
        char v[96];
        const char *lock;
        setting_value(m, i, v, sizeof v, &lock);
        int xr = X0 + w - 32;
        if (lock) {
            int cw = chip_w(lock);
            chip(f, xr - cw, y + ty + 2, lock, C_DIM, C_MUTEBG);
            xr -= cw + 24;
        }
        if (focus && (i == JP_SET_FPS || i == JP_SET_KITSERVER || (i == JP_SET_FACEFIX && m->face_state <= 1))) {
            char a[120];
            snprintf(a, sizeof a, "‹  %s  ›", v);
            text_right(f, JK_FONT_BODY, xr, y + ty, a, C_CHALK);
        } else {
            text_right(f, JK_FONT_BODY, xr, y + ty, v, locked ? C_DIM : C_CHALK);
        }
        const char *under = !focus ? NULL
                          : i == JP_SET_KITSERVER ? "Kitserver is your own mod. Without kits turns off only its kit module."
                          : i == JP_SET_FACEFIX ? "Turn on if PES 2013 closes while you scroll fast through teams. Broken faces "
                                                  "then show a default face."
                          : i == JP_SET_ONLINE ? "Play online through ThirteenServer. Press Cross for your account and how to "
                                                 "log in inside the game."
                          : NULL;
        if (under) {
            text_fit(f, JK_FONT_HINT, X0 + 32, y + rh + 2, under, C_DIM, w - 64);
            y += 34;
        }
    }
    const char *note = m->set_msg[0] ? m->set_msg
                     : "PES 2013 runs at 1920 × 1080, 60 FPS, High, full screen: tested on the PS4.";
    text_fit(f, JK_FONT_HINT, X0, y + 6, note, C_DIM, X1 - X0);
    Hint h[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, "Change" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
}

/* ---------------------------------------------------------------- saves */

static const char *SAV_TEXT[JP_SAV_COUNT] = { "Load saves", "Restore a backup", "Export saves" };

/* A label and its value (a path in chalk); returns the height used. */
static int info_row(JkFrame *f, int x, int y, int w, const char *label, const char *value, const char *note)
{
    int h = 0;
    jk_text(f, JK_FONT_HINT, x, y, label, C_DIM);
    h += wrap(f, JK_FONT_BODY, x + 230, y - 4, w - 230, value, C_CHALK);
    if (note && note[0]) h += wrap(f, JK_FONT_HINT, x + 230, y + h - 2, w - 230, note, C_DIM);
    return h + 10;
}

static void draw_saves(JkFrame *f, const JpModel *m)
{
    heading(f, "SAVE GAMES", game_folder(m));
    int y = 150;
    /* waiting files (and a name hint) on top */
    if (m->import_files > 0) {
        char b[320];
        snprintf(b, sizeof b, "%d file%s ready to load%s: %s", m->import_files, m->import_files == 1 ? "" : "s",
                 m->import_usb ? " (USB)" : "", m->import_list);
        int hb = 60 + (m->import_warn[0] ? wrap(NULL, JK_FONT_HINT, 0, 0, X1 - X0 - 48, m->import_warn, 0) + 4 : 0);
        jk_fill(f, X0, y, X1 - X0, hb, C_WARNBG);
        frame_rect(f, X0, y, X1 - X0, hb, 2, 0x6B5520);
        text_fit(f, JK_FONT_BODY, X0 + 24, y + 10, b, C_FLOOD, X1 - X0 - 48);
        if (m->import_warn[0]) wrap(f, JK_FONT_HINT, X0 + 24, y + 56, X1 - X0 - 48, m->import_warn, C_CHALK);
        y += hb + 24;
    }
    if (m->import_old[0]) {
        text_fit(f, JK_FONT_HINT, X0, y - 8, m->import_old, C_DIM, X1 - X0);
        y += 32;
    }
    if (m->dp_wait) {
        char b[200];
        snprintf(b, sizeof b, "A data pack is waiting in Import (%s): open DATA PACKS in the game menu.", m->dp_wait_name);
        text_fit(f, JK_FONT_HINT, X0, y - 8, b, C_FLOOD, X1 - X0);
        y += 32;
    }
    /* the actions */
    int step = jk_font_line_height(JK_FONT_HEAD) + 30, my = y;
    for (int i = 0; i < JP_SAV_COUNT; i++) {
        int fy = my + i * step, focus = i == m->sav_focus;
        if (focus) jk_fill(f, X0, fy, 460, step - 12, C_FLOOD);
        text_fit(f, JK_FONT_HEAD, X0 + 24, fy + 8, SAV_TEXT[i], focus ? C_INK : C_DIM, 420);
    }
    char nb[48], bk[96];
    nice_stamp(m->last_backup, nb, sizeof nb);
    if (m->backups) snprintf(bk, sizeof bk, "%d kept (the newest %d)", m->backups, JKPRO_BACKUPS_KEPT);
    else snprintf(bk, sizeof bk, "none yet");
    jk_text(f, JK_FONT_HINT, X0, my + 3 * step + 6, "Backups", C_DIM);
    text_fit(f, JK_FONT_HINT, X0, my + 3 * step + 40, bk, C_CHALK, 460);
    if (m->backups) {
        char nw[80];
        snprintf(nw, sizeof nw, "newest %s", nb);
        text_fit(f, JK_FONT_HINT, X0, my + 3 * step + 74, nw, C_CHALK, 460);
    }
    /* where everything is: the real paths */
    int px = 660, pw = X1 - px;
    int py = y;
    char put[260];
    snprintf(put, sizeof put, "%s  (FTP)", m->sav_drop);
    py += info_row(f, px, py, pw, "Put files in", put, "or on a USB drive: JKPro/saves/");
    py += info_row(f, px, py, pw, "Names", "Option file (teams, names, kits): EDIT.bin. Game settings: OPTION.bin.",
                   "Files keep their names: rename yours to EDIT.bin or OPTION.bin if they are called something else. A whole save/ "
                   "folder works too.");
    py += info_row(f, px, py, pw, "Live saves", m->sav_live, "");
    py += info_row(f, px, py, pw, "Backups go to", m->sav_bak,
                   "One folder per backup, named by date and time, with a backup.txt. Made before an import and before a "
                   "match when the saves changed. Pick any of them under Restore a backup.");
    if (m->sav_msg[0]) {
        py += 6;
        wrap(f, JK_FONT_HINT, px, py, pw, m->sav_msg, m->sav_msg_bad ? C_BAD : C_OK);
    }
    Hint h[] = { { JK_BTN_CROSS, "Select" }, { JK_BTN_SQUARE, "Help" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
}

/* ---------------------------------------------------------------- data packs */

static void size_text(long long n, char *out, size_t cap)
{
    if (n < 1024 * 1024) snprintf(out, cap, "%lld KB", (n + 512) / 1024);
    else {
        long long t = (n * 10 + 524288) / 1048576;
        snprintf(out, cap, "%lld.%lld MB", t / 10, t % 10);
    }
}

static void draw_datapacks(JkFrame *f, const JpModel *m)
{
    heading(f, "DATA PACKS", game_folder(m));
    int y = 150;
    char sz[32], line[300];
    /* the active pack */
    size_text(m->dp_size, sz, sizeof sz);
    if (m->dp_have) snprintf(line, sizeof line, "Active: data pack %s  \xC2\xB7  %s", m->dp_ver[0] ? m->dp_ver : "(version unknown)", sz);
    else snprintf(line, sizeof line, "No data pack: the game uses its own base data (0.00)");
    jk_text(f, JK_FONT_HEAD, X0, y, line, m->dp_have ? C_OK : C_CHALK);
    y += jk_font_line_height(JK_FONT_HEAD) + 16;
    if (m->dp_wait) {
        size_text(m->dp_wait_size, sz, sizeof sz);
        snprintf(line, sizeof line, "Waiting in Import: %s  \xC2\xB7  data pack %s  \xC2\xB7  %s", m->dp_wait_name,
                 m->dp_wait_ver[0] ? m->dp_wait_ver : "(version unknown)", sz);
        jk_fill(f, X0, y, X1 - X0, 56, C_WARNBG);
        text_fit(f, JK_FONT_BODY, X0 + 24, y + 8, line, C_FLOOD, X1 - X0 - 48);
        y += 72;
    }
    if (m->dp_bad[0]) { text_fit(f, JK_FONT_HINT, X0, y, m->dp_bad, C_BAD, X1 - X0); y += 40; }
    /* the actions */
    int kinds[8], idx[8], n = jkpro_dp_rows(m, kinds, idx, 8), rh = 64;
    int ly = y;
    for (int i = 0; i < n; i++, ly += rh + 6) {
        int focus = i == m->dp_focus;
        if (focus) jk_fill(f, X0, ly, 700, rh, C_FLOOD);
        char t[160];
        if (kinds[i] == 0) snprintf(t, sizeof t, "Install %s", m->dp_wait_name);
        else if (kinds[i] == 1) snprintf(t, sizeof t, "Remove the active pack");
        else {
            size_text(m->dp_old[idx[i]].size, sz, sizeof sz);
            snprintf(t, sizeof t, "Use %s (%s)", m->dp_old[idx[i]].ver[0] ? m->dp_old[idx[i]].ver : "?", sz);
        }
        text_fit(f, JK_FONT_BODY, X0 + 24, ly + 12, t, focus ? C_INK : C_CHALK, 652);
    }
    if (n == 0) wrap(f, JK_FONT_BODY, X0, ly, 640, "Nothing to do yet: put a data pack (dt0f.img) in the Import folder.", C_DIM);
    /* where everything is */
    int px = 900, pw = X1 - px, py = y;
    char put[260];
    snprintf(put, sizeof put, "%s  (FTP)", m->sav_drop);
    py += info_row(f, px, py, pw, "Put it in", put, "The same folder as the option file. Any name; JKPro checks the file "
                   "itself (an official PES 2013 data pack, dt0f.img).");
    py += info_row(f, px, py, pw, "Active pack", m->dp_live, "Moved there, not copied: it is large.");
    char keep[80];
    snprintf(keep, sizeof keep, "Older packs, the last %d kept. Use one again from the list.", JKPRO_PACKS_OLD);
    py += info_row(f, px, py, pw, "Older packs", m->dp_olddir, keep);
    if (m->dp_msg[0]) wrap(f, JK_FONT_HINT, px, py + 6, pw, m->dp_msg, m->dp_msg_bad ? C_BAD : C_OK);
    Hint h[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, "Select" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
}

/* ---------------------------------------------------------------- restore a backup */

static void draw_restore(JkFrame *f, const JpModel *m)
{
    heading(f, "RESTORE A BACKUP", game_folder(m));
    jk_text(f, JK_FONT_HINT, X0, 140, "Backups are in", C_DIM);
    jk_text(f, JK_FONT_BODY, X0 + 220, 136, m->sav_bak, C_CHALK);
    int y = 200, rh = 92;
    if (m->nbk == 0) {
        wrap(f, JK_FONT_BODY, X0, y, 1500, "There are no backups yet. JKPro makes one before every import and before a match "
             "when your saves changed.", C_DIM);
    }
    for (int i = m->bk_top; i < m->nbk && i < m->bk_top + 7; i++, y += rh + 6) {
        int focus = i == m->bk_focus;
        if (focus) { jk_fill(f, X0, y, X1 - X0, rh, C_STAND2); frame_rect(f, X0, y, X1 - X0, rh, 4, C_FLOOD); }
        char when[48];
        nice_stamp(m->bk[i].name, when, sizeof when);
        jk_text(f, JK_FONT_BODY, X0 + 28, y + 10, when, C_CHALK);
        int imp = !strcmp(m->bk[i].reason, "before import");
        chip(f, X0 + 420, y + 12, m->bk[i].reason, imp ? C_FLOOD : C_DIM, imp ? C_WARNBG : C_MUTEBG);
        text_fit(f, JK_FONT_HINT, X0 + 28, y + 54, m->bk[i].files[0] ? m->bk[i].files : "(empty)", C_DIM, X1 - X0 - 56);
        if (i == 0) text_right(f, JK_FONT_HINT, X1 - 28, y + 14, "newest", C_DIM);
    }
    char cnt[96];
    snprintf(cnt, sizeof cnt, "%d backup%s  \xC2\xB7  the newest %d are kept", m->nbk, m->nbk == 1 ? "" : "s",
             JKPRO_BACKUPS_KEPT);
    jk_text(f, JK_FONT_HINT, X0, HINT_Y, cnt, C_DIM);
    Hint h[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, "Restore" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
}

/* ---------------------------------------------------------------- help, logs, about */

/* Help: a guide in JkFlow's manner, one section per page, with the real paths. */
typedef struct { int kind; char text[320]; } HelpLine;   /* kind 0 text, 1 path */

static int help_lines(const JpModel *m, int page, HelpLine *l, int max, const char **title)
{
    int n = 0;
#define T(...) do { if (n < max) { l[n].kind = 0; snprintf(l[n].text, sizeof l[n].text, __VA_ARGS__); n++; } } while (0)
#define P(...) do { if (n < max) { l[n].kind = 1; snprintf(l[n].text, sizeof l[n].text, __VA_ARGS__); n++; } } while (0)
    const char *g = m->game_path, *r = m->root_path;
    switch (page) {
    case JP_HELP_GAME:
        *title = "1  PES 2013: where to put the game";
        T("Copy your whole PES 2013 folder from the PC, as it is, over FTP into:");
        P("%s/", g);
        T("It should look like this:");
        P("%s/pes2013.exe", g);
        P("%s/settings.exe", g);
        P("%s/img/", g);
        P("%s/\xE2\x80\xA6  (all other PES 2013 files and folders)", g);
        T("pes2013.exe must be directly in this folder. Only use a copy of PES 2013 you own. Kitserver is optional (your "
          "own mod): if PES 2013 closes while you scroll fast through teams, turn on SETTINGS > Kitserver face fix (Off "
          "restores your original fserv.dll). Without kits or Kitserver Off are other ways out.");
        T("Then open the game menu (press Triangle) and choose PATCH once.");
        T("Check game files (a red tile): a file is missing or pes2013.exe is a version JKPro doesn't support. Copy the "
          "whole folder from your PC again (version 1.04). Press the tile to see exactly what is wrong.");
        break;
    case JP_HELP_REG:
        *title = "2  The registry file (.reg)";
        T("PES 2013 needs the registry values its PC installer wrote. Without them it says it has \"not been installed\".");
        T("Put your registry file in the game folder. Any name ending in .reg works, for example:");
        P("%s/PES_2013_Registry_Fix.reg", g);
        T("It must hold the KONAMI\\PES2013 key with the \"code\" value. Then run PATCH: step 4 imports it. E210 = no "
          ".reg file found, E211 = the file has no code.");
        T("JKPro also looks for it in %s/ and on a USB drive in JKPro/.", r);
        break;
    case JP_HELP_SAVES:
        *title = "3  Saves and option files";
        T("Put the files in:");
        P("%s", m->sav_drop[0] ? m->sav_drop : "Saves/PES13/Import/");
        T("EDIT.bin = option file (teams, names, kits). OPTION.bin = game settings. Files keep their names; a whole "
          "save/ folder works too.");
        T("Then: Triangle > SAVE GAMES > Load saves. Your saves are backed up "
          "first, and Import is empty again afterwards.");
        T("Live saves:");
        P("%s", m->sav_live);
        T("Backups (the newest 10, one folder each with a backup.txt). Pick any under SAVE GAMES > Restore "
          "a backup:");
        P("%s", m->sav_bak);
        break;
    case JP_HELP_PACKS:
        *title = "4  Data packs (DLC)";
        T("Put the official data pack dt0f.img (for example Data Pack 5.00 or 6.00) in:");
        P("%s", m->sav_drop[0] ? m->sav_drop : "Saves/PES13/Import/");
        T("Then: Triangle > DATA PACKS > Install. It is moved, not copied: it is "
          "large. JKPro checks the file itself, so the name does not matter.");
        if (m->dp_on) {
            T("The active pack:");
            P("%s", m->dp_live);
            T("Older packs (the last 3). Remove the active one or use an older one again in DATA PACKS:");
            P("%s", m->dp_olddir);
        }
        break;
    case JP_HELP_USB:
        *title = "5  USB drive";
        T("Saves and option files can also come from a USB drive. Put them in:");
        P("USB:/JKPro/saves/");
        T("Then SAVE GAMES > Load saves, as from the Import folder. Files on USB are copied and left as "
          "they are.");
        T("Export saves writes to USB:/JKPro/export/ when a drive is in, otherwise to:");
        P("%s/Export/", r);
        T("The registry file can also be on the drive, in USB:/JKPro/.");
        break;
    case JP_HELP_ONLINE:
        *title = "6  Online (ThirteenServer)";
        T("Online play goes through ThirteenServer, a free community server for PES 2013. Create an account on "
          "thirteenserver.online (the password has 6 to 20 characters).");
        T("Then SETTINGS > Online: turn on ThirteenServer and enter the same ID and password. JKPro keeps them only on "
          "this PS4, in:");
        P("%s", m->online_file);
        T("In the game the PS4 keyboard does not open. In the box: L1 + touchpad = ID, R1 + touchpad = password, "
          "L1 + R1 + touchpad = registration code (PES asks for it before the login; ThirteenServer takes any text, and "
          "with none set your ID is typed).");
        T("Leave Kitserver on: without it PES 2013 stops at a \"minimum system requirements\" message.");
        break;
    default:
        *title = "7  Support and if something goes wrong";
        T("Settings > Logs & support > Save support bundle. It is written to:");
        P("%s/support/", m->logs_path);
        T("and to USB:/JKPro-support/ when a drive is in. It holds logs only: no saves, no registry file.");
        T("PES 2013 does not start: run PATCH again, then try once more. A message with a code (like E210) says what "
          "to do next.");
        T("Everything JKPro keeps is in %s/. Updating JKPro leaves it alone; deleting that folder deletes your saves "
          "and settings too.", r);
        break;
    }
#undef T
#undef P
    return n;
}

static void draw_help(JkFrame *f, const JpModel *m)
{
    char pg[32];
    snprintf(pg, sizeof pg, "%d / %d", m->help_page + 1, JP_HELP_COUNT);
    heading(f, "HELP", pg);
    jk_text(f, JK_FONT_HINT, X0, 132, "JKPro \xC2\xB7 Made by " JKPRO_AUTHOR "  \xC2\xB7  " JKPRO_OFFICIAL_URL, C_DIM);
    static HelpLine l[24];
    const char *title = "";
    int n = help_lines(m, m->help_page, l, 24, &title);
    int y = 186;
    jk_text(f, JK_FONT_HEAD, X0, y, title, C_FLOOD);
    y += jk_font_line_height(JK_FONT_HEAD) + 14;
    for (int i = 0; i < n && y < HINT_LINE - 40; i++) {
        if (l[i].kind == 1) {
            text_fit(f, JK_FONT_BODY, X0 + 40, y, l[i].text, C_CHALK, X1 - X0 - 40);
            y += jk_font_line_height(JK_FONT_BODY) + 4;
        } else {
            y += wrap(f, JK_FONT_BODY, X0, y, X1 - X0, l[i].text, C_DIM) + 6;
        }
    }
    Hint h[] = { { JK_BTN_LEFTRIGHT, "Page" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 2);
}

int jkpro_help_lines(const JpModel *m, int page, char *out, size_t cap)
{
    static HelpLine l[24];
    const char *title = "";
    int n = help_lines(m, page, l, 24, &title);
    snprintf(out, cap, "%s\n", title);
    for (int i = 0; i < n; i++) {
        size_t k = strlen(out);
        snprintf(out + k, cap - k, "%s\n", l[i].text);
    }
    return n;
}

static void draw_online(JkFrame *f, const JpModel *m)
{
    heading(f, "ONLINE", "ThirteenServer");
    static const char *LBL[JP_ONL_COUNT] = { "Online server", "Online ID", "Password", "Registration code" };
    int y = 170, w = 1360, rh = 72;
    for (int i = 0; i < JP_ONL_COUNT; i++, y += rh + 8) {
        int focus = i == m->online_focus && !m->kb_field;
        if (focus) { jk_fill(f, X0, y, w, rh, C_STAND2); frame_rect(f, X0, y, w, rh, 4, C_FLOOD); }
        jk_text(f, JK_FONT_BODY, X0 + 32, y + 12, LBL[i], C_CHALK);
        char v[96];
        if (i == JP_ONL_SERVER) snprintf(v, sizeof v, focus ? "â¹  %s  âº" : "%s", m->online ? "ThirteenServer" : "Off");
        else if (i == JP_ONL_ID) snprintf(v, sizeof v, "%s  âº", m->online_id[0] ? m->online_id : "Not set");
        else if (i == JP_ONL_CODE) snprintf(v, sizeof v, "%s  \xE2\x80\xBA", m->online_code[0] ? m->online_code : "(uses your ID)");
        else if (m->online_pw[0]) {
            size_t n = strlen(m->online_pw);
            if (n > 20) n = 20;
            memset(v, '*', n);
            snprintf(v + n, sizeof v - n, "  âº");
        } else snprintf(v, sizeof v, "Not set  âº");
        int dim = (i == JP_ONL_ID && !m->online_id[0]) || (i == JP_ONL_PW && !m->online_pw[0]) ||
                  (i == JP_ONL_CODE && !m->online_code[0]);
        text_right(f, JK_FONT_BODY, X0 + w - 32, y + 12, v, dim ? C_DIM : C_CHALK);
    }
    y += 16;
    y += wrap(f, JK_FONT_HINT, X0, y, X1 - X0, "Create a free account on thirteenserver.online (password 6 to 20 characters), "
              "then enter the same ID and password here. JKPro keeps them only on this PS4:", C_DIM) + 4;
    text_fit(f, JK_FONT_HINT, X0 + 40, y, m->online_file, C_CHALK, X1 - X0 - 40);
    y += 44;
    y += wrap(f, JK_FONT_HINT, X0, y, X1 - X0, "In the game the PS4 keyboard does not open: L1 + touchpad = ID, R1 + touchpad = "
              "password, L1 + R1 + touchpad = registration code. Leave Kitserver on.", C_DIM) + 10;
    if (m->online_msg[0]) text_fit(f, JK_FONT_BODY, X0, y, m->online_msg, C_OK, X1 - X0);
    Hint h[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, "Change" }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
    if (m->kb_field) jk_keyboard_draw(f, &m->kb);
}

static void draw_logs(JkFrame *f, const JpModel *m)
{
    heading(f, "LOGS & SUPPORT", "");
    int y = 200, w = 1360, rh = 76;
    int rows = m->can_bundle ? 2 : 1;
    for (int i = 0; i < rows; i++, y += rh + 8) {
        if (i == m->logs_focus) { jk_fill(f, X0, y, w, rh, C_STAND2); frame_rect(f, X0, y, w, rh, 4, C_FLOOD); }
        jk_text(f, JK_FONT_BODY, X0 + 32, y + 14, i == 0 ? "Logs" : "Save support bundle", C_CHALK);
        text_right(f, JK_FONT_BODY, X0 + w - 32, y + 14, i == 0 ? (m->logs ? "On" : "Off") : "›", C_CHALK);
    }
    y += 30;
    /* information only (owner: native for 1.0; the PE fallback is a support switch, not a choice) */
    jk_text(f, JK_FONT_BODY, X0, y, m->pe_renderer ? "Renderer: compatible (support fallback)" : "Renderer: PS4 native",
            C_DIM);
    y += jk_font_line_height(JK_FONT_BODY) + 24;
    if (m->bundle_msg[0]) y += wrap(f, JK_FONT_BODY, X0, y, 1300, m->bundle_msg, m->bundle_bad ? C_BAD : C_OK) + 24;
    wrap(f, JK_FONT_BODY, X0, y, 1300,
         "Logs help find out why a game does not start. Turn them on only when you need them: they make the game start "
         "a little slower. A support bundle collects them in one folder; it never "
         "contains your saves or your registry file.",
         C_DIM);
    const char *act = m->logs_focus == 1 ? "Save" : m->logs ? "Turn off" : "Turn on";
    Hint h[] = { { JK_BTN_UPDOWN, "Choose" }, { JK_BTN_CROSS, act }, { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 3);
}

/* The credits (JKPro's own list; the package also carries about/CREDITS.txt). */
static const struct { const char *name, *role, *link; } CREDITS[] = {
    { "JKPro", "Launcher and PS4 port: " JKPRO_AUTHOR, JKPRO_OFFICIAL_URL },
    { "JkFlow runtime", "Wine on PS4, native DXVK server: " JKPRO_AUTHOR, "github.com/Salru13/JkFlow" },
    { "DXVK", "Philip Rebohle and contributors (zlib licence)", "github.com/doitsujin/dxvk" },
    { "Wine", "The Wine project (LGPL)", "winehq.org" },
    { "GoldHEN", "SiSTRo and the GoldHEN team", "github.com/GoldHEN/GoldHEN" },
    { "Kitserver 13", "juce: the player's own mod, not shipped", "" },
    { "Ubuntu Sans", "The launcher's font (Ubuntu Font Licence)", "" },
};

static void draw_about(JkFrame *f, const JpModel *m)
{
    heading(f, "ABOUT", m->build_id[0] ? m->build_id : "JKPro 1.0");
    int y = 160;
    const char *title, *body;
    uint32_t tc;
    if (m->rel_state == 1) {
        title = "OFFICIAL RELEASE"; tc = C_OK;
        body = "This copy of JKPro is an official release. Its files match the list signed by the JKPro project.";
    } else if (m->rel_state == 2) {
        title = "CHANGED FILES"; tc = C_FLOOD;
        body = "This package was signed by the JKPro project, but some of its files are different from the signed list. "
               "Someone may have changed them after release. It still works as normal.";
    } else {
        title = "UNOFFICIAL BUILD"; tc = C_CHALK;
        body = "This copy of JKPro could not be verified as an official release. It may have been changed or repackaged "
               "by someone else. It still works as normal; nothing has been turned off.";
    }
    jk_text(f, JK_FONT_HEAD, X0, y, title, tc);
    y += jk_font_line_height(JK_FONT_HEAD) + 8;
    y += wrap(f, JK_FONT_BODY, X0, y, 1600, body, C_DIM);
    if (m->rel_state == 0) {
        y += wrap(f, JK_FONT_HINT, X0, y, 1600, m->rel_reason[0] ? m->rel_reason : "No signature was found in this package.",
                  C_DIM);
    }
    y += 16;
    char sig[128], ver[64];
    snprintf(ver, sizeof ver, "%s", m->rel_version[0] ? m->rel_version : m->build_id[0] ? m->build_id : "1.0");
    snprintf(sig, sizeof sig, "%s%s%s", m->rel_key_label[0] ? m->rel_key_label : "-", m->rel_key_id[0] ? "  ·  key " : "",
             m->rel_key_id);
    const char *rows[3][2] = { { "Version", ver }, { "Signed by", m->rel_state ? sig : "-" },
                               { "Official", JKPRO_OFFICIAL_URL } };
    for (int i = 0; i < 3; i++, y += 42) {
        jk_text(f, JK_FONT_BODY, X0, y, rows[i][0], C_DIM);
        jk_text(f, JK_FONT_BODY, X0 + 260, y, rows[i][1], C_CHALK);
    }
    y += 24;
    jk_text(f, JK_FONT_HEAD, X0, y, "Credits", C_CHALK);
    y += jk_font_line_height(JK_FONT_HEAD) + 6;
    for (size_t i = 0; i < sizeof CREDITS / sizeof CREDITS[0]; i++, y += 40) {
        jk_text(f, JK_FONT_BODY, X0, y, CREDITS[i].name, C_CHALK);
        text_fit(f, JK_FONT_HINT, X0 + 300, y + 4, CREDITS[i].role, C_DIM, 760);
        if (CREDITS[i].link[0]) jk_text(f, JK_FONT_HINT, X0 + 1100, y + 4, CREDITS[i].link, C_DIM);
    }
    Hint h[] = { { JK_BTN_CIRCLE, "Back" } };
    hints(f, h, 1);
}

/* ---------------------------------------------------------------- dialog */

static void draw_dialog(JkFrame *f, const JpModel *m)
{
    jk_fill_alpha(f, 0, 0, JK_W, JK_H, 0x05090D, 215);
    int w = 1180, x = (JK_W - w) / 2, pad = 56, tw = w - 2 * pad;
    /* measure */
    int h = pad + jk_font_line_height(JK_FONT_HEAD) + 24 + wrap(NULL, JK_FONT_BODY, 0, 0, tw, m->dlg_body, 0) + 24;
    if (m->dlg_nothing_changed) h += jk_font_line_height(JK_FONT_BODY) + 18;
    h += 80 + pad;
    int y = (JK_H - h) / 2 - 20;
    panel(f, x, y, w, h);
    jk_fill(f, x, y, w, 6, m->dialog == JP_DLG_ERROR || m->dialog == JP_DLG_NO_BACKUP ? C_BAD : C_FLOOD);
    if (m->dlg_code[0]) text_right(f, JK_FONT_HINT, x + w - 32, y + 24, m->dlg_code, C_DIM);
    int yy = y + pad;
    jk_text(f, JK_FONT_HEAD, x + pad, yy, m->dlg_title, C_CHALK);
    yy += jk_font_line_height(JK_FONT_HEAD) + 24;
    yy += wrap(f, JK_FONT_BODY, x + pad, yy, tw, m->dlg_body, C_DIM) + 24;
    if (m->dlg_nothing_changed) {
        jk_text(f, JK_FONT_BODY, x + pad, yy, "Nothing was changed.", C_OK);
        yy += jk_font_line_height(JK_FONT_BODY) + 18;
    }
    /* buttons */
    const char *ok = "OK", *no = NULL;
    if (m->dialog == JP_DLG_LOAD) { ok = "Load saves"; no = "Cancel"; }
    else if (m->dialog == JP_DLG_RESTORE) { ok = "Restore"; no = "Cancel"; }
    else if (m->dialog == JP_DLG_NO_BACKUP) { ok = "Play without backup"; no = "Back"; }
    int bx = x + pad, bh = 64;
    int bw = jk_text_width(JK_FONT_BODY, ok) + GLYPH + 72;
    jk_fill(f, bx, yy, bw, bh, C_FLOOD);
    jk_button_glyph(f, bx + 20, yy + (bh - GLYPH) / 2, GLYPH, JK_BTN_CROSS);
    jk_text(f, JK_FONT_BODY, bx + 20 + GLYPH + 16, yy + 12, ok, C_INK);
    if (no) {
        bx += bw + 24;
        int nw = jk_text_width(JK_FONT_BODY, no) + GLYPH + 72;
        frame_rect(f, bx, yy, nw, bh, 2, C_LINE);
        jk_button_glyph(f, bx + 20, yy + (bh - GLYPH) / 2, GLYPH, JK_BTN_CIRCLE);
        jk_text(f, JK_FONT_BODY, bx + 20 + GLYPH + 16, yy + 12, no, C_DIM);
    }
}

/* ---------------------------------------------------------------- didn't start */

static void draw_failed(JkFrame *f, const JpModel *m)
{
    wordmark(f);
    int w = 1300, x = X0, y = 200, pad = 56, tw = w - 2 * pad;
    char saves[160], nb[48];
    nice_stamp(m->last_backup, nb, sizeof nb);
    if (m->last_backup[0]) snprintf(saves, sizeof saves, "Your saves are safe: the last backup is from %s.", nb);
    else snprintf(saves, sizeof saves, "Your saves were not touched.");
    int h = pad + jk_font_line_height(JK_FONT_MENU) + 24 + wrap(NULL, JK_FONT_BODY, 0, 0, tw, m->failure, 0) + 16 +
            wrap(NULL, JK_FONT_BODY, 0, 0, tw, saves, 0) + 16 + jk_font_line_height(JK_FONT_BODY) + pad;
    panel(f, x, y, w, h);
    jk_fill(f, x, y, w, 6, C_BAD);
    text_right(f, JK_FONT_HINT, x + w - 32, y + 24, "E101", C_DIM);
    int yy = y + pad;
    char title[64];
    snprintf(title, sizeof title, "%s didn't start", game_title(m));
    jk_text(f, JK_FONT_MENU, x + pad, yy, title, C_CHALK);
    yy += jk_font_line_height(JK_FONT_MENU) + 24;
    yy += wrap(f, JK_FONT_BODY, x + pad, yy, tw, m->failure, C_DIM) + 16;
    yy += wrap(f, JK_FONT_BODY, x + pad, yy, tw, saves, C_DIM) + 16;
    jk_text(f, JK_FONT_BODY, x + pad, yy, "Nothing was changed.", C_OK);
    if (m->bundle_msg[0]) wrap(f, JK_FONT_BODY, x, y + h + 32, w, m->bundle_msg, m->bundle_bad ? C_BAD : C_OK);
    Hint h2[] = { { JK_BTN_CROSS, "Try again" }, { JK_BTN_SQUARE, m->can_bundle ? "Save support bundle" : "Help" },
                  { JK_BTN_CIRCLE, "Home" } };
    hints(f, h2, 3);
}

/* ---------------------------------------------------------------- the release badge */

void jkpro_badge_text(const JpModel *m, char *out, size_t cap)
{
    if (m->rel_state != 1) { snprintf(out, cap, "Unofficial build"); return; }
    snprintf(out, cap, "Official Build by " JKPRO_AUTHOR "%s%s%s%s", m->build_id[0] ? "  \xC2\xB7  " : "", m->build_id,
             m->rel_key_id[0] ? "  \xC2\xB7  key " : "", m->rel_key_id);
}

/* Top right of every screen, with a dot: green for official, grey otherwise. */
static void badge(JkFrame *f, const JpModel *m)
{
    char t[160];
    jkpro_badge_text(m, t, sizeof t);
    int official = m->rel_state == 1;
    int w = jk_text_width(JK_FONT_HINT, t), x = X1 - w;
    jk_fill(f, x - 28, TOP_Y + 12, 14, 14, official ? C_OK : 0x5D7486);
    jk_text(f, JK_FONT_HINT, x, TOP_Y, t, official ? C_CHALK : C_DIM);
}

/* ---------------------------------------------------------------- entry */

void jkpro_draw(JkFrame *f, const JpModel *m)
{
    if (!f || !m) return;
    background(f);
    switch (m->screen) {
    case JP_SCR_SHELF: draw_shelf(f, m, 0); break;
    case JP_SCR_MENU: draw_menu(f, m); break;
    case JP_SCR_PATCH: draw_patch(f, m); break;
    case JP_SCR_SETTINGS: draw_settings(f, m); break;
    case JP_SCR_SAVES: draw_saves(f, m); break;
    case JP_SCR_HELP: draw_help(f, m); break;
    case JP_SCR_LOGS: draw_logs(f, m); break;
    case JP_SCR_ABOUT: draw_about(f, m); break;
    case JP_SCR_FAILED: draw_failed(f, m); break;
    case JP_SCR_RESTORE: draw_restore(f, m); break;
    case JP_SCR_DATAPACKS: draw_datapacks(f, m); break;
    case JP_SCR_ONLINE: draw_online(f, m); break;
    }
    badge(f, m);
    if (m->screen == JP_SCR_SHELF || m->screen == JP_SCR_FAILED)
        jk_text(f, JK_FONT_HINT, X0, HINT_Y, "JKPro \xC2\xB7 Made by " JKPRO_AUTHOR, C_DIM);
    if (m->dialog != JP_DLG_NONE) draw_dialog(f, m);
}
