/*
 * GBA view for eMP-gba (T113-S3, 480x480 screen).
 *
 * Layout (requirement 1):
 *   - GBA native resolution is 240x160. It is scaled 2x to 480x320 and
 *     shown top-center.
 *   - The remaining 480x160 strip at the bottom holds the on-screen GBA
 *     controls: D-pad left-center, A/B/L/R right-center, START/SELECT
 *     stacked top/bottom in the center.
 * Virtual keys are plain lv_obj (not lv_btn): no pressed-scale animation,
 * bg only changes on LV_STATE_PRESSED (FOCUSED keeps the default), and the
 * label is white so it stays readable on both the default and the pressed
 * background. The touch target is enlarged with lv_obj_set_ext_click_area.
 */
#include "gba_emu.h"
#include <stdio.h>
#include "gba_internal.h"

struct gba_view_s {
    lv_obj_t* root;

    struct {
        lv_obj_t* canvas;
        lv_draw_buf_t draw_buf;
        lv_color_t* scaled;  /* 480x320 RGB565 buffer, integer 2x of the 240x160 GBA frame */
    } screen;

    /* bottom 480x160 button container */
    lv_obj_t* btn_area;

    struct {
        struct {
            lv_obj_t* cont;
            lv_obj_t* up;
            lv_obj_t* down;
            lv_obj_t* left;
            lv_obj_t* right;
        } dir;

        struct {
            lv_obj_t* cont;
            lv_obj_t* A;
            lv_obj_t* B;
            lv_obj_t* L;
            lv_obj_t* R;
        } func;

        struct {
            lv_obj_t* cont;
            lv_obj_t* start;
            lv_obj_t* select;
        } ctrl;
    } btn;
};

typedef struct {
    const char* txt;
    lv_align_t align;
} btn_map_t;

static const btn_map_t btn_dir_map[] = {
    { LV_SYMBOL_UP, LV_ALIGN_TOP_MID },
    { LV_SYMBOL_DOWN, LV_ALIGN_BOTTOM_MID },
    { LV_SYMBOL_LEFT, LV_ALIGN_LEFT_MID },
    { LV_SYMBOL_RIGHT, LV_ALIGN_RIGHT_MID },
};

static const btn_map_t btn_func_map[] = {
    { "A", LV_ALIGN_LEFT_MID },
    { "B", LV_ALIGN_TOP_MID },
    { "L", LV_ALIGN_BOTTOM_MID },
    { "R", LV_ALIGN_RIGHT_MID },
};

static const btn_map_t btn_ctrl_map[] = {
    { "START", LV_ALIGN_TOP_MID },
    { "SELECT", LV_ALIGN_BOTTOM_MID },
};

static uint32_t btn_read_cb(void* user_data)
{
    gba_view_t* view = user_data;

    uint32_t key_state = 0;

#define BTN_STATE_DEF(obj, joypad_id)                  \
    do {                                               \
        if (lv_obj_has_state(obj, LV_STATE_PRESSED)) { \
            key_state |= 1 << joypad_id;               \
        }                                              \
    } while (0)

    BTN_STATE_DEF(view->btn.dir.up, GBA_JOYPAD_UP);
    BTN_STATE_DEF(view->btn.dir.down, GBA_JOYPAD_DOWN);
    BTN_STATE_DEF(view->btn.dir.left, GBA_JOYPAD_LEFT);
    BTN_STATE_DEF(view->btn.dir.right, GBA_JOYPAD_RIGHT);

    BTN_STATE_DEF(view->btn.func.A, GBA_JOYPAD_A);
    BTN_STATE_DEF(view->btn.func.B, GBA_JOYPAD_B);
    BTN_STATE_DEF(view->btn.func.L, GBA_JOYPAD_L);
    BTN_STATE_DEF(view->btn.func.R, GBA_JOYPAD_R);

    BTN_STATE_DEF(view->btn.ctrl.select, GBA_JOYPAD_SELECT);
    BTN_STATE_DEF(view->btn.ctrl.start, GBA_JOYPAD_START);

    return key_state;
}

