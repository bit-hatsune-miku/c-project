cmake_minimum_required(VERSION 3.16)
project(OurUndergroundBITIdol CXX)

# Generate compile_commands.json for clangd / VS Code IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_RMLUI "Fetch and build the default RmlUi battle experience." ON)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Fetch nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Find packages
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)

set(SDL2_TTF_FOUND TRUE)
set(SDL2_TTF_LIBRARIES SDL2_ttf::SDL2_ttf)
set(SDL2_IMAGE_FOUND TRUE)
set(SDL2_IMAGE_LIBRARIES SDL2_image::SDL2_image)

find_package(PkgConfig QUIET)

add_compile_definitions(SDL2_TTF_FOUND=1)
add_compile_definitions(SDL2_IMAGE_FOUND=1)

# Source files
set(MAIN_APP_SOURCES
    src/main.cpp
    src/GameMenu/main_menu.cpp
    src/GameMenu/load_menu.cpp
    src/GameMenu/pause_menu.cpp
    src/GameMenu/exit_to_main_menu.cpp
    src/Settings/settings.cpp
    src/window.cpp
    src/game/audio/ui_music_controller.cpp
    src/game/audio/ui_music_controller.h
    src/game/audio/ui_music_types.h
    src/game/credits/credits_data.cpp
    src/game/credits/credits_data.h
    src/game/vn/vn_system.cpp
    src/game/vn/vn_script.cpp
    src/game/save/save.cpp
)

set(BATTLE_CORE_SOURCES
    src/game/core/battle_manager.cpp
    src/game/core/battle_manager.h
    src/game/core/battle_loader.cpp
    src/game/core/battle_loader.h
    src/game/core/turn_system.cpp
    src/game/core/turn_system.h
    src/game/core/ability_system.cpp
    src/game/core/ability_system.h
    src/game/core/battle_flow_controller.cpp
    src/game/core/battle_flow_controller.h
    src/game/core/battle_turn_flow.cpp
    src/game/core/battle_turn_flow.h
    src/game/core/player_progression.cpp
    src/game/core/player_progression.h
    src/game/render/battle_ui.cpp
    src/game/render/battle_ui.h
    src/game/render/battle_asset_loading.cpp
    src/game/render/battle_asset_loading.h
    src/game/render/battle_camera_staging.cpp
    src/game/render/battle_camera_staging.h
    src/game/render/battle_combat_begin_animation.cpp
    src/game/render/battle_combat_begin_animation.h
    src/game/render/battle_feedback.cpp
    src/game/render/battle_feedback.h
    src/game/render/battle_party_staging.cpp
    src/game/render/battle_party_staging.h
    src/game/render/battle_scene_types.h
    src/game/render/battle_world_renderer.cpp
    src/game/render/battle_world_renderer.h
    src/game/core/easing.cpp
    src/game/core/easing.h
    src/game/render/camera_3d.cpp
    src/game/render/camera_3d.h
)

