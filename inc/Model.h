#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>

#include "View.h"
#include "common_inc.h"

namespace Page
{
    /**
     * @brief Application model for eMP-gba.
     *        Owns the GBA lifecycle (menu -> select ROM -> emulate -> exit),
     *        the ROM directory / volume configuration, and the LVGL thread
     *        that pumps lv_timer_handler (which in turn drives the emulator
     *        timer created inside lv_gba_emu_create).
     *
     *        Mirrors the eMP-tokenMonitor MVVM split: HAL owns hardware,
     *        Model owns logic + the LVGL thread, View owns the widgets.
     */
    class Model
    {
    public:
        Model(std::function<void(void)> exitCb);
        ~Model();

    private:
        void threadLvglHandler(void);

        void launch(const std::string & romPath);
        void backToMenu(void);

        std::thread _threadLvgl;                 /* LVGL timer thread */
        std::atomic<bool> _exitFlag{false};      /* thread exit flag */
        std::function<void(void)> _exitCb;       /* full-app exit (signal etc.) */

        std::string _romDir;                     /* ROM directory (env EMP_GBA_ROM_DIR) */
        int _volume = 100;                       /* 0..100 (env EMP_GBA_VOLUME) */

        View _view;
    };
}
