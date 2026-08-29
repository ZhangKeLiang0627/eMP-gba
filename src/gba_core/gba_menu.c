/*
 * GBA ROM launcher for eMP-gba.
 * Lists *.gba files from a directory and reports the selection back to
 * the caller. Adapted from lv_gba_emu (CJK font dependency removed).
 */
#include "gba_emu.h"
#include "gba_font.h"
#include "gba_menu.h"

typedef struct {
    char base_path[512];
    gba_menu_select_cb_t cb;
    void* user_data;
} menu_ctx_t;

static menu_ctx_t g_menu_ctx;

static int is_gba_file(const char* filename)
{
    return lv_strcmp(lv_fs_get_ext(filename), "gba") == 0;
}

static void event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        if (!label)
            return;

        const char* filename = lv_label_get_text(label);
        if (!filename)
            return;

        char full_path[1024];
        lv_snprintf(full_path, sizeof(full_path), "%s/%s", g_menu_ctx.base_path, filename);

        if (g_menu_ctx.cb) {
            g_menu_ctx.cb(full_path, g_menu_ctx.user_data);
        }
    } else if (code == LV_EVENT_FOCUSED) {
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        if (label) {
            lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        if (label) {
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        }
    }
}

lv_obj_t* gba_menu_create(lv_obj_t* parent, const char* dir_path, gba_menu_select_cb_t cb, void* user_data)
{
    char fs_path[512];
    if (dir_path[0] != '/') {
        lv_snprintf(fs_path, sizeof(fs_path), "/%s", dir_path);
    } else {
        lv_snprintf(fs_path, sizeof(fs_path), "%s", dir_path);
    }

    lv_strlcpy(g_menu_ctx.base_path, fs_path, sizeof(g_menu_ctx.base_path));
    g_menu_ctx.cb = cb;
    g_menu_ctx.user_data = user_data;

    /* Plain container root (NOT a flex list): page-level overlays (pinned
     * top bar / volume bar) attach here and are positioned absolutely above
     * the list, unaffected by the list's flex layout. */
    lv_obj_t* root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    lv_obj_t* list = lv_list_create(root);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_center(list);
    /* Leave room for the pinned top bar (38px) so the title/items are not
     * covered by it. */
    lv_obj_set_style_pad_top(list, 44, 0);
    lv_obj_set_style_pad_bottom(list, 12, 0);

    lv_list_add_text(list, "Select ROM");

    lv_fs_dir_t dir;
    lv_fs_res_t res;
    res = lv_fs_dir_open(&dir, fs_path);

    int count = 0;
    if (res == LV_FS_RES_OK) {
        char fn[256];
        while (1) {
            res = lv_fs_dir_read(&dir, fn, sizeof(fn));
            if (res != LV_FS_RES_OK || fn[0] == '\0')
                break;

            if (fn[0] == '/')
                continue;

            if (is_gba_file(fn)) {
                lv_obj_t* btn = lv_list_add_btn(list, NULL, fn);
                lv_obj_add_event(btn, event_handler, LV_EVENT_CLICKED, NULL);
                lv_obj_add_event(btn, event_handler, LV_EVENT_FOCUSED, NULL);
                lv_obj_add_event(btn, event_handler, LV_EVENT_DEFOCUSED, NULL);

                lv_obj_t* label = lv_obj_get_child(btn, 0);
                if (label) {
                    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
                    /* CJK-capable font so Chinese ROM names render properly */
                    lv_font_t* f = gba_font_get(16);
                    if (f) lv_obj_set_style_text_font(label, f, 0);
                }
                count++;
            }
        }
        lv_fs_dir_close(&dir);
    } else {
        lv_list_add_text(list, "Failed to open directory:");
        lv_list_add_text(list, fs_path);
    }

    if (count == 0 && res == LV_FS_RES_OK) {
        lv_list_add_text(list, "No .gba files found");
    }

    return root;
}
