/*
 * mt_probe.c -- dump raw MT events from /dev/input/event1 AND the per-frame
 * slot state as computed by the same protocol-agnostic parser used in
 * eMP-gba (src/HAL/input_mt.cpp). Run it on the T113 board and touch with
 * one / two fingers; the output shows exactly what the driver sends and
 * whether slot coordinates come out right.
 *
 * Build (T113 cross):
 *   arm-openwrt-linux-muslgnueabi-gcc -O2 -static -o mt_probe mt_probe.c
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define MAX_SLOTS 10

static const char * abs_name(int c)
{
    switch(c) {
        case ABS_X: return "ABS_X";
        case ABS_Y: return "ABS_Y";
        case ABS_MT_SLOT: return "ABS_MT_SLOT";
        case ABS_MT_TOUCH_MAJOR: return "ABS_MT_TOUCH_MAJOR";
        case ABS_MT_WIDTH_MAJOR: return "ABS_MT_WIDTH_MAJOR";
        case ABS_MT_POSITION_X: return "ABS_MT_POSITION_X";
        case ABS_MT_POSITION_Y: return "ABS_MT_POSITION_Y";
        case ABS_MT_TRACKING_ID: return "ABS_MT_TRACKING_ID";
        default: return "ABS_?";
    }
}

int main(void)
{
    int fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    if(fd < 0) { perror("open event1"); return 1; }

    /* calibration ranges */
    struct input_absinfo ai;
    int minx=0,maxx=0,miny=0,maxy=0;
    if(ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ai)==0){minx=ai.minimum;maxx=ai.maximum;}
    if(ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai)==0){miny=ai.minimum;maxy=ai.maximum;}
    printf("cal MT_X[%d,%d] MT_Y[%d,%d]\n", minx, maxx, miny, maxy);

    /* parser state (mirror of input_mt.cpp) */
    int cur_slot = 0;
    int saw_slot = 0;
    int slot_x[MAX_SLOTS] = {0}, slot_y[MAX_SLOTS] = {0};
    int slot_pr[MAX_SLOTS] = {0};   /* 1 = pressed */
    int tid_slot[MAX_SLOTS];        /* tid -> slot */
    int slot_tid[MAX_SLOTS];        /* slot -> tid */
    for(int i=0;i<MAX_SLOTS;i++){ tid_slot[i]=-1; slot_tid[i]=-1; }
    int last_active = -1;

    struct input_event e;
    unsigned long frames = 0;
    for(;;) {
        ssize_t r = read(fd, &e, sizeof(e));
        if(r == (ssize_t)sizeof(e)) {
            if(e.type == EV_ABS) {
                switch(e.code) {
                case ABS_MT_SLOT:
                    saw_slot = 1;
                    cur_slot = (e.value>=0 && e.value<MAX_SLOTS) ? e.value : MAX_SLOTS-1;
                    break;
                case ABS_MT_TRACKING_ID:
                    if(e.value < 0) {
                        if(cur_slot<MAX_SLOTS) {
                            slot_pr[cur_slot] = 0;
                            if(!saw_slot && slot_tid[cur_slot]>=0) {
                                tid_slot[slot_tid[cur_slot]] = -1;
                                slot_tid[cur_slot] = -1;
                            }
                        }
                    } else {
                        if(!saw_slot) {
                            int s = -1;
                            for(int i=0;i<MAX_SLOTS;i++) if(tid_slot[i]==e.value) s=i;
                            if(s<0) for(int i=0;i<MAX_SLOTS;i++) if(slot_tid[i]<0){s=i;break;}
                            if(s<0) s=MAX_SLOTS-1;
                            tid_slot[s]=e.value; slot_tid[s]=e.value; cur_slot=s;
                        }
                        if(cur_slot<MAX_SLOTS) slot_pr[cur_slot]=1;
                    }
                    break;
                case ABS_MT_POSITION_X:
                    if(cur_slot<MAX_SLOTS) slot_x[cur_slot]=e.value;
                    break;
                case ABS_MT_POSITION_Y:
                    if(cur_slot<MAX_SLOTS) slot_y[cur_slot]=e.value;
                    break;
                case ABS_X: slot_x[0]=e.value; break;
                case ABS_Y: slot_y[0]=e.value; break;
                }
                /* raw line */
                printf("  ABS %s v=%d (cur_slot=%d)\n", abs_name(e.code), e.value, cur_slot);
            }
            else if(e.type == EV_KEY && e.code == BTN_TOUCH) {
                printf("  KEY BTN_TOUCH v=%d\n", e.value);
                slot_pr[0] = (e.value!=0);
            }
            else if(e.type == EV_SYN && e.code == SYN_MT_REPORT) {
                printf("  SYN_MT_REPORT\n");
            }
            else if(e.type == EV_SYN && e.code == SYN_REPORT) {
                frames++;
                int n=0; for(int i=0;i<MAX_SLOTS;i++) if(slot_pr[i]) n++;
                printf("FRAME %lu: active=%d slots=[", frames, n);
                for(int i=0;i<MAX_SLOTS;i++)
                    if(slot_pr[i]) printf(" (%d:%d,%d)", i, slot_x[i], slot_y[i]);
                printf(" ]\n");
                if(n != last_active) { last_active = n; printf("  >> active changed to %d\n", n); }
                fflush(stdout);
            }
        }
        else if(r == -1) {
            if(errno != EAGAIN) { perror("read"); break; }
            usleep(20000);
        }
    }
    return 0;
}
