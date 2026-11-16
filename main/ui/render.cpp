/*
  render.cpp — Rendu de FROGBALL : plateau portrait 240x520 qui defile, tourne a 90 degres sur l'ecran 320x240.
  Les sprites (ui/sprites.cpp, generes par tools/gen_sprites.py) sont stockes en colonnes pour que chaque
  colonne du sprite devienne une LIGNE de l'ecran : un seul raw_blit_row par course de pixels opaques.
*/
#include "render.h"
#include "gfx.h"
#include "sprites.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern char font8x8_basic[128][8];   // police 8x8 du SDK (gb_graphics), bit 0 = pixel de gauche

using namespace frog;

namespace render {

// ---------------------------------------------------------------------------
// Rotation portrait (240x320) -> paysage (320x240), console tournee a 90 degres vers la droite :
// le haut du portrait est a GAUCHE de l'ecran physique.      sx = py     sy = 239 - px
// ---------------------------------------------------------------------------
static inline int SX(int py) { return py; }
static inline int SY(int px) { return vid::H - 1 - px; }
static inline uint16_t C(uint32_t c) { return vid::rgb888(c); }

static void pfill(int x, int y, int w, int h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    vid::fill_rect(SX(y), SY(x + w - 1), h, w, col);
}
static void pline(int x0, int y0, int x1, int y1, uint16_t col) { vid::line(SX(y0), SY(x0), SX(y1), SY(x1), col); }
static void pfillcircle(int cx, int cy, int r, uint16_t col) { vid::fill_circle(SX(cy), SY(cx), r, col); }
static void pcircle(int cx, int cy, int r, uint16_t col) { vid::circle(SX(cy), SY(cx), r, col); }
static void ptri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t col) {
    vid::fill_triangle(SX(y0), SY(x0), SX(y1), SY(x1), SX(y2), SY(x2), col);
}
static void pthick(int x0, int y0, int x1, int y1, float hw, uint16_t col) {
    const float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) { pfillcircle(x0, y0, (int)(hw + 0.5f), col); return; }
    const int nx = (int)lroundf(-dy / len * hw), ny = (int)lroundf(dx / len * hw);
    ptri(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, col);
    ptri(x0 - nx, y0 - ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, col);
    pfillcircle(x0, y0, (int)(hw + 0.5f), col);
    pfillcircle(x1, y1, (int)(hw + 0.5f), col);
}
static void ptaper(int x0, int y0, int x1, int y1, float hw0, float hw1, uint16_t col) {
    const float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) return;
    const float ux = -dy / len, uy = dx / len;
    const int ax = x0 + (int)lroundf(ux * hw0), ay = y0 + (int)lroundf(uy * hw0);
    const int bx = x0 - (int)lroundf(ux * hw0), by = y0 - (int)lroundf(uy * hw0);
    const int cx = x1 + (int)lroundf(ux * hw1), cy = y1 + (int)lroundf(uy * hw1);
    const int ex = x1 - (int)lroundf(ux * hw1), ey = y1 - (int)lroundf(uy * hw1);
    ptri(ax, ay, bx, by, cx, cy, col);
    ptri(bx, by, cx, cy, ex, ey, col);
    pfillcircle(x0, y0, (int)(hw0 + 0.5f), col);
    pfillcircle(x1, y1, (int)(hw1 + 0.5f), col);
}

// Texte 8x8 (ASCII) agrandi d'un facteur s, en coordonnees portrait.
static void ptext(int x, int y, const char* str, uint16_t col, int s = 1) {
    for (; *str; ++str, x += 8 * s) {
        unsigned char ch = (unsigned char)*str;
        if (ch >= 128) ch = '?';
        const uint8_t* glyph = (const uint8_t*)font8x8_basic[ch];
        for (int gy = 0; gy < 8; gy++) {
            const uint8_t row = glyph[gy];
            int gx = 0;
            while (gx < 8) {
                if (!(row & (1 << gx))) { gx++; continue; }
                const int start = gx;
                while (gx < 8 && (row & (1 << gx))) gx++;
                pfill(x + start * s, y + gy * s, (gx - start) * s, s, col);
            }
        }
    }
}
static int text_w(const char* s, int sc = 1) { return (int)strlen(s) * 8 * sc; }
static void ptext_center(int cx, int y, const char* str, uint16_t col, int s = 1) {
    ptext(cx - text_w(str, s) / 2, y, str, col, s);
}
// Texte avec ombre portee (lisible sur n'importe quel fond)
static void ptext_sh(int x, int y, const char* str, uint16_t col, uint16_t sh, int s = 1) {
    ptext(x + s, y + s, str, sh, s);
    ptext(x, y, str, col, s);
}
static void ptext_center_sh(int cx, int y, const char* str, uint16_t col, uint16_t sh, int s = 1) {
    ptext_sh(cx - text_w(str, s) / 2, y, str, col, sh, s);
}

