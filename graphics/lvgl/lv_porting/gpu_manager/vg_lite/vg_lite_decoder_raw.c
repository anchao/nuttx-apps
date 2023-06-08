/**
 * @file vg_lite_decoder_raw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_conv.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "vg_lite_decoder.h"
#include "vg_lite_utils.h"

/*********************
 *      DEFINES
 *********************/

#define IS_BUILT_IN_CF(cf) ((cf) >= LV_IMG_CF_TRUE_COLOR && (cf) <= LV_IMG_CF_ALPHA_8BIT)

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

static lv_res_t decode_rgb(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);
static lv_res_t decode_indexed(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_decoder_raw_init(lv_img_decoder_t * dec)
{
    lv_img_decoder_set_info_cb(dec, decoder_info);
    lv_img_decoder_set_open_cb(dec, decoder_open);
    lv_img_decoder_set_close_cb(dec, decoder_close);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_res_t decoder_info(lv_img_decoder_t * decoder, const void * src, lv_img_header_t * header)
{
    LV_UNUSED(decoder); /*Unused*/

    lv_img_src_t src_type = lv_img_src_get_type(src);

    if(src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = src;
        lv_img_cf_t cf = img_dsc->header.cf;

        if(!IS_BUILT_IN_CF(cf)) {
            return LV_RES_INV;
        }

        header->w = img_dsc->header.w;
        header->h = img_dsc->header.h;
        header->cf = img_dsc->header.cf;

        return LV_RES_OK;
    }

    if(src_type == LV_IMG_SRC_FILE) {
        /*Support only "*.bin", and ".gpu" files*/
        const char * ext = lv_fs_get_ext(src);
        if(!(strcmp(ext, "bin") == 0 || strcmp(ext, "gpu") == 0)) {
            return LV_RES_INV;
        }

        lv_fs_file_t f;
        lv_fs_res_t res = lv_fs_open(&f, src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_GPU_LOG_INFO("GPU decoder open %s failed", (const char *)src);
            return LV_RES_INV;
        }

        uint32_t rn;
        res = lv_fs_read(&f, header, sizeof(lv_img_header_t), &rn);
        lv_fs_close(&f);
        if(res != LV_FS_RES_OK || rn != sizeof(lv_img_header_t)) {
            LV_GPU_LOG_WARN("Image get info read file header failed");
            return LV_RES_INV;
        }

        return LV_RES_OK;
    }

    if(src_type == LV_IMG_SRC_SYMBOL) {
        /* The size depend on the font but it is unknown here. It should be handled
         * outside of the function */
        header->w = 1;
        header->h = 1;
        /* Symbols always have transparent parts. Important because of cover check
         * in the draw function. The actual value doesn't matter because
         * lv_draw_label will draw it */
        header->cf = LV_IMG_CF_ALPHA_1BIT;
        return LV_RES_OK;
    }

    LV_GPU_LOG_WARN("Image get info found unknown src type");
    return LV_RES_INV;
}

static lv_res_t decoder_open(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    lv_img_cf_t cf = dsc->header.cf;
    const lv_img_dsc_t * img_dsc = dsc->src;

    /* check if it's already decoded, if so, return directly */
    if(dsc->src_type == LV_IMG_SRC_VARIABLE) {
        vg_lite_img_header_t * header = (vg_lite_img_header_t *)img_dsc->data;
        if(header->magic == VG_LITE_IMAGE_MAGIC_NUM) {
            /* already decoded, just pass the pointer */
            LV_GPU_LOG_INFO("%" LV_PRIx32 " already decoded %p @ %p", header->magic,
                            header->vg_buf.memory, header);
            dsc->img_data = img_dsc->data;
            dsc->user_data = NULL;
            return LV_RES_OK;
        }
    }
    else if(dsc->src_type == LV_IMG_SRC_FILE) {
        /* let's process "gpu" file firstly. */
        if(strcmp(lv_fs_get_ext(dsc->src), "gpu") == 0) {
            /* No need to decode gpu file, simply load it to ram */
            lv_fs_file_t f;
            lv_fs_res_t res = lv_fs_open(&f, dsc->src, LV_FS_MODE_RD);
            if(res != LV_FS_RES_OK) {
                LV_GPU_LOG_WARN("gpu_decoder can't open the file");
                return LV_RES_INV;
            }

            /* alloc new buffer that meets GPU requirements(width, alignment) */
            lv_img_dsc_t * gpu_dsc = vg_lite_img_dsc_create(dsc->header.w, dsc->header.h, dsc->header.cf);
            if(gpu_dsc == NULL) {
                LV_GPU_LOG_ERROR("out of memory");
                lv_fs_close(&f);
                return LV_RES_INV;
            }
            uint8_t * gpu_data = (uint8_t *)gpu_dsc->data;

            lv_fs_seek(&f, 4, LV_FS_SEEK_SET); /* skip file header. */
            res = lv_fs_read(&f, gpu_data, gpu_dsc->data_size, NULL);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK) {
                vg_lite_img_dsc_del(gpu_dsc);
                LV_GPU_LOG_ERROR("file read failed");
                return LV_RES_INV;
            }
            dsc->img_data = gpu_data;
            dsc->user_data = (void *)VG_LITE_IMAGE_MAGIC_NUM;

            vg_lite_img_dsc_update_header(gpu_dsc);
            lv_gpu_free(gpu_dsc); /* ??????????????? */

            return LV_RES_OK;
        }
    }

    /*GPU hasn't processed, decode now. */

    /*Process true color formats*/
    if(cf == LV_IMG_CF_TRUE_COLOR || cf == LV_IMG_CF_TRUE_COLOR_ALPHA
       || cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
        lv_res_t ret = decode_rgb(decoder, dsc);
        LV_GPU_LOG_TRACE("decode_rgb: img_data = %p, size %d x %d, cf = %d, ret = %d",
                         dsc->img_data, (int)dsc->header.w, (int)dsc->header.h, (int)dsc->header.cf, ret);
        return ret;
    }

    if((cf >= LV_IMG_CF_INDEXED_1BIT && cf <= LV_IMG_CF_INDEXED_8BIT)
       || cf == LV_IMG_CF_ALPHA_8BIT || cf == LV_IMG_CF_ALPHA_4BIT) {
        lv_res_t ret = decode_indexed(decoder, dsc);
        LV_GPU_LOG_TRACE("decode_indexed: img_data = %p, size %d x %d, cf = %d, ret = %d",
                         dsc->img_data, (int)dsc->header.w, (int)dsc->header.h, (int)dsc->header.cf, ret);
        return ret;
    }

    /*Unknown format. Can't decode it.*/
    LV_GPU_LOG_WARN("unsupported color format %d", cf);
    return LV_RES_INV;
}

static void decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder); /*Unused*/
    if(dsc->img_data == NULL)
        return;

    if((uint32_t)dsc->user_data == VG_LITE_IMAGE_MAGIC_NUM) {
        vg_lite_img_free((void *)dsc->img_data);
    }

    dsc->img_data = NULL;
}

