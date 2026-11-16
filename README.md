# Frogball — flipper à grenouilles pour la Gamebuino AKA

Flipper rétro « fantasy » écrit de zéro pour la console **Gamebuino AKA** (ESP32-S3, 320×240, ESP-IDF).
Inspiré d'un flipper style Game Boy des années 90 avec des grenouilles ; le code, la physique, le rendu et
les sons sont originaux.

**Tourner la console de 90° vers la droite** (comme AKA Pinball) : la table est en portrait 240×520 et défile
verticalement derrière une fenêtre de 240×280, avec un HUD de 40 px.

## Le jeu
- 1 bille, 3 vies (icônes grenouille), protection de bille de 15 s (une fois par bille), tilt.
- 3 bumpers (grenouilles qui ouvrent la bouche, champignon), 2 slingshots, 4 nénuphars à allumer
  (les 4 allumés : 5 000 pts + **canard étourdi** 12 s, double dégâts), 4 couloirs **F-R-O-G**
  (les flippers font tourner les lampes ; les 4 allumés : multiplicateur +1, jusqu'à ×5),
  spinner « mouches » (10 mouches = 5 000 pts + 8 s de protection).
- **Boss canard** qui patrouille en haut de la mare : points de vie croissants (3, 5, 7… 15), score
  10 000 × niveau à chaque victoire, il revient plus fort.
- **9 trophées** et **top 5** sauvegardés sur la carte SD (`/sdcard/frogball/save.bin`).

## Contrôles
| Action | Touches |
|---|---|
| Flipper gauche | A / L1 / Gauche |
| Flipper droit | D / Droite |
| Ressort | R1 / Bas (maintenir = armer, relâcher = tirer) |
| Secousse | B / Haut (attention au TILT) |
| Pause / menu | MENU (menu système : reprendre, commandes, langue, crédits, retour loader) |

## Arborescence
```
main/game/      frogball_core.*  coeur portable (physique 360 Hz, règles, boss, trophées)
                frog_sound.*     effets et musique synthétisés (aucun échantillon)
main/ui/        render.*         rendu tourné à 90°, sprites en colonnes (un blit par course de pixels)
                sprites.*        GÉNÉRÉ par tools/gen_sprites.py depuis assets/sprites_sheet.png
main/platform/  couche console : entrées, audio (gb_audio_player), SD, backend graphique
test/host/      test PC (g++) : physique + rendu + sons, captures PPM — sans ESP-IDF
sdcard_files/frogball/   à copier à la racine de la carte SD (langues, meta.json, screen.bmp, Picture.png)
```

## Compiler et tester
```bash
# test hôte (aucun ESP-IDF requis) ; le dossier optionnel reçoit des captures PPM
bash test/host/run.sh /tmp/captures

# firmware (ESP-IDF configuré)
idf.py build
```
Régénérer les sprites : `python3 tools/gen_sprites.py` (Pillow, NumPy, SciPy).

## Version PC (Windows / Linux)
Même jeu, même rendu et mêmes sons, via SDL2 (`pc/`). La table s'affiche droite dans une fenêtre portrait.
```bash
cmake -S pc -B build-pc -DCMAKE_BUILD_TYPE=Release && cmake --build build-pc
./build-pc/frogball
```
Touches : A/Z ou ← = flipper gauche · D/S ou → = flipper droit · ↓/Espace = ressort · ↑/B = secousse ·
P = pause · F11 = plein écran · Échap = quitter. Manette SDL prise en charge.

## Releases (GitHub Actions)
- `build-aka.yml` : test hôte, firmware ESP-IDF 5.5.1, paquet carte SD (`frogball/` + `AKA/`).
- `build-pc.yml` : builds Linux (tar.gz) et Windows (zip avec SDL2.dll).
- Release : pousser un tag `v*`, ou lancer le workflow à la main avec « publier ».

## Licence
CC BY-NC 4.0 (voir `LICENSE`).
