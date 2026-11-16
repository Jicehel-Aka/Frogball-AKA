idf_component_register(
    SRCS
        app_main.cpp
        game/frogball_core.cpp
        game/frog_sound.cpp
        ui/gfx_prims.cpp
        ui/render.cpp
        ui/sprites.cpp
        platform/gfx_backend_aka.cpp
        platform/input_aka.cpp
        platform/audio_aka.cpp
        platform/store_aka.cpp
    INCLUDE_DIRS
        .
    # RAPPEL : les composants vont dans REQUIRES, PAS dans INCLUDE_DIRS.
    REQUIRES
        gamebuino aka_runtime
    PRIV_REQUIRES
        esp_timer
        heap
)

# Le rendu et la physique sont du calcul pur : toujours optimises, meme en -Og.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
