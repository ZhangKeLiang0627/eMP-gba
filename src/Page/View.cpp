/*
 * View for eMP-gba: thin C++ wrapper over the lv_gba_emu / lv_gba_menu widgets.
 */
#include "View.h"

extern "C" {
#include "lvgl/lvgl.h"
#include "gba_emu.h"
#include "gba_menu.h"
#include "port.h"
}

using namespace Page;

void View::showMenu(const std::string & romDir, RomSelectedCb cb)
{
    _romCb = cb;
    gba_menu_create(lv_scr_act(), romDir.c_str(), menuSelectBridge, this);
}

void View::menuSelectBridge(const char * path, void * user_data)
{
    View * self = (View *)user_data;
    if (self && self->_romCb)
        self->_romCb(std::string(path));
}

lv_obj_t * View::showEmu(const std::string & romPath, int volume, ExitCb exitCb)
{
    _exitCb = exitCb;

    lv_obj_t * gba_emu = lv_gba_emu_create(lv_scr_act(), romPath.c_str(),
                                          LV_GBA_VIEW_MODE_VIRTUAL_KEYPAD);
    if (!gba_emu)
        return nullptr;

    lv_gba_emu_set_on_exit_cb(gba_emu, exitBridge, this);
    gba_port_init(gba_emu);

    /* Requirement 2: ALSA audio is already supported by the port; wire it up
     * when the volume is > 0 (default 100). */
    if (volume > 0) {
        if (gba_audio_init(gba_emu) < 0) {
            LV_LOG_WARN("ALSA audio init failed (continuing without sound)");
        }
    }

    return gba_emu;
}

void View::stopAudio(void)
{
    gba_audio_deinit(NULL);
}

void View::clearScreen(void)
{
    lv_obj_clean(lv_scr_act());
}

void View::exitBridge(void * user_data)
{
    View * self = (View *)user_data;
    /* Schedule the return-to-menu as an async task so we do not delete the
     * running emulator timer reentrantly from inside its own callback. */
    lv_async_call([](void * p) {
        ((View *)p)->invokeExit();
    }, self);
}

void View::invokeExit(void)
{
    if (_exitCb)
        _exitCb();
}
