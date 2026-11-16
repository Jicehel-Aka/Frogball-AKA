#include "platform.h"
#include "core/input.h"        // aka_runtime : struct Keys
#include "gb_ll_common.h"      // EXPANDER_KEY_*

namespace platform {

frog::Input input_from_keys(const Keys& k) {
    frog::Input in;
    // Console tournee a 90 degres vers la droite (comme AKA Pinball). Plusieurs touches par action :
    // on prend celle qui tombe sous le pouce. Le joystick emule le D-pad (deja fusionne dans k par aka_runtime).
    in.left    = k.A || k.L1 || k.left;
    in.right   = k.D || k.right;
    in.plunger = k.R1 || k.down;
    in.nudge   = k.B || k.up;
    // Pas de touche de pause : la pause, c'est le menu systeme (MENU). C et RUN (RUN+MENU = retour au loader)
    // ne doivent JAMAIS mettre le jeu en pause par accident.
    return in;
}

} // namespace platform
