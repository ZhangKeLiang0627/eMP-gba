/*
 * Multi-touch input for eMP-gba (Allwinner T113-S3, Goodix gt9xx touchscreen).
 *
 * Why a custom driver instead of lv_evdev_create?
 *   LVGL v9's built-in evdev driver opens ONE fd and its pointer read callback
 *   always reports touch_data[0] (slot 0) only -- it can never surface a second
 *   touch point. Per LVGL's own docs, multi-touch is done by creating several
 *   POINTER indevs, each reporting a different touch slot. This module does
 *   exactly that: it opens /dev/input/event1 once per indev, parses the kernel
 *   MT event stream, and hands slot N to indev N. The on-screen virtual GBA
 *   buttons (lv_btn) are then pressable by independent fingers, so e.g.
 *   "Up + A" or "L + R" can be held simultaneously.
 *
 * The Goodix device on this board advertises ABS_MT_POSITION_X/Y and
 * ABS_MT_TRACKING_ID but NOT ABS_MT_SLOT (no MT slot bit in the abs bitmap),
 * i.e. it may use MT protocol A. The parser below is protocol-agnostic:
 *   - If ABS_MT_SLOT is seen (protocol B), it selects the active slot directly.
 *   - Otherwise (protocol A) each ABS_MT_TRACKING_ID is mapped to a stable
 *     slot index so contacts keep a consistent identity across SYN_REPORTs.
 * Legacy single-touch (ABS_X/ABS_Y + BTN_TOUCH) is also handled as slot 0.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

extern "C" {
#include "lvgl/lvgl.h"
}

/* Number of simultaneous touch points exposed to LVGL (one POINTER indev
 * each). Bump this if the panel and use-case need more (the GBA pad rarely
 * needs more than 2-3 held at once, but 2 satisfies "at least two points"). */
#ifndef GBA_INPUT_TOUCH_POINTS
#define GBA_INPUT_TOUCH_POINTS 2
#endif

#define MT_MAX_SLOTS 10

typedef struct {
    int fd;                 /* own open fd on the evdev device */
    int slot;               /* which contact (slot) this indev reports */
    int min_x, min_y;       /* calibration input range */
    int max_x, max_y;
    bool saw_slot;          /* protocol B (ABS_MT_SLOT seen)? */

    /* Parsed state for every contact, refreshed from the event stream. */
    int cur_slot;                       /* slot in progress this frame */
    int slot_x[MT_MAX_SLOTS];
    int slot_y[MT_MAX_SLOTS];
    lv_indev_state_t slot_state[MT_MAX_SLOTS];

    /* protocol A: stable mapping tracking_id -> slot index. */
    int tid_slot[MT_MAX_SLOTS];         /* tid -> slot, -1 = free */
    int slot_tid[MT_MAX_SLOTS];         /* slot -> tid, -1 = free */

    lv_point_t last;        /* last reported point (used on release) */
    int last_active;        /* last printed active-contact count (diag) */
} mt_indev_ctx_t;

static int mt_calib(int v, int in_min, int in_max, int out_min, int out_max)
{
    if(in_max > in_min)
        v = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if(v < out_min) v = out_min;
    if(v > out_max) v = out_max;
    return v;
}

static int mt_tid_to_slot(mt_indev_ctx_t * c, int tid)
{
    /* Already mapped? */
    for(int s = 0; s < MT_MAX_SLOTS; s++)
        if(c->tid_slot[s] == tid) return s;
    /* Allocate a free slot. */
    for(int s = 0; s < MT_MAX_SLOTS; s++) {
        if(c->slot_tid[s] < 0) {
            c->tid_slot[s] = tid;
            c->slot_tid[s] = tid;
            return s;
        }
    }
    return MT_MAX_SLOTS - 1; /* all full: reuse last */
}

