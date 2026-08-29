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
#define OVERLAY_VOL_BAR_W   144
#define OVERLAY_VOL_BAR_H   192

/* --------------------------------------------------------------------------
 * small helpers
 * ------------------------------------------------------------------------ */

static void gba_anim_obj(lv_obj_t* obj, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t time)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
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

    /* knob */
    lv_obj_set_style_border_width(obj, 3, LV_PART_KNOB);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xbbbbbb), LV_PART_KNOB);
    lv_obj_set_style_pad_all(obj, 1, LV_PART_KNOB);
    lv_obj_set_style_radius(obj, 10, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_60, LV_PART_KNOB | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x445588), LV_PART_KNOB);

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

static void top_bar_show(gba_overlay_t* ov)
{
    ov->top_visible = true;
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_y,
                 -OVERLAY_TOP_BAR_H, 0, 280);
}

static void top_bar_hide(gba_overlay_t* ov)
{
    ov->top_visible = false;
    gba_anim_obj(ov->top_bar, (lv_anim_exec_xcb_t)lv_obj_set_y,
                 0, -OVERLAY_TOP_BAR_H, 280);
}

static void vol_bar_show(gba_overlay_t* ov)
{
    ov->vol_visible = true;
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_x,
                 GBA_SCREEN_W, GBA_SCREEN_W - OVERLAY_VOL_BAR_W, 280);
}

static void vol_bar_hide(gba_overlay_t* ov)
{
    ov->vol_visible = false;
    gba_anim_obj(ov->vol_bar, (lv_anim_exec_xcb_t)lv_obj_set_x,
                 GBA_SCREEN_W - OVERLAY_VOL_BAR_W, GBA_SCREEN_W, 280);
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
        gba_screenshot_capture(NULL);
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

/* game-page swipe handler */
static void overlay_swipe_cb(lv_dir_t dir, void* user_data)
{
    gba_overlay_t* ov = (gba_overlay_t*)user_data;
    if (ov == NULL) return;

    /* While the volume bar is open, ignore SWIPE-DOWN so a slider drag
     * (also vertical) can not pop the top bar open. Swipe-UP still closes
     * the top bar when it is visible (harmless during a slider drag, since
     * the top bar is hidden then). Close the volume bar with a right-swipe. */
    switch (dir) {
    case LV_DIR_BOTTOM:
        if (!ov->top_visible && !ov->vol_visible) top_bar_show(ov);
        break;
    case LV_DIR_TOP:
        if (ov->top_visible) top_bar_hide(ov);
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
                   top_pinned ? 0 : -OVERLAY_TOP_BAR_H);
    lv_obj_set_style_bg_opa(bar, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_radius(bar, 5, 0);
    ov->top_bar = bar;
    ov->top_visible = top_pinned;

    lv_obj_t* b = overlay_btn_create(bar, "截图", lv_color_hex(0x0078BA), lv_color_hex(0x005E93), 64, 34);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_screenshot_btn = b;

    b = overlay_btn_create(bar, "音量", lv_color_hex(0x4EA35A), lv_color_hex(0x3D8346), 64, 34);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 84, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_volume_btn = b;

    b = overlay_btn_create(bar, "退出", lv_color_hex(0xFF6056), lv_color_hex(0xE44543), 64, 34);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(b, overlay_event_cb, LV_EVENT_CLICKED, ov);
    ov->top_exit_btn = b;

    /* ---- volume bar (eMP-video sliderContCreate look) ---- */
    lv_obj_t* vbar = lv_obj_create(root);
    lv_obj_remove_style_all(vbar);
    lv_obj_clear_flag(vbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(vbar, OVERLAY_VOL_BAR_W, OVERLAY_VOL_BAR_H);
    /* top-right (-20, 60) in absolute coords, off-screen right when hidden */
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
