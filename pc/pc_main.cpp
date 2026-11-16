/*
===============================================================================
  pc_main.cpp — FROGBALL version PC (Windows / Linux) avec SDL2
-------------------------------------------------------------------------------
  Meme jeu, meme rendu, memes sons que sur la console : seul le "dessous" change.
    - ecran : le rendu ecrit dans un framebuffer 320x240 (paysage, comme la console, ou la table est
      tournee a 90 degres) ; ici on l'affiche tourne a nouveau de 90 degres, la table est donc DROITE
      dans une fenetre portrait 240x320 ;
    - sons  : les memes motifs de notes (frog_sound) sont synthetises par SDL_audio ;
    - sauvegarde : save.bin dans le dossier de preferences de l'utilisateur.
===============================================================================
*/
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../main/game/frogball_core.h"
#include "../main/game/frog_sound.h"
#include "../main/platform/platform.h"
#include "../main/ui/gfx.h"
#include "../main/ui/render.h"

// police 8x8 du SDK (definie dans le header, comme dans gb_graphics.cpp et le test hote)
#include "../components/gamebuino/include_lib/font8x8_basic.h"

// ------------------------------------------------------------------ backend graphique
namespace vid {

static uint16_t g_fb[W * H];
static SDL_Renderer* g_ren = nullptr;
static SDL_Texture*  g_tex = nullptr;

// Meme format materiel que la console (R bits 0-4, V bits 5-10, B bits 11-15).
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11)); }
void raw_pixel(int x, int y, uint16_t c) { g_fb[y * W + x] = c; }
void raw_fill_rect(int x, int y, int w, int h, uint16_t c) {
    for (int j = 0; j < h; j++) { uint16_t* p = &g_fb[(y + j) * W + x]; for (int i = 0; i < w; i++) p[i] = c; }
}
void raw_blit_row(int x, int y, const uint16_t* px, int n) { memcpy(&g_fb[y * W + x], px, (size_t)n * sizeof(uint16_t)); }

void present() {
    uint32_t* dst = nullptr; int pitch = 0;
    if (SDL_LockTexture(g_tex, nullptr, (void**)&dst, &pitch) != 0) return;
    for (int y = 0; y < H; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)dst + y * pitch);
        for (int x = 0; x < W; x++) {
            const uint16_t c = g_fb[y * W + x];
            const uint32_t r = (c & 31) << 3, g = ((c >> 5) & 63) << 2, b = ((c >> 11) & 31) << 3;
            row[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    SDL_UnlockTexture(g_tex);
    SDL_SetRenderDrawColor(g_ren, 8, 24, 12, 255);
    SDL_RenderClear(g_ren);
    // Fenetre logique 240x320 (portrait). Le framebuffer 320x240 est tourne de 90 degres (sens horaire)
    // autour du centre de la fenetre : (sx, sy) -> (239 - sy, sx), exactement l'inverse du rendu console.
    SDL_Rect dstRect = {120 - W / 2, 160 - H / 2, W, H};
    SDL_RenderCopyEx(g_ren, g_tex, nullptr, &dstRect, 90.0, nullptr, SDL_FLIP_NONE);
    SDL_RenderPresent(g_ren);
}

} // namespace vid