// ---------------------------------------------------------------------------
// Sprites : RGB565 standard -> format materiel, transposes (colonne par colonne)
// ---------------------------------------------------------------------------
constexpr int POOL = 20480;
struct HW { const spr::Sprite* s; uint16_t* px; uint8_t* a; };
static HW g_hw[spr::COUNT];
static uint16_t g_pool[POOL];
static uint8_t g_apool[POOL];

void init() {
    int off = 0;
    for (int k = 0; k < spr::COUNT; k++) {
        const spr::Sprite* s = spr::all[k];
        const int w = s->w, h = s->h;
        if (off + w * h > POOL) { g_hw[k] = {s, nullptr, nullptr}; continue; }
        uint16_t* px = &g_pool[off];
        uint8_t* a = &g_apool[off];
        off += w * h;
        for (int i = 0; i < w; i++) {
            for (int j = 0; j < h; j++) {
                const uint16_t v = s->px[j * w + i];
                const int idx = i * h + j;
                if (v == 0xF81F) { a[idx] = 0; px[idx] = 0; continue; }
                const int r5 = (v >> 11) & 31, g6 = (v >> 5) & 63, b5 = v & 31;
                a[idx] = 1;
                px[idx] = vid::rgb((uint8_t)((r5 << 3) | (r5 >> 2)), (uint8_t)((g6 << 2) | (g6 >> 4)), (uint8_t)((b5 << 3) | (b5 >> 2)));
            }
        }
        g_hw[k] = {s, px, a};
    }
}

static const HW* find(const spr::Sprite& s) {
    for (int k = 0; k < spr::COUNT; k++) if (g_hw[k].s == &s) return &g_hw[k];
    return nullptr;
}

// (x0, y0) = coin haut-gauche en coordonnees portrait ; scale >= 1 (agrandissement entier)
static void psprite(const spr::Sprite& s, int x0, int y0, bool flip = false, int scale = 1) {
    const HW* hw = find(s);
    if (!hw || !hw->px) return;
    const int w = s.w, h = s.h;
    for (int i = 0; i < w * scale; i++) {
        const int sy = SY(x0 + i);
        if (sy < 0 || sy >= vid::H) continue;
        const int col = flip ? (w - 1 - i / scale) : (i / scale);
        const uint16_t* px = hw->px + col * h;
        const uint8_t* a = hw->a + col * h;
        int j = 0;
        while (j < h) {
            if (!a[j]) { j++; continue; }
            const int j0 = j;
            while (j < h && a[j]) j++;
            if (scale == 1) {
                int xs = SX(y0 + j0), n = j - j0, skip = 0;
                if (xs < 0) { skip = -xs; xs = 0; n -= skip; }
                if (xs + n > vid::W) n = vid::W - xs;
                if (n > 0) vid::raw_blit_row(xs, sy, px + j0 + skip, n);
            } else {
                for (int q = j0; q < j; q++) vid::fill_rect(SX(y0 + q * scale), sy, scale, 1, px[q]);
            }
        }
    }
}
static void psprite_c(const spr::Sprite& s, int cx, int cy, bool flip = false, int scale = 1) {
    psprite(s, cx - (s.w * scale) / 2, cy - (s.h * scale) / 2, flip, scale);
}

// ---------------------------------------------------------------------------
// Decor
// ---------------------------------------------------------------------------
static float g_cam = 0.0f;
static inline int WY(float wy) { return HUD_H + (int)lroundf(wy - g_cam); }
static inline int WX(float wx) { return (int)lroundf(wx); }