static void mt_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    mt_indev_ctx_t * c = (mt_indev_ctx_t *)lv_indev_get_driver_data(indev);
    if(c == NULL) return;

    struct input_event in;
    ssize_t br;
    while((br = read(c->fd, &in, sizeof(in))) > 0) {
        if(in.type == EV_ABS) {
            switch(in.code) {
            case ABS_MT_SLOT:
                c->saw_slot = true;
                if(in.value >= 0 && in.value < MT_MAX_SLOTS)
                    c->cur_slot = in.value;
                else
                    c->cur_slot = MT_MAX_SLOTS - 1;
                break;

            case ABS_MT_TRACKING_ID:
                if(in.value < 0) {
                    /* contact lifted on the current slot */
                    if(c->cur_slot < MT_MAX_SLOTS)
                        c->slot_state[c->cur_slot] = LV_INDEV_STATE_RELEASED;
                }
                else {
                    if(!c->saw_slot) {
                        /* protocol A: assign a stable slot for this tid */
                        c->cur_slot = mt_tid_to_slot(c, in.value);
                    }
                    if(c->cur_slot < MT_MAX_SLOTS)
                        c->slot_state[c->cur_slot] = LV_INDEV_STATE_PRESSED;
                }
                break;

            case ABS_MT_POSITION_X:
                if(c->cur_slot < MT_MAX_SLOTS) c->slot_x[c->cur_slot] = in.value;
                break;
            case ABS_MT_POSITION_Y:
                if(c->cur_slot < MT_MAX_SLOTS) c->slot_y[c->cur_slot] = in.value;
                break;

            case ABS_X: /* legacy single-touch mirror -> slot 0 */
                c->slot_x[0] = in.value;
                break;
            case ABS_Y:
                c->slot_y[0] = in.value;
                break;

            default:
                break;
            }
        }
        else if(in.type == EV_KEY && in.code == BTN_TOUCH) {
            /* single-touch emulation (no MT): drive slot 0 */
            c->slot_state[0] = (in.value == 0)
                                 ? LV_INDEV_STATE_RELEASED
                                 : LV_INDEV_STATE_PRESSED;
        }
        /* EV_SYN SYN_REPORT: frame boundary, nothing to flush here. */
    }

    /* Report the contact assigned to this indev. */
    int s = c->slot;
    if(s >= MT_MAX_SLOTS) s = MT_MAX_SLOTS - 1;

    data->state = c->slot_state[s];
    if(data->state == LV_INDEV_STATE_PRESSED) {
        lv_display_t * disp = lv_indev_get_display(indev);
        int off_x = disp ? (int)lv_display_get_offset_x(disp) : 0;
        int off_y = disp ? (int)lv_display_get_offset_y(disp) : 0;
        int w = disp ? (int)lv_display_get_horizontal_resolution(disp) : 480;
        int h = disp ? (int)lv_display_get_vertical_resolution(disp) : 480;
        c->last.x = mt_calib(c->slot_x[s], c->min_x, c->max_x, off_x, off_x + w - 1);
        c->last.y = mt_calib(c->slot_y[s], c->min_y, c->max_y, off_y, off_y + h - 1);
    }
    data->point = c->last;

    /* Diagnostic: indev #0 also reports how many contacts are currently
     * active (across all slots) so a two-finger press is visible in the log. */
    if(c->slot == 0) {
        int n = 0;
        for(int i = 0; i < MT_MAX_SLOTS; i++)
            if(c->slot_state[i] == LV_INDEV_STATE_PRESSED) n++;
        if(n != c->last_active) {
            fprintf(stderr, "[MT] active contacts = %d\n", n);
            c->last_active = n;
        }
    }
}

namespace HAL {

void InitMultiTouchInput(void)
{
    const char * dev = "/dev/input/event1";

    for(int i = 0; i < GBA_INPUT_TOUCH_POINTS; i++) {
        int fd = open(dev, O_RDONLY | O_NOCTTY | O_NONBLOCK);
        if(fd < 0) {
            LV_LOG_WARN("MT touch: open %s failed: %s", dev, strerror(errno));
            continue;
        }

        mt_indev_ctx_t * c = (mt_indev_ctx_t *)lv_malloc_zeroed(sizeof(*c));
        if(c == NULL) {
            LV_LOG_WARN("MT touch: alloc failed for indev #%d", i);
            close(fd);
            continue;
        }
        c->fd = fd;
        c->slot = i;
        c->cur_slot = 0;
    c->last.x = 0;
    c->last.y = 0;
    c->last_active = -1;
        for(int s = 0; s < MT_MAX_SLOTS; s++) {
            c->slot_state[s] = LV_INDEV_STATE_RELEASED;
            c->slot_tid[s] = -1;
            c->tid_slot[s] = -1;
        }

        /* Calibration range: prefer the MT position axes (this panel only
         * advertises ABS_MT_POSITION_*, not ABS_X/ABS_Y), fall back to the
         * legacy axes, then to the display size. */
        struct input_absinfo ai;
        if(ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0) {
            c->min_x = ai.minimum; c->max_x = ai.maximum;
        }
        else if(ioctl(fd, EVIOCGABS(ABS_X), &ai) == 0) {
            c->min_x = ai.minimum; c->max_x = ai.maximum;
        }
        else {
            c->min_x = 0; c->max_x = 480;
        }
        if(ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0) {
            c->min_y = ai.minimum; c->max_y = ai.maximum;
        }
        else if(ioctl(fd, EVIOCGABS(ABS_Y), &ai) == 0) {
            c->min_y = ai.minimum; c->max_y = ai.maximum;
        }
        else {
            c->min_y = 0; c->max_y = 480;
        }

        lv_indev_t * indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, mt_read_cb);
        lv_indev_set_driver_data(indev, c);

        LV_LOG_USER("MT touch: indev #%d (contact %d) fd=%d cal x[%d,%d] y[%d,%d]",
                    i, i, fd, c->min_x, c->max_x, c->min_y, c->max_y);
        fprintf(stderr, "[MT] indev #%d (contact %d) fd=%d cal x[%d,%d] y[%d,%d]\n",
                i, i, fd, c->min_x, c->max_x, c->min_y, c->max_y);
    }
}

} /* namespace HAL */
