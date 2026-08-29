#ifndef GBA_FONT_H
#define GBA_FONT_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the SmileySans font at the given pixel size.
 *        The TTF is loaded once (first call) and fonts are cached by size,
 *        so a size is created only when first used and reused afterwards.
 * @return font pointer, or NULL if the TTF is unavailable (caller falls back
 *         to the default font).
 */
lv_font_t* gba_font_get(int size);

#ifdef __cplusplus
}
#endif

#endif /* GBA_FONT_H */
