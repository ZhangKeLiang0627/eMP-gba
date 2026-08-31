/*
 * Model for eMP-gba: GBA lifecycle + LVGL thread.
 */
#include "Model.h"

#include <chrono>
#include <cstdlib>

extern "C" {
#include "lvgl/lvgl.h"
}

using namespace Page;

Model::Model(std::function<void(void)> exitCb)
{
    _exitFlag = false;
    _exitCb = exitCb;

    const char * dir = getenv("EMP_GBA_ROM_DIR");
    _romDir = (dir != nullptr) ? dir : "/mnt/UDISK/roms";

    const char * vol = getenv("EMP_GBA_VOLUME");
    _volume = (vol != nullptr) ? atoi(vol) : 100;

    LV_LOG_USER("[Model] ROM dir = %s, volume = %d", _romDir.c_str(), _volume);

    /* Show the ROM picker (runs in the main thread before the LVGL thread
     * is spawned, so no concurrent LVGL access yet). */
    auto selectCb = [this](const std::string & path) { this->launch(path); };

    /* Optional auto-launch: EMP_GBA_AUTOSTART=/path/to/rom.gba */
    const char * autoRom = getenv("EMP_GBA_AUTOSTART");
    if (autoRom != nullptr && autoRom[0] != '\0') {
        LV_LOG_USER("[Model] autostart ROM: %s", autoRom);
        launch(autoRom);
    }
    else {
        _view.showMenu(_romDir, selectCb, _exitCb);
    }

    /* Spawn the LVGL thread that drives lv_timer_handler (and thus the
     * emulator timer + animations). */
    _threadLvgl = std::thread([](Model * p) { p->threadLvglHandler(); }, this);
    _threadLvgl.detach();
}

Model::~Model()
{
    _exitFlag = true;
    if (_threadLvgl.joinable())
        _threadLvgl.join();
}

void Model::launch(const std::string & romPath)
{
    _view.clearScreen();

    auto exitCb = [this]() { this->backToMenu(); };
    lv_obj_t * emu = _view.showEmu(romPath, _volume, exitCb);

    if (emu == nullptr) {
        LV_LOG_WARN("[Model] create gba emu failed for %s", romPath.c_str());
        /* ROM load failed -> back to the picker */
        _view.showMenu(_romDir, [this](const std::string & path) { this->launch(path); }, _exitCb);
    }
}

void Model::backToMenu(void)
{
    /* Runs as an LVGL async task (see View::exitBridge). */
    _view.stopAudio();
    _view.clearScreen();
    _view.showMenu(_romDir, [this](const std::string & path) { this->launch(path); }, _exitCb);
}

void Model::threadLvglHandler(void)
{
    while (!_exitFlag) {
        uint32_t ms = lv_timer_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    LV_LOG_USER("[Model] LVGL thread exit");
}
