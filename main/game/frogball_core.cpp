/*
  frogball_core.cpp — Physique, regles, boss canard, succes de FROGBALL. Voir frogball_core.h.
  Systeme de coordonnees : plateau portrait 240 x 520, y vers le BAS, flippers en bas.
*/
#include "frogball_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace frog {

static const char* const kAchName[ACH_COUNT] = {
    "FIRST HOP", "FLY CATCHER", "PAD MASTER", "DUCK DOWN", "HAT TRICK",
    "TOAD x5", "25 000", "100 000", "SURVIVOR"};
static const char* const kAchDesc[ACH_COUNT] = {
    "Hit a frog bumper", "Complete F-R-O-G", "Light all 4 lily pads", "Beat the duck boss",
    "3 ducks in one game", "Reach multiplier x5", "Score 25 000 points", "Score 100 000 points",
    "Keep one ball 90 seconds"};
const char* ach_name(int i) { return (i >= 0 && i < ACH_COUNT) ? kAchName[i] : ""; }
const char* ach_desc(int i) { return (i >= 0 && i < ACH_COUNT) ? kAchDesc[i] : ""; }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static constexpr float DEG = 0.017453292f;

Game::Game() {
    static const char* tri[TOP_N] = {"FRG", "TAD", "LIL", "DUK", "AKA"};
    static const uint32_t sc[TOP_N] = {20000, 12000, 8000, 5000, 2000};
    for (int i = 0; i < TOP_N; i++) {
        save.top[i].score = sc[i];
        memcpy(save.top[i].tri, tri[i], 4);
    }
    buildTable(TABLE_POND);
}

void Game::begin(const SaveStore& store) {
    store_ = store;
    if (store_.load) store_.load(save);
    state = State::Title;
}

float Game::frand() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (float)(rng_ & 0xFFFF) / 65535.0f;
}

void Game::emit(Sfx id, uint8_t arg) {
    const int n = (qHead_ + 1) % 24;
    if (n == qTail_) return;   // file pleine : on perd le son, jamais l'image
    q_[qHead_] = {id, arg};
    qHead_ = n;
}
bool Game::popSfx(SfxEvent& out) {
    if (qTail_ == qHead_) return false;
    out = q_[qTail_];
    qTail_ = (qTail_ + 1) % 24;
    return true;
}

void Game::say(const char* txt, float sec) {
    snprintf(banner, sizeof(banner), "%s", txt);
    bannerTimer = sec;
}

void Game::persist() { if (store_.save) store_.save(save); }

void Game::unlock(int i) {
    if (i < 0 || i >= ACH_COUNT || hasAch(i)) return;
    save.ach |= (uint16_t)(1u << i);
    achPopup = i;
    achTimer = 3.5f;
    emit(Sfx::Achievement);
    persist();
}

