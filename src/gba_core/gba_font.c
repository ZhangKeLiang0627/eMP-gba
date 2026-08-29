/*
 * Runtime font manager for eMP-gba.
 *
 * Loads SmileySans.ttf (CJK-capable) once into RAM and creates LVGL fonts
 * on demand, cached by pixel size so a size is only ever created once and
 * reused afterwards (same idea as CardputerZero/Template's AssetManager
 * load_font: create when used, cache, reuse). Fonts that are never used are
 * never created.
 *
 * Uses LVGL's tiny TTF (LV_USE_TINY_TTF, stb_truetype based) so no external
 * FreeType library is needed on the target.
 */
#include "gba_font.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

#define GBA_FONT_CACHE_MAX 8

/* Search order: the board's eMP-video font location first, then common spots. */
static const char* const g_font_paths[] = {
    "/mnt/UDISK/font/SmileySans.ttf",
    "/root/fonts/SmileySans.ttf",
    "/usr/share/fonts/SmileySans.ttf",
};

static unsigned char* g_ttf_data = NULL;
static size_t g_ttf_size = 0;
static bool g_ttf_tried = false;

static lv_font_t* g_font_cache[GBA_FONT_CACHE_MAX];
static int g_font_size[GBA_FONT_CACHE_MAX];
static int g_font_count = 0;

static bool gba_font_load_ttf(void)
{
    if(g_ttf_data || g_ttf_tried) return g_ttf_data != NULL;
    g_ttf_tried = true;

    for(size_t i = 0; i < sizeof(g_font_paths) / sizeof(g_font_paths[0]); i++) {
        FILE* f = fopen(g_font_paths[i], "rb");
        if(f == NULL) continue;

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if(sz <= 0) {
            fclose(f);
            continue;
        }

        unsigned char* buf = (unsigned char*)malloc((size_t)sz);
        if(buf == NULL) {
            fclose(f);
            continue;
        }
        if(fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
            free(buf);
            fclose(f);
            continue;
        }
        fclose(f);

        g_ttf_data = buf;
        g_ttf_size = (size_t)sz;
        LV_LOG_USER("SmileySans.ttf loaded (%zu bytes) from %s", g_ttf_size, g_font_paths[i]);
        return true;
    }

    LV_LOG_WARN("SmileySans.ttf not found (tried %d paths)", (int)(sizeof(g_font_paths) / sizeof(g_font_paths[0])));
    return false;
}

lv_font_t* gba_font_get(int size)
{
    if(!gba_font_load_ttf()) return NULL;

    /* cached? */
    for(int i = 0; i < g_font_count; i++) {
        if(g_font_size[i] == size) return g_font_cache[i];
    }
    if(g_font_count >= GBA_FONT_CACHE_MAX) return NULL;

    lv_font_t* font = lv_tiny_ttf_create_data(g_ttf_data, g_ttf_size, size);
    if(font == NULL) {
        LV_LOG_WARN("tiny_ttf create(size=%d) failed", size);
        return NULL;
    }

    g_font_cache[g_font_count] = font;
    g_font_size[g_font_count] = size;
    g_font_count++;
    return font;
}