static void draw_water(const Game& g) {
    vid::clear(C(0x0A241C));
    const int cam = (int)lroundf(g_cam);
    for (int py = HUD_H; py < SCREEN_H; py++) {
        const int wy = py - HUD_H + cam;
        int xl = 12, xr = 214;
        if (wy < 118) {
            const float dy = 118.0f - (float)wy;
            const float q = 112.0f * 112.0f - dy * dy;
            const float hw = q > 0.0f ? sqrtf(q) : 0.0f;
            xl = (int)lroundf(124.0f - hw); xr = (int)lroundf(124.0f + hw);
        }
        const uint16_t col = (((wy / 14) & 1) ? C(0x1D7F73) : C(0x238B7B));
        pfill(xl, py, xr - xl, 1, col);
        if (wy >= 118 && wy < 520) pfill(214, py, 22, 1, ((wy / 10) & 1) ? C(0x2E3D2B) : C(0x364832));   // couloir de lancement
    }
    // rides de la mare
    const uint16_t rip = C(0x5CC2AD);
    static const int rx[10] = {40, 150, 90, 180, 60, 120, 30, 170, 100, 140};
    static const int ry[10] = {150, 200, 260, 330, 380, 120, 300, 420, 440, 270};
    for (int i = 0; i < 10; i++) {
        const int x = rx[i] + (int)lroundf(3.0f * sinf(g.animT * 0.8f + (float)i));
        const int y = WY((float)ry[i]);
        if (y < HUD_H - 6 || y > SCREEN_H) continue;
        pline(x - 5, y, x + 5, y, rip);
        pline(x - 3, y + 2, x + 3, y + 2, rip);
    }
}

static void draw_corners(const Game& g) {
    (void)g;
    // decor hors table (sous les guides, coins sombres) : roseaux, buche, rocher, lotus
    psprite(spr::cattail_post, 2, WY(2));
    psprite(spr::cattail_post, 212, WY(2));
    psprite(spr::log, 2, WY(460));
    psprite(spr::cattails, 4, WY(488));
    psprite(spr::rock, 168, WY(460));
    psprite(spr::lotus, 172, WY(488));
}

static void draw_walls(const Game& g) {
    const uint16_t dark = C(0x3B2A14), mid = C(0x8A6330), lite = C(0xD9B46E);
    // slingshots (triangles pleins) : geometrie identique a Game::buildTable()
    const uint16_t leaf = C(0x3CC24F), leafd = C(0x1E7A38);
    ptri(WX(36), WY(330), WX(36), WY(376), WX(70), WY(408), leaf);
    ptri(WX(190), WY(330), WX(190), WY(376), WX(156), WY(408), leaf);
    pline(WX(36), WY(330), WX(70), WY(408), C(0xB6F07A));
    pline(WX(190), WY(330), WX(156), WY(408), C(0xB6F07A));
    pline(WX(36), WY(330), WX(36), WY(376), leafd);  pline(WX(36), WY(376), WX(70), WY(408), leafd);
    pline(WX(190), WY(330), WX(190), WY(376), leafd); pline(WX(190), WY(376), WX(156), WY(408), leafd);
    // cadre en bois (les slingshots, kind != 0, sont dessines en triangles ci-dessus)
    for (int i = 0; i < g.wallCount; i++) {
        const Seg& s = g.walls[i];
        if (s.kind) continue;
        const int x1 = WX(s.x1), y1 = WY(s.y1), x2 = WX(s.x2), y2 = WY(s.y2);
        if ((y1 < -20 && y2 < -20) || (y1 > SCREEN_H + 20 && y2 > SCREEN_H + 20)) continue;
        pthick(x1, y1, x2, y2, 3.0f, dark);
    }
    for (int i = 0; i < g.wallCount; i++) {
        const Seg& s = g.walls[i];
        if (s.kind) continue;
        pthick(WX(s.x1), WY(s.y1), WX(s.x2), WY(s.y2), 1.6f, mid);
        pline(WX(s.x1), WY(s.y1), WX(s.x2), WY(s.y2), lite);
    }
    // piquets entre les couloirs
    for (int i = 0; i < 3; i++) { pfillcircle(81 + 32 * i, WY(60), 3, dark); pfillcircle(81 + 32 * i, WY(60), 1, lite); }
}