// ----------------------------------------------------------------------------
// Plateau
// ----------------------------------------------------------------------------
void Game::buildTable(int which) {
    table = which;
    wallCount = 0;
    auto W = [&](float x1, float y1, float x2, float y2, float e = 0.5f, uint8_t kind = 0) {
        if (wallCount < MAX_WALLS) walls[wallCount++] = {x1, y1, x2, y2, e, kind};
    };
    // Voute : arc de cercle (centre 124,118, rayon 112), de la droite (couloir) vers la gauche
    {
        float px = 236.0f, py = 118.0f;
        for (int a = 15; a <= 180; a += 15) {
            const float r = (float)a * DEG;
            const float x = 124.0f + 112.0f * cosf(r), y = 118.0f - 112.0f * sinf(r);
            W(px, py, x, y, 0.45f);
            px = x; py = y;
        }
    }
    W(12, 118, 12, 226);                 // mur gauche
    W(214, 130, 214, 276);               // separation plateau / couloir de lancement
    W(214, 130, 204, 120);               // deflecteur : la bille qui retombe ne rentre pas dans le couloir
    W(236, 118, 236, 276);               // paroi exterieure du couloir
    W(214, 276, 236, 276, 0.1f);         // plancher du couloir
    W(12, 226, 62, 262);                 // guide gauche
    W(214, 226, 164, 262);               // guide droit
    // slingshots (la face inclinee donne l'impulsion)
    W(36, 202, 70, 248, 0.5f, 1);  W(36, 202, 36, 226, 0.5f, 3);  W(36, 226, 70, 248, 0.5f, 3);
    W(190, 202, 156, 248, 0.5f, 2); W(190, 202, 190, 226, 0.5f, 3); W(190, 226, 156, 248, 0.5f, 3);

    // Flippers
    fl[0] = {true,  68.0f, 266.0f, 42.0f, 42.0f * DEG, -22.0f * DEG, 42.0f * DEG, 42.0f * DEG, 0.0f};
    fl[1] = {false, 158.0f, 266.0f, 42.0f, 42.0f * DEG, -22.0f * DEG, 42.0f * DEG, 42.0f * DEG, 0.0f};

    for (int i = 0; i < LANE_COUNT; i++) lanes[i] = {65.0f + 32.0f * (float)i, 34.0f};
    padLit = 0; padReset = 0.0f; laneLit = 0;
    if (which == TABLE_POND) {
        // la mare : 3 bumpers, la grenouille centrale (4 nenuphars autour), le spinner a mouches
        bumperCount = 3;
        bumpers[0] = {84.0f, 92.0f, 15.0f};
        bumpers[1] = {144.0f, 92.0f, 15.0f};
        bumpers[2] = {114.0f, 134.0f, 15.0f};
        padCount = 4;
        pads[0] = {50.0f, 174.0f, 8.0f};
        pads[1] = {88.0f, 188.0f, 8.0f};
        pads[2] = {140.0f, 188.0f, 8.0f};
        pads[3] = {178.0f, 174.0f, 8.0f};
        spinner = {30.0f, 150.0f, 10.0f};
    } else {
        // le canard : pas de bumper, 6 oeufs a ramasser (3 a gauche, 3 a droite), le canard patrouille en haut
        bumperCount = 0;
        padCount = 6;
        pads[0] = {52.0f, 120.0f, 7.0f};
        pads[1] = {72.0f, 146.0f, 7.0f};
        pads[2] = {50.0f, 172.0f, 7.0f};
        pads[3] = {178.0f, 120.0f, 7.0f};
        pads[4] = {158.0f, 146.0f, 7.0f};
        pads[5] = {180.0f, 172.0f, 7.0f};
        spinner = {114.0f, 190.0f, 10.0f};
    }
}

void Game::travel(int to) {
    travelTo = to;
    travelT = 1.6f;
    ball.active = false;
    respawnDelay = 0.0f;
}

// ----------------------------------------------------------------------------
// Collisions
// ----------------------------------------------------------------------------
bool Game::hitSeg(float x1, float y1, float x2, float y2, float e, float kick, float thick,
                  const Flipper* fp) {
    Ball& b = ball;
    const float dx = x2 - x1, dy = y2 - y1, l2 = dx * dx + dy * dy;
    if (l2 < 1e-6f) return false;
    const float t = clampf(((b.x - x1) * dx + (b.y - y1) * dy) / l2, 0.0f, 1.0f);
    const float cx = x1 + t * dx, cy = y1 + t * dy;
    float nx = b.x - cx, ny = b.y - cy;
    const float R = BALL_R + thick;
    const float d2 = nx * nx + ny * ny;
    if (d2 >= R * R) return false;
    float d = sqrtf(d2);
    if (d < 1e-4f) {   // centre sur le segment : normale perpendiculaire, du cote d'ou la bille arrive
        const float l = sqrtf(l2);
        nx = -dy / l; ny = dx / l;
        const float qx = b.x - b.vx * PHYSICS_DT - cx, qy = b.y - b.vy * PHYSICS_DT - cy;
        if (nx * qx + ny * qy < 0.0f) { nx = -nx; ny = -ny; }
    } else {
        nx /= d; ny /= d;
    }
    b.x = cx + nx * R;
    b.y = cy + ny * R;
    float svx = 0.0f, svy = 0.0f;
    if (fp) {   // vitesse de la surface du flipper au point de contact
        const float s = fp->isLeft ? 1.0f : -1.0f;
        const float rx = cx - fp->px, ry = cy - fp->py;
        svx = -s * fp->omega * ry;
        svy = s * fp->omega * rx;
    }
    float rvx = b.vx - svx, rvy = b.vy - svy;
    const float vn = rvx * nx + rvy * ny;
    if (vn < 0.0f) { rvx -= (1.0f + e) * vn * nx; rvy -= (1.0f + e) * vn * ny; }
    b.vx = rvx + svx; b.vy = rvy + svy;
    if (kick > 0.0f) {
        const float on = b.vx * nx + b.vy * ny;
        if (on < kick) { b.vx += (kick - on) * nx; b.vy += (kick - on) * ny; }
    }
    return true;
}