static uint32_t bit_rev8(uint32_t x)
{
    uint32_t y = 0;
    asm("rbit %0, %1"
        : "=r"(y)
        : "r"(x));
    asm("rev %0, %1"
        : "=r"(y)
        : "r"(y));
    return y;
}

static void bit_rev(uint8_t px_size, uint8_t * buf, uint32_t stride)
{
    switch(px_size) {
        case 1:
            for(int_fast16_t i = 0; i < (stride >> 2); i++) {
                ((uint32_t *)buf)[i] = bit_rev8(((uint32_t *)buf)[i]);
            }
            uint8_t tail = stride & 3;
            if(tail) {
                uint32_t r = bit_rev8(((uint32_t *)buf)[stride >> 2]);
                for(uint8_t i = 0; i < tail; i++) {
                    buf[stride - tail + i] = r >> (i << 3) & 0xFF;
                }
            }
            break;
        case 2:
            for(int_fast16_t i = 0; i < stride; i++) {
                buf[i] = buf[i] << 6 | (buf[i] << 2 & 0x30)
                         | (buf[i] >> 2 & 0x0C) | buf[i] >> 6;
            }
            break;
        case 4:
            for(int_fast16_t i = 0; i < stride; i++) {
                buf[i] = buf[i] << 4 | buf[i] >> 4;
            }
            break;
        case 8:
        default:
            break;
    }
}

