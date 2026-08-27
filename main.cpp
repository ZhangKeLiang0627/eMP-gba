/*
 * eMP-gba entry point.
 *
 * Architecture (per requirement 3) mirrors eMP-tokenMonitor:
 *   HAL::Init()  -> hardware (fbdev / evdev / FS)
 *   Page::Model  -> GBA lifecycle + LVGL thread (MVVM Model)
 *   Page::View   -> GBA widgets (MVVM View)
 * The main thread only initializes and then sleeps; the LVGL thread pumps
 * lv_timer_handler.
 */
#include "common_inc.h"
#include "HAL.h"
#include "Model.h"

static Page::Model * model;

static void exitCallback(void)
{
    LV_LOG_USER("[Sys] exit requested");
    exit(0);
}

int main(int argc, char * argv[])
{
    (void)argc;
    (void)argv;

    LV_LOG_USER("[Sys] eMP-gba starting ...");

    /* Hardware init: LVGL + POSIX FS + fbdev + evdev */
    HAL::Init();

    /* Build the UI + start the LVGL thread */
    model = new Page::Model(exitCallback);

    /* Main thread: idle. Everything else runs in the LVGL thread. */
    while (1) {
        usleep(10 * 1000 * 1000);
    }

    return 0;
}
