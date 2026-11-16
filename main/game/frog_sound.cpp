#include "frog_sound.h"
#include <math.h>

namespace frog {

static void add(Pattern& p, uint16_t at, float f0, float f1, float v0, float v1, uint16_t dur, Wave w) {
    if (p.count >= MAX_NOTES) return;
    p.n[p.count++] = {at, f0, f1, v0, v1, dur, (uint8_t)w};
}

// demi-tons au-dessus de A3 (220 Hz)
static float hz(int semi) { return 220.0f * powf(2.0f, (float)semi / 12.0f); }

void build_pattern(Sfx id, uint8_t arg, Pattern& p) {
    p.count = 0;
    switch (id) {
        case Sfx::MenuMove:  add(p, 0, 660, 660, 0.35f, 0.0f, 40, WAVE_SQUARE); break;
        case Sfx::MenuOk:    add(p, 0, 520, 520, 0.4f, 0.2f, 60, WAVE_SQUARE); add(p, 60, 780, 780, 0.4f, 0.0f, 90, WAVE_SQUARE); break;
        case Sfx::Flipper:   add(p, 0, arg ? 150.0f : 170.0f, 80, 0.5f, 0.0f, 55, WAVE_TRIANGLE); break;
        case Sfx::Launch:    add(p, 0, 160, 900, 0.5f, 0.1f, 330, WAVE_SQUARE); add(p, 0, 80, 40, 0.5f, 0.0f, 120, WAVE_NOISE); break;
        case Sfx::Bumper: {   // « ribbit » : deux notes basses glissees, differentes selon la grenouille
            const float b = 140.0f + 35.0f * (float)(arg % 3);
            add(p, 0, b, b * 1.7f, 0.55f, 0.35f, 70, WAVE_SQUARE);
            add(p, 70, b * 1.7f, b * 1.1f, 0.5f, 0.0f, 90, WAVE_TRIANGLE);
            break;
        }
        case Sfx::Sling:     add(p, 0, 330, 200, 0.45f, 0.0f, 70, WAVE_SQUARE); break;
        case Sfx::Pad:       add(p, 0, hz(arg * 3 + 12), hz(arg * 3 + 12), 0.45f, 0.0f, 110, WAVE_SINE); add(p, 0, 90, 60, 0.3f, 0.0f, 60, WAVE_NOISE); break;
        case Sfx::PadsDone:
            for (int i = 0; i < 5; i++) add(p, (uint16_t)(i * 70), hz(12 + i * 4), hz(12 + i * 4), 0.5f, 0.1f, 100, WAVE_SQUARE);
            break;
        case Sfx::Rollover:  add(p, 0, hz(17 + arg * 2), hz(17 + arg * 2), 0.4f, 0.0f, 90, WAVE_SINE); break;
        case Sfx::LanesDone:
            for (int i = 0; i < 4; i++) add(p, (uint16_t)(i * 80), hz(14 + i * 3), hz(14 + i * 3), 0.5f, 0.1f, 110, WAVE_SQUARE);
            add(p, 320, hz(26), hz(26), 0.55f, 0.0f, 260, WAVE_TRIANGLE);
            break;
        case Sfx::Spinner:   add(p, 0, 900, 1200, 0.25f, 0.0f, 35, WAVE_SQUARE); break;
        case Sfx::DuckHit:   add(p, 0, 700, 300, 0.55f, 0.1f, 80, WAVE_SQUARE); add(p, 90, 520, 220, 0.5f, 0.0f, 110, WAVE_SQUARE); break;
        case Sfx::DuckDown:
            add(p, 0, 800, 120, 0.6f, 0.0f, 500, WAVE_SQUARE);
            add(p, 0, 120, 40, 0.5f, 0.0f, 450, WAVE_NOISE);
            for (int i = 0; i < 4; i++) add(p, (uint16_t)(520 + i * 90), hz(12 + i * 4), hz(12 + i * 4), 0.5f, 0.1f, 120, WAVE_SQUARE);
            break;
        case Sfx::Multiplier:
            add(p, 0, hz(12), hz(12), 0.5f, 0.1f, 90, WAVE_SQUARE);
            add(p, 90, hz(19), hz(19), 0.5f, 0.0f, 180, WAVE_SQUARE);
            break;
        case Sfx::Drain:     add(p, 0, 400, 60, 0.55f, 0.0f, 600, WAVE_TRIANGLE); break;
        case Sfx::BallSaved:
            add(p, 0, hz(12), hz(12), 0.5f, 0.0f, 100, WAVE_SINE);
            add(p, 110, hz(16), hz(16), 0.5f, 0.0f, 100, WAVE_SINE);
            add(p, 220, hz(19), hz(19), 0.5f, 0.0f, 160, WAVE_SINE);
            break;
        case Sfx::Nudge:     add(p, 0, 90, 60, 0.5f, 0.0f, 80, WAVE_NOISE); break;
        case Sfx::Tilt:      add(p, 0, 200, 100, 0.6f, 0.4f, 500, WAVE_SQUARE); add(p, 0, 100, 60, 0.5f, 0.0f, 500, WAVE_NOISE); break;
        case Sfx::Achievement:
            for (int i = 0; i < 4; i++) add(p, (uint16_t)(i * 100), hz(15 + i * 4), hz(15 + i * 4), 0.5f, 0.15f, 130, WAVE_SQUARE);
            add(p, 420, hz(27), hz(27), 0.55f, 0.0f, 400, WAVE_TRIANGLE);
            break;
        case Sfx::GameOver:
            for (int i = 0; i < 4; i++) add(p, (uint16_t)(i * 180), hz(12 - i * 3), hz(12 - i * 3), 0.5f, 0.1f, 200, WAVE_TRIANGLE);
            break;
        case Sfx::NewRecord:
            for (int i = 0; i < 5; i++) add(p, (uint16_t)(i * 110), hz(12 + (i % 3) * 4 + (i / 3) * 5), hz(12 + (i % 3) * 4 + (i / 3) * 5), 0.5f, 0.1f, 140, WAVE_SQUARE);
            break;
        case Sfx::None:
        case Sfx::Count:
            break;
    }
}

// ---------------------------------------------------------------------------
// Musique : pentatonique mineure de la, rebonds de « grenouille ». 4 phrases de 16 pas.
// Les chiffres sont des degres (1 = la3 ... 9 = mi5), 0 = silence. Quand la melodie se
// tait sur un temps fort, une note de basse la remplace (la voix est unique).
// ---------------------------------------------------------------------------
static const float kScale[10] = {0, 220.0f, 261.63f, 293.66f, 329.63f, 392.0f, 440.0f, 523.25f, 587.33f, 659.25f};
static const uint8_t kMelody[MUSIC_STEPS] = {
    6,0,5,0, 4,0,3,5,  6,0,8,0, 7,0,5,0,
    4,0,3,0, 2,0,1,3,  4,0,5,0, 3,0,0,0,
    6,6,0,5, 4,0,3,0,  5,5,0,4, 3,0,2,0,
    1,3,5,6, 8,6,5,3,  4,3,2,1, 0,0,1,0,
};
static const float kBass[4] = {110.0f, 82.41f, 98.0f, 73.42f};   // la2, mi2, sol2, re2

uint16_t music_step_ms(bool intense) { return intense ? 118 : 140; }

bool music_note(uint32_t step, MusicNote& out) {
    const uint32_t s = step % MUSIC_STEPS;
    const uint8_t d = kMelody[s];
    if (d) {
        out = {kScale[d], 105, WAVE_SQUARE, 0.20f};
        return true;
    }
    if ((s % 4) == 0) {
        out = {kBass[(s / 16) % 4], 150, WAVE_TRIANGLE, 0.30f};
        return true;
    }
    return false;
}

} // namespace frog