bool Game::hitCircle(float cx, float cy, float r, float e, float kick) {
    Ball& b = ball;
    float nx = b.x - cx, ny = b.y - cy;
    const float R = r + BALL_R;
    const float d2 = nx * nx + ny * ny;
    if (d2 >= R * R) return false;
    const float d = sqrtf(d2);
    if (d < 1e-4f) { nx = 0.0f; ny = -1.0f; } else { nx /= d; ny /= d; }
    b.x = cx + nx * R;
    b.y = cy + ny * R;
    const float vn = b.vx * nx + b.vy * ny;
    if (vn < 0.0f) { b.vx -= (1.0f + e) * vn * nx; b.vy -= (1.0f + e) * vn * ny; }
    if (kick > 0.0f) {
        const float on = b.vx * nx + b.vy * ny;
        if (on < kick) { b.vx += (kick - on) * nx; b.vy += (kick - on) * ny; }
    }
    return true;
}

// ----------------------------------------------------------------------------
// Score, multiplicateur
// ----------------------------------------------------------------------------
void Game::addScore(uint32_t base) {
    score += base * (uint32_t)multiplier;
    if (score >= 25000) unlock(ACH_SCORE_25K);
    if (score >= 100000) unlock(ACH_SCORE_100K);
}

void Game::bumpMultiplier() {
    if (multiplier >= MAX_MULT) { addScore(1000); return; }
    multiplier++;
    emit(Sfx::Multiplier);
    char buf[28];
    snprintf(buf, sizeof(buf), "MULTIPLIER x%d", multiplier);
    say(buf, 1.8f);
    if (multiplier >= MAX_MULT) unlock(ACH_TOAD_X5);
}

