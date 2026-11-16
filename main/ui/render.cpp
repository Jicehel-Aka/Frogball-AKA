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
// Deux styles au choix (option du menu) :
//  - GAME BOY : palette du jeu d'origine, 3 verts + 2 oranges. Toute couleur demandee est ramenee a la plus proche,
//    et les sprites sont quantifies de la meme facon : l'ensemble garde un look coherent.
//  - COULEUR : memes roles, mais en couleurs franches et contrastees ; les couleurs d'origine sont conservees.
static const uint8_t PAL_GB[5][3] = {
    {6, 63, 50},      // 0 vert tres sombre (contours, murs)
    {18, 152, 140},   // 1 vert d'eau (ombres, nenuphars)
    {169, 229, 162},  // 2 vert clair (fond du plateau, textes)
    {249, 179, 32},   // 3 orange (balle, flippers, cibles actives)
    {201, 120, 16},   // 4 orange fonce
};
static const uint8_t PAL_COL[5][3] = {
    {12, 22, 52},     // 0 bleu nuit (contours, murs)
    {22, 120, 214},   // 1 bleu eau / nenuphars
    {196, 244, 150},  // 2 vert clair vif (fond du plateau, textes)
    {255, 214, 20},   // 3 jaune dore (balle, flippers, cibles actives)
    {230, 70, 40},    // 4 rouge-orange (canard, becs, details chauds)
};
static int g_style = STYLE_GAMEBOY;
static int pal_index(int r, int g, int b) {
    int best = 0, bd = 1 << 30;
    for (int i = 0; i < 5; i++) {
        const int dr = r - PAL_GB[i][0], dg = g - PAL_GB[i][1], db = b - PAL_GB[i][2];
        const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static inline uint16_t PC(int i) {
    const uint8_t* p = (g_style == STYLE_COLOR) ? PAL_COL[i] : PAL_GB[i];
    return vid::rgb(p[0], p[1], p[2]);
}
static uint16_t C(uint32_t c) {
    if (g_style == STYLE_COLOR) return vid::rgb888(c);
    const int i = pal_index((int)((c >> 16) & 255), (int)((c >> 8) & 255), (int)(c & 255));
    return PC(i);
}
enum { P_DARK = 0, P_TEAL = 1, P_LIGHT = 2, P_ORANGE = 3, P_ORANGE_D = 4 };

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
struct HW { const spr::Sprite* s; uint16_t* px; uint8_t* a; };   // a : index de palette Game Boy, 255 = transparent
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
                if (v == 0xF81F) { a[idx] = 255; px[idx] = 0; continue; }
                const int r5 = (v >> 11) & 31, g6 = (v >> 5) & 63, b5 = v & 31;
                const int R = (r5 << 3) | (r5 >> 2), G = (g6 << 2) | (g6 >> 4), B = (b5 << 3) | (b5 >> 2);
                a[idx] = (uint8_t)pal_index(R, G, B);
                px[idx] = vid::rgb((uint8_t)R, (uint8_t)G, (uint8_t)B);   // couleur d'origine (style COULEUR)
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
        const bool gb = (g_style != STYLE_COLOR);
        int j = 0;
        while (j < h) {
            if (a[j] == 255) { j++; continue; }
            const int j0 = j;
            while (j < h && a[j] != 255) j++;
            if (scale == 1) {
                int xs = SX(y0 + j0), n = j - j0, skip = 0;
                if (xs < 0) { skip = -xs; xs = 0; n -= skip; }
                if (xs + n > vid::W) n = vid::W - xs;
                if (n <= 0) continue;
                if (!gb) { vid::raw_blit_row(xs, sy, px + j0 + skip, n); continue; }
                uint16_t tmp[64];
                if (n > 64) n = 64;   // les sprites font au plus 64 px de haut
                for (int q = 0; q < n; q++) tmp[q] = PC(a[j0 + skip + q]);
                vid::raw_blit_row(xs, sy, tmp, n);
            } else {
                for (int q = j0; q < j; q++) vid::fill_rect(SX(y0 + q * scale), sy, scale, 1, gb ? PC(a[q]) : px[q]);
            }
        }
    }
}
static void psprite_c(const spr::Sprite& s, int cx, int cy, bool flip = false, int scale = 1) {
    psprite(s, cx - (s.w * scale) / 2, cy - (s.h * scale) / 2, flip, scale);
}

// ---------------------------------------------------------------------------
// Decor : la table tient sur un seul ecran (pas de defilement) ; le bandeau HUD est SOUS la table.
// ---------------------------------------------------------------------------
static inline int WY(float wy) { return (int)lroundf(wy); }
static inline int WX(float wx) { return (int)lroundf(wx); }

