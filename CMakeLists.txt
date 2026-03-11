cmake_minimum_required(VERSION 3.16)
project(vn_testing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)

# Source files
set(SOURCES
    src/main.cpp
    src/main_menu.cpp
    src/pause_menu.cpp
    src/settings.cpp
    src/window.cpp
    src/game/vn_system.cpp
    src/game/vn_script.cpp
)

set(HEADERS
    src/menu_shared.h
    src/settings.h
    src/window.h
    src/game/vn_system.h
    src/game/vn_script.h
)

# Create executable
add_executable(vn_testing ${SOURCES} ${HEADERS})

add_executable(battle_testing
    src/battle_main.cpp
    src/window.cpp
    src/window.h
    src/game/battle_manager.cpp
    src/game/battle_manager.h
    src/game/battle_loader.cpp
    src/game/battle_loader.h
    src/game/turn_system.cpp
    src/game/turn_system.h
    src/game/ability_system.cpp
    src/game/ability_system.h
    src/game/battle_ui.cpp
    src/game/battle_ui.h
    src/game/easing.cpp
    src/game/easing.h
    src/game/camera_3d.cpp
    src/game/camera_3d.h
)

add_executable(demo
    src/demo.cpp
    src/window.cpp
    src/window.h
    src/game/battle_manager.cpp
    src/game/battle_manager.h
    src/game/battle_loader.cpp
    src/game/battle_loader.h
    src/game/turn_system.cpp
    src/game/turn_system.h
    src/game/ability_system.cpp
    src/game/ability_system.h
    src/game/battle_ui.cpp
    src/game/battle_ui.h
    src/game/easing.cpp
    src/game/easing.h
    src/game/camera_3d.cpp
    src/game/camera_3d.h
    src/game/vn_system.cpp
    src/game/vn_system.h
    src/game/vn_script.cpp
    src/game/vn_script.h
)

# Include directories
target_include_directories(vn_testing PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(battle_testing PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(demo PRIVATE ${SDL2_INCLUDE_DIRS})
if (SDL2_TTF_FOUND)
    target_include_directories(vn_testing PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(vn_testing PRIVATE VN_ENABLE_TTF=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_TTF=1)
    target_include_directories(demo PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_TTF=1 VN_ENABLE_TTF=1)
endif()
if (SDL2_IMAGE_FOUND)
    target_include_directories(vn_testing PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(vn_testing PRIVATE VN_ENABLE_IMAGE=1)
    target_include_directories(battle_testing PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(battle_testing PRIVATE BATTLE_ENABLE_IMAGE=1)
    target_include_directories(demo PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
    target_compile_definitions(demo PRIVATE BATTLE_ENABLE_IMAGE=1 VN_ENABLE_IMAGE=1)
endif()

# Link libraries
target_link_libraries(vn_testing PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(battle_testing PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
target_link_libraries(demo PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json)
if (SDL2_TTF_FOUND)
    target_link_libraries(vn_testing PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(battle_testing PRIVATE ${SDL2_TTF_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_TTF_LIBRARIES})
endif()
if (SDL2_IMAGE_FOUND)
    target_link_libraries(vn_testing PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(battle_testing PRIVATE ${SDL2_IMAGE_LIBRARIES})
    target_link_libraries(demo PRIVATE ${SDL2_IMAGE_LIBRARIES})
endif()

# Output directory
set_target_properties(vn_testing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(battle_testing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