// ----------------------------------------------------------------------------
// Pas de physique
// ----------------------------------------------------------------------------
void Game::substep(float dt, bool leftHeld, bool rightHeld) {
    if (tilted) { leftHeld = false; rightHeld = false; }
    for (int i = 0; i < 2; i++) {
        Flipper& f = fl[i];
        const bool held = (i == 0) ? leftHeld : rightHeld;
        f.prevAngle = f.angle;
        const float target = held ? f.activeRad : f.restRad;
        const float spd = (held ? 26.0f : 14.0f) * dt;
        if (f.angle < target) f.angle = fminf(f.angle + spd, target);
        else                  f.angle = fmaxf(f.angle - spd, target);
        f.omega = (f.angle - f.prevAngle) / dt;
    }

    Ball& b = ball;
    if (!b.active) return;
    b.vy += GRAVITY * dt;
    b.x += b.vx * dt;
    b.y += b.vy * dt;

    // murs et slingshots
    for (int i = 0; i < wallCount; i++) {
        const Seg& s = walls[i];
        const bool sling = (s.kind == 1 || s.kind == 2);
        const float kick = (sling && slingCool_ <= 0.0f) ? 280.0f : 0.0f;
        if (hitSeg(s.x1, s.y1, s.x2, s.y2, s.bounce, kick, 2.0f, nullptr) && sling) {
            addScore(10);
            emit(Sfx::Sling, (uint8_t)(s.kind - 1));
            slingWindow_ = 2.5f;
            if (++slingHits_ >= 4) { slingCool_ = 3.0f; slingHits_ = 0; }
        }
    }
    // flippers
    for (int i = 0; i < 2; i++) {
        const Flipper& f = fl[i];
        const float dx = f.isLeft ? cosf(f.angle) : -cosf(f.angle);
        hitSeg(f.px, f.py, f.px + dx * f.len, f.py + sinf(f.angle) * f.len, 0.45f, 0.0f, 4.0f, &f);
    }
    // piquets entre les couloirs F-R-O-G
    for (int i = 0; i < 3; i++) hitCircle(81.0f + 32.0f * (float)i, 28.0f, 2.5f, 0.6f, 0.0f);
    // bumpers-grenouilles
    for (int i = 0; i < bumperCount; i++) {
        if (hitCircle(bumpers[i].x, bumpers[i].y, bumpers[i].r, 0.9f, 380.0f)) {
            bumperFlash[i] = 0.25f;
            bumperHits++;
            addScore(100);
            emit(Sfx::Bumper, (uint8_t)i);
            unlock(ACH_FIRST_HOP);
        }
    }
    // nenuphars : solides ; le premier contact les allume
    const uint8_t allPads = (uint8_t)((1u << padCount) - 1u);
    for (int i = 0; i < padCount; i++) {
        if (hitCircle(pads[i].x, pads[i].y, pads[i].r, 0.6f, 120.0f)) {
            padFlash[i] = 0.3f;
            if (!((padLit >> i) & 1) && padReset <= 0.0f) {
                padLit |= (uint8_t)(1u << i);
                addScore(table == TABLE_POND ? 500 : 300);
                emit(Sfx::Pad, (uint8_t)i);
                if (padLit == allPads) {
                    emit(Sfx::PadsDone);
                    if (table == TABLE_POND) {
                        // les 4 nenuphars : la mare est « nettoyee », la bille part vers la table du canard
                        addScore(5000);
                        unlock(ACH_PAD_MASTER);
                        say("LILY PADS! TO THE DUCK", 2.2f);
                        travel(TABLE_DUCK);
                        return;
                    }
                    // tous les oeufs : le canard est etourdi (double degat) et les oeufs reviennent
                    addScore(3000);
                    duck.stun = 12.0f;
                    padReset = 0.8f;
                    say("ALL EGGS! DUCK STUNNED", 2.2f);
                }
            }
        }
    }
    // canard (table 2 seulement)
    if (table == TABLE_DUCK && duck.st == DuckState::Patrol) {
        if (hitCircle(duck.x, duck.y, 13.0f, 0.9f, 170.0f) && duck.cool <= 0.0f) {
            duck.cool = 0.3f;
            duck.flash = 0.3f;
            duck.hp -= (duck.stun > 0.0f) ? 2 : 1;
            addScore(500u * (uint32_t)duck.level);
            emit(Sfx::DuckHit);
            if (duck.hp <= 0) {
                duck.st = DuckState::Dying;
                duck.timer = 2.0f;
                ducksDown++;
                addScore(10000u * (uint32_t)duck.level);
                unlock(ACH_DUCK_DOWN);
                if (ducksDown >= 3) unlock(ACH_HAT_TRICK);
                emit(Sfx::DuckDown);
                char buf[28];
                snprintf(buf, sizeof(buf), "DUCK DOWN! +%u", (unsigned)(10000u * (unsigned)duck.level * (unsigned)multiplier));
                say(buf, 2.5f);
            }
        }
    }
    // couloirs F-R-O-G (capteurs)
    for (int i = 0; i < LANE_COUNT; i++) {
        const float dx = b.x - lanes[i].x, dy = b.y - lanes[i].y;
        if (dx * dx + dy * dy < 8.0f * 8.0f && !((laneLit >> i) & 1)) {
            laneLit |= (uint8_t)(1u << i);
            addScore(200);
            emit(Sfx::Rollover, (uint8_t)i);
            if (laneLit == 0x0F) {
                laneLit = 0;
                addScore(3000);
                bumpMultiplier();
                unlock(ACH_FLY_CATCHER);
                emit(Sfx::LanesDone);
                char buf[28];
                snprintf(buf, sizeof(buf), "F-R-O-G! x%d", multiplier);
                say(buf, 2.0f);
            }
        }
    }
    // spinner "mouches"
    {
        const float dx = b.x - spinner.x, dy = b.y - spinner.y;
        const bool in = dx * dx + dy * dy < spinner.r * spinner.r;
        if (in && !inSpinner) {
            const float sp = sqrtf(b.vx * b.vx + b.vy * b.vy);
            spinVel = clampf(sp / 12.0f, 6.0f, 40.0f);
            addScore(100);
            flies++;
            emit(Sfx::Spinner);
            if (flies % 10 == 0) {
                addScore(5000);
                ballSaveTimer += 8.0f;
                say("10 FLIES! BALL SAVE+", 2.0f);
            }
        }
        inSpinner = in;
    }

    // vitesse bornee, anti-blocage
    const float sp2 = b.vx * b.vx + b.vy * b.vy;
    if (sp2 > MAX_SPEED * MAX_SPEED) {
        const float k = MAX_SPEED / sqrtf(sp2);
        b.vx *= k; b.vy *= k;
    }
    b.inShooter = (b.x > 214.0f && b.y > 150.0f);
    if (!b.inShooter && sp2 < 12.0f * 12.0f) {
        b.stuck += dt;
        if (b.stuck > 4.0f) {
            b.vx = (frand() - 0.5f) * 160.0f;
            b.vy = -150.0f;
            b.stuck = 0.0f;
        }
    } else {
        b.stuck = 0.0f;
    }
    if (b.y > DRAIN_Y) onDrain();
}

