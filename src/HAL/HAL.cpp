/*
 * HAL for eMP-gba (Allwinner T113-S3, 480x480 RGB565 framebuffer).
 *
 * Display + input are driven by LVGL's built-in Linux fbdev + evdev drivers
 * (validated on this board), so no sunxifb/lv_drivers dependency is needed.
 */
#include "HAL.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <sys/time.h>

extern "C" {
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/fb/lv_linux_fbdev.h"
#include "lvgl/src/drivers/evdev/lv_evdev.h"
void lv_fs_posix_init(void);
}

void signalExitCallback(int signal);
void install_signal_handler(void);
uint32_t custom_tick_get(void);

void HAL::Init(void)
{
    /* LittlevGL init */
    lv_init();

    /* Register the custom POSIX file system driver (letter '/') */
    lv_fs_posix_init();

    /* Linux frame buffer device init (480x480 RGB565) */
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");
    lv_linux_fbdev_set_force_refresh(disp, true);

    /* Touchscreen via evdev */
    lv_indev_t * indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event1");

    /* Register exit signal handler */
    install_signal_handler();
}

/**
 * @brief System exit callback.
 *        An atomic_flag guarantees the teardown runs only once, since a signal
 *        can be delivered to any thread and concurrent exit(0) calls would
 *        double-free glibc atexit handlers (random crash).
 */
void signalExitCallback(int signal)
{
    static std::atomic_flag exitFlag = ATOMIC_FLAG_INIT;
    if (exitFlag.test_and_set())
        return;

    LV_LOG_USER("[HAL] Got signal %d, exiting ...", signal);
    exit(0);
}

void install_signal_handler(void)
{
    signal(SIGBUS,  signalExitCallback);
    signal(SIGFPE,  signalExitCallback);
    signal(SIGHUP,  signalExitCallback);
    signal(SIGILL,  signalExitCallback);
    signal(SIGINT,  signalExitCallback);
    signal(SIGIOT,  signalExitCallback);
    signal(SIGPIPE, signalExitCallback);
    signal(SIGQUIT, signalExitCallback);
    signal(SIGSEGV, signalExitCallback);
    signal(SIGSYS,  signalExitCallback);
    signal(SIGTERM, signalExitCallback);
    signal(SIGTRAP, signalExitCallback);
    signal(SIGUSR1, signalExitCallback);
    signal(SIGUSR2, signalExitCallback);
}

/* Set in lv_conf.h as LV_TICK_CUSTOM_SYS_TIME_EXPR */
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = ((uint64_t)tv_start.tv_sec * 1000000 + (uint64_t)tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms = ((uint64_t)tv_now.tv_sec * 1000000 + (uint64_t)tv_now.tv_usec) / 1000;

    return (uint32_t)(now_ms - start_ms);
}
