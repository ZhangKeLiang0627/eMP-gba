/*
 * Page-independent gesture overlays for eMP-gba (see gba_overlay.h).
 *
 * Volume bar styling mirrors eMP-video's sliderContCreate()/sliderCreate():
 *   container 30% x 40% of the screen, bg #eeeeee @90% opa, radius 10,
 *   aligned top-right (-20, 60); vertical slider with
 *     KNOB:       border 3 #bbbbbb, radius 10, bg #445588 (60% when pressed)
 *     MAIN:       radius 8, bg #3c9ba6
 *     INDICATOR:  radius 8, bg #a4d9b2
 *   plus the LV_SYMBOL_VOLUME_MAX icon inside the slider.
 *
 * Top bar mirrors eMP-video's topContCreate(): 90% width, ~8% height,
 * radius 5, bg #eeeeee @90% opa. Buttons stay 截图/音量/退出.
 */
#include "gba_overlay.h"
#include "gba_emu.h"
#include "gba_font.h"
#include "gba_internal.h"
#include "port.h"
#include <stdio.h>

#define OVERLAY_TOP_BAR_H   38
#define OVERLAY_VOL_BAR_W   124   /* 144 - 20: narrower body */
#define OVERLAY_VOL_BAR_H   192
#define OVERLAY_VOL_BAR_Y   60
#define OVERLAY_VOL_BAR_X   (GBA_SCREEN_W - OVERLAY_VOL_BAR_W - 10)  /* 10px off the right edge */
#define OVERLAY_ANIM_TIME   400   /* eMP-video-style: 400ms ease-out */

/* --------------------------------------------------------------------------
 * small helpers
 * ------------------------------------------------------------------------ */

static void gba_anim_obj_path(lv_obj_t* obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to,
                              uint32_t time, lv_anim_path_cb_t path)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

static void gba_anim_obj(lv_obj_t* obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t time)
{
    gba_anim_obj_path(obj, exec, from, to, time, lv_anim_path_ease_out);
}

static lv_obj_t* overlay_btn_create(lv_obj_t* parent, const char* text,
                                    lv_color_t bg, lv_color_t pressed, int w, int h)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_color(btn, pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_ext_click_area(btn, 8);

    lv_obj_t* label = lv_label_create(btn);
    lv_font_t* f = gba_font_get(18);
    if (f) lv_obj_set_style_text_font(label, f, 0);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return btn;
}

/* eMP-video sliderCreate(): the exact knob/main/indicator styling */
static lv_obj_t* vol_slider_create(lv_obj_t* parent)
{
    lv_obj_t* obj = lv_slider_create(parent);
    lv_obj_remove_style_all(obj);
    lv_slider_set_mode(obj, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_range(obj, 0, 100);
    lv_slider_set_value(obj, gba_audio_get_volume(), LV_ANIM_OFF);

    lv_obj_set_size(obj, lv_pct(40), lv_pct(90));
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, -6);

    /* knob: transparent (like eMP-video's brightnessSlider) - drag directly
     * on the track, no visible thumb */
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 0, LV_PART_KNOB);
    lv_obj_set_style_radius(obj, 10, LV_PART_KNOB);
    lv_obj_set_style_pad_all(obj, 1, LV_PART_KNOB);

    /* main track */
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x3c9ba6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    /* indicator */
    lv_obj_set_style_radius(obj, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xa4d9b2), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);

    /* volume icon inside the slider (mirrors eMP-video) */
    lv_obj_t* img = lv_img_create(obj);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 10, 0);
    lv_img_set_src(img, LV_SYMBOL_VOLUME_MAX);
    return obj;
}

/* --------------------------------------------------------------------------
 * show / hide
 * ------------------------------------------------------------------------ */

/* eMP-video style enter/exit: ease-out with an "expand" effect - the top
 * bar slides down while growing from 20px wide, the volume bar slides in
 * while growing from 30px tall. The top bar grows/shrinks AROUND ITS
 * CENTER (x animates together with width so the bar never slides to one
 * side). */