void Game::stepPhysics(float dt, const Input& in) {
    physAccum_ += clampf(dt, 0.0f, MAX_FRAME_DT);
    int n = 0;
    while (physAccum_ >= PHYSICS_DT && n < MAX_SUBSTEPS) {
        substep(PHYSICS_DT, in.left, in.right);
        physAccum_ -= PHYSICS_DT;
        n++;
    }
    if (n == MAX_SUBSTEPS) physAccum_ = 0.0f;
}

// ----------------------------------------------------------------------------
// Bille, partie
// ----------------------------------------------------------------------------
void Game::spawnBall() {
    ball = Ball();
    ball.active = true;
    ball.x = LANE_X; ball.y = LANE_REST_Y;
    ballLife = 0.0f;
    plungerCharge = 0.0f;
    tilt = 0.0f;
    tilted = false;
    ballSaveTimer = 0.0f;
}

void Game::startNewGame() {
    score = 0;
    ballsLeft = START_BALLS;
    multiplier = 1;
    flies = 0;
    ducksDown = 0;
    bumperHits = 0;
    buildTable(TABLE_POND);
    travelT = 0.0f;
    duck = Duck();
    respawnDelay = 0.0f;
    saveArmed_ = true;
    lastRank = -1;
    state = State::Playing;
    spawnBall();
    say("PULL THE PLUNGER!", 2.5f);
}

void Game::onDrain() {
    ball.active = false;
    if (ballSaveTimer > 0.0f && !tilted) {
        ballSaveTimer = 0.0f;
        saveArmed_ = false;
        emit(Sfx::BallSaved);
        say("BALL SAVED!", 1.8f);
        respawnDelay = 0.9f;
        return;
    }
    emit(tilted ? Sfx::Tilt : Sfx::Drain);
    saveArmed_ = true;
    multiplier = 1;
    laneLit = 0;
    ballsLeft--;
    if (ballsLeft <= 0) { endGame(); return; }
    respawnDelay = 1.4f;
}

int Game::rankOf(uint32_t sc) const {
    for (int i = 0; i < TOP_N; i++) if (sc > save.top[i].score) return i;
    return TOP_N;
}

void Game::endGame() {
    state = State::GameOver;
    stateLock = 1.2f;
    lastRank = rankOf(score);
    emit(Sfx::GameOver);
    persist();
}

void Game::insertScore() {
    const int r = rankOf(score);
    if (r >= TOP_N) return;
    for (int i = TOP_N - 1; i > r; i--) save.top[i] = save.top[i - 1];
    save.top[r].score = score;
    memcpy(save.top[r].tri, nameBuf, 4);
    lastRank = r;
    persist();
}