set(BATTLE_PRESENTATION_SOURCES
    src/game/presentation/ari_boss_presentation.cpp
    src/game/presentation/ari_boss_presentation.h
    src/game/presentation/ari_skill_presentation.cpp
    src/game/presentation/ari_skill_presentation.h
    src/game/presentation/ari_ultimate_presentation.cpp
    src/game/presentation/ari_ultimate_presentation.h
    src/game/presentation/ability_presentation.cpp
    src/game/presentation/ability_presentation.h
    src/game/presentation/miku_rhythm_game.cpp
    src/game/presentation/miku_rhythm_game.h
    src/game/presentation/miku_sing_presentation.cpp
    src/game/presentation/miku_sing_presentation.h
    src/game/presentation/miku_diandong_presentation.cpp
    src/game/presentation/miku_diandong_presentation.h
    src/game/presentation/miku_self_corruption_presentation.cpp
    src/game/presentation/miku_self_corruption_presentation.h
    src/game/presentation/cupcakke_drum_presentation.cpp
    src/game/presentation/cupcakke_drum_presentation.h
    src/game/presentation/cupcakke_ultimate_presentation.cpp
    src/game/presentation/cupcakke_ultimate_presentation.h
    src/game/presentation/lyoo_heal_presentation.cpp
    src/game/presentation/lyoo_heal_presentation.h
    src/game/presentation/jiafei_boss_presentation.cpp
    src/game/presentation/jiafei_boss_presentation.h
    src/game/presentation/jiafei_scream_presentation.cpp
    src/game/presentation/jiafei_scream_presentation.h
    src/game/presentation/jiafei_ultimate_presentation.cpp
    src/game/presentation/jiafei_ultimate_presentation.h
    src/game/presentation/lyoo_boss_presentation.cpp
    src/game/presentation/lyoo_boss_presentation.h
    src/game/presentation/lyoo_plot_twist_presentation.cpp
    src/game/presentation/lyoo_plot_twist_presentation.h
    src/game/presentation/luotianyi_boss_presentation.cpp
    src/game/presentation/luotianyi_boss_presentation.h
    src/game/presentation/luotianyi_ultimate_presentation.cpp
    src/game/presentation/luotianyi_ultimate_presentation.h
    src/game/presentation/qr_code_attack_presentation.cpp
    src/game/presentation/qr_code_attack_presentation.h
    src/game/presentation/qr_code_shield_presentation.cpp
    src/game/presentation/qr_code_shield_presentation.h
    src/game/presentation/wechatalipay_ultimate_presentation.cpp
    src/game/presentation/wechatalipay_ultimate_presentation.h
    src/game/presentation/presentation_runtime.cpp
    src/game/presentation/presentation_runtime.h
    src/game/presentation/splash_art_animation.cpp
    src/game/presentation/splash_art_animation.h
    src/game/presentation/luotianyi_skill_presentation.cpp
    src/game/presentation/luotianyi_skill_presentation.h
    src/game/presentation/presentation_registry.cpp
)

set(BATTLE_DEMO_SOURCES
    src/game/demo_battle_session.cpp
    src/game/demo_battle_session.h
    src/game/battle_session_core.cpp
    src/game/battle_session_core.h
    src/game/demo/battle_party_setup.cpp
    src/game/demo/battle_party_setup.h
    src/game/demo/battle_party_setup_ui.cpp
    src/game/demo/battle_party_setup_ui.h
    src/game/demo/demo_narrative_flow.cpp
    src/game/demo/demo_narrative_flow.h
)

set(RMLUI_APP_BATTLE_SOURCES
    src/game/app_battle_session.cpp
    src/game/app_battle_session.h
    src/game/render/gl_battle_scene_renderer.cpp
    src/game/render/gl_battle_scene_renderer.h
    src/game/render/gl_function_loader.cpp
    src/game/render/gl_function_loader.h
)

set(RMLUI_SELECTOR_SOURCES
    src/game/boss_selector_session.cpp
    src/game/boss_selector_session.h
    src/game/party_loader_session.cpp
    src/game/party_loader_session.h
)

set(FRONT_UI_SOURCES
    src/graphics/rmlui_loading_overlay.cpp
    src/graphics/rmlui_loading_overlay.h
    src/graphics/rmlui_sdl_gl_renderer.cpp
    src/graphics/rmlui_sdl_gl_renderer.h
    src/graphics/front_ui_credits.cpp
    src/graphics/front_ui_credits.h
    src/graphics/front_ui_document.h
    src/graphics/front_ui_story.cpp
    src/graphics/front_ui_story.h
    src/graphics/front_ui_pause.cpp
    src/graphics/front_ui_pause.h
    src/graphics/front_ui_load.cpp
    src/graphics/front_ui_load.h
    src/graphics/front_ui_main_menu.cpp
    src/graphics/front_ui_main_menu.h
    src/graphics/front_ui_settings.cpp
    src/graphics/front_ui_settings.h
    src/graphics/front_ui_session.cpp
    src/graphics/front_ui_session.h
)

set(HEADERS
    src/GameMenu/menu_shared.h
    src/Settings/settings.h
    src/window.h
    src/game/vn/vn_system.h
    src/game/vn/vn_script.h
    src/game/save/save.h
)

# Create executable
add_executable(OurUndergroundBITIdol
    ${MAIN_APP_SOURCES}
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    ${HEADERS}
)

add_executable(finale
    ${MAIN_APP_SOURCES}
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    ${HEADERS}
)

target_compile_definitions(finale PRIVATE
    VN_SCRIPT_PATH="assets/vn/json/finale.json"
    VN_AUTO_START_STORY=1
)

