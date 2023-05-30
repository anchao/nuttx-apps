/**
 * @file vg_lite_decoder_etc2.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_decoder.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

/*********************
 *      DEFINES
 *********************/

#define ETC2_RGB_NO_MIPMAPS             1
#define ETC2_RGBA_NO_MIPMAPS            3

#define ETC2_PKM_HEADER_SIZE            16
#define ETC2_PKM_FORMAT_OFFSET          6
#define ETC2_PKM_ENCODED_WIDTH_OFFSET   8
#define ETC2_PKM_ENCODED_HEIGHT_OFFSET  10
#define ETC2_PKM_WIDTH_OFFSET           12
#define ETC2_PKM_HEIGHT_OFFSET          14

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header);
static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_decoder_etc2_init(lv_img_decoder_t * dec)
{
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint16_t read_big_endian_uint16(const uint8_t * buf)
{
    return (buf[0] << 8) | buf[1];
}

static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_img_src_t src_type = lv_img_src_get_type(src);

    if(src_type == LV_IMG_SRC_FILE) {
        const char * ext = lv_fs_get_ext(src);
        if(!(strcmp(ext, "pkm") == 0)) {
            return LV_RES_INV;
        }

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_GPU_LOG_INFO("GPU decoder open %s failed", (const char *)src);
            return LV_RES_INV;
        }

        uint32_t rn;
        uint8_t pkm_header[ETC2_PKM_HEADER_SIZE];
        const char pkm_magic[] = { 'P', 'K', 'M', ' ', '2', '0' };
        res = lv_fs_read(&f, pkm_header, ETC2_PKM_HEADER_SIZE, &rn);
        lv_fs_close(&f);
        if(res != LV_FS_RES_OK || rn != ETC2_PKM_HEADER_SIZE) {
            LV_GPU_LOG_WARN("Image get info read file magic number failed");
            return LV_RES_INV;
        }

        if(memcmp(pkm_header, pkm_magic, sizeof(pkm_magic)) != 0) {
            LV_GPU_LOG_WARN("Image get info magic number invalid");
            return LV_RES_INV;
        }

        header->always_zero = 0;
        header->w = (lv_coord_t)read_big_endian_uint16(pkm_header + ETC2_PKM_WIDTH_OFFSET);
        header->h = (lv_coord_t)read_big_endian_uint16(pkm_header + ETC2_PKM_HEIGHT_OFFSET);
        header->cf = VG_LITE_ETC2_COLOR_FORMAT;

        return LV_RES_OK;
    }

    if(src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = src;
        lv_img_cf_t cf = img_dsc->header.cf;

        if(cf != VG_LITE_ETC2_COLOR_FORMAT) {
            return LV_RES_INV;
        }

        header->w = img_dsc->header.w;
        header->h = img_dsc->header.h;
        header->cf = img_dsc->header.cf;

        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/

    if(dsc->src_type == LV_IMG_SRC_FILE) {
        const char * fn = dsc->src;
        uint32_t size = 0;
        uint8_t * vg_lite_img = vg_lite_img_file_alloc(fn, &size, ETC2_PKM_HEADER_SIZE);

        if(vg_lite_img == NULL) {
            LV_GPU_LOG_WARN("vg_lite_img_file_alloc read file %s failed", fn);
            return LV_RES_INV;
        }

        dsc->user_data = (void *)VG_LITE_IMAGE_MAGIC_NUM;
        vg_lite_img_header_t * vg_lite_img_header = (vg_lite_img_header_t *)vg_lite_img;
        vg_lite_img_header->magic = VG_LITE_IMAGE_MAGIC_NUM;
        vg_lite_img_header->recolor = dsc->color.full;
        dsc->img_data = vg_lite_img;

        vg_lite_custom_buffer_init(
            &vg_lite_img_header->vg_buf,
            vg_lite_img + sizeof(vg_lite_img_header_t),
            dsc->header.w,
            dsc->header.h,
            VG_LITE_RGBA8888_ETC2_EAC);

        return LV_RES_OK;
    }

    return LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    if(dsc->src_type == LV_IMG_SRC_FILE) {
        if(dsc->img_data) {
            lv_gpu_free((uint8_t *)dsc->img_data);
            dsc->img_data = NULL;
        }
    }
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