void Game::returnToTitle() {
    state = State::Title;
    ball.active = false;
    respawnDelay = 0.0f;
    bannerTimer = 0.0f;
}

// ----------------------------------------------------------------------------
// Canard
// ----------------------------------------------------------------------------
void Game::updateDuck(float dt) {
    Duck& d = duck;
    d.flash = fmaxf(0.0f, d.flash - dt);
    d.cool = fmaxf(0.0f, d.cool - dt);
    d.stun = fmaxf(0.0f, d.stun - dt);
    switch (d.st) {
        case DuckState::Patrol: {
            d.t += dt * (0.9f + 0.12f * (float)d.level);
            const float nx = 113.0f + 72.0f * sinf(d.t);
            d.dir = (nx >= d.x) ? 1 : -1;
            d.x = nx;
            d.y = 70.0f + 5.0f * sinf(d.t * 3.1f);
            break;
        }
        case DuckState::Dying:
            d.timer -= dt;
            if (d.timer <= 0.0f) { d.st = DuckState::Away; d.timer = 2.5f; }
            break;
        case DuckState::Away:
            d.timer -= dt;
            if (d.timer <= 0.0f) {
                d.level++;
                d.maxHp = 3 + 2 * (d.level - 1);
                if (d.maxHp > 15) d.maxHp = 15;
                d.hp = d.maxHp;
                d.t = 0.0f;
                d.st = DuckState::Patrol;
                if (state == State::Playing) {
                    char buf[28];
                    snprintf(buf, sizeof(buf), "BACK TO THE POND");
                    say(buf, 2.0f);
                    travel(TABLE_POND);
                }
            }
            break;
    }
}

// ----------------------------------------------------------------------------
// Boucle de jeu
// ----------------------------------------------------------------------------

void Game::updatePlaying(float dt, const Input& in, bool leftEdge, bool rightEdge, bool nudgeEdge) {
    if (travelT > 0.0f) {   // ecran de transition (bulles) : le jeu est fige
        travelT -= dt;
        if (travelT <= 0.0f) {
            travelT = 0.0f;
            buildTable(travelTo);
            if (travelTo == TABLE_DUCK) { duck.st = DuckState::Patrol; duck.t = 0.0f; }
            spawnBall();
            say(travelTo == TABLE_DUCK ? "DUCK TABLE" : "THE POND", 2.0f);
        }
        return;
    }
    if (respawnDelay > 0.0f) {
        respawnDelay -= dt;
        if (respawnDelay <= 0.0f && ballsLeft > 0) spawnBall();
    }
    // lance-bille
    if (ball.active && ball.inShooter && ball.y > LANE_REST_Y - 20.0f) {
        if (in.plunger) {
            plungerCharge = fminf(1.0f, plungerCharge + dt / 0.9f);
        } else if (plungerCharge > 0.05f) {
            ball.vy = -(620.0f + 520.0f * plungerCharge);
            ball.vx = 0.0f;
            plungerCharge = 0.0f;
            if (saveArmed_) ballSaveTimer = BALL_SAVE_SEC;
            emit(Sfx::Launch);
        } else {
            plungerCharge = 0.0f;
        }
    }
    // changement de couloir (comme sur un vrai flipper) + sons de flippers
    if (leftEdge)  { laneLit = (uint8_t)(((laneLit << 1) | (laneLit >> 3)) & 0x0F); emit(Sfx::Flipper, 0); }
    if (rightEdge) { laneLit = (uint8_t)(((laneLit >> 1) | (laneLit << 3)) & 0x0F); emit(Sfx::Flipper, 1); }
    // secousse
    if (nudgeEdge && ball.active && !ball.inShooter && !tilted) {
        tilt += 0.4f;
        ball.vx += (frand() - 0.5f) * 220.0f;
        ball.vy -= 60.0f;
        emit(Sfx::Nudge);
        if (tilt > 1.0f) { tilted = true; say("TILT!", 2.0f); emit(Sfx::Tilt); }
    }
    tilt = fmaxf(0.0f, tilt - 0.35f * dt);
    slingCool_ = fmaxf(0.0f, slingCool_ - dt);
    if ((slingWindow_ -= dt) <= 0.0f) { slingWindow_ = 0.0f; slingHits_ = 0; }

    stepPhysics(dt, in);

    if (ball.active && !ball.inShooter) {
        ballLife += dt;
        ballSaveTimer = fmaxf(0.0f, ballSaveTimer - dt);
        if (ballLife >= 90.0f) unlock(ACH_SURVIVOR);
    }
    for (int i = 0; i < BUMPER_COUNT; i++) bumperFlash[i] = fmaxf(0.0f, bumperFlash[i] - dt);
    for (int i = 0; i < PAD_COUNT; i++) padFlash[i] = fmaxf(0.0f, padFlash[i] - dt);
    if (padReset > 0.0f) {
        padReset -= dt;
        if (padReset <= 0.0f) padLit = 0;
    }
    spinAngle += spinVel * dt;
    spinVel *= (1.0f - fminf(1.0f, 1.2f * dt));
    if (table == TABLE_DUCK) updateDuck(dt);
}