add_executable(ch0
    ${MAIN_APP_SOURCES}
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    ${HEADERS}
)

target_compile_definitions(ch0 PRIVATE
    VN_SCRIPT_PATH="assets/vn/json/ch0.json"
    VN_AUTO_START_STORY=1
)

add_executable(demo
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

add_executable(demo_jiafei
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

target_compile_definitions(demo_jiafei PRIVATE JIAFEI_DEMO=1)

add_executable(demo_lyoo_plot_twist
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

target_compile_definitions(demo_lyoo_plot_twist PRIVATE LYOO_PLOT_TWIST_DEMO=1)

add_executable(demo_wechatalipay
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

target_compile_definitions(demo_wechatalipay PRIVATE WECHATALIPAY_DEMO=1)

add_executable(demo_ari
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

target_compile_definitions(demo_ari PRIVATE ARI_DEMO=1)

add_executable(demo_luotianyi
    src/demo.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    src/game/vn/vn_system.cpp
    src/game/vn/vn_system.h
    src/game/vn/vn_script.cpp
    src/game/vn/vn_script.h
)

target_compile_definitions(demo_luotianyi PRIVATE 
    LUOTIANYI_DEMO=1
    VN_ENABLE_TTF=1
    VN_ENABLE_IMAGE=1
)


if (ENABLE_RMLUI)
    find_package(OpenGL REQUIRED)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
    set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG 6.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(rmlui)

    add_executable(battle_testing
        src/rmlui_battle_main.cpp
        src/window.cpp
        src/window.h
        src/game/vn/vn_system.cpp
        src/game/vn/vn_system.h
        src/game/vn/vn_script.cpp
        src/game/demo/demo_narrative_flow.cpp
        src/game/demo/demo_narrative_flow.h
        src/graphics/rmlui_loading_overlay.cpp
        src/graphics/rmlui_loading_overlay.h
        src/graphics/rmlui_sdl_gl_renderer.cpp
        src/graphics/rmlui_sdl_gl_renderer.h
        src/graphics/front_ui_pause.cpp
        src/graphics/front_ui_pause.h
        src/graphics/front_ui_settings.cpp
        src/graphics/front_ui_settings.h
        ${RMLUI_APP_BATTLE_SOURCES}
        ${BATTLE_CORE_SOURCES}
        ${BATTLE_PRESENTATION_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    add_executable(boss_selector
        src/boss_selector_main.cpp
        src/window.cpp
        src/window.h
        src/graphics/rmlui_loading_overlay.cpp
        src/graphics/rmlui_loading_overlay.h
        src/graphics/rmlui_sdl_gl_renderer.cpp
        src/graphics/rmlui_sdl_gl_renderer.h
        src/game/boss_selector_session.cpp
        src/game/boss_selector_session.h
        src/game/save/save.cpp
        src/game/core/battle_loader.cpp
        src/game/core/battle_loader.h
        src/game/core/battle_manager.h
        src/game/core/player_progression.cpp
        src/game/core/player_progression.h
        src/game/core/easing.cpp
        src/game/core/easing.h
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    add_executable(menu_preview
        src/menu_preview_main.cpp
        src/window.cpp
        src/window.h
        src/graphics/rmlui_loading_overlay.cpp
        src/graphics/rmlui_loading_overlay.h
        src/graphics/menu_preview_session.cpp
        src/graphics/menu_preview_session.h
        src/graphics/rmlui_sdl_gl_renderer.cpp
        src/graphics/rmlui_sdl_gl_renderer.h
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    add_executable(battle_selector_preview
        src/battle_selector_preview_main.cpp
        src/window.cpp
        src/window.h
        src/graphics/rmlui_loading_overlay.cpp
        src/graphics/rmlui_loading_overlay.h
        src/graphics/battle_selector_preview_session.cpp
        src/graphics/battle_selector_preview_session.h
        src/graphics/rmlui_sdl_gl_renderer.cpp
        src/graphics/rmlui_sdl_gl_renderer.h
        src/game/save/save.cpp
        src/game/core/battle_loader.cpp
        src/game/core/battle_loader.h
        src/game/core/battle_manager.h
        src/game/core/player_progression.cpp
        src/game/core/player_progression.h
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    add_executable(party_loader_preview
        src/party_loader_preview_main.cpp
        src/window.cpp
        src/window.h
        src/graphics/party_loader_preview_session.cpp
        src/graphics/party_loader_preview_session.h
        src/graphics/rmlui_sdl_gl_renderer.cpp
        src/graphics/rmlui_sdl_gl_renderer.h
        src/game/core/battle_loader.cpp
        src/game/core/battle_loader.h
        src/game/core/battle_manager.h
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_sources(OurUndergroundBITIdol PRIVATE
        ${FRONT_UI_SOURCES}
        ${RMLUI_APP_BATTLE_SOURCES}
        ${RMLUI_SELECTOR_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_sources(finale PRIVATE
        ${FRONT_UI_SOURCES}
        ${RMLUI_APP_BATTLE_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_sources(ch0 PRIVATE
        ${FRONT_UI_SOURCES}
        ${RMLUI_APP_BATTLE_SOURCES}
        ${RMLUI_SELECTOR_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_sources(finale PRIVATE
        ${RMLUI_SELECTOR_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_sources(ch0 PRIVATE
        ${RMLUI_SELECTOR_SOURCES}
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_include_directories(battle_testing PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_compile_definitions(battle_testing PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_link_libraries(battle_testing PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    target_include_directories(boss_selector PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_compile_definitions(boss_selector PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_link_libraries(boss_selector PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    target_include_directories(menu_preview PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_include_directories(battle_selector_preview PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_include_directories(party_loader_preview PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_compile_definitions(menu_preview PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_compile_definitions(battle_selector_preview PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_compile_definitions(party_loader_preview PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_link_libraries(menu_preview PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    target_link_libraries(battle_selector_preview PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    target_link_libraries(party_loader_preview PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    add_custom_command(TARGET battle_testing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:battle_testing>
            $<TARGET_FILE_DIR:battle_testing>/battle${CMAKE_EXECUTABLE_SUFFIX}
    )
    if (SDL2_TTF_FOUND)
        target_include_directories(battle_testing PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_TTF=1)
        target_link_libraries(battle_testing PRIVATE ${SDL2_TTF_LIBRARIES})
        target_include_directories(boss_selector PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(boss_selector PRIVATE BATTLE_ENABLE_TTF=1)
        target_link_libraries(boss_selector PRIVATE ${SDL2_TTF_LIBRARIES})
        target_include_directories(menu_preview PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(menu_preview PRIVATE BATTLE_ENABLE_TTF=1)
        target_link_libraries(menu_preview PRIVATE ${SDL2_TTF_LIBRARIES})
        target_include_directories(battle_selector_preview PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(battle_selector_preview PRIVATE BATTLE_ENABLE_TTF=1)
        target_link_libraries(battle_selector_preview PRIVATE ${SDL2_TTF_LIBRARIES})
        target_include_directories(party_loader_preview PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(party_loader_preview PRIVATE BATTLE_ENABLE_TTF=1)
        target_link_libraries(party_loader_preview PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
    if (SDL2_IMAGE_FOUND)
        target_include_directories(battle_testing PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(battle_testing PRIVATE ${SDL2_IMAGE_LIBRARIES})
        target_include_directories(boss_selector PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(boss_selector PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(boss_selector PRIVATE ${SDL2_IMAGE_LIBRARIES})
        target_include_directories(menu_preview PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(menu_preview PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(menu_preview PRIVATE ${SDL2_IMAGE_LIBRARIES})
        target_include_directories(battle_selector_preview PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(battle_selector_preview PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(battle_selector_preview PRIVATE ${SDL2_IMAGE_LIBRARIES})
        target_include_directories(party_loader_preview PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(party_loader_preview PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(party_loader_preview PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    add_executable(battle_testing_legacy
        src/battle_main.cpp
        src/window.cpp
        src/window.h
        ${BATTLE_CORE_SOURCES}
        ${BATTLE_PRESENTATION_SOURCES}
    )

    add_executable(battle_testing_jiafei
        src/battle_main.cpp
        src/window.cpp
        src/window.h
        ${BATTLE_CORE_SOURCES}
        ${BATTLE_PRESENTATION_SOURCES}
    )
    target_compile_definitions(battle_testing_jiafei PRIVATE JIAFEI_BATTLE_DEFAULT=1)

else()
    add_executable(battle_testing
        src/battle_main.cpp
        src/window.cpp
        src/window.h
        ${BATTLE_CORE_SOURCES}
        ${BATTLE_PRESENTATION_SOURCES}
    )

    add_executable(battle_testing_jiafei
        src/battle_main.cpp
        src/window.cpp
        src/window.h
        ${BATTLE_CORE_SOURCES}
        ${BATTLE_PRESENTATION_SOURCES}
    )
    target_compile_definitions(battle_testing_jiafei PRIVATE JIAFEI_BATTLE_DEFAULT=1)
endif()

# Include directories
target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(finale PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(ch0 PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(battle_testing PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo_jiafei PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo_lyoo_plot_twist PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo_wechatalipay PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo_ari PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo_luotianyi PRIVATE ${SDL2_INCLUDE_DIRS})
if (TARGET boss_selector)
    target_include_directories(boss_selector PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if (TARGET menu_preview)
    target_include_directories(menu_preview PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if (TARGET party_loader_preview)
    target_include_directories(party_loader_preview PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if (ENABLE_RMLUI)
    target_include_directories(OurUndergroundBITIdol PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    target_include_directories(finale PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    target_include_directories(ch0 PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    if (TARGET boss_selector)
        target_include_directories(boss_selector PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    endif()
    if (TARGET menu_preview)
        target_include_directories(menu_preview PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    endif()
    if (TARGET party_loader_preview)
        target_include_directories(party_loader_preview PRIVATE ${rmlui_SOURCE_DIR}/Backends)
    endif()
endif()
if (TARGET battle_testing_legacy)
    target_include_directories(battle_testing_legacy PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if (SDL2_TTF_FOUND)
    target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(OurUndergroundBITIdol PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(finale PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(finale PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(ch0 PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(ch0 PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_TTF=1)
    target_include_directories(battle_testing_jiafei PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(battle_testing_jiafei PRIVATE BATTLE_ENABLE_TTF=1)
    target_include_directories(demo PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(demo_jiafei PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo_jiafei PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(demo_lyoo_plot_twist PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo_lyoo_plot_twist PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(demo_wechatalipay PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo_wechatalipay PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(demo_ari PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo_ari PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(demo_luotianyi PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo_luotianyi PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    if (TARGET boss_selector)
        target_include_directories(boss_selector PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(boss_selector PRIVATE BATTLE_ENABLE_TTF=1)
    endif()
    if (TARGET menu_preview)
        target_include_directories(menu_preview PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(menu_preview PRIVATE BATTLE_ENABLE_TTF=1)
    endif()
    if (TARGET party_loader_preview)
        target_include_directories(party_loader_preview PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(party_loader_preview PRIVATE BATTLE_ENABLE_TTF=1)
    endif()
    if (TARGET battle_testing_legacy)
        target_include_directories(battle_testing_legacy PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
        target_compile_definitions(battle_testing_legacy PRIVATE BATTLE_ENABLE_TTF=1)
    endif()
endif()
if (SDL2_IMAGE_FOUND)
    target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(OurUndergroundBITIdol PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(finale PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(finale PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(ch0 PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(ch0 PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_IMAGE=1)
    target_include_directories(battle_testing_jiafei PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(battle_testing_jiafei PRIVATE BATTLE_ENABLE_IMAGE=1)
    target_include_directories(demo PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(demo_jiafei PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo_jiafei PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(demo_lyoo_plot_twist PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo_lyoo_plot_twist PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(demo_wechatalipay PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo_wechatalipay PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(demo_ari PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo_ari PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(demo_luotianyi PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo_luotianyi PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    if (TARGET boss_selector)
        target_include_directories(boss_selector PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(boss_selector PRIVATE BATTLE_ENABLE_IMAGE=1)
    endif()
    if (TARGET menu_preview)
        target_include_directories(menu_preview PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(menu_preview PRIVATE BATTLE_ENABLE_IMAGE=1)
    endif()
    if (TARGET party_loader_preview)
        target_include_directories(party_loader_preview PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(party_loader_preview PRIVATE BATTLE_ENABLE_IMAGE=1)
    endif()
    if (TARGET battle_testing_legacy)
        target_include_directories(battle_testing_legacy PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(battle_testing_legacy PRIVATE BATTLE_ENABLE_IMAGE=1)
    endif()
endif()

# Link libraries
target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(finale PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(ch0 PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo_jiafei PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo_lyoo_plot_twist PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo_wechatalipay PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo_ari PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo_luotianyi PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
if (ENABLE_RMLUI)
    target_compile_definitions(OurUndergroundBITIdol PRIVATE RMLUI_SDL_VERSION_MAJOR=2 APP_ENABLE_RMLUI=1)
    target_link_libraries(OurUndergroundBITIdol PRIVATE OpenGL::GL RmlUi::RmlUi)
    target_compile_definitions(finale PRIVATE RMLUI_SDL_VERSION_MAJOR=2 APP_ENABLE_RMLUI=1)
    target_link_libraries(finale PRIVATE OpenGL::GL RmlUi::RmlUi)
    target_compile_definitions(ch0 PRIVATE RMLUI_SDL_VERSION_MAJOR=2 APP_ENABLE_RMLUI=1)
    target_link_libraries(ch0 PRIVATE OpenGL::GL RmlUi::RmlUi)
    if (TARGET boss_selector)
        target_compile_definitions(boss_selector PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
        target_link_libraries(boss_selector PRIVATE OpenGL::GL RmlUi::RmlUi)
    endif()
    if (TARGET menu_preview)
        target_compile_definitions(menu_preview PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
        target_link_libraries(menu_preview PRIVATE OpenGL::GL RmlUi::RmlUi)
    endif()
    if (TARGET party_loader_preview)
        target_compile_definitions(party_loader_preview PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
        target_link_libraries(party_loader_preview PRIVATE OpenGL::GL RmlUi::RmlUi)
    endif()
endif()
if (NOT ENABLE_RMLUI)
    target_link_libraries(battle_testing PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
    target_link_libraries(battle_testing_jiafei PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
endif()
if (TARGET battle_testing_legacy)
    target_link_libraries(battle_testing_legacy PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
    target_link_libraries(battle_testing_jiafei PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
endif()
if (SDL2_TTF_FOUND)
    target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(finale PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(ch0 PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo_jiafei PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo_lyoo_plot_twist PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo_wechatalipay PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo_ari PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo_luotianyi PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(battle_testing_jiafei PRIVATE ${SDL2_TTF_LIBRARIES})
    if (TARGET boss_selector)
        target_link_libraries(boss_selector PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
    if (TARGET menu_preview)
        target_link_libraries(menu_preview PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
    if (TARGET party_loader_preview)
        target_link_libraries(party_loader_preview PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
    if (NOT ENABLE_RMLUI)
        target_link_libraries(battle_testing PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
    if (TARGET battle_testing_legacy)
        target_link_libraries(battle_testing_legacy PRIVATE ${SDL2_TTF_LIBRARIES})
    endif()
endif()
if (SDL2_IMAGE_FOUND)
    target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(finale PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(ch0 PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo_jiafei PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo_lyoo_plot_twist PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo_wechatalipay PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo_ari PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo_luotianyi PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(battle_testing_jiafei PRIVATE ${SDL2_IMAGE_LIBRARIES})
    if (TARGET boss_selector)
        target_link_libraries(boss_selector PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    if (TARGET menu_preview)
        target_link_libraries(menu_preview PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    if (TARGET party_loader_preview)
        target_link_libraries(party_loader_preview PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    if (NOT ENABLE_RMLUI)
        target_link_libraries(battle_testing PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    if (TARGET battle_testing_legacy)
        target_link_libraries(battle_testing_legacy PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
endif()

# Output directory
set_target_properties(OurUndergroundBITIdol PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(finale PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(ch0 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(battle_testing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (TARGET battle_testing_legacy)
    set_target_properties(battle_testing_legacy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

set_target_properties(battle_testing_jiafei PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo_jiafei PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo_lyoo_plot_twist PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo_wechatalipay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo_ari PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo_luotianyi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if (TARGET boss_selector)
    set_target_properties(boss_selector PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
if (TARGET menu_preview)
    set_target_properties(menu_preview PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
if (TARGET party_loader_preview)
    set_target_properties(party_loader_preview PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