static void draw_table_bg(const Game& g) {
    vid::clear(PC(P_DARK));
    for (int py = 0; py < VIEW_H; py++) {
        int xl = 12, xr = 214;
        if (py < 118) {
            const float dy = 118.0f - (float)py;
            const float q = 112.0f * 112.0f - dy * dy;
            const float hw = q > 0.0f ? sqrtf(q) : 0.0f;
            xl = (int)lroundf(124.0f - hw); xr = (int)lroundf(124.0f + hw);
        }
        pfill(xl, py, xr - xl, 1, PC(P_LIGHT));
        if (py >= 118) pfill(214, py, 22, 1, ((py / 6) & 1) ? PC(P_TEAL) : PC(P_DARK));   // couloir de lancement
    }
    // l'eau sous les flippers (la bille s'y perd) : bande d'eau avec vagues
    for (int py = 272; py < VIEW_H; py++) {
        const int wob = (int)lroundf(2.0f * sinf(g.animT * 2.0f + (float)py * 0.5f));
        pfill(12, py, 202, 1, ((py - 272) / 5 & 1) ? PC(P_TEAL) : PC(P_LIGHT));
        for (int k = 0; k < 6; k++) pfill(20 + k * 34 + wob, py, 10, 1, PC(P_TEAL));
    }
    // dessous des guides : roche sombre
    ptri(12, 226, 62, 262, 12, VIEW_H, PC(P_DARK));
    ptri(214, 226, 164, 262, 214, VIEW_H, PC(P_DARK));
    pfill(12, 262, 50, VIEW_H - 262, PC(P_DARK));
    pfill(164, 262, 50, VIEW_H - 262, PC(P_DARK));
}

static void draw_corners(const Game& g) {
    // decor hors table (coins sombres) : roseaux, buche, rocher, lotus
    psprite(spr::cattail_post, 2, 2);
    psprite(spr::cattail_post, 212, 2);
    psprite(spr::cattails, 3, 262);
    psprite(spr::lotus, 172, 268);
    if (g.table == TABLE_DUCK) psprite(spr::rock, 22, 275);
}

static void draw_walls(const Game& g) {
    const uint16_t dark = PC(P_DARK), teal = PC(P_TEAL);
    // slingshots (triangles pleins) : geometrie identique a Game::buildTable()
    ptri(36, 202, 36, 226, 70, 248, teal);
    ptri(190, 202, 190, 226, 156, 248, teal);
    pline(36, 202, 70, 248, dark);  pline(36, 202, 36, 226, dark);  pline(36, 226, 70, 248, dark);
    pline(190, 202, 156, 248, dark); pline(190, 202, 190, 226, dark); pline(190, 226, 156, 248, dark);
    pline(37, 204, 68, 247, PC(P_LIGHT));
    pline(189, 204, 158, 247, PC(P_LIGHT));
    // cadre (les slingshots, kind != 0, sont dessines ci-dessus)
    for (int i = 0; i < g.wallCount; i++) {
        const Seg& s = g.walls[i];
        if (s.kind) continue;
        pthick(WX(s.x1), WY(s.y1), WX(s.x2), WY(s.y2), 3.0f, dark);
    }
    for (int i = 0; i < g.wallCount; i++) {
        const Seg& s = g.walls[i];
        if (s.kind) continue;
        pline(WX(s.x1), WY(s.y1), WX(s.x2), WY(s.y2), teal);
    }
    // piquets entre les couloirs
    for (int i = 0; i < 3; i++) { pfillcircle(81 + 32 * i, 28, 3, dark); pfillcircle(81 + 32 * i, 28, 1, teal); }
}

static void draw_lanes(const Game& g) {
    for (int i = 0; i < LANE_COUNT; i++) {
        const int x = WX(g.lanes[i].x), y = WY(g.lanes[i].y);
        const bool lit = (g.laneLit >> i) & 1;
        pfillcircle(x, y, 8, PC(P_DARK));
        pfillcircle(x, y, 7, lit ? PC(P_ORANGE) : PC(P_LIGHT));
        const char l[2] = {"FROG"[i], 0};
        ptext(x - 4, y - 4, l, lit ? PC(P_DARK) : PC(P_TEAL));
    }
}

