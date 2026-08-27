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

#include <unistd.h>
#include <cstdio>

static Page::Model * model;

static void exitCallback(void)
{
    LV_LOG_USER("[Sys] exit requested");
    exit(0);
}

/* Debug: forward LVGL logs to stderr */
static void log_print_cb(lv_log_level_t level, const char * buf)
{
    LV_UNUSED(level);
    fprintf(stderr, "%s", buf);
    fflush(stderr);
}

int main(int argc, char * argv[])
{
    (void)argc;
    (void)argv;

    fprintf(stderr, "[main] eMP-gba starting ...\n");
    fflush(stderr);

#if LV_USE_LOG
    lv_log_register_print_cb(log_print_cb);
#endif

    /* Hardware init: LVGL + POSIX FS + fbdev + evdev */
    HAL::Init();
    fprintf(stderr, "[main] HAL::Init done\n");

    /* Build the UI + start the LVGL thread */
    model = new Page::Model(exitCallback);
    fprintf(stderr, "[main] Model created\n");

    /* Main thread: idle. Everything else runs in the LVGL thread. */
    while (1) {
        usleep(10 * 1000 * 1000);
    }

    return 0;
}