static void top_bar_show(gba_overlay_t* ov)
{
    ov->top_visible = true;
    const int w = GBA_SCREEN_W * 9 / 10;
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_y, -40, 0, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_width, 20, w, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_x,
                 (GBA_SCREEN_W - 20) / 2, (GBA_SCREEN_W - w) / 2, OVERLAY_ANIM_TIME);
}

static void top_bar_hide(gba_overlay_t* ov)
{
    ov->top_visible = false;
    const int w = GBA_SCREEN_W * 9 / 10;
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_y, 0, -40, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_width, w, 20, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_x,
                 (GBA_SCREEN_W - w) / 2, (GBA_SCREEN_W - 20) / 2, OVERLAY_ANIM_TIME);
}

static void vol_bar_show(gba_overlay_t* ov)
{
    ov->vol_visible = true;
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_x, GBA_SCREEN_W, OVERLAY_VOL_BAR_X, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_height, 30, OVERLAY_VOL_BAR_H, OVERLAY_ANIM_TIME);
}

static void vol_bar_hide(gba_overlay_t* ov)
{
    ov->vol_visible = false;
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_x, OVERLAY_VOL_BAR_X, GBA_SCREEN_W, OVERLAY_ANIM_TIME);
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_height, OVERLAY_VOL_BAR_H, 30, OVERLAY_ANIM_TIME);
}

/* --------------------------------------------------------------------------
 * toast (side tip popup)
 * Full animation, as requested:
 *   1) slide in from OFF-SCREEN RIGHT at mid height (y = 480/2 = 240),
 *      700ms, stopping with the right ~1/8 of the bar (11px) still clipped
 *      outside the screen (the bar is 90px wide -> rests at x = 480-90+11);
 *   2) wait 1.5s;
 *   3) drop towards the bottom-left while fading out (~500ms), then delete.
 * LVGL v9.4 has no lv_anim_move / lv_anim_drop_out (v8 APIs), so the same
 * motion is expressed with absolute-position + opacity lv_anim animations.
 * ------------------------------------------------------------------------ */

#define TOAST_W       90
#define TOAST_H       40
#define TOAST_X_REST  (GBA_SCREEN_W - TOAST_W + 11)   /* right 11px (~1/8) stays off-screen */
#define TOAST_Y       (GBA_SCREEN_H / 2)              /* slide in at mid height */

static void toast_set_opa(void* obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

static void toast_drop_ready(lv_anim_t* a)
{
    lv_obj_t* pop = (lv_obj_t*)a->var;
    lv_obj_del(pop);
}

static void toast_timer_cb(lv_timer_t* t)
{
    lv_obj_t* pop = (lv_obj_t*)lv_timer_get_user_data(t);
    if (pop == NULL) return;

    /* projectile-like drop: X moves at constant speed (linear), Y falls
     * with acceleration (ease-in) - a parabola, not a straight 45 deg line.
     * Fade out simultaneously, then delete. */
    const int x = lv_obj_get_x(pop);
    const int y = lv_obj_get_y(pop);
    gba_anim_obj_path(pop, (lv_anim_exec_xcb_t)lv_obj_set_x, x, x - 30, 500, lv_anim_path_linear);
    gba_anim_obj_path(pop, (lv_anim_exec_xcb_t)lv_obj_set_y, y, y + 100, 500, lv_anim_path_ease_in);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pop);
    lv_anim_set_exec_cb(&a, toast_set_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 500);
    lv_anim_set_ready_cb(&a, toast_drop_ready);
    lv_anim_start(&a);
}

