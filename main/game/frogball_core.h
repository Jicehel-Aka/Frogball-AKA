/*
===============================================================================
  frogball_core.h — Coeur de FROGBALL (flipper a grenouilles), PORTABLE
-------------------------------------------------------------------------------
  Aucune dependance materielle : ce code est compile tel quel par la console
  (ESP-IDF) ET par le test PC (test/host).

  Inspiration : un flipper "fantasy" retro facon Game Boy, avec des grenouilles.
  Ici tout est ecrit de zero (physique, plateau, rendu, sons) et pense pour la
  Gamebuino AKA : ecran 320x240 TOURNE a 90 degres (comme AKA Pinball), plateau
  portrait 240 x 520 qui defile verticalement derriere une fenetre de 240 x 280.

  Physique a pas fixe (360 Hz), une bille, deux flippers, trois bumpers-grenouilles,
  quatre nenuphars a toucher, quatre couloirs F-R-O-G, un spinner "mouches" et un
  BOSS CANARD qui patrouille en haut de la mare.
===============================================================================
*/
#pragma once
#include <stdint.h>

namespace frog {

// --- Ecran (vue portrait : la console est tournee a 90 degres) ------------------
constexpr int   SCREEN_W = 240;
constexpr int   SCREEN_H = 320;
constexpr int   HUD_H    = 24;                      // bandeau score / multiplicateur / bille, SOUS la table
constexpr int   VIEW_H   = SCREEN_H - HUD_H;       // 296 : la table tient sur UN SEUL ecran (pas de defilement)
constexpr int   WORLD_W  = 240;
constexpr int   WORLD_H  = VIEW_H;
constexpr float MAX_CAM_Y = 0.0f;

// --- Physique -------------------------------------------------------------------
constexpr float BALL_R       = 5.0f;
constexpr float GRAVITY      = 640.0f;
constexpr float PHYSICS_DT   = 1.0f / 360.0f;
constexpr int   MAX_SUBSTEPS = 16;
constexpr float MAX_FRAME_DT = 0.040f;
constexpr float MAX_SPEED    = 1100.0f;

// --- Plateau --------------------------------------------------------------------
constexpr int   MAX_WALLS    = 48;
constexpr int   BUMPER_COUNT = 3;                  // capacite (la table du canard n'en a pas)
constexpr int   PAD_COUNT    = 8;                  // capacite : 4 nenuphars (mare) ou 6 oeufs (canard)
constexpr int   TABLE_POND   = 0;                  // table 1 : la mare (bumpers, grenouille, nenuphars)
constexpr int   TABLE_DUCK   = 1;                  // table 2 : le canard et ses oeufs
constexpr int   LANE_COUNT   = 4;                  // F R O G
constexpr float LANE_X       = 225.0f;             // axe du couloir de lancement
constexpr float LANE_REST_Y  = 268.0f;             // bille au repos sur le ressort
constexpr float DRAIN_Y      = 304.0f;

constexpr int   TOP_N        = 5;
constexpr float BALL_SAVE_SEC = 15.0f;
constexpr int   START_BALLS  = 3;
constexpr int   MAX_MULT     = 5;

struct Vec2 { float x, y; };

struct Seg {
    float x1, y1, x2, y2;
    float bounce;
    uint8_t kind;           // 0 mur, 1 slingshot gauche, 2 slingshot droit, 3 flanc de slingshot (mur, dessine avec le triangle)
};

struct Ball {
    float x = LANE_X, y = LANE_REST_Y, vx = 0, vy = 0;
    bool active = false;
    bool inShooter = true;
    float stuck = 0.0f;     // secondes quasi immobile (anti-blocage)
};

struct Flipper {
    bool  isLeft;
    float px, py, len;
    float restRad, activeRad;
    float angle, prevAngle, omega;
};

struct Circle { float x, y, r; };

// Boss canard
enum class DuckState : uint8_t { Patrol, Dying, Away };
struct Duck {
    DuckState st = DuckState::Patrol;
    float x = 120, y = 70;
    float t = 0.0f;         // horloge de patrouille
    int   hp = 3, maxHp = 3, level = 1;
    float flash = 0.0f, cool = 0.0f, timer = 0.0f, stun = 0.0f;
    int   dir = 1;
};

// --- Succes ----------------------------------------------------------------------
enum Ach : uint8_t {
    ACH_FIRST_HOP = 0,   // toucher un bumper
    ACH_FLY_CATCHER,     // completer F-R-O-G
    ACH_PAD_MASTER,      // allumer les 4 nenuphars
    ACH_DUCK_DOWN,       // vaincre le canard
    ACH_HAT_TRICK,       // 3 canards dans une partie
    ACH_TOAD_X5,         // multiplicateur x5
    ACH_SCORE_25K,
    ACH_SCORE_100K,
    ACH_SURVIVOR,        // une bille qui dure 90 s
    ACH_COUNT
};
const char* ach_name(int i);
const char* ach_desc(int i);

// --- Entrees ----------------------------------------------------------------------
struct Input {
    bool left    = false;     // flipper gauche (+ navigation)
    bool right   = false;     // flipper droit  (+ navigation)
    bool plunger = false;     // ressort (+ valider)
    bool nudge   = false;     // secousse (+ valider / retour)
};

enum class State : uint8_t { Title, Playing, GameOver, NameEntry, Trophies, Options };

constexpr int STYLE_GAMEBOY = 0;   // palette « Game Boy » (3 verts + orange)
constexpr int STYLE_COLOR   = 1;   // pleines couleurs contrastees

enum class Sfx : uint8_t {
    None = 0,
    MenuMove, MenuOk, Flipper, Launch, Bumper, Sling, Pad, PadsDone, Rollover, LanesDone,
    Spinner, DuckHit, DuckDown, Multiplier, Drain, BallSaved, Nudge, Tilt, Achievement,
    GameOver, NewRecord, Count
};
struct SfxEvent { Sfx id; uint8_t arg; };

// --- Sauvegarde (carte SD : /sdcard/frogball/save.bin) ----------------------------
struct ScoreEntry { uint32_t score; char tri[4]; };
struct SaveData {
    uint16_t   ach = 0;
    ScoreEntry top[TOP_N];
    uint8_t    style = STYLE_GAMEBOY;   // option d'affichage
};
struct SaveStore {
    void (*load)(SaveData& out) = nullptr;
    void (*save)(const SaveData& in) = nullptr;
};

class Game {
public:
    Game();
    void begin(const SaveStore& store);
    void update(float dt, const Input& in, uint32_t nowMs);
    void syncInput(const Input& in);   // la reprise du menu systeme ne rejoue pas la touche qui a valide
    bool popSfx(SfxEvent& out);
    void returnToTitle();              // menu systeme -> retour au titre (la partie en cours est abandonnee)

