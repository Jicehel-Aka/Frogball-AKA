// GENERE par tools/gen_sprites.py — ne pas editer.
#pragma once
#include <stdint.h>
namespace spr {
// RGB565 standard (R poids fort), cle de transparence 0xF81F. Converti au format materiel par render::init().
struct Sprite { uint16_t w, h; const uint16_t* px; };
extern const Sprite frog_stand;
extern const Sprite frog_sit;
extern const Sprite frog_tongue;
extern const Sprite ball;
extern const Sprite fly;
extern const Sprite bumper_stack;
extern const Sprite bumper_mushroom;
extern const Sprite bumper_mouth;
extern const Sprite cattail_post;
extern const Sprite fly_post;
extern const Sprite bell;
extern const Sprite star;
extern const Sprite lilypad;
extern const Sprite cattails;
extern const Sprite splash_a;
extern const Sprite rock;
extern const Sprite lotus;
extern const Sprite log;
extern const Sprite boom;
extern const Sprite frog_heart;
constexpr int COUNT = 20;
extern const Sprite* const all[COUNT];
} // namespace spr
