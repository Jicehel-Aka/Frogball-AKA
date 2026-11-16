// Test hote du coeur de jeu + du rendu : compile sous g++, sans ESP-IDF ni materiel.
//   bash test/host/run.sh [dossier_captures]
#include "../../main/game/frogball_core.h"
#include "../../main/game/frog_sound.h"
#include "../../main/ui/gfx.h"
#include "../../main/ui/render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace vid {
extern uint16_t g_fb[];
void dump_ppm(const char* path);
unsigned count_non_background(uint16_t bg);
}
using namespace frog;

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_fail++; printf("  ECHEC: " #cond " -- "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static SaveData g_disk; static int g_saves = 0; static bool g_diskValid = false;
static void s_load(SaveData& out) { if (g_diskValid) out = g_disk; }
static void s_save(const SaveData& in) { g_disk = in; g_diskValid = true; g_saves++; }

static uint32_t g_ms = 0;
static void tick(Game& g, const Input& in, float dt = 1.0f / 60.0f) { g_ms += (uint32_t)(dt * 1000.0f + 0.5f); g.update(dt, in, g_ms); }
static void press(Game& g, bool Input::*field) { Input a; a.*field = true; tick(g, a); tick(g, Input()); }

static const char* g_out = "";
static void shot(const char* name, const Game& g) {
    render::draw(g);
    if (g_out[0]) { char p[512]; snprintf(p, sizeof(p), "%s/%s.ppm", g_out, name); vid::dump_ppm(p); }
}

// Bot : tire le ressort, frappe quand la bille est pres d'un flipper
static Input bot(const Game& g, float t) {
    Input in;
    const Ball& b = g.ball;
    if (b.active && b.inShooter && b.y > 470) in.plunger = fmodf(t, 2.0f) < 1.0f;
    if (b.active && !b.inShooter && b.y > 400) {
        const bool left = b.x < 113;
        if (fabsf(b.x - (left ? 100.0f : 126.0f)) < 30.0f && b.y > 430) { if (left) in.left = true; else in.right = true; }
    }
    return in;
}

int main(int argc, char** argv) {
    if (argc > 1) g_out = argv[1];
    render::init();
    Game g;
    SaveStore st; st.load = s_load; st.save = s_save;
    g.begin(st);

    printf("[1] ecran titre\n");
    shot("01_title", g);
    CHECK(vid::count_non_background(0) > 40000, "le titre doit remplir l'ecran");
    press(g, &Input::right);
    CHECK(g.menuSel == 1, "droite -> TROPHEES");
    shot("02_title_trophies_sel", g);
    press(g, &Input::plunger);
    CHECK(g.state == State::Trophies, "entree dans les trophees");
    shot("03_trophies", g);
    press(g, &Input::nudge);
    CHECK(g.state == State::Title, "retour au titre");
    press(g, &Input::left);
    press(g, &Input::plunger);
    CHECK(g.state == State::Playing, "JOUER lance la partie");
    shot("04_start", g);

    printf("[2] physique : lancement et parcours\n");
    float t = 0.0f; bool leftLane = false; float minY = 999.0f; int shots = 0;
    for (int f = 0; f < 60 * 120 && g.state == State::Playing; f++) {
        Input in = bot(g, t);
        tick(g, in); t += 1.0f / 60.0f;
        CHECK(g.ball.x == g.ball.x && g.ball.y == g.ball.y, "bille NaN");
        if (g.ball.active && !g.ball.inShooter) leftLane = true;
        if (g.ball.active && g.ball.y < minY) minY = g.ball.y;
        if (g.ball.active) CHECK(g.ball.x > -2 && g.ball.x < 242, "bille hors table x=%.1f", g.ball.x);
        if (f == 90 || f == 600 || f == 1500) { char n[32]; snprintf(n, sizeof(n), "05_play_%d", shots++); shot(n, g); }
    }
    CHECK(leftLane, "la bille doit sortir du couloir de lancement");
    CHECK(minY < 60.0f, "la bille doit atteindre le haut de la table (minY=%.0f)", minY);
    CHECK(g.score > 0, "du score doit etre marque");

    printf("[3] fin de partie, nom, records\n");
    // pas de flippers : les 3 billes se perdent
    Game h; h.begin(st);
    press(h, &Input::plunger);
    float tt = 0.0f;
    for (int f = 0; f < 60 * 300 && h.state == State::Playing; f++) { Input in; const Ball& b = h.ball; if (b.active && b.inShooter && b.y > 470) in.plunger = fmodf(tt, 2.0f) < 0.6f; tick(h, in); tt += 1.0f / 60.0f; }
    CHECK(h.state == State::GameOver, "3 billes perdues -> GAME OVER (state=%d, balls=%d)", (int)h.state, h.ballsLeft);
    shot("06_gameover", h);
    for (int i = 0; i < 100; i++) tick(h, Input());
    h.score = 99999;   // force un record
    h.lastRank = h.rankOf(h.score);
    press(h, &Input::plunger);
    CHECK(h.state == State::NameEntry, "record -> saisie du nom (state=%d)", (int)h.state);
    press(h, &Input::right); press(h, &Input::plunger);
    press(h, &Input::right); press(h, &Input::right); press(h, &Input::plunger);
    shot("07_nameentry", h);
    press(h, &Input::plunger);
    CHECK(h.state == State::Title, "nom valide -> titre");
    CHECK(h.save.top[0].score == 99999 && h.save.top[0].tri[0] == 'B' && h.save.top[0].tri[1] == 'C' && h.save.top[0].tri[2] == 'A', "record insere: %u %s", (unsigned)h.save.top[0].score, h.save.top[0].tri);
    CHECK(g_saves > 0, "la sauvegarde a ete ecrite");

    printf("[4] succes et boss\n");
    g_diskValid = false;
    Game k; k.begin(st);
    press(k, &Input::plunger);
    k.unlock(ACH_DUCK_DOWN);
    CHECK(k.hasAch(ACH_DUCK_DOWN) && k.achPopup == ACH_DUCK_DOWN, "succes debloque");
    shot("08_achievement", k);
    k.duck.hp = 1; k.duck.stun = 0.0f;
    k.ball.x = k.duck.x; k.ball.y = k.duck.y + 18.0f; k.ball.vx = 0; k.ball.vy = -300.0f; k.ball.inShooter = false;
    for (int i = 0; i < 60 && k.duck.st == DuckState::Patrol; i++) tick(k, Input());
    CHECK(k.duck.st == DuckState::Dying, "le canard est vaincu (hp=%d)", k.duck.hp);
    CHECK(k.hasAch(ACH_DUCK_DOWN), "succes DUCK DOWN");
    shot("09_duck_dying", k);
    for (int i = 0; i < 60 * 6; i++) tick(k, Input());
    CHECK(k.duck.st == DuckState::Patrol && k.duck.level == 2 && k.duck.hp == 5, "canard niveau 2 (lvl=%d hp=%d)", k.duck.level, k.duck.hp);

    printf("[5] sons et musique\n");
    for (int i = 1; i < (int)Sfx::Count; i++) {
        Pattern p; build_pattern((Sfx)i, 1, p);
        CHECK(p.count > 0, "evenement sonore %d sans note", i);
        for (int n = 0; n < p.count; n++) {
            CHECK(p.n[n].f0 > 20.0f && p.n[n].f0 < 5000.0f && p.n[n].f1 > 20.0f && p.n[n].f1 < 5000.0f, "frequence hors plage (evt %d note %d)", i, n);
            CHECK(p.n[n].durMs > 0 && p.n[n].atMs < 2000, "duree/delai invalide (evt %d)", i);
        }
    }
    int notes = 0;
    for (uint32_t s2 = 0; s2 < MUSIC_STEPS; s2++) { MusicNote m; if (music_note(s2, m)) { notes++; CHECK(m.freq > 40.0f && m.freq < 2000.0f && m.durMs < music_step_ms(true) + 60, "note de musique invalide"); } }
    CHECK(notes > 24, "la boucle musicale doit avoir des notes (%d)", notes);

    printf("%s (%d echec%s)\n", g_fail ? "ECHEC" : "OK", g_fail, g_fail > 1 ? "s" : "");
    return g_fail ? 1 : 0;
}