static void draw_lanes(const Game& g) {
    for (int i = 0; i < LANE_COUNT; i++) {
        const int x = WX(g.lanes[i].x), y = WY(g.lanes[i].y);
        const bool lit = (g.laneLit >> i) & 1;
        pfillcircle(x, y, 8, lit ? C(0xFFE04A) : C(0x14463C));
        pcircle(x, y, 8, lit ? C(0xFFF6B0) : C(0x0B2A22));
        const char l[2] = {"FROG"[i], 0};
        ptext(x - 4, y - 4, l, lit ? C(0x3B2A14) : C(0x7FD0B8));
    }
}

static void draw_pads(const Game& g) {
    for (int i = 0; i < PAD_COUNT; i++) {
        const int x = WX(g.pads[i].x), y = WY(g.pads[i].y);
        const bool lit = (g.padLit >> i) & 1;
        const bool flash = g.padFlash[i] > 0.0f;
        pfillcircle(x, y, 10, C(0x1E7A38));
        pfillcircle(x, y, 9, flash ? C(0x9BF07A) : C(0x2FA84F));
        pthick(x, y, x + 9, y - 3, 1.2f, C(0x238B7B));   // l'encoche du nenuphar
        pline(x - 6, y + 2, x + 3, y + 5, C(0x56C874));
        if (lit) {   // fleur de lotus
            for (int k = 0; k < 5; k++) {
                const float a = (float)k * 1.2566f - 1.5708f;
                pfillcircle(x + (int)lroundf(cosf(a) * 4.5f), y + (int)lroundf(sinf(a) * 4.5f), 3, C(0xFF8CC8));
            }
            pfillcircle(x, y, 3, C(0xFFE04A));
        }
    }
}

static void draw_bumpers(const Game& g) {
    for (int i = 0; i < BUMPER_COUNT; i++) {
        const int x = WX(g.bumpers[i].x), y = WY(g.bumpers[i].y);
        const bool flash = g.bumperFlash[i] > 0.0f;
        if (i == 2) {
            if (flash) psprite_c(spr::boom, x, y);
            psprite_c(spr::bumper_mushroom, x, y);
        } else {
            psprite_c(flash ? spr::bumper_mouth : spr::bumper_stack, x, y);
        }
    }
}

static void draw_spinner(const Game& g) {
    const int x = WX(g.spinner.x), y = WY(g.spinner.y);
    psprite_c(spr::fly_post, x, y);
    if (g.spinVel > 0.5f) {   // une mouche tourne autour du poteau
        const float a = g.spinAngle;
        psprite_c(spr::fly, x + (int)lroundf(cosf(a) * 13.0f), y + (int)lroundf(sinf(a) * 13.0f) - 2);
    }
}

static void draw_duck(const Game& g) {
    const Duck& d = g.duck;
    if (d.st == DuckState::Away) return;
    int cx = WX(d.x), cy = WY(d.y);
    const bool white = d.flash > 0.0f;
    if (d.st == DuckState::Dying) {
        const float t = 2.0f - d.timer;
        cy += (int)lroundf(t * 26.0f);
        if (((int)(t * 12.0f)) & 1) return;   // clignote en coulant
        if (t < 0.4f) psprite_c(spr::boom, cx, cy);
    }
    const int dr = d.dir;
    const uint16_t body = white ? C(0xFFFFFF) : C(0xFFD83A);
    const uint16_t wing = white ? C(0xDDDDDD) : C(0xE0B020);
    const uint16_t beak = white ? C(0xFFFFFF) : C(0xFF8A1F);
    pfillcircle(cx, cy, 11, body);
    ptri(cx - dr * 9, cy - 2, cx - dr * 17, cy - 8, cx - dr * 10, cy + 5, body);   // queue
    pfillcircle(cx - dr * 2, cy + 2, 5, wing);
    const int hx = cx + dr * 8, hy = cy - 9;
    pfillcircle(hx, hy, 6, body);
    ptri(hx + dr * 5, hy - 1, hx + dr * 13, hy + 1, hx + dr * 5, hy + 4, beak);
    pfillcircle(hx + dr * 1, hy - 2, 1, C(0x101010));
    if (d.st == DuckState::Patrol) {
        // barre de vie
        const int bw = 30;
        pfill(cx - bw / 2 - 1, cy - 25, bw + 2, 5, C(0x101010));
        const int fw = d.maxHp > 0 ? (bw * (d.hp > 0 ? d.hp : 0)) / d.maxHp : 0;
        pfill(cx - bw / 2, cy - 24, fw, 3, d.stun > 0.0f ? C(0xFFE04A) : C(0xE0453A));
        if (d.stun > 0.0f) {   // etoiles d'etourdissement
            for (int k = 0; k < 3; k++) {
                const float a = g.animT * 5.0f + (float)k * 2.0944f;
                psprite_c(spr::star, cx + (int)lroundf(cosf(a) * 14.0f), cy - 14 + (int)lroundf(sinf(a) * 3.0f));
            }
        }
    }
}

