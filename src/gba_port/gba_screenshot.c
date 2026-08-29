/*
 * Screenshot for eMP-gba (Allwinner T113-S3).
 *
 * Grabs the visible page of /dev/fb0 (the LVGL framebuffer) and writes it as
 * a 24-bit BMP so it can be opened on a PC directly. Supports the two formats
 * the fbdev driver uses here: 32bpp XRGB8888 and 16bpp RGB565.
 */
#include "port.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FB_DEV "/dev/fb0"

static int write_bmp(const char* path, const unsigned char* rgb24,
                     int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL) return -1;

    int rowsz = w * 3;
    int padsz = (4 - (rowsz % 4)) % 4;
    int img_size = (rowsz + padsz) * h;
    int file_size = 54 + img_size;

    unsigned char hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (unsigned char)(file_size); hdr[3] = (unsigned char)(file_size >> 8);
    hdr[4] = (unsigned char)(file_size >> 16); hdr[5] = (unsigned char)(file_size >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (unsigned char)(w); hdr[19] = (unsigned char)(w >> 8);
    hdr[20] = (unsigned char)(w >> 16); hdr[21] = (unsigned char)(w >> 24);
    hdr[22] = (unsigned char)(h); hdr[23] = (unsigned char)(h >> 8);
    hdr[24] = (unsigned char)(h >> 16); hdr[25] = (unsigned char)(h >> 24);
    hdr[26] = 1;
    hdr[28] = 24;
    hdr[34] = (unsigned char)(img_size); hdr[35] = (unsigned char)(img_size >> 8);
    hdr[36] = (unsigned char)(img_size >> 16); hdr[37] = (unsigned char)(img_size >> 24);

    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return -1;
    }

    /* BMP rows are bottom-up */
    unsigned char pad[4] = {0, 0, 0, 0};
    for (int y = h - 1; y >= 0; y--) {
        if (fwrite(rgb24 + (size_t)y * rowsz, 1, (size_t)rowsz, f) != (size_t)rowsz) {
            fclose(f);
            return -1;
        }
        if (padsz && fwrite(pad, 1, (size_t)padsz, f) != (size_t)padsz) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int gba_screenshot_capture(const char* path)
{
    int fd = open(FB_DEV, O_RDONLY);
    if (fd < 0) {
        LV_LOG_WARN("screenshot: open %s failed", FB_DEV);
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        LV_LOG_WARN("screenshot: FBIOGET_VSCREENINFO failed");
        close(fd);
        return -1;
    }

    int w = vinfo.xres;
    int h = vinfo.yres;
    int bpp = vinfo.bits_per_pixel;
    int line_len = w * (bpp >> 3);

    /* read the visible page (virtual fb may be double buffered) */
    off_t page_off = (off_t)vinfo.yoffset * line_len;
    size_t page_size = (size_t)h * line_len;
    unsigned char* fb = malloc(page_size);
    if (fb == NULL) {
        close(fd);
        return -1;
    }

    if (pread(fd, fb, page_size, page_off) != (ssize_t)page_size) {
        LV_LOG_WARN("screenshot: pread %zu bytes failed", page_size);
        free(fb);
        close(fd);
        return -1;
    }
    close(fd);

    /* convert to 24-bit RGB */
    unsigned char* rgb24 = malloc((size_t)w * h * 3);
    if (rgb24 == NULL) {
        free(fb);
        return -1;
    }

    if (bpp == 32) {
        /* XRGB8888 little-endian: B,G,R,X per pixel */
        for (int i = 0; i < w * h; i++) {
            rgb24[i * 3 + 0] = fb[i * 4 + 0];
            rgb24[i * 3 + 1] = fb[i * 4 + 1];
            rgb24[i * 3 + 2] = fb[i * 4 + 2];
        }
    } else if (bpp == 16) {
        /* RGB565 little-endian */
        for (int i = 0; i < w * h; i++) {
            uint16_t c = (uint16_t)(fb[i * 2] | (fb[i * 2 + 1] << 8));
            rgb24[i * 3 + 0] = (unsigned char)(((c >> 0) & 0x1F) * 255 / 31);
            rgb24[i * 3 + 1] = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);
            rgb24[i * 3 + 2] = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);
        }
    } else {
        LV_LOG_WARN("screenshot: unsupported bpp %d", bpp);
        free(rgb24);
        free(fb);
        return -1;
    }
    free(fb);

    char auto_path[96];
    if (path == NULL) {
        time_t t = time(NULL);
        lv_snprintf(auto_path, sizeof(auto_path), "/root/screenshot_%ld.bmp", (long)t);
        path = auto_path;
    }

    int ret = write_bmp(path, rgb24, w, h);
    free(rgb24);

    if (ret == 0) {
        LV_LOG_USER("screenshot saved: %s (%dx%d)", path, w, h);
    } else {
        LV_LOG_WARN("screenshot: write %s failed", path);
    }
    return ret;
}
