/*
  frog_sound.h — Effets sonores et musique de FROGBALL, decrits comme des suites de notes synthetisees
  (aucun echantillon, aucun fichier audio : tout est calcule, composition originale).
  Portable : le test PC verifie que chaque evenement a un son valide, la console les joue.
*/
#pragma once
#include "frogball_core.h"

namespace frog {

enum Wave : uint8_t { WAVE_SINE = 0, WAVE_SQUARE = 1, WAVE_TRIANGLE = 2, WAVE_NOISE = 3 };

struct Note {
    uint16_t atMs;       // delai depuis le declenchement de l'evenement
    float f0, f1;        // frequence de depart / d'arrivee (Hz)
    float v0, v1;        // volume de depart / d'arrivee (0..1)
    uint16_t durMs;
    uint8_t wave;
};

constexpr int MAX_NOTES = 10;
struct Pattern {
    Note n[MAX_NOTES];
    uint8_t count = 0;
};

void build_pattern(Sfx id, uint8_t arg, Pattern& out);

// Musique : une boucle de 64 pas (4 phrases), jouee sur UNE voix.
struct MusicNote {
    float freq;          // Hz
    uint16_t durMs;
    uint8_t wave;
    float vol;
};
constexpr int MUSIC_STEPS = 64;
uint16_t music_step_ms(bool intense);          // intense : un canard est a l'ecran -> tempo plus vif
bool music_note(uint32_t step, MusicNote& out); // false : silence

} // namespace frog