static void draw_plunger(const Game& g) {
    const int x = WX(LANE_X);
    const int base = WY(516.0f);
    const float push = (g.ball.active && g.ball.inShooter) ? g.plungerCharge * 12.0f : 0.0f;
    const int head = WY(LANE_REST_Y + 6.0f + push);
    pfill(x - 10, base, 20, 4, C(0x707070));
    // ressort en zigzag
    int py = base;
    int sgn = 1;
    while (py > head + 3) {
        const int ny = py - 4;
        pline(x - 5 * sgn, py, x + 5 * sgn, ny, C(0xC8C8C8));
        sgn = -sgn;
        py = ny;
    }
    pfill(x - 8, head, 16, 4, C(0xD8D8D8));
    pfill(x - 8, head + 3, 16, 1, C(0x707070));
}

static void draw_flippers(const Game& g) {
    for (int i = 0; i < 2; i++) {
        const Flipper& f = g.fl[i];
        const float dx = f.isLeft ? cosf(f.angle) : -cosf(f.angle);
        const int x0 = WX(f.px), y0 = WY(f.py);
        const int x1 = WX(f.px + dx * f.len), y1 = WY(f.py + sinf(f.angle) * f.len);
        ptaper(x0, y0, x1, y1, 6.5f, 4.0f, C(0x1E5A2A));
        ptaper(x0, y0, x1, y1, 5.0f, 2.6f, C(0xF6EEC4));
        pfillcircle(x0, y0, 3, C(0x9AA0A6));
        pfillcircle(x0, y0, 1, C(0xE8E8E8));
    }
}

static void draw_ball(const Game& g) {
    const Ball& b = g.ball;
    if (!b.active) return;
    float by = b.y;
    if (b.inShooter) by += g.plungerCharge * 12.0f;
    const int x = WX(b.x), y = WY(by);
    if (g.ballSaveTimer > 0.0f) {   // aura de protection : clignote de plus en plus vite
        const bool on = g.ballSaveTimer > 3.0f || (((int)(g.animT * 8.0f)) & 1);
        if (on) pcircle(x, y, 8, C(0x7FF0FF));
    }
    psprite_c(spr::ball, x, y);
}

static void draw_world(const Game& g) {
    g_cam = g.cameraY;
    draw_water(g);
    draw_corners(g);
    draw_walls(g);
    draw_lanes(g);
    draw_pads(g);
    draw_spinner(g);
    draw_bumpers(g);
    draw_duck(g);
    draw_plunger(g);
    draw_flippers(g);
    draw_ball(g);
}

// ---------------------------------------------------------------------------
// HUD et bandeaux
// ---------------------------------------------------------------------------
static void draw_hud(const Game& g) {
    pfill(0, 0, SCREEN_W, HUD_H, C(0x07261D));
    pfill(0, HUD_H - 2, SCREEN_W, 2, C(0x3CC24F));
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", (unsigned)g.score);
    ptext_sh(6, 4, buf, C(0xFFF6B0), C(0x000000), 2);
    // billes restantes
    for (int i = 0; i < g.ballsLeft; i++) psprite(spr::frog_heart, 6 + i * 18, 22);
    // mouches
    psprite(spr::fly, 66, 22);
    snprintf(buf, sizeof(buf), "%d", g.flies);
    ptext(84, 26, buf, C(0xE8FFD0));
    // multiplicateur
    psprite(spr::bell, 120, 21);
    snprintf(buf, sizeof(buf), "x%d", g.multiplier);
    ptext_sh(140, 24, buf, C(0xFFE04A), C(0x000000), 1);
    // niveau du canard et protection de bille
    snprintf(buf, sizeof(buf), "DUCK L%d", g.duck.level);
    ptext(172, 6, buf, C(0xFFD83A));
    if (g.ballSaveTimer > 0.0f) {
        const int w = (int)lroundf(60.0f * g.ballSaveTimer / BALL_SAVE_SEC);
        pfill(172, 26, 62, 6, C(0x101010));
        pfill(173, 27, w, 4, C(0x7FF0FF));
    }
}

