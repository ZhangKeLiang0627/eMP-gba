#pragma once

#include "common_inc.h"
#include <string>
#include <functional>

namespace Page
{
    /* ROM picker callback: receives the chosen absolute ROM path. */
    using RomSelectedCb = std::function<void(const std::string &)>;
    /* Emulator exit callback (fired on SELECT long-press). */
    using ExitCb = std::function<void(void)>;

    /**
     * @brief Presentation layer for eMP-gba.
     *        Wraps the C-level gba_emu / gba_menu LVGL widgets. The Model
     *        owns the lifecycle and drives this View; the View never touches
     *        application state directly.
     */
    class View
    {
    public:
        /* Show the ROM picker; cb is called with the absolute rom path. */
        void showMenu(const std::string & romDir, RomSelectedCb cb);

        /* Create the emulator screen for a rom. Returns the gba_emu object
         * (NULL on failure). When SELECT is long-pressed, exitCb fires. */
        lv_obj_t * showEmu(const std::string & romPath, int volume, ExitCb exitCb);

        /* Stop the ALSA audio drain thread (safe to call before teardown). */
        void stopAudio(void);

        /* Clear the active screen. */
        void clearScreen(void);

    private:
        static void menuSelectBridge(const char * path, void * user_data);
        static void exitBridge(void * user_data);
        void invokeExit(void);

        RomSelectedCb _romCb;
        ExitCb _exitCb;
    };
}
