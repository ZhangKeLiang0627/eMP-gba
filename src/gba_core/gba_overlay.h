/*
 * Page-independent gesture overlays for eMP-gba (T113-S3, 480x480).
 *
 * Provides the two floating controls used on BOTH the ROM menu and the
 * in-game screen:
 *   - top bar:  截图 / 音量 / 退出  buttons (slide in on swipe-down on the
 *                game page; pinned permanently visible on the menu page)
 *   - volume bar: vertical 0-100 slider styled after eMP-video's
 *                sliderContCreate() (swipe left to open on the game page;
 *                never opened by gesture on the menu page)
 *
 * The menu page pins the top bar and disables gestures entirely
 * (top_pinned = true, gesture_on = false); the game page hides both bars
 * until a swipe reveals them (gesture_on = true).
 *
 * Exit action is decoupled from the page: pass a gba_context_t* (game page,
 * uses ctx->exit_cb, registered later by the caller) OR a plain function
 * pointer (menu page, exits the app).
 */
#ifndef GBA_OVERLAY_H
#define GBA_OVERLAY_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gba_context_s gba_context_t;

typedef struct {
    lv_obj_t* root;                    /* page root the overlay is attached to */

    gba_context_t* gba_ctx;            /* game page: exit goes through ctx->exit_cb */
    void (*exit_fn)(void*);            /* menu page: direct exit callback */
    void* exit_ud;

    bool top_pinned;                   /* top bar permanently visible (menu) */
    bool gesture_on;                   /* swipes control the overlays (game) */

    lv_obj_t* top_bar;
    lv_obj_t* top_screenshot_btn;
    lv_obj_t* top_volume_btn;
    lv_obj_t* top_exit_btn;

    lv_obj_t* vol_bar;
    lv_obj_t* vol_slider;
    lv_obj_t* vol_icon;
    lv_obj_t* vol_label;

    bool top_visible;
    bool vol_visible;
} gba_overlay_t;

/* Create the overlays on `root`. Registers (or clears) the global swipe
 * callback according to gesture_on. Returns the overlay handle. */
gba_overlay_t* gba_overlay_create(lv_obj_t* root,
                                  gba_context_t* gba_ctx,
                                  void (*exit_fn)(void*), void* exit_ud,
                                  bool top_pinned, bool gesture_on);

#ifdef __cplusplus
}
#endif

#endif /* GBA_OVERLAY_H */
