#pragma once
#include "../game/frogball_core.h"

namespace render {

// A appeler une fois au demarrage, APRES l'initialisation de l'ecran : convertit les sprites
// (RGB565 standard) dans le format materiel du pilote.
void init();

// Dessine l'etat complet du jeu dans le framebuffer (l'appelant fait vid::present()).
// Le jeu pense en portrait 240x320 ; ici tout est tourne a 90 degres vers l'ecran paysage 320x240.
void draw(const frog::Game& g);

} // namespace render