static void draw_pads(const Game& g) {
    for (int i = 0; i < g.padCount; i++) {
        const int x = WX(g.pads[i].x), y = WY(g.pads[i].y);
        const bool lit = (g.padLit >> i) & 1;
        const bool flash = g.padFlash[i] > 0.0f;
        if (g.table == TABLE_POND) {   // nenuphars : le premier contact allume une fleur orange
            pfillcircle(x, y, 10, PC(P_DARK));
            pfillcircle(x, y, 9, flash ? PC(P_LIGHT) : PC(P_TEAL));
            pthick(x, y, x + 9, y - 3, 1.2f, PC(P_DARK));   // l'encoche du nenuphar
            if (lit) {
                for (int k = 0; k < 5; k++) {
                    const float a = (float)k * 1.2566f - 1.5708f;
                    pfillcircle(x + (int)lroundf(cosf(a) * 4.5f), y + (int)lroundf(sinf(a) * 4.5f), 3, PC(P_ORANGE));
                }
                pfillcircle(x, y, 2, PC(P_DARK));
            }
        } else {   // oeufs : orange tant qu'ils sont la, un nid vide (anneau) une fois ramasses
            if (lit) {
                pcircle(x, y + 2, 7, PC(P_TEAL));
                pcircle(x, y + 2, 6, PC(P_TEAL));
            } else {
                pfillcircle(x, y + 1, 8, PC(P_DARK));
                pfillcircle(x, y + 1, 7, flash ? PC(P_LIGHT) : PC(P_ORANGE));
                pfillcircle(x, y - 3, 5, flash ? PC(P_LIGHT) : PC(P_ORANGE));
                pfill(x - 3, y - 4, 2, 2, PC(P_LIGHT));
            }
        }
    }
}

static void draw_bumpers(const Game& g) {
    for (int i = 0; i < g.bumperCount; i++) {
        const int x = WX(g.bumpers[i].x), y = WY(g.bumpers[i].y);
        const bool flash = g.bumperFlash[i] > 0.0f;
        pfillcircle(x, y, 15, PC(P_DARK));
        pfillcircle(x, y, 13, flash ? PC(P_ORANGE) : PC(P_TEAL));
        pfillcircle(x, y, 8, PC(P_DARK));
        pfillcircle(x, y, 6, flash ? PC(P_LIGHT) : PC(P_TEAL));
        pfillcircle(x, y, 2, PC(P_DARK));
    }
}

