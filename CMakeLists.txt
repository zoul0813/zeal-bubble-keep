cmake_minimum_required(VERSION 3.16)

set(target_name bubblekeep)
option(EMULATOR "Save hiscores to H:/ (HostFS) instead of B:/" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Where the .bin goes")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

zos_use(zvb)
include($ENV{ZGDK_PATH}/cmake/zgdk_init.cmake)

project(bubble_keep C ASM)

add_executable(${target_name}
    src/controls.c
    src/main.c
    src/room.c
    src/actors.c
    src/level.c
    src/hud.c
    src/sfx.c
    src/music.c
    src/assets.c
    src/autopilot.c
)

# Smaller code leaves more RAM for the stack; the frame rate is unchanged (tools/hfps.py)
target_compile_options(${target_name} PRIVATE --opt-code-size)


option(DEBUG_FPS "Show game loop frames per second on the bottom row" OFF)
if(DEBUG_FPS)
    target_compile_definitions(${target_name} PRIVATE DEBUG_FPS=1)
endif()
option(CHEATS "Q key clears the round" OFF)
if(CHEATS)
    target_compile_definitions(${target_name} PRIVATE CHEATS=1)
endif()
# For recording videos: the bot (src/autopilot.c, also the title's attract demo) plays every game, and the game can start on a later round
option(AUTOPILOT "Bot plays the game (video recording)" OFF)
if(AUTOPILOT)
    target_compile_definitions(${target_name} PRIVATE AUTOPILOT=1)
endif()
set(START_ROUND 0 CACHE STRING "First round, 0-based (video recording)")
target_compile_definitions(${target_name} PRIVATE START_ROUND=${START_ROUND})
if(EMULATOR)
    target_compile_definitions(${target_name} PRIVATE EMULATOR=1)
endif()

find_package(ZVB_SDK REQUIRED CONFIG PATHS "$ENV{ZVB_SDK_PATH}/cmake" NO_DEFAULT_PATH)
find_package(ZGDK REQUIRED CONFIG PATHS "$ENV{ZGDK_PATH}/cmake" NO_DEFAULT_PATH)
target_link_libraries(${target_name} PUBLIC zgdk zvb_gfx zvb_sound)

# Native map bytes and spawn metadata are generated from assets/**/*.tmx.
# Tileset GIF and music remain tracked assets; map exports stay in the build tree.
include(cmake/maps.cmake)
include(cmake/assets.cmake)

zos_add_outputs(${target_name})

set_property(TARGET ${target_name} APPEND PROPERTY LINK_DEPENDS "$ENV{ZGDK_PATH}/lib/zgdk.lib")