/* Virtual-key button built from a plain lv_obj instead of lv_btn:
 *  - no pressed-scale animation (lv_obj has none)
 *  - bg stays dark blue-grey by default (including FOCUSED/other states)
 *  - on LV_STATE_PRESSED the bg brightens to a lighter blue; white label text
 *    stays readable on both
 *  - ext_click_area enlarges the touch target beyond the small widget bounds
 */
static lv_obj_t* vk_btn_create(lv_obj_t* parent, const char* text)
{
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x37474F), 0);   /* dark blue-grey */
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    /* press feedback only: brighter bg, white text still readable */
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x5A9BD5), LV_STATE_PRESSED);

    /* enlarge the clickable area for small buttons (5px, keep adjacent
     * buttons' touch targets from overlapping) */
    lv_obj_set_ext_click_area(btn, 5);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return btn;
}

static void btn_create(gba_context_t* ctx)
{
    gba_view_t* view = ctx->view;
    lv_obj_t* area = view->btn_area;
    /* 150x150 group boxes spread the four cross keys / A-B-L-R further apart
     * than the old 130x130 (buttons keep their size, only the spacing grows).
     * Fits the 480x160 strip: 150 < 160, vertically centered leaves 5px. */
    const lv_coord_t cont_size = 150;

    /* D-pad: left, vertically centered */
    {
        lv_obj_t* cont = lv_obj_create(area);
        view->btn.dir.cont = cont;
        lv_obj_remove_style_all(cont);
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(cont, cont_size, cont_size);
        lv_obj_align(cont, LV_ALIGN_LEFT_MID, 8, 0);

        lv_obj_t** btn_arr = &view->btn.dir.up;
        for (int i = 0; i < GBA_ARRAY_SIZE(btn_dir_map); i++) {
            lv_obj_t* btn = vk_btn_create(cont, btn_dir_map[i].txt);
            btn_arr[i] = btn;
            lv_obj_set_size(btn, 56, 56);
            lv_obj_align(btn, btn_dir_map[i].align, 0, 0);
        }
    }

    /* A / B / L / R: right, vertically centered */
    {
        lv_obj_t* cont = lv_obj_create(area);
        view->btn.func.cont = cont;
        lv_obj_remove_style_all(cont);
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(cont, cont_size, cont_size);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -8, 0);

        lv_obj_t** btn_arr = &view->btn.func.A;
        for (int i = 0; i < GBA_ARRAY_SIZE(btn_func_map); i++) {
            lv_obj_t* btn = vk_btn_create(cont, btn_func_map[i].txt);
            btn_arr[i] = btn;
            lv_obj_set_size(btn, 58, 58);
            lv_obj_align(btn, btn_func_map[i].align, 0, 0);
        }
    }

    /* START / SELECT: center of the button area, stacked top/bottom */
    {
        lv_obj_t* cont = lv_obj_create(area);
        view->btn.ctrl.cont = cont;
        lv_obj_remove_style_all(cont);
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(cont, 120, 96);
        lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t** btn_arr = &view->btn.ctrl.start;
        for (int i = 0; i < GBA_ARRAY_SIZE(btn_ctrl_map); i++) {
            lv_obj_t* btn = vk_btn_create(cont, btn_ctrl_map[i].txt);
            btn_arr[i] = btn;
            lv_obj_set_size(btn, 110, 42);
            lv_obj_align(btn, btn_ctrl_map[i].align, 0, 0);
        }
    }

    lv_gba_emu_add_input_read_cb(view->root, btn_read_cb, view);
}

