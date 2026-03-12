cmake_minimum_required(VERSION 3.16)
project(OurUndergroundBITIdol CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_RMLUI "Fetch and build RmlUi for the upcoming HUD migration." OFF)

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

    add_executable(battle_testing_rmlui
        src/rmlui_battle_smoke.cpp
        src/game/battle_manager.cpp
        src/game/battle_loader.cpp
        src/game/turn_system.cpp
        src/game/ability_system.cpp
        src/game/battle_ui.cpp
        src/game/easing.cpp
        src/game/camera_3d.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
    )
    target_include_directories(battle_testing_rmlui PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${rmlui_SOURCE_DIR}/Backends
    )
    target_compile_definitions(battle_testing_rmlui PRIVATE RMLUI_SDL_VERSION_MAJOR=2)
    target_link_libraries(battle_testing_rmlui PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        nlohmann_json::nlohmann_json
        RmlUi::RmlUi
    )
    if (SDL2_IMAGE_FOUND)
        target_include_directories(battle_testing_rmlui PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
        target_compile_definitions(battle_testing_rmlui PRIVATE BATTLE_ENABLE_IMAGE=1)
        target_link_libraries(battle_testing_rmlui PRIVATE ${SDL2_IMAGE_LIBRARIES})
    endif()
    set_target_properties(battle_testing_rmlui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Find packages
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)

# Source files
set(MAIN_APP_SOURCES
    src/main.cpp
    src/GameMenu/main_menu.cpp
    src/GameMenu/pause_menu.cpp
    src/GameMenu/exit_to_main_menu.cpp
    src/Settings/settings.cpp
    src/window.cpp
    src/game/vn/vn_system.cpp
    src/game/vn/vn_script.cpp
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
    src/game/render/battle_ui.cpp
    src/game/render/battle_ui.h
    src/game/core/easing.cpp
    src/game/core/easing.h
    src/game/render/camera_3d.cpp
    src/game/render/camera_3d.h
)

set(BATTLE_PRESENTATION_SOURCES
    src/game/presentation/ability_presentation.cpp
    src/game/presentation/ability_presentation.h
    src/game/presentation/miku_sing_presentation.cpp
    src/game/presentation/miku_sing_presentation.h
    src/game/presentation/miku_diandong_presentation.cpp
    src/game/presentation/miku_diandong_presentation.h
    src/game/presentation/cupcakke_drum_presentation.cpp
    src/game/presentation/cupcakke_drum_presentation.h
    src/game/presentation/presentation_registry.cpp
)

set(BATTLE_DEMO_SOURCES
    src/game/demo_battle_session.cpp
    src/game/demo_battle_session.h
)

set(HEADERS
    src/GameMenu/menu_shared.h
    src/Settings/settings.h
    src/window.h
    src/game/vn/vn_system.h
    src/game/vn/vn_script.h
)

# Create executable
add_executable(OurUndergroundBITIdol
    ${MAIN_APP_SOURCES}
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_DEMO_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
    ${HEADERS}
)

add_executable(battle_testing
    src/battle_main.cpp
    src/window.cpp
    src/window.h
    ${BATTLE_CORE_SOURCES}
    ${BATTLE_PRESENTATION_SOURCES}
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

# Include directories
target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(battle_testing PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo PRIVATE ${SDL2_INCLUDE_DIRS})
if (SDL2_TTF_FOUND)
    target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(OurUndergroundBITIdol PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_TTF=1)
    target_include_directories(demo PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
endif()
if (SDL2_IMAGE_FOUND)
    target_include_directories(OurUndergroundBITIdol PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(OurUndergroundBITIdol PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_IMAGE=1)
    target_include_directories(demo PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
endif()

# Link libraries
target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(battle_testing PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
if (SDL2_TTF_FOUND)
    target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(battle_testing PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_TTF_LIBRARIES})
endif()
if (SDL2_IMAGE_FOUND)
    target_link_libraries(OurUndergroundBITIdol PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(battle_testing PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

# Output directory
set_target_properties(OurUndergroundBITIdol PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(battle_testing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
