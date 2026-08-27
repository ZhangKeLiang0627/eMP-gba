#pragma once

#include "lvgl/lvgl.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/* LVGL tick source for embedded Linux (gettimeofday based).
 * Referenced by LV_TICK_CUSTOM_SYS_TIME_EXPR in lv_conf.h. */
extern "C" uint32_t custom_tick_get(void);

/* Port stubs (gba core expects these symbols). */
extern "C" void lv_port_sleep(uint32_t ms);
extern "C" void lv_port_init(void);
extern "C" void gba_port_init(lv_obj_t* gba_emu);
extern "C" uint32_t lv_port_tick_get(void);

/* POSIX file system driver init (registers LVGL FS with letter '/'). */
extern "C" void lv_fs_posix_init(void);