static void draw_banner(const Game& g) {
    if (g.bannerTimer > 0.0f && g.banner[0]) {
        const int y = HUD_H + 34;
        const int w = text_w(g.banner) + 12;
        pfill(SCREEN_W / 2 - w / 2, y - 5, w, 18, C(0x07261D));
        pfill(SCREEN_W / 2 - w / 2, y - 5, w, 1, C(0xFFE04A));
        pfill(SCREEN_W / 2 - w / 2, y + 12, w, 1, C(0xFFE04A));
        ptext_center(SCREEN_W / 2, y, g.banner, C(0xFFF6B0));
    }
    if (g.achTimer > 0.0f && g.achPopup >= 0) {
        const int y = SCREEN_H - 40;
        pfill(10, y, 220, 30, C(0x07261D));
        pfill(10, y, 220, 1, C(0xFFE04A));
        pfill(10, y + 29, 220, 1, C(0xFFE04A));
        psprite(spr::star, 16, y + 6);
        ptext(40, y + 5, "TROPHY UNLOCKED", C(0xE8FFD0));
        ptext(40, y + 17, ach_name(g.achPopup), C(0xFFE04A));
    }
}

// ---------------------------------------------------------------------------
// Ecrans
// ---------------------------------------------------------------------------
static void draw_menu_bg(const Game& g) {
    vid::clear(C(0x0A241C));
    for (int py = 0; py < SCREEN_H; py++) {
        pfill(0, py, SCREEN_W, 1, (((py + (int)(g.animT * 6.0f)) / 14) & 1) ? C(0x1D7F73) : C(0x238B7B));
    }
}

static void draw_title(const Game& g) {
    draw_menu_bg(g);
    psprite(spr::cattails, 4, 280);
    psprite(spr::rock, 190, 292);
    psprite(spr::lilypad, 150, 150);
    psprite(spr::lotus, 24, 150);
    ptext_center_sh(SCREEN_W / 2, 18, "FROGBALL", C(0xB6F07A), C(0x0B2A22), 3);
    ptext_center_sh(SCREEN_W / 2, 48, "AKA EDITION", C(0xFFF6B0), C(0x0B2A22), 1);
    const int bob = (int)lroundf(3.0f * sinf(g.animT * 3.0f));
    psprite(spr::frog_stand, SCREEN_W / 2 - 46, 76 + bob, false, 2);
    psprite(spr::fly, 40 + (int)lroundf(20.0f * sinf(g.animT * 2.0f)), 90 + (int)lroundf(10.0f * cosf(g.animT * 3.0f)));
    const char* items[2] = {"PLAY", "TROPHIES"};
    for (int i = 0; i < 2; i++) {
        const int y = 196 + i * 24;
        const bool sel = (g.menuSel == i);
        if (sel) {
            pfill(50, y - 4, 140, 20, C(0x07261D));
            pfill(50, y - 4, 140, 1, C(0xFFE04A));
            pfill(50, y + 15, 140, 1, C(0xFFE04A));
        }
        ptext_center(SCREEN_W / 2, y, items[i], sel ? C(0xFFE04A) : C(0xE8FFD0), 1);
        if (sel) { ptext(58, y, ">", C(0xFFE04A)); ptext(174, y, "<", C(0xFFE04A)); }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "BEST %u", (unsigned)g.save.top[0].score);
    ptext_center_sh(SCREEN_W / 2, 250, buf, C(0xFFF6B0), C(0x0B2A22));
    int got = 0;
    for (int i = 0; i < ACH_COUNT; i++) got += g.hasAch(i) ? 1 : 0;
    snprintf(buf, sizeof(buf), "TROPHIES %d/%d", got, (int)ACH_COUNT);
    ptext_center_sh(SCREEN_W / 2, 264, buf, C(0xE8FFD0), C(0x0B2A22));
    ptext_center_sh(SCREEN_W / 2, 296, "L/R: MOVE  B/R1: OK", C(0xB6F07A), C(0x0B2A22));
}

