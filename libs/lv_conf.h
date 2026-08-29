/**
 * @file lv_conf.h
 * Configuration for eMP-gba (Allwinner T113-S3, 480x480 RGB565 framebuffer)
 * Based on LVGL v9.4.0
 */
#if 1

#ifndef LV_CONF_H
#define LV_CONF_H

/*==================== COLOR SETTINGS ====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*==================== STDLIB WRAPPER ====================*/
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING   LV_STDLIB_CLIB
#define LV_USE_STDLIB_SNPRINTF LV_STDLIB_CLIB
#define LV_USE_STDLIB_TIME     LV_STDLIB_CLIB
#define LV_USE_STDLIB_DATA_SIZE LV_STDLIB_CLIB

/*==================== MEMORY SETTINGS ====================*/
#define LV_USE_OS      LV_OS_NONE
#define LV_MEM_SIZE    (8 * 1024 * 1024)   /* T113 has plenty of RAM */
#define LV_MEM_BUF_MAX_NUM 128
#define LV_MEM_POOL_INCLUDE 0

/*==================== TICK & LOG ====================*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <sys/time.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (custom_tick_get())

#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   0

/*==================== DISPLAY ====================*/
/* GBA core runs at 60fps; the default 30ms refresh period caps the visible
 * frame rate at 33fps and drops half of the emulated frames. 16ms -> 60fps. */
#define LV_DISP_DEF_REFR_PERIOD 16

/*==================== DRAW ENGINE ====================*/
#define LV_USE_DRAW_SW      1
#define LV_DRAW_SW_COMPLEX  1   /* needed for object transform (canvas zoom) */

/*==================== DRIVERS (embedded Linux) ====================*/
#define LV_USE_LINUX_FBDEV 1
#define LV_USE_EVDEV       1
#define LV_USE_SDL         0

/*==================== FILESYSTEM ====================*/
/* This LVGL copy ships no built-in POSIX FS driver. We provide our own
 * (src/gba_port/lv_fs_posix.c, letter '/'), so the upstream driver is
 * disabled here to avoid a missing-symbol build. */
#define LV_USE_FS_POSIX 0

/*==================== WIDGETS ====================*/
#define LV_USE_WIDGETS 1
#define LV_USE_CANVAS  1
#define LV_USE_LIST    1
#define LV_USE_BTN     1
#define LV_USE_LABEL   1
#define LV_USE_OBJ     1

/*==================== THEME ====================*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_COLOR_PRIMARY    lv_palette_main(LV_PALETTE_BLUE)
    #define LV_THEME_DEFAULT_COLOR_SECONDARY  lv_palette_main(LV_PALETTE_CYAN)
    #define LV_THEME_DEFAULT_FONT_SMALL        LV_FONT_DEFAULT
    #define LV_THEME_DEFAULT_FONT_NORMAL       LV_FONT_DEFAULT
    #define LV_THEME_DEFAULT_FONT_LARGE        LV_FONT_DEFAULT
    #define LV_THEME_DEFAULT_FONT_TITLE        LV_FONT_DEFAULT
    #define LV_THEME_DEFAULT_BORDER_WIDTH      1
    #define LV_THEME_DEFAULT_RADIUS            LV_RADIUS_CIRCLE
#endif

/*==================== FONTS ====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

/* Runtime TTF (SmileySans for CJK): tiny TTF parser, no external dep. */
#define LV_USE_TINY_TTF        1
#define LV_TINY_TTF_FILE_SUPPORT 0
#define LV_TINY_TTF_CACHE_GLYPH_CNT 512
#define LV_TINY_TTF_CACHE_KERNING_CNT 256

/*==================== EXAMPLES / DEMOS (disabled) ====================*/
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0

/*==================== OTHERS ====================*/
#define LV_USE_OBSERVER 1
#define LV_USE_USER_DATA 1

#endif /* LV_CONF_H */

#endif /* set to 1 above */
