# Frogball — flipper à grenouilles pour la Gamebuino AKA

Flipper rétro « fantasy » écrit de zéro pour la console **Gamebuino AKA** (ESP32-S3, 320×240, ESP-IDF).
Inspiré d'un flipper style Game Boy des années 90 avec des grenouilles ; le code, la physique, le rendu et
les sons sont originaux.

**Tourner la console de 90° vers la droite** (comme AKA Pinball) : chaque table tient sur UN SEUL écran
(portrait 240×296, sans défilement), le bandeau score / multiplicateur / bille est sous la table, et le rendu
reprend la palette « Game Boy » (3 verts + orange) observée dans la vidéo de référence.

## Le jeu
- **Deux tables reliées** : la **mare** (3 bumpers, grenouille centrale, 4 nénuphars, spinner « mouches »)
  puis la table du **canard** (6 œufs à ramasser, canard qui patrouille). Allumer les 4 nénuphars envoie la bille
  sur la table du canard (écran de transition à bulles) ; vaincre le canard ramène à la mare, canard plus fort.
- 3 billes (« BALL n »), protection de bille de 15 s (une fois par bille), tilt, multiplicateur ×1 à ×5
  (les 4 couloirs **F-R-O-G** allumés : +1 ; les flippers font tourner les lampes).
- **Boss canard** : points de vie 3, 5, 7… 15 ; les 6 œufs ramassés l'étourdissent 12 s (double dégâts) ;
  10 000 × niveau à chaque victoire.
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
main/ui/        render.*         rendu tourné à 90°, palette Game Boy (toute couleur est ramenée à 5 teintes)
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

## Ce qui est observé / ce qui est inventé
Observé dans la vidéo « Frogball (2026) » : style Game Boy, tables à écran unique, table mare et table canard
avec œufs, HUD « score · ×N · BALL n », menu, transition à bulles (voir `docs/OBSERVATIONS_VIDEO.md`).
Inventés ici (le jeu d'origine n'a pas été joué) : les règles exactes, la progression mare ↔ canard, les trophées,
la musique et les sons (synthétisés, composition originale). Les sprites viennent de planches fournies par l'auteur.
