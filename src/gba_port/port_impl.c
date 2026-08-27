/*
 * Port implementation stubs for T113-S3.
 * Display/input are handled directly by HAL via LVGL's built-in
 * fbdev + evdev drivers, so the legacy lv_port_* hooks are no-ops here.
 * lv_port_sleep is required because the vba-next core macro-expands
 * retro_sleep -> lv_port_sleep.
 */
#include "lvgl/lvgl.h"
#include <unistd.h>

void lv_port_sleep(uint32_t ms)
{
    usleep(ms * 1000);
}

void lv_port_init(void)
{
    /* display/input initialized in HAL::Init() */
}

void gba_port_init(lv_obj_t* gba_emu)
{
    LV_UNUSED(gba_emu);
    /* nothing extra needed on T113 (touch via evdev, audio via ALSA) */
}

uint32_t lv_port_tick_get(void)
{
    return lv_tick_get();
}
