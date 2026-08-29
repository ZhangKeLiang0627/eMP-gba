/*
 * evswipe: inject protocol-A MT events (tap / swipe) into an evdev device.
 *
 * The gt9xx panel does NOT advertise ABS_MT_SLOT in its abs bitmap, so the
 * kernel silently DROPS any injected ABS_MT_SLOT event (capability check in
 * input_handle_event). Injecting a protocol-B shaped sequence therefore
 * leaves an incomplete frame and a protocol-A parser ignores it.
 *
 * This tool emits the exact protocol-A shape the real driver uses:
 *   per contact: POSITION_X, POSITION_Y, TOUCH_MAJOR, WIDTH_MAJOR,
 *                TRACKING_ID, then SYN_MT_REPORT (contact boundary),
 *   frame end:   SYN_REPORT.
 *
 * Usage:
 *   evswipe <dev> <x0> <y0> <x1> <y1> [steps]     swipe from (x0,y0) to (x1,y1)
 *   evswipe <dev> <x> <y> tap <hold_ms>            single tap at (x,y)
 */
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

static int fd;

static void send(int t, int c, int v)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = t;
    ev.code = c;
    ev.value = v;
    if (write(fd, &ev, sizeof(ev)) < 0) {
        perror("write");
        exit(1);
    }
}

/* one contact group, protocol A shape (coords before tid, like gt9xx) */
static void contact(int x, int y, int tid)
{
    send(EV_ABS, ABS_MT_POSITION_X, x);
    send(EV_ABS, ABS_MT_POSITION_Y, y);
    send(EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
    send(EV_ABS, ABS_MT_WIDTH_MAJOR, 10);
    send(EV_ABS, ABS_MT_TRACKING_ID, tid);
    send(EV_SYN, SYN_MT_REPORT, 0);
}

static void frame_end(void)
{
    send(EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <dev> <x0> <y0> <x1> <y1> [steps]\n"
                        "       %s <dev> <x> <y> tap [hold_ms]\n", argv[0], argv[0]);
        return 1;
    }

    fd = open(argv[1], O_WRONLY);
    if (fd < 0) { perror("open"); return 1; }

    if (argc >= 5 && argv[4][0] == 't' && argv[4][1] == 'a' && argv[4][2] == 'p') {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        int hold = (argc > 5) ? atoi(argv[5]) : 80;
        printf("tap (%d,%d) hold %dms\n", x, y, hold);
        contact(x, y, 0);
        frame_end();
        usleep(hold * 1000);
        send(EV_ABS, ABS_MT_TRACKING_ID, -1);
        send(EV_SYN, SYN_MT_REPORT, 0);
        frame_end();
        close(fd);
        return 0;
    }

    int x0 = atoi(argv[2]), y0 = atoi(argv[3]);
    int x1 = atoi(argv[4]), y1 = atoi(argv[5]);
    int steps = (argc > 6) ? atoi(argv[6]) : 10;
    if (steps < 1) steps = 1;

    printf("swipe (%d,%d) -> (%d,%d) steps=%d\n", x0, y0, x1, y1, steps);

    contact(x0, y0, 0);
    frame_end();

    for (int i = 1; i <= steps; i++) {
        int x = x0 + (x1 - x0) * i / steps;
        int y = y0 + (y1 - y0) * i / steps;
        contact(x, y, 0);
        frame_end();
        usleep(12000);   /* ~12ms per frame, matches the LVGL indev tick */
    }

    usleep(50000);       /* hold at the end */
    send(EV_ABS, ABS_MT_TRACKING_ID, -1);
    send(EV_SYN, SYN_MT_REPORT, 0);
    frame_end();

    close(fd);
    return 0;
}
