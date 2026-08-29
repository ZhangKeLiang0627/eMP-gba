cmake_minimum_required(VERSION 3.10)
project(eMP_gba)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_C_STANDARD 99)
# vba-next core is old C++11 code; build everything at C++11 to avoid
# C++17 keyword churn in the core (GCC 6.4.1 musl on T113).
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Compiler / linker flags ----
# -O3: vba-next is a CPU-bound ARM7TDMI interpreter; O3 over O2 gives a
# measurable speedup for the emulation core on Cortex-A7.
# -g -funwind-tables -rdynamic -fno-omit-frame-pointer: keep symbols and the
# ARM frame-pointer chain so a crash pc/lr/fp can be resolved offline with
# addr2line (binary size grows, runtime unaffected).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g -funwind-tables -rdynamic -fno-omit-frame-pointer -Wall -Wno-unused-function -Wno-unused-variable -Wno-return-type -Wno-sign-compare -Wno-memset-transposed-args")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

# ---- Include paths ----
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_port
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/vba-next/libretro
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/vba-next/libretro-common/include
)

# ---- LVGL config ----
# Use a single lv_conf.h placed next to lvgl (LV_CONF_INCLUDE_SIMPLE)
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)
add_definitions(-DLV_USE_LINUX_FBDEV=1 -DLV_USE_EVDEV=1 -DLV_USE_SDL=0)
add_definitions(-DGBA_SCREEN_W=480 -DGBA_SCREEN_H=480)

# ---- vba-next (libretro GBA core) defines (from lv_gba_emu) ----
add_definitions(
    -DFRONTEND_SUPPORTS_RGB565=1
    -DTILED_RENDERING=1
    -DSTATIC_LINKING=1
    -DUSE_TWEAKS=1
    -DUSE_FRAME_SKIP=1
    -Dretro_sleep=lv_port_sleep
    -DNDEBUG
    -D__LIBRETRO__
    -DINLINE=inline
    -DHAVE_STDINT_H
    -DHAVE_INTTYPES_H
    -DHAVE_NEON=0
    -DGBA_FRAME_SKIP="0"
)

# ---- Sources: GBA core (C) ----
file(GLOB GBA_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gba_core/*.c"
)

# ---- Sources: platform port (T113 only) ----
# Only the T113 port files are compiled. The upstream lv_gba_emu port/ also
# ships SDL2 and Raspberry Pi ports which must NOT be built for T113.
set(GBA_PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_port/port_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_port/gba_port_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_port/gba_screenshot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gba_port/lv_fs_posix.c
)

# ---- Sources: our app (HAL / Model / View / main) ----
file(GLOB APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HAL/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Page/*.cpp"
)
set(MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# ---- Sources: vba-next ----
set(VBA_NEXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/vba-next)
file(GLOB VBA_NEXT_SOURCES
    ${VBA_NEXT_DIR}/src/*.c
    ${VBA_NEXT_DIR}/src/*.cpp
    ${VBA_NEXT_DIR}/libretro/libretro.cpp
    ${VBA_NEXT_DIR}/libretro-common/rthreads/rsemaphore.c
    ${VBA_NEXT_DIR}/libretro-common/rthreads/rthreads.c
    ${VBA_NEXT_DIR}/libretro-common/compat/*.c
    ${VBA_NEXT_DIR}/libretro-common/encodings/*.c
    ${VBA_NEXT_DIR}/libretro-common/file/*.c
    ${VBA_NEXT_DIR}/libretro-common/streams/*.c
    ${VBA_NEXT_DIR}/libretro-common/string/*.c
    ${VBA_NEXT_DIR}/libretro-common/time/*.c
)

# keep vba-next at C++11 to avoid C++17 keyword churn in the old core
set_source_files_properties(${VBA_NEXT_DIR}/src/gba.cpp ${VBA_NEXT_DIR}/src/memory.cpp
    PROPERTIES COMPILE_FLAGS "-std=c++11")

set(ALL_SRCS ${GBA_CORE_SOURCES} ${GBA_PORT_SOURCES} ${APP_SOURCES} ${MAIN_SRC} ${VBA_NEXT_SOURCES})

add_executable(${PROJECT_NAME} ${ALL_SRCS})

# ---- LVGL ----
# LVGL v9's cmake looks for lv_conf.h either at the top-level project dir or
# at an explicitly configured location. Ours lives in libs/lv_conf.h.
set(LV_BUILD_CONF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs CACHE PATH "Directory containing lv_conf.h")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/lvgl)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    lvgl
    pthread
    m
    asound
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