// ------------------------------------------------------------------ audio (synthese)
namespace platform {
namespace {

constexpr int RATE = 44100;
constexpr int SFX_VOICES = 8;
constexpr int MAX_PENDING = 48;

struct Voice {
    bool on = false;
    float f0 = 0, f1 = 0, v0 = 0, v1 = 0;
    uint32_t dur = 0, pos = 0;
    uint8_t wave = 0;
    float phase = 0;
    uint32_t lfsr = 0x1234567u;
    float hold = 0;
};
struct Pending { uint64_t due; frog::Note note; bool used; };
struct Event { uint8_t id, arg; };

SDL_AudioDeviceID g_dev = 0;
Voice g_sfx[SFX_VOICES];
Voice g_music;
Pending g_pend[MAX_PENDING];
Event g_events[32];
int g_nEvents = 0;
uint64_t g_clock = 0;                // echantillons ecrits depuis le debut
volatile bool g_want = false, g_intense = false;
bool g_musicOn = false;
uint32_t g_step = 0;
uint64_t g_musicDue = 0;

void start(Voice& v, const frog::Note& n) {
    v.on = true; v.f0 = n.f0; v.f1 = n.f1; v.v0 = n.v0; v.v1 = n.v1;
    v.dur = (uint32_t)n.durMs * RATE / 1000 + 1; v.pos = 0; v.wave = n.wave; v.phase = 0;
}

void fire(const frog::Note& n) {
    int idx = -1;
    for (int i = 0; i < SFX_VOICES; i++) if (!g_sfx[i].on) { idx = i; break; }
    if (idx < 0) { idx = 0; for (int i = 1; i < SFX_VOICES; i++) if (g_sfx[i].pos > g_sfx[idx].pos) idx = i; }
    start(g_sfx[idx], n);
}

float sample(Voice& v) {
    if (!v.on) return 0.0f;
    const float t = (float)v.pos / (float)v.dur;
    const float f = v.f0 + (v.f1 - v.f0) * t;
    const float vol = v.v0 + (v.v1 - v.v0) * t;
    float s = 0;
    v.phase += f / (float)RATE;
    if (v.phase >= 1.0f) {
        v.phase -= floorf(v.phase);
        if (v.wave == frog::WAVE_NOISE) {   // bruit : un nouveau hasard a chaque "periode"
            v.lfsr ^= v.lfsr << 13; v.lfsr ^= v.lfsr >> 17; v.lfsr ^= v.lfsr << 5;
            v.hold = ((v.lfsr & 0xFFFF) / 32768.0f) - 1.0f;
        }
    }
    switch (v.wave) {
        case frog::WAVE_SQUARE:   s = v.phase < 0.5f ? 0.6f : -0.6f; break;
        case frog::WAVE_TRIANGLE: s = 4.0f * fabsf(v.phase - 0.5f) - 1.0f; break;
        case frog::WAVE_NOISE:    s = v.hold; break;
        default:                  s = sinf(6.2831853f * v.phase); break;
    }
    float env = 1.0f;
    const uint32_t fade = RATE / 400;                      // 2,5 ms : pas de "clic"
    if (v.pos < fade) env = (float)v.pos / (float)fade;
    if (v.dur - v.pos < fade) env *= (float)(v.dur - v.pos) / (float)fade;
    if (++v.pos >= v.dur) v.on = false;
    return s * vol * env;
}

void audio_cb(void*, Uint8* stream, int len) {
    int16_t* out = (int16_t*)stream;
    const int n = len / 2;
    for (int i = 0; i < n; i++) {
        if ((i & 63) == 0) {   // ordonnancement par blocs de 64 echantillons (1,5 ms)
            for (int e = 0; e < g_nEvents; e++) {
                frog::Pattern p;
                frog::build_pattern((frog::Sfx)g_events[e].id, g_events[e].arg, p);
                for (int k = 0; k < p.count; k++)
                    for (int m = 0; m < MAX_PENDING; m++)
                        if (!g_pend[m].used) { g_pend[m] = {g_clock + (uint64_t)p.n[k].atMs * RATE / 1000, p.n[k], true}; break; }
            }
            g_nEvents = 0;
            for (int m = 0; m < MAX_PENDING; m++)
                if (g_pend[m].used && g_clock >= g_pend[m].due) { fire(g_pend[m].note); g_pend[m].used = false; }
            const bool want = g_want;
            if (want != g_musicOn) { g_musicOn = want; g_step = 0; g_musicDue = g_clock + RATE / 4; g_music.on = false; }
            if (g_musicOn && g_clock >= g_musicDue) {
                frog::MusicNote mn;
                if (frog::music_note(g_step, mn)) {
                    frog::Note nn = {0, mn.freq, mn.freq, mn.vol, mn.vol * 0.4f, mn.durMs, mn.wave};
                    start(g_music, nn);
                }
                g_step++;
                g_musicDue += (uint64_t)frog::music_step_ms(g_intense) * RATE / 1000;
            }
        }
        float s = sample(g_music) * 0.5f;
        for (int v = 0; v < SFX_VOICES; v++) s += sample(g_sfx[v]) * 0.45f;
        if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
        out[i] = (int16_t)(s * 14000.0f);
        g_clock++;
    }
}

char g_savePath[1024] = "";
constexpr uint32_t kMagic = 0x31475246;   // "FRG1", meme format que la console

void pc_load(frog::SaveData& out) {
    FILE* f = fopen(g_savePath, "rb");
    if (!f) return;
    uint32_t magic = 0; frog::SaveData tmp;
    if (fread(&magic, sizeof(magic), 1, f) == 1 && magic == kMagic && fread(&tmp, sizeof(tmp), 1, f) == 1) {
        for (int i = 0; i < frog::TOP_N; i++) tmp.top[i].tri[3] = 0;
        out = tmp;
    }
    fclose(f);
}
void pc_save(const frog::SaveData& in) {
    FILE* f = fopen(g_savePath, "wb");
    if (!f) return;
    fwrite(&kMagic, sizeof(kMagic), 1, f);
    fwrite(&in, sizeof(in), 1, f);
    fclose(f);
}

} // namespace

void audio_init() {
    SDL_AudioSpec want; SDL_zero(want);
    want.freq = RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 512; want.callback = audio_cb;
    SDL_AudioSpec have;
    g_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (g_dev) SDL_PauseAudioDevice(g_dev, 0);
    else fprintf(stderr, "audio indisponible : %s\n", SDL_GetError());
}
void audio_play(frog::Sfx id, uint8_t arg) {
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    if (g_nEvents < 32) g_events[g_nEvents++] = {(uint8_t)id, arg};
    SDL_UnlockAudioDevice(g_dev);
}
void audio_music(bool on, bool intense) { g_intense = intense; g_want = on; }
void audio_silence() {
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    g_want = false; g_nEvents = 0;
    for (auto& p : g_pend) p.used = false;
    for (auto& v : g_sfx) v.on = false;
    g_music.on = false;
    SDL_UnlockAudioDevice(g_dev);
}
frog::SaveStore store() {
    char* pref = SDL_GetPrefPath("jeancharleslebeau", "frogball");
    snprintf(g_savePath, sizeof(g_savePath), "%ssave.bin", pref ? pref : "");
    if (pref) SDL_free(pref);
    frog::SaveStore s; s.load = pc_load; s.save = pc_save;
    return s;
}

} // namespace platform