static lv_res_t decode_rgb(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_GPU_LOG_TRACE("start");
    const uint8_t * img_data = NULL; /* points to the input image */
    lv_fs_file_t f;
    uint8_t * fs_buf = NULL;
    uint32_t data_size;
    bool no_processing = dsc->header.cf == LV_IMG_CF_TRUE_COLOR
                         && VG_LITE_IS_ALIGNED(dsc->header.w, VG_LITE_IMG_SRC_PX_ALIGN);
    /*Open the file if it's a file*/
    if(dsc->src_type == LV_IMG_SRC_FILE) {
        LV_GPU_LOG_WARN("opening %s", (const char *)dsc->src);
        const char * ext = lv_fs_get_ext(dsc->src);
        /*Support only "*.bin" files*/
        if(strcmp(ext, "bin") != 0) {
            LV_GPU_LOG_WARN("can't open %s", (const char *)dsc->src);
            return LV_RES_INV;
        }

        lv_fs_res_t res = lv_fs_open(&f, dsc->src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_GPU_LOG_WARN("gpu_decoder can't open the file");
            return LV_RES_INV;
        }

        data_size = lv_img_cf_get_px_size(dsc->header.cf);
        data_size *= dsc->header.w * dsc->header.h;
        data_size >>= 3; /* bits to bytes */
        if(!no_processing) {
            fs_buf = lv_gpu_malloc(data_size);
            if(fs_buf == NULL) {
                LV_GPU_LOG_ERROR("out of memory");
                lv_fs_close(&f);
                return LV_RES_INV;
            }

            /*Skip the header*/
            lv_fs_seek(&f, sizeof(lv_img_header_t), LV_FS_SEEK_SET);
            res = lv_fs_read(&f, fs_buf, data_size, NULL);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK) {
                lv_gpu_free(fs_buf);
                LV_GPU_LOG_ERROR("file read failed");
                return LV_RES_INV;
            }

            img_data = fs_buf;
        }
    }
    else if(dsc->src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t * img_dsc = dsc->src;
        /*The variables should have valid data*/
        if(img_dsc->data == NULL) {
            LV_GPU_LOG_WARN("no data");
            return LV_RES_INV;
        }
        img_data = img_dsc->data;
        data_size = img_dsc->data_size;
    }
    else {
        /* No way to get the image data, return invalid */
        return LV_RES_INV;
    }

    /* alloc new buffer that meets GPU requirements(width, alignment) */
    uint8_t * gpu_data;
    gpu_data = vg_lite_img_alloc(dsc->header.w, dsc->header.h, dsc->header.cf, NULL);
    if(gpu_data == NULL) {
        LV_GPU_LOG_ERROR("out of memory");
        if(fs_buf) {
            /* release file cache */
            lv_gpu_free(fs_buf);
        }
        return LV_RES_INV;
    }

    dsc->user_data = (void *)VG_LITE_IMAGE_MAGIC_NUM;
    /* add gpu header right at beginning of gpu image buffer */
    vg_lite_img_header_t * header = (vg_lite_img_header_t *)gpu_data;
    header->magic = VG_LITE_IMAGE_MAGIC_NUM;
    header->recolor = dsc->color.full;
    dsc->img_data = gpu_data;
    lv_res_t ret = LV_RES_OK;

    uint8_t * gpu_data_buf = gpu_data + sizeof(vg_lite_img_header_t);
    LV_GPU_LOG_TRACE("gpu_data_buf = %p", gpu_data_buf);
    if(dsc->src_type == LV_IMG_SRC_FILE && no_processing) {
        LV_GPU_LOG_TRACE("no processing");
        lv_fs_seek(&f, 4, LV_FS_SEEK_SET);
        lv_fs_res_t res = lv_fs_read(&f, gpu_data_buf, data_size, NULL);
        lv_fs_close(&f);
        if(res != LV_FS_RES_OK) {
            lv_gpu_free(gpu_data);
            LV_GPU_LOG_ERROR("file read failed");
            return LV_RES_INV;
        }
        vg_lite_custom_buffer_init(
            &header->vg_buf,
            gpu_data_buf,
            dsc->header.w,
            dsc->header.h,
            VG_LITE_NATIVE_COLOR_FMT);
        VG_LITE_DUMP_BUFFER_INFO(&header->vg_buf);
    }
    else {
        LV_GPU_LOG_TRACE("create vg buf");
        ret = vg_lite_create_vg_buf_from_img_data(
                  &header->vg_buf,
                  img_data,
                  &dsc->header,
                  gpu_data_buf,
                  dsc->color,
                  false)
              ? LV_RES_OK
              : LV_RES_INV;
    }

    if(fs_buf) {
        /* file cache is no longger needed. */
        lv_gpu_free(fs_buf);
    }

    return ret;
}