static void draw_trophies(const Game& g) {
    draw_menu_bg(g);
    ptext_center_sh(SCREEN_W / 2, 8, "TROPHIES", C(0xFFE04A), C(0x0B2A22), 2);
    for (int i = 0; i < ACH_COUNT; i++) {
        const int y = 34 + i * 30;
        const bool got = g.hasAch(i);
        pfill(8, y, 224, 27, C(0x07261D));
        if (got) psprite(spr::star, 14, y + 4);
        else     pfillcircle(23, y + 13, 5, C(0x2A4A40));
        ptext(40, y + 4, ach_name(i), got ? C(0xFFE04A) : C(0x7FA89A));
        ptext(40, y + 15, ach_desc(i), got ? C(0xE8FFD0) : C(0x587A6E));
    }
    ptext_center_sh(SCREEN_W / 2, 306, "B/R1: BACK", C(0xB6F07A), C(0x0B2A22));
}

static void draw_gameover(const Game& g) {
    pfill(20, 90, 200, 120, C(0x07261D));
    pfill(20, 90, 200, 2, C(0xFFE04A));
    pfill(20, 208, 200, 2, C(0xFFE04A));
    ptext_center(SCREEN_W / 2, 100, "GAME OVER", C(0xFFE04A), 2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", (unsigned)g.score);
    ptext_center(SCREEN_W / 2, 128, buf, C(0xFFF6B0), 2);
    if (g.lastRank >= 0 && g.lastRank < TOP_N) ptext_center(SCREEN_W / 2, 154, "NEW HIGH SCORE!", C(0xB6F07A));
    else { snprintf(buf, sizeof(buf), "BEST %u", (unsigned)g.save.top[0].score); ptext_center(SCREEN_W / 2, 154, buf, C(0xE8FFD0)); }
    psprite(spr::frog_sit, SCREEN_W / 2 - 23, 168);
    if (g.stateLock <= 0.0f) ptext_center(SCREEN_W / 2, 198, "PRESS B / R1", C(0xB6F07A));
}

static void draw_nameentry(const Game& g) {
    draw_menu_bg(g);
    ptext_center_sh(SCREEN_W / 2, 30, "NEW HIGH SCORE!", C(0xFFE04A), C(0x0B2A22), 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", (unsigned)g.score);
    ptext_center_sh(SCREEN_W / 2, 50, buf, C(0xFFF6B0), C(0x0B2A22), 2);
    ptext_center_sh(SCREEN_W / 2, 90, "ENTER YOUR NAME", C(0xE8FFD0), C(0x0B2A22), 1);
    for (int i = 0; i < 3; i++) {
        const int x = SCREEN_W / 2 - 54 + i * 40;
        pfill(x - 4, 116, 32, 44, C(0x07261D));
        const bool cur = (i == g.nameIdx);
        pfill(x - 4, 158, 32, 3, cur ? C(0xFFE04A) : C(0x3CC24F));
        const char c[2] = {g.nameBuf[i], 0};
        ptext(x, 126, c, cur ? C(0xFFE04A) : C(0xE8FFD0), 3);
    }
    psprite(spr::frog_sit, SCREEN_W / 2 - 23, 180);
    ptext_center_sh(SCREEN_W / 2, 250, "L/R: LETTER", C(0xB6F07A), C(0x0B2A22));
    ptext_center_sh(SCREEN_W / 2, 264, "R1: OK   B: BACK", C(0xB6F07A), C(0x0B2A22));
}

void draw(const Game& g) {
    switch (g.state) {
        case State::Title:     draw_title(g); break;
        case State::Trophies:  draw_trophies(g); break;
        case State::NameEntry: draw_nameentry(g); break;
        case State::Playing:
        case State::GameOver:
            draw_world(g);
            draw_hud(g);
            draw_banner(g);
            if (g.tilted && g.state == State::Playing) ptext_center_sh(SCREEN_W / 2, HUD_H + 70, "TILT", C(0xFF5050), C(0x000000), 3);
            if (g.state == State::GameOver) draw_gameover(g);
            break;
    }
}

} // namespace render