void Game::syncInput(const Input& in) {
    prevLeft_ = in.left; prevRight_ = in.right; prevPlunger_ = in.plunger; prevNudge_ = in.nudge;
}

void Game::update(float dt, const Input& in, uint32_t now) {
    nowMs = now;
    animT += dt;
    bannerTimer = fmaxf(0.0f, bannerTimer - dt);
    achTimer = fmaxf(0.0f, achTimer - dt);
    stateLock = fmaxf(0.0f, stateLock - dt);
    const bool le = in.left && !prevLeft_;
    const bool re = in.right && !prevRight_;
    const bool pe = in.plunger && !prevPlunger_;
    const bool ne = in.nudge && !prevNudge_;

    switch (state) {
        case State::Title:
            if (le) { menuSel = (menuSel + 2) % 3; emit(Sfx::MenuMove); }
            if (re) { menuSel = (menuSel + 1) % 3; emit(Sfx::MenuMove); }
            if (pe || ne) {
                emit(Sfx::MenuOk);
                if (menuSel == 0) startNewGame();
                else if (menuSel == 1) state = State::Options;
                else state = State::Trophies;
            }
            break;
        case State::Options:
            if (le || re) { save.style = (save.style == STYLE_COLOR) ? STYLE_GAMEBOY : STYLE_COLOR; emit(Sfx::MenuMove); persist(); }
            if (pe || ne) { emit(Sfx::MenuOk); state = State::Title; }
            break;
        case State::Trophies:
            if (pe || ne) { emit(Sfx::MenuOk); state = State::Title; }
            break;
        case State::Playing:
            updatePlaying(dt, in, le, re, ne);
            break;
        case State::GameOver:
            // la boucle d'animation continue (le canard, les ondes) mais plus de bille
            updateDuck(dt);
            if (stateLock <= 0.0f && (pe || ne)) {
                if (lastRank >= 0 && lastRank < TOP_N) {
                    state = State::NameEntry;
                    nameIdx = 0;
                    nameBuf[0] = 'A'; nameBuf[1] = 'A'; nameBuf[2] = 'A'; nameBuf[3] = 0;
                    emit(Sfx::NewRecord);
                } else {
                    state = State::Title;
                }
                emit(Sfx::MenuOk);
            }
            break;
        case State::NameEntry: {
            char& c = nameBuf[nameIdx];
            if (le) { c = (c <= 'A') ? 'Z' : (char)(c - 1); emit(Sfx::MenuMove); }
            if (re) { c = (c >= 'Z') ? 'A' : (char)(c + 1); emit(Sfx::MenuMove); }
            if (ne && nameIdx > 0) { nameIdx--; emit(Sfx::MenuMove); }
            if (pe) {
                emit(Sfx::MenuOk);
                if (++nameIdx >= 3) { insertScore(); state = State::Title; }
            }
            break;
        }
    }
    prevLeft_ = in.left; prevRight_ = in.right; prevPlunger_ = in.plunger; prevNudge_ = in.nudge;
}

} // namespace frog