static lv_res_t decode_indexed(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    LV_GPU_LOG_TRACE("start");
    lv_fs_file_t f;
    lv_img_cf_t cf = dsc->header.cf;
    const lv_img_dsc_t * img_dsc = dsc->src;
    /*Open the file if it's a file*/
    if(dsc->src_type == LV_IMG_SRC_FILE) {
        /*Support only "*.bin" files*/
        LV_GPU_LOG_WARN("opening %s", (const char *)dsc->src);
        if(strcmp(lv_fs_get_ext(dsc->src), "bin")) {
            LV_GPU_LOG_WARN("can't open %s", (const char *)dsc->src);
            return LV_RES_INV;
        }

        lv_fs_res_t res = lv_fs_open(&f, dsc->src, LV_FS_MODE_RD);
        if(res != LV_FS_RES_OK) {
            LV_GPU_LOG_WARN("gpu_decoder can't open the file");
            return LV_RES_INV;
        }
        /*Skip the header*/
        lv_fs_seek(&f, 4, LV_FS_SEEK_SET);
    }
    else if(dsc->src_type == LV_IMG_SRC_VARIABLE) {
        /*The variables should have valid data*/
        if(img_dsc->data == NULL) {
            LV_GPU_LOG_WARN("no data");
            return LV_RES_INV;
        }
    }

    uint8_t * gpu_data;
    gpu_data = vg_lite_img_alloc(dsc->header.w, dsc->header.h, dsc->header.cf, NULL);
    if(gpu_data == NULL) {
        LV_GPU_LOG_ERROR("out of memory");
        return LV_RES_INV;
    }

    dsc->user_data = (void *)VG_LITE_IMAGE_MAGIC_NUM;
    vg_lite_img_header_t * header = (vg_lite_img_header_t *)gpu_data;
    header->magic = VG_LITE_IMAGE_MAGIC_NUM;
    header->recolor = dsc->color.full;
    dsc->img_data = gpu_data;

    bool indexed = (cf >= LV_IMG_CF_INDEXED_1BIT && cf <= LV_IMG_CF_INDEXED_8BIT);
    uint8_t px_size = lv_img_cf_get_px_size(dsc->header.cf);
    int32_t img_w = dsc->header.w;
    int32_t img_h = dsc->header.h;
    int32_t vgbuf_w = VG_LITE_ALIGN(img_w, vg_lite_img_buf_get_align_width(cf));
    vg_lite_buffer_format_t vgbuf_format = vg_lite_img_cf_to_vg_fmt(dsc->header.cf);
    int32_t vgbuf_stride = vgbuf_w * px_size >> 3;
    int32_t map_stride = (img_w * px_size + 7) >> 3;
    uint32_t vgbuf_data_size = vgbuf_stride * img_h;
    uint32_t palette_size = 1 << px_size;
    uint32_t * palette = (uint32_t *)(gpu_data + sizeof(vg_lite_img_header_t)
                                      + vgbuf_data_size);
    lv_color32_t * palette_p = NULL;
    if(indexed) {
        if(dsc->src_type == LV_IMG_SRC_FILE) {
            /*Read the palette from file*/
            lv_fs_res_t res = lv_fs_read(&f, palette,
                                         sizeof(lv_color32_t) * palette_size, NULL);
            if(res != LV_FS_RES_OK) {
                LV_GPU_LOG_ERROR("file read failed");
                lv_fs_close(&f);
                lv_gpu_free(gpu_data);
                return LV_RES_INV;
            }
            palette_p = (lv_color32_t *)palette;
        }
        else {
            /*The palette is in the beginning of the image data. Just point to it.*/
            palette_p = (lv_color32_t *)img_dsc->data;
        }
    }
    lv_gpu_conv_recolor_palette_dsc_t conv_dsc;
    lv_memset_00(&conv_dsc, sizeof(conv_dsc));
    conv_dsc.dst = (lv_color32_t *)palette;
    conv_dsc.src = palette_p;
    conv_dsc.size = palette_size;
    conv_dsc.recolor = dsc->color.full;
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_RECOLOR_PALETTE, &conv_dsc);

    vg_lite_buffer_t * vgbuf = &header->vg_buf;

    void * mem = gpu_data + sizeof(vg_lite_img_header_t);
    vg_lite_custom_buffer_init(vgbuf, mem, vgbuf_w, img_h, vgbuf_format);

    uint8_t * px_buf = vgbuf->memory;
    const uint8_t * px_map = img_dsc->data;
    if(indexed) {
        px_map += palette_size * sizeof(lv_color32_t);
    }

    uint32_t data_size = map_stride * img_h;
    if(map_stride == vgbuf_stride) {
        if(dsc->src_type == LV_IMG_SRC_FILE) {
            lv_fs_res_t res = lv_fs_read(&f, px_buf, data_size, NULL);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK) {
                lv_gpu_free(gpu_data);
                LV_GPU_LOG_ERROR("file read failed");
                return LV_RES_INV;
            }
        }
        else {
            lv_memcpy(px_buf, px_map, data_size);
        }
        bit_rev(px_size, px_buf, data_size);
    }
    else {
        uint8_t * fs_buf = NULL;
        if(dsc->src_type == LV_IMG_SRC_FILE) {
            fs_buf = lv_gpu_malloc(data_size);
            if(fs_buf == NULL) {
                LV_GPU_LOG_ERROR("out of memory");
                lv_fs_close(&f);
                lv_gpu_free(gpu_data);
                return LV_RES_INV;
            }
            lv_fs_res_t res = lv_fs_read(&f, fs_buf, data_size, NULL);
            lv_fs_close(&f);
            if(res != LV_FS_RES_OK) {
                lv_gpu_free(gpu_data);
                lv_gpu_free(fs_buf);
                LV_GPU_LOG_ERROR("file read failed");
                return LV_RES_INV;
            }
            px_map = fs_buf;
        }
        uint8_t zero_id = 0;
        while(zero_id < palette_size && palette[zero_id]) {
            zero_id++;
        }
        if(zero_id == palette_size) {
            zero_id = 0;
            if(map_stride < vgbuf_stride) {
                LV_GPU_LOG_ERROR("no transparent found in palette but padding required!");
            }
        }
        const uint8_t multiplier[4] = { 0xFF, 0x55, 0x11, 0x01 };
        uint8_t padding = zero_id * multiplier[__builtin_ctz(px_size)];
        for(int_fast16_t i = 0; i < img_h; i++) {
            lv_memcpy(px_buf, px_map, map_stride);
            lv_memset(px_buf + map_stride, padding, vgbuf_stride - map_stride);
            bit_rev(px_size, px_buf, map_stride);
            px_map += map_stride;
            px_buf += vgbuf_stride;
        }
        if(fs_buf) {
            lv_gpu_free(fs_buf);
        }
    }
    return LV_RES_OK;
}

#endif
