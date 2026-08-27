/*
 * Custom POSIX file system driver for LVGL (eMP-gba / T113-S3).
 *
 * This vendored LVGL copy ships no built-in POSIX FS driver, but the GBA
 * core requires lv_fs_* for:
 *   - vba-next libretro VFS implementation (ROM file I/O)
 *   - ROM size probe and save/load game state (gba_retro_save/load_game)
 *   - the on-screen ROM picker (gba_menu, lv_fs_dir_*)
 *
 * All paths reaching us are absolute and already start with the drive
 * letter '/', so we pass them verbatim to the C stdio API. A possible
 * legacy "X:" prefix is stripped if present.
 */
#include "lvgl/lvgl.h"

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#ifndef LV_FS_POSIX_LETTER
#define LV_FS_POSIX_LETTER '/'
#endif

/* Strip a possible "X:" drive prefix; return pointer to the real path. */
static const char * strip_letter(const char * path)
{
    if(path != NULL && path[0] != '\0' && path[1] == ':')
        return path + 2;
    return path;
}

static lv_fs_res_t fs_open_cb(lv_fs_drv_t * drv, void * file_p, const char * path, lv_fs_mode_t mode)
{
    (void) drv;

    const char * flags = NULL;
    if(mode == LV_FS_MODE_WR) flags = "wb";
    else if(mode == LV_FS_MODE_RD) flags = "rb";
    else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) flags = "rb+";
    else return LV_FS_RES_INV_PARAM;

    const char * real = strip_letter(path);

    FILE * fp = fopen(real, flags);
    if(fp == NULL)
        return LV_FS_RES_FS_ERR;

    /* file_p points at the lv_fs_file_t::file_d member; store our FILE* there. */
    *(void **)file_p = (void *)fp;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t * drv, void * file_p)
{
    (void) drv;
    FILE * fp = (FILE *)file_p;
    if(fp == NULL)
        return LV_FS_RES_OK;
    fclose(fp);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    (void) drv;
    FILE * fp = (FILE *)file_p;
    if(fp == NULL)
        return LV_FS_RES_FS_ERR;

    *br = (uint32_t)fread(buf, 1, btr, fp);
    if(ferror(fp))
        return LV_FS_RES_FS_ERR;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write_cb(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    (void) drv;
    FILE * fp = (FILE *)file_p;
    if(fp == NULL)
        return LV_FS_RES_FS_ERR;

    *bw = (uint32_t)fwrite(buf, 1, btw, fp);
    if(ferror(fp))
        return LV_FS_RES_FS_ERR;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void) drv;
    FILE * fp = (FILE *)file_p;
    if(fp == NULL)
        return LV_FS_RES_FS_ERR;

    int w = SEEK_SET;
    if(whence == LV_FS_SEEK_CUR) w = SEEK_CUR;
    else if(whence == LV_FS_SEEK_END) w = SEEK_END;

    if(fseek(fp, (long)pos, w) != 0)
        return LV_FS_RES_FS_ERR;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos)
{
    (void) drv;
    FILE * fp = (FILE *)file_p;
    if(fp == NULL)
        return LV_FS_RES_FS_ERR;

    long p = ftell(fp);
    if(p < 0)
        return LV_FS_RES_FS_ERR;
    *pos = (uint32_t)p;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_open_cb(lv_fs_drv_t * drv, void * dir_p, const char * path)
{
    (void) drv;
    const char * real = strip_letter(path);

    DIR * d = opendir(real);
    if(d == NULL)
        return LV_FS_RES_FS_ERR;

    *(void **)dir_p = (void *)d;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_read_cb(lv_fs_drv_t * drv, void * dir_p, char * fn)
{
    (void) drv;
    DIR * d = (DIR *)dir_p;
    if(d == NULL) {
        fn[0] = '\0';
        return LV_FS_RES_OK;
    }

    struct dirent * e = readdir(d);
    if(e == NULL) {
        fn[0] = '\0';
        return LV_FS_RES_OK;
    }

    strncpy(fn, e->d_name, 255);
    fn[255] = '\0';
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close_cb(lv_fs_drv_t * drv, void * dir_p)
{
    (void) drv;
    DIR * d = (DIR *)dir_p;
    if(d)
        closedir(d);
    return LV_FS_RES_OK;
}

static lv_fs_drv_t g_fs_drv;

void lv_fs_posix_init(void)
{
    lv_memzero(&g_fs_drv, sizeof(g_fs_drv));

    g_fs_drv.letter       = LV_FS_POSIX_LETTER;
    g_fs_drv.open_cb      = fs_open_cb;
    g_fs_drv.close_cb     = fs_close_cb;
    g_fs_drv.read_cb      = fs_read_cb;
    g_fs_drv.write_cb     = fs_write_cb;
    g_fs_drv.seek_cb      = fs_seek_cb;
    g_fs_drv.tell_cb      = fs_tell_cb;
    g_fs_drv.dir_open_cb  = fs_dir_open_cb;
    g_fs_drv.dir_read_cb  = fs_dir_read_cb;
    g_fs_drv.dir_close_cb = fs_dir_close_cb;

    lv_fs_drv_register(&g_fs_drv);
}