static void draw_pond_decor(const Game& g) {
    // la grenouille centrale (entouree des nenuphars) et les cibles en losange comme dans l'original
    const int cx = 114, cy = 176;
    psprite_c(spr::frog_sit, cx, cy + 4, false, 1);
    for (int k = 0; k < 6; k++) {
        const float a = (float)k * 1.0472f + 0.5236f;
        const int x = cx + (int)lroundf(cosf(a) * 28.0f), y = cy + (int)lroundf(sinf(a) * 16.0f);
        pfill(x - 2, y - 2, 5, 5, PC(P_TEAL));
        pfill(x - 1, y - 1, 3, 3, PC(P_LIGHT));
    }
    // touches A / S, comme sur le vrai ecran
    (void)g;
    ptext(96, 226, "A", PC(P_DARK)); ptext(136, 226, "S", PC(P_DARK));
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
    if (g.table != TABLE_DUCK || d.st == DuckState::Away) return;
    int cx = WX(d.x), cy = WY(d.y);
    const bool white = d.flash > 0.0f;
    if (d.st == DuckState::Dying) {
        const float t = 2.0f - d.timer;
        cy += (int)lroundf(t * 26.0f);
        if (((int)(t * 12.0f)) & 1) return;   // clignote en coulant
        if (t < 0.4f) psprite_c(spr::boom, cx, cy);
    }
    const int dr = d.dir;
    const uint16_t body = white ? PC(P_LIGHT) : PC(P_ORANGE);
    const uint16_t wing = white ? PC(P_LIGHT) : PC(P_ORANGE_D);
    const uint16_t beak = white ? PC(P_LIGHT) : PC(P_ORANGE_D);
    pfillcircle(cx, cy, 12, PC(P_DARK));
    pfillcircle(cx, cy, 11, body);
    ptri(cx - dr * 9, cy - 2, cx - dr * 17, cy - 8, cx - dr * 10, cy + 5, body);   // queue
    pfillcircle(cx - dr * 2, cy + 2, 5, wing);
    const int hx = cx + dr * 8, hy = cy - 9;
    pfillcircle(hx, hy, 7, PC(P_DARK));
    pfillcircle(hx, hy, 6, body);
    ptri(hx + dr * 5, hy - 1, hx + dr * 13, hy + 1, hx + dr * 5, hy + 4, beak);
    pfillcircle(hx + dr * 1, hy - 2, 1, PC(P_DARK));
    if (d.st == DuckState::Patrol) {
        // barre de vie
        const int bw = 30;
        pfill(cx - bw / 2 - 1, cy - 25, bw + 2, 5, PC(P_DARK));
        const int fw = d.maxHp > 0 ? (bw * (d.hp > 0 ? d.hp : 0)) / d.maxHp : 0;
        pfill(cx - bw / 2, cy - 24, fw, 3, d.stun > 0.0f ? PC(P_LIGHT) : PC(P_ORANGE));
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
    const int base = 295;
    const float push = (g.ball.active && g.ball.inShooter) ? g.plungerCharge * 12.0f : 0.0f;
    const int head = WY(LANE_REST_Y + 6.0f + push);
    int py = base;
    int sgn = 1;
    while (py > head + 3) {
        const int ny = py - 3;
        pline(x - 5 * sgn, py, x + 5 * sgn, ny, PC(P_LIGHT));
        sgn = -sgn;
        py = ny;
    }
    pfill(x - 8, head, 16, 3, PC(P_ORANGE));
    pfill(x - 8, head + 3, 16, 1, PC(P_DARK));
}

static void draw_flippers(const Game& g) {
    for (int i = 0; i < 2; i++) {
        const Flipper& f = g.fl[i];
        const float dx = f.isLeft ? cosf(f.angle) : -cosf(f.angle);
        const int x0 = WX(f.px), y0 = WY(f.py);
        const int x1 = WX(f.px + dx * f.len), y1 = WY(f.py + sinf(f.angle) * f.len);
        ptaper(x0, y0, x1, y1, 6.5f, 4.0f, PC(P_DARK));
        ptaper(x0, y0, x1, y1, 5.0f, 2.6f, PC(P_ORANGE));
        pfillcircle(x0, y0, 3, PC(P_DARK));
        pfillcircle(x0, y0, 1, PC(P_LIGHT));
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
        if (on) pcircle(x, y, 8, PC(P_TEAL));
    }
    pfillcircle(x, y, 6, PC(P_DARK));
    pfillcircle(x, y, 5, PC(P_ORANGE));
    pfill(x - 2, y - 2, 2, 2, PC(P_LIGHT));
}

// Ecran de transition entre deux tables : on plonge dans l'eau (bulles qui montent), comme dans l'original.
static void draw_travel(const Game& g) {
    vid::clear(PC(P_TEAL));
    for (int i = 0; i < 18; i++) {
        const float sp = 30.0f + (float)((i * 37) % 50);
        const int x = 14 + (i * 53) % 212;
        const int y = SCREEN_H - (int)fmodf(g.animT * sp + (float)(i * 71), (float)SCREEN_H + 20.0f);
        const int r = 3 + (i % 4) * 2;
        pcircle(x, y, r, PC(P_LIGHT));
        pfill(x - r / 2, y - r / 2, 2, 2, PC(P_LIGHT));
    }
    // vagues en haut
    for (int x = 0; x < SCREEN_W; x += 8)
        pfillcircle(x + 4, 4 + (int)lroundf(2.0f * sinf(g.animT * 4.0f + (float)x * 0.2f)), 6, PC(P_LIGHT));
    const char* t = (g.travelTo == TABLE_DUCK) ? "DUCK TABLE" : "THE POND";
    pfill(40, 140, 160, 30, PC(P_DARK));
    ptext_center(SCREEN_W / 2, 150, t, PC(P_ORANGE), 2);
}

static void draw_world(const Game& g) {
    if (g.travelT > 0.0f) { draw_travel(g); return; }
    draw_table_bg(g);
    draw_corners(g);
    if (g.table == TABLE_POND) draw_pond_decor(g);
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
// HUD (SOUS la table, comme dans l'original) et bandeaux
// ---------------------------------------------------------------------------
static void draw_hud(const Game& g) {
    const int y0 = VIEW_H;
    pfill(0, y0, SCREEN_W, HUD_H, PC(P_TEAL));
    pfill(0, y0, SCREEN_W, 2, PC(P_DARK));
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", (unsigned)g.score);
    ptext_sh(6, y0 + 7, buf, PC(P_ORANGE), PC(P_DARK), 1);
    snprintf(buf, sizeof(buf), "X%d", g.multiplier);
    ptext_center_sh(SCREEN_W / 2 + 2, y0 + 7, buf, PC(P_ORANGE), PC(P_DARK), 1);
    snprintf(buf, sizeof(buf), "BALL %d", g.ballsLeft);
    ptext_sh(SCREEN_W - 6 - text_w(buf), y0 + 7, buf, PC(P_ORANGE), PC(P_DARK), 1);
    if (g.ballSaveTimer > 0.0f) {   // protection de bille : fine barre au-dessus du bandeau
        const int w = (int)lroundf((float)(SCREEN_W - 4) * g.ballSaveTimer / BALL_SAVE_SEC);
        pfill(2, y0 + 3, w, 2, PC(P_LIGHT));
    }
}

static void draw_banner(const Game& g) {
    if (g.bannerTimer > 0.0f && g.banner[0] && g.travelT <= 0.0f) {
        const int y = 62;
        const int w = text_w(g.banner) + 12;
        pfill(SCREEN_W / 2 - w / 2, y - 5, w, 18, PC(P_DARK));
        pfill(SCREEN_W / 2 - w / 2, y - 5, w, 1, PC(P_ORANGE));
        pfill(SCREEN_W / 2 - w / 2, y + 12, w, 1, PC(P_ORANGE));
        ptext_center(SCREEN_W / 2, y, g.banner, PC(P_LIGHT));
    }
    if (g.achTimer > 0.0f && g.achPopup >= 0) {
        const int y = 2;   // en haut de la table : ne cache ni les flippers ni la bille
        pfill(10, y, 220, 30, PC(P_DARK));
        pfill(10, y, 220, 1, PC(P_ORANGE));
        pfill(10, y + 29, 220, 1, PC(P_ORANGE));
        psprite(spr::star, 16, y + 6);
        ptext(40, y + 5, "TROPHY UNLOCKED", PC(P_LIGHT));
        ptext(40, y + 17, ach_name(g.achPopup), PC(P_ORANGE));
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
    const char* items[3] = {"PLAY", "OPTIONS", "TROPHIES"};
    for (int i = 0; i < 3; i++) {
        const int y = 174 + i * 22;
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

static void draw_options(const Game& g) {
    draw_menu_bg(g);
    ptext_center_sh(SCREEN_W / 2, 20, "OPTIONS", C(0xFFE04A), C(0x0B2A22), 2);
    ptext_center_sh(SCREEN_W / 2, 70, "STYLE", C(0xE8FFD0), C(0x0B2A22), 1);
    const bool col = (g.save.style == STYLE_COLOR);
    pfill(24, 88, 192, 26, PC(P_DARK));
    pfill(24, 88, 192, 1, PC(P_ORANGE));
    pfill(24, 113, 192, 1, PC(P_ORANGE));
    ptext(32, 97, "<", PC(P_ORANGE));
    ptext(200, 97, ">", PC(P_ORANGE));
    ptext_center(SCREEN_W / 2, 97, col ? "FULL COLOR" : "GAME BOY", PC(P_LIGHT));
    // apercu des deux palettes (celle en cours est entouree)
    for (int st = 0; st < 2; st++) {
        const int y = 140 + st * 44;
        const uint8_t (*pal)[3] = st ? PAL_COL : PAL_GB;
        ptext_center_sh(SCREEN_W / 2, y, st ? "FULL COLOR" : "GAME BOY", PC(P_LIGHT), PC(P_DARK), 1);
        if ((st == 1) == col) pfill(36, y + 12, 168, 26, PC(P_ORANGE));
        pfill(38, y + 14, 164, 22, PC(P_DARK));
        for (int i = 0; i < 5; i++) pfill(42 + i * 31, y + 17, 28, 16, vid::rgb(pal[i][0], pal[i][1], pal[i][2]));
    }
    ptext_center_sh(SCREEN_W / 2, 270, "L/R: CHANGE", PC(P_LIGHT), PC(P_DARK));
    ptext_center_sh(SCREEN_W / 2, 284, "B/R1: BACK", PC(P_LIGHT), PC(P_DARK));
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
    g_style = (g.save.style == STYLE_COLOR) ? STYLE_COLOR : STYLE_GAMEBOY;
    switch (g.state) {
        case State::Options:   draw_options(g); break;
        case State::Title:     draw_title(g); break;
        case State::Trophies:  draw_trophies(g); break;
        case State::NameEntry: draw_nameentry(g); break;
        case State::Playing:
        case State::GameOver:
            draw_world(g);
            draw_hud(g);
            draw_banner(g);
            if (g.tilted && g.state == State::Playing) ptext_center_sh(SCREEN_W / 2, 70, "TILT", C(0xFF5050), C(0x000000), 3);
            if (g.state == State::GameOver) draw_gameover(g);
            break;
    }
}

} // namespace render
