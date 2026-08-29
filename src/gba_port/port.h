/**
 * @file port.h
 *
 */

#ifndef PORT_H
#define PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

int lv_port_init(void);

void lv_port_sleep(uint32_t ms);

uint32_t lv_port_tick_get(void);

void gba_port_init(lv_obj_t* gba_emu);

int gba_audio_init(lv_obj_t* gba_emu);
void gba_audio_deinit(lv_obj_t* gba_emu);
void gba_audio_set_volume(int volume);
int  gba_audio_get_volume(void);

/* Grab the current framebuffer (visible page of /dev/fb0) and write it as a
 * 24-bit BMP. Returns 0 on success, -1 on error. */
int gba_screenshot_capture(const char* path);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PORT_H*/