void gba_view_init(gba_context_t* ctx, lv_obj_t* par, int mode)
{
    gba_view_t* view = lv_malloc(sizeof(gba_view_t));
    LV_ASSERT_MALLOC(view);
    lv_memzero(view, sizeof(gba_view_t));
    ctx->view = view;

    lv_obj_t* root = lv_obj_create(par);
    view->root = root;
    lv_obj_set_user_data(root, ctx);   /* gba_emu accessors read ctx via the object's user_data */
    lv_obj_remove_style_all(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* ---- top: GBA screen, native 240x160 scaled 2x -> 480x320 ---- */
    lv_obj_t* top = lv_obj_create(root);
    lv_obj_remove_style_all(top);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(top, GBA_SCREEN_W, GBA_SCREEN_H / 3 * 2); /* 480 x 320 */
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    view->screen.canvas = lv_canvas_create(top);
    /* Native 480x320 canvas: we upscale the 240x160 GBA frame ourselves with a
     * cheap integer 2x copy in gba_view_draw_frame(). This avoids LVGL's
     * software transform (transform_zoom), which cost ~ms of CPU every frame. */
    lv_obj_set_size(view->screen.canvas, GBA_SCREEN_W, GBA_SCREEN_H / 3 * 2);
    lv_obj_set_style_bg_opa(view->screen.canvas, LV_OPA_COVER, 0);

    /* ---- bottom: 480x160 control area ---- */
    if (mode == LV_GBA_VIEW_MODE_VIRTUAL_KEYPAD) {
        lv_obj_t* bottom = lv_obj_create(root);
        lv_obj_remove_style_all(bottom);
        lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(bottom, GBA_SCREEN_W, GBA_SCREEN_H / 3); /* 480 x 160 */
        lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
        /* No flex here: the three groups are placed manually in btn_create()
         * (D-pad left-center, A/B/L/R right-center, START/SELECT center). */
        view->btn_area = bottom;
        btn_create(ctx);
    }
}

void gba_view_deinit(gba_context_t* ctx)
{
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(ctx->view);
    if (ctx->view->screen.scaled) {
        lv_free(ctx->view->screen.scaled);
        ctx->view->screen.scaled = NULL;
    }
    lv_free(ctx->view);
}

lv_obj_t* gba_view_get_root(gba_context_t* ctx)
{
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(ctx->view);
    return ctx->view->root;
}

void gba_view_invalidate_frame(gba_context_t* ctx)
{
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(ctx->view);
    lv_obj_invalidate(ctx->view->screen.canvas);
}

void gba_view_draw_frame(gba_context_t* ctx, const uint16_t* buf, lv_coord_t width, lv_coord_t height)
{
    lv_obj_t* canvas = ctx->view->screen.canvas;

    /* First frame: allocate the 480x320 RGB565 canvas buffer once and bind it. */
    if (ctx->view->screen.scaled == NULL) {
        ctx->view->screen.scaled = lv_malloc(GBA_SCREEN_W * (GBA_SCREEN_H / 3 * 2) * sizeof(uint16_t));
        LV_ASSERT_MALLOC(ctx->view->screen.scaled);
        lv_draw_buf_init(
            &ctx->view->screen.draw_buf,
            GBA_SCREEN_W, GBA_SCREEN_H / 3 * 2,
            LV_COLOR_FORMAT_RGB565, GBA_SCREEN_W * sizeof(uint16_t),
            ctx->view->screen.scaled,
            GBA_SCREEN_W * (GBA_SCREEN_H / 3 * 2) * sizeof(uint16_t));
        lv_canvas_set_draw_buf(canvas, &ctx->view->screen.draw_buf);
    }

    /* Integer 2x nearest-neighbor upscale 240x160 -> 480x320.
     * ~153K 16-bit writes, well under 1ms on a Cortex-A7 - much cheaper than
     * LVGL's software transform (transform_zoom) used before. */
    const uint16_t* src = buf;
    uint16_t* dst = ctx->view->screen.scaled;
    uint32_t src_stride = ctx->av_info.fb_stride; /* pixels per row (256) */

    for (int y = 0; y < height; y++) {
        const uint16_t* srow = src + y * src_stride;
        uint16_t* drow = dst + (y * 2) * GBA_SCREEN_W;
        for (int x = 0; x < width; x++) {
            uint16_t c = srow[x];
            drow[x * 2] = c;
            drow[x * 2 + 1] = c;
            drow[GBA_SCREEN_W + x * 2] = c;
            drow[GBA_SCREEN_W + x * 2 + 1] = c;
        }
    }

#if THREADED_RENDERER
    ctx->invalidate = true;
#else
    gba_view_invalidate_frame(ctx);
#endif
}
