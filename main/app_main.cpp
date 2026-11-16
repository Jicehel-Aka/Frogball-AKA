/*
===============================================================================
  app_main.cpp — FROGBALL pour la Gamebuino AKA
-------------------------------------------------------------------------------
  Meme structure que les autres portages AKA recents (AKA Pinball, Race3D-AKA) :
    - input_poll() est l'UNIQUE lecteur du bus I2C / de l'ADC (composant aka_runtime) ;
    - akaRuntime.update() gere le menu systeme du lanceur (MENU) : reprendre, commandes,
      langue, credits, retour au loader, capture d'ecran ;
    - le jeu (game/) et le rendu (ui/) sont independants de la plateforme ;
    - AudioTask (CPU1) fait tourner le mixeur gb_audio_player.
===============================================================================
*/
#include "gb_core.h"
#include "gb_graphics.h"
#include "gb_ll_lcd.h"
#include "core/input.h"
#include "aka_runtime/aka_runtime.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>

#include "game/frogball_core.h"
#include "platform/platform.h"
#include "ui/gfx.h"
#include "ui/render.h"

gb_core g_core;           // utilise par aka_runtime et core/input
extern gb_graphics gfx;   // defini dans platform/gfx_backend_aka.cpp

static const char* TAG = "frogball";
static frog::Game g_game;

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

static void on_back_to_title() { g_game.returnToTitle(); }   // entree « Choisir un jeu » du menu systeme

extern "C" void app_main(void) {
    g_core.init();
    gfx.set_backlight_percent(80);
    gfx.set_refresh_rate(60);
    input_init();

    akaRuntime.begin("frogball");
    static const char* const kControls[] = {"CTRL_FLIP", "CTRL_PLUNGER", "CTRL_NUDGE", "CTRL_PAUSE", nullptr};
    akaRuntime.setControlsKeys(kControls);
    akaRuntime.setCredits("Frogball", "jeancharleslebeau", "CC BY-NC 4.0", "Gamebuino AKA");
    akaRuntime.setGameMenuCallback(on_back_to_title);
    akaRuntime.setBeforeExitCallback([] { platform::audio_silence(); });
    mkdir("/sdcard/frogball", 0777);

    render::init();
    platform::audio_init();
    g_game.begin(platform::store());
    ESP_LOGI(TAG, "pret, SRAM libre %u, PSRAM libre %u", (unsigned)g_core.free_sram(), (unsigned)g_core.free_psram());

    uint32_t last = millis();
    while (true) {
        input_poll(g_keys);
        if (!akaRuntime.update(g_keys)) {   // le menu systeme est ouvert et se dessine lui-meme
            platform::audio_silence();
            g_game.syncInput(platform::input_from_keys(g_keys));   // la reprise ne rejoue pas la touche qui a valide « Reprendre »
            last = millis();
            vTaskDelay(pdMS_TO_TICKS(16));
            continue;
        }

        while (!lcd_refresh_completed()) {}   // le transfert DMA precedent est fini : on peut dessiner
        const uint32_t now = millis();
        const float dt = (float)(now - last) * 0.001f;
        last = now;

        g_game.update(dt, platform::input_from_keys(g_keys), now);

        frog::SfxEvent ev;
        while (g_game.popSfx(ev)) platform::audio_play(ev.id, ev.arg);
        const bool playing = (g_game.state == frog::State::Playing || g_game.state == frog::State::Title);
        platform::audio_music(playing, g_game.state == frog::State::Playing && g_game.duck.st == frog::DuckState::Patrol && g_game.duck.hp <= 2);

        render::draw(g_game);
        vid::present();   // vsync + DMA vers la dalle

        vTaskDelay(1);    // laisse la tache idle nourrir le watchdog
    }
}