// ------------------------------------------------------------------ entrees
static SDL_GameController* g_pad = nullptr;

static frog::Input read_input() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    frog::Input in;
    // Touches PHYSIQUES (scancodes) : valable en AZERTY comme en QWERTY.
    in.left    = k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A] || k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_LSHIFT];
    in.right   = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D] || k[SDL_SCANCODE_S] || k[SDL_SCANCODE_RSHIFT];
    in.plunger = k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN];
    in.nudge   = k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_B] || k[SDL_SCANCODE_BACKSPACE];
    if (g_pad) {
        auto b = [](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(g_pad, x) != 0; };
        in.left    |= b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)  || b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        in.right   |= b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || b(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        in.plunger |= b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)  || b(SDL_CONTROLLER_BUTTON_A);
        in.nudge   |= b(SDL_CONTROLLER_BUTTON_DPAD_UP)    || b(SDL_CONTROLLER_BUTTON_X) || b(SDL_CONTROLLER_BUTTON_B);
        const int ax = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
        if (ax < -16000) in.left = true;
        if (ax > 16000) in.right = true;
    }
    return in;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("Frogball", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 480, 640, SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "fenetre : %s\n", SDL_GetError()); return 1; }
    vid::g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!vid::g_ren) vid::g_ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!vid::g_ren) { fprintf(stderr, "rendu : %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(vid::g_ren, 240, 320);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");   // pixels nets
    vid::g_tex = SDL_CreateTexture(vid::g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, vid::W, vid::H);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { g_pad = SDL_GameControllerOpen(i); if (g_pad) break; }

    render::init();
    platform::audio_init();
    frog::Game game;
    game.begin(platform::store());

    bool running = true, paused = false;
    uint64_t last = SDL_GetTicks64();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
                if (e.key.keysym.scancode == SDL_SCANCODE_P) { paused = !paused; platform::audio_silence(); }
                if (e.key.keysym.scancode == SDL_SCANCODE_F11 || (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)))
                    SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED && !g_pad) g_pad = SDL_GameControllerOpen(e.cdevice.which);
        }
        const uint64_t now = SDL_GetTicks64();
        float dt = (float)(now - last) * 0.001f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // fenetre deplacee, machine lente : pas de saut de physique
        if (!paused) {
            game.update(dt, read_input(), (uint32_t)now);
            frog::SfxEvent ev;
            while (game.popSfx(ev)) platform::audio_play(ev.id, ev.arg);
            const bool playing = (game.state == frog::State::Playing || game.state == frog::State::Title);
            platform::audio_music(playing, game.state == frog::State::Playing && game.duck.st == frog::DuckState::Patrol && game.duck.hp <= 2);
        }
        render::draw(game);
        vid::present();
    }
    platform::audio_silence();
    if (g_pad) SDL_GameControllerClose(g_pad);
    SDL_Quit();
    return 0;
}