static void overlay_toast_create(const char* tips)
{
    lv_obj_t* pop = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(pop);
    lv_obj_clear_flag(pop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pop, TOAST_W, TOAST_H);
    lv_obj_set_style_bg_opa(pop, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(pop, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_radius(pop, 10, 0);
    /* 1) slide in from off-screen right at mid height */
    lv_obj_set_pos(pop, GBA_SCREEN_W, TOAST_Y);
    gba_anim_obj(pop, (lv_anim_exec_xcb_t)lv_obj_set_x, GBA_SCREEN_W, TOAST_X_REST, 700);

    lv_obj_t* label = lv_label_create(pop);
    lv_font_t* f = gba_font_get(16);
    if (f) lv_obj_set_style_text_font(label, f, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_label_set_text(label, tips);

    /* 2) wait 1.5s, then drop + fade out */
    lv_timer_t* t = lv_timer_create(toast_timer_cb, 1500, pop);
    lv_timer_set_repeat_count(t, 1);
}

/* --------------------------------------------------------------------------
 * events
 * ------------------------------------------------------------------------ */

static void overlay_event_cb(lv_event_t* e)
{
    gba_overlay_t* ov = (gba_overlay_t*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_current_target(e);
    if (ov == NULL) return;

    if (btn == ov->top_screenshot_btn) {
        /* the full save path+name is printed by gba_screenshot_capture() */
        gba_screenshot_capture(NULL);
        overlay_toast_create("截图成功");
    }
    else if (btn == ov->top_volume_btn) {
        if (ov->vol_visible)
            vol_bar_hide(ov);
        else
            vol_bar_show(ov);
    }
    else if (btn == ov->top_exit_btn) {
        if (ov->gba_ctx != NULL && ov->gba_ctx->exit_cb != NULL) {
            ov->gba_ctx->exit_cb(ov->gba_ctx->exit_cb_user_data);
        }
        else if (ov->exit_fn != NULL) {
            ov->exit_fn(ov->exit_ud);
        }
    }
}

static void vol_slider_event_cb(lv_event_t* e)
{
    gba_overlay_t* ov = (gba_overlay_t*)lv_event_get_user_data(e);
    lv_obj_t* slider = lv_event_get_current_target(e);
    if (ov == NULL) return;

    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int v = (int)lv_slider_get_value(slider);
        gba_audio_set_volume(v);
        lv_label_set_text_fmt(ov->vol_label, "音量 %d", v);
    }
}

/* game-page swipe handler.
 * The swipe callback carries the press-start point: a gesture that begins
 * INSIDE the open volume bar is a slider drag (vertical), so it must not
 * flip the top bar; gestures starting anywhere else behave normally. */
static void overlay_swipe_cb(lv_dir_t dir, int start_x, int start_y, void* user_data)
{
    gba_overlay_t* ov = (gba_overlay_t*)user_data;
    if (ov == NULL) return;

    bool in_vol = (ov->vol_visible &&
                   start_x >= OVERLAY_VOL_BAR_X && start_x < OVERLAY_VOL_BAR_X + OVERLAY_VOL_BAR_W &&
                   start_y >= OVERLAY_VOL_BAR_Y && start_y < OVERLAY_VOL_BAR_Y + OVERLAY_VOL_BAR_H);

    switch (dir) {
    case LV_DIR_BOTTOM:
        /* top bar can be pulled out even while the volume bar is open,
         * unless the gesture started on the volume slider itself */
        if (!ov->top_visible && !in_vol) top_bar_show(ov);
        break;
    case LV_DIR_TOP:
        if (ov->top_visible && !in_vol) top_bar_hide(ov);
        break;
    case LV_DIR_LEFT:
        if (!ov->vol_visible) vol_bar_show(ov);
        break;
    case LV_DIR_RIGHT:
        if (ov->vol_visible) vol_bar_hide(ov);
        break;
    default:
        break;
    }
}

/* free the overlay struct when its root is deleted */
static void overlay_delete_cb(lv_event_t* e)
{
    gba_overlay_t* ov = (gba_overlay_t*)lv_event_get_user_data(e);
    lv_free(ov);
}

/* --------------------------------------------------------------------------
 * create
 * ------------------------------------------------------------------------ */

gba_overlay_t* gba_overlay_create(lv_obj_t* root,
                                  gba_context_t* gba_ctx,
                                  void (*exit_fn)(void*), void* exit_ud,
                                  bool top_pinned, bool gesture_on)
{
    gba_overlay_t* ov = (gba_overlay_t*)lv_malloc(sizeof(gba_overlay_t));
    LV_ASSERT_MALLOC(ov);
    lv_memzero(ov, sizeof(gba_overlay_t));
    ov->root = root;
    ov->gba_ctx = gba_ctx;
    ov->exit_fn = exit_fn;
    ov->exit_ud = exit_ud;
    ov->top_pinned = top_pinned;
    ov->gesture_on = gesture_on;

    /* ---- top bar (eMP-video topContCreate look: 90% width, radius 5) ---- */
    lv_obj_t* bar = lv_obj_create(root);
    lv_obj_remove_style_all(bar);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar, GBA_SCREEN_W * 9 / 10, OVERLAY_TOP_BAR_H);
    /* absolute position (no alignment: set_x/y on an aligned object is a
     * relative offset which confused the hide/show animation) */
    lv_obj_set_pos(bar, (GBA_SCREEN_W - GBA_SCREEN_W * 9 / 10) / 2,
                   top_pinned ? 0 : -40);
    lv_obj_set_style_bg_opa(bar, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_radius(bar, 5, 0);
    ov->top_bar = bar;
    ov->top_visible = top_pinned;

    lv_obj_t* b = overlay_btn_create(bar, "截图", lv_color_hex(0x0078BA), lv_color_hex(0x005E93), 54, 34);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_screenshot_btn = b;

    b = overlay_btn_create(bar, "音量", lv_color_hex(0x4EA35A), lv_color_hex(0x3D8346), 54, 34);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 74, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_volume_btn = b;

    b = overlay_btn_create(bar, "x", lv_color_hex(0xFF6056), lv_color_hex(0xE44543), 34, 34);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_exit_btn = b;

    /* centered title */
    lv_obj_t* title = lv_label_create(bar);
    lv_font_t* tf = gba_font_get(20);
    if (tf) lv_obj_set_style_text_font(title, tf, 0);
    lv_label_set_text(title, "GAME BOY");
    lv_obj_set_style_text_color(title, lv_color_hex(0x555555), 0);
    lv_obj_center(title);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    /* ---- volume bar (eMP-video sliderContCreate look) ---- */
    lv_obj_t* vbar = lv_obj_create(root);
    lv_obj_remove_style_all(vbar);
    lv_obj_clear_flag(vbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(vbar, OVERLAY_VOL_BAR_W, OVERLAY_VOL_BAR_H);
    /* absolute coords: visible at OVERLAY_VOL_BAR_X (=10px off the right
     * edge, y=60), off-screen right (x=GBA_SCREEN_W) when hidden */
    lv_obj_set_pos(vbar, GBA_SCREEN_W, 60);
    lv_obj_set_style_bg_opa(vbar, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(vbar, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_radius(vbar, 10, 0);
    ov->vol_bar = vbar;
    ov->vol_visible = false;

    lv_obj_t* slider = vol_slider_create(vbar);
    lv_obj_add_event_cb(slider, vol_slider_event_cb, LV_EVENT_VALUE_CHANGED, ov);
    ov->vol_slider = slider;

    lv_obj_t* lab = lv_label_create(vbar);
    lv_font_t* f = gba_font_get(16);
    if (f) lv_obj_set_style_text_font(lab, f, 0);
    lv_label_set_text_fmt(lab, "音量 %d", (int)gba_audio_get_volume());
    lv_obj_align(lab, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_color(lab, lv_color_hex(0x666666), 0);
    ov->vol_label = lab;

    /* gesture wiring: the global swipe callback is owned by the input
     * driver; the game page owns it, the menu page must CLEAR it so no
     * stale (freed) context is ever called after exiting a game. */
    if (gesture_on) {
        lv_gba_emu_set_swipe_cb(overlay_swipe_cb, ov);
    }
    else {
        lv_gba_emu_set_swipe_cb(NULL, NULL);
    }

    lv_obj_add_event_cb(root, overlay_delete_cb, LV_EVENT_DELETE, ov);
    return ov;
}
