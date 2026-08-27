#pragma once

#include "common_inc.h"

namespace HAL
{
    /**
     * @brief Hardware abstraction layer init for T113-S3 (TinaLinux).
     *        Initializes LVGL, the custom POSIX FS driver, the Linux
     *        framebuffer display (/dev/fb0) and the evdev touchscreen
     *        (/dev/input/event1), plus the signal handler.
     */
    void Init(void);
}