    // --- Etat lisible par le rendu et les tests -----------------------------------
    State state = State::Title;
    int   menuSel = 0;                 // 0 JOUER, 1 OPTIONS, 2 TROPHEES

    Ball    ball;
    Flipper fl[2];
    Seg     walls[MAX_WALLS];
    int     wallCount = 0;
    int     table = TABLE_POND;        // table en cours
    int     bumperCount = 0, padCount = 0;
    float   travelT = 0.0f;            // > 0 : ecran de transition (bulles) vers travelTo
    int     travelTo = TABLE_POND;
    Circle  bumpers[BUMPER_COUNT];
    float   bumperFlash[BUMPER_COUNT] = {};
    Circle  pads[PAD_COUNT];
    uint8_t padLit = 0;                // bit i : nenuphar i allume
    float   padFlash[PAD_COUNT] = {};
    float   padReset = 0.0f;
    Vec2    lanes[LANE_COUNT];
    uint8_t laneLit = 0;
    Circle  spinner;                   // capteur "mouches"
    float   spinAngle = 0.0f, spinVel = 0.0f;
    bool    inSpinner = false;
    Duck    duck;

    uint32_t score = 0;
    int   ballsLeft = START_BALLS;
    int   multiplier = 1;
    int   flies = 0;
    int   ducksDown = 0;               // canards vaincus dans la partie en cours
    float ballLife = 0.0f;             // duree de la bille en jeu (hors lanceur)
    float ballSaveTimer = 0.0f;
    float plungerCharge = 0.0f;
    float tilt = 0.0f;
    bool  tilted = false;
    float respawnDelay = 0.0f;
    uint32_t bumperHits = 0;

    float animT = 0.0f;                // horloge d'animation (decor)
    char  banner[28] = "";
    float bannerTimer = 0.0f;
    int   achPopup = -1;
    float achTimer = 0.0f;

    SaveData save;
    char  nameBuf[4] = "AAA";
    int   nameIdx = 0;
    int   lastRank = -1;               // rang de la derniere partie (-1 : hors top)
    float stateLock = 0.0f;
    uint32_t nowMs = 0;

    // Tests : pilotage direct de la physique
    void buildTable(int which);        // monte la table (mare ou canard)
    void travel(int to);               // ecran de transition vers l'autre table
    void startNewGame();
    void stepPhysics(float dt, const Input& in);
    int  rankOf(uint32_t sc) const;
    bool hasAch(int i) const { return (save.ach >> i) & 1; }
    void unlock(int i);

private:
    SaveStore store_;
    bool prevLeft_ = false, prevRight_ = false, prevPlunger_ = false, prevNudge_ = false;
    float physAccum_ = 0.0f;
    bool  saveArmed_ = true;           // la protection ne sert qu'une fois par bille
    int   slingHits_ = 0;              // anti-boucle : les slingshots se coupent quand la bille ping-pong entre eux
    float slingWindow_ = 0.0f, slingCool_ = 0.0f;
    uint32_t rng_ = 0x2468ACEu;
    SfxEvent q_[24];
    int qHead_ = 0, qTail_ = 0;

    void emit(Sfx id, uint8_t arg = 0);
    void say(const char* txt, float sec);
    float frand();
    void spawnBall();
    void substep(float dt, bool leftHeld, bool rightHeld);
    void updatePlaying(float dt, const Input& in, bool leftEdge, bool rightEdge, bool nudgeEdge);
    void updateDuck(float dt);
    void onDrain();
    void endGame();
    void addScore(uint32_t base);
    void bumpMultiplier();
    void insertScore();
    void persist();
    bool hitCircle(float cx, float cy, float r, float e, float kick);
    bool hitSeg(float x1, float y1, float x2, float y2, float e, float kick, float thick,
                const Flipper* fp);
};

} // namespace frog
