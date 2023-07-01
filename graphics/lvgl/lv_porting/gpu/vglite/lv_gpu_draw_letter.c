/**
 * @file vg_lite_draw_letter.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_draw_utils.h"
#include "lv_porting/lv_gpu_interface.h"
#include <float.h>

#ifdef CONFIG_LV_GPU_DRAW_LETTER

#include "vg_lite.h"

#define VLC_SET_OP_CODE(data, op_code) (*((uint16_t*)(&data)) = (op_code))

#define PATH_QUALITY VG_LITE_MEDIUM
#define PATH_DATA_COORD_FORMAT VG_LITE_S16
#define FT_F26Dot6_SHIFT 6

/** Keep the original 26dot6 data to ensure accuracy, and the reference size of the font is 128,
 * so it can be guaranteed that it will not exceed the upper limit of int16
 */
#define FT_F26Dot6_TO_PATH_DATA(x) ((path_data_t)(x))

/** After converting the font reference size, it is also necessary to scale the 26dot6 data
 * in the path to the real physical size
 */
#define FT_F26Dot6_TO_PATH_SCALE(x) (LV_FREETYPE_F26Dot6_TO_FLOAT(x) / (1 << FT_F26Dot6_SHIFT))

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef int16_t path_data_t;

typedef struct {
    lv_freetype_outline_t base;
    path_data_t* cur_ptr;
    vg_lite_path_t path;
} vg_lite_outline_t;

typedef struct {
    vg_lite_float_t x;
    vg_lite_float_t y;
} point_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static lv_res_t draw_letter_normal(lv_draw_ctx_t* draw_ctx, const lv_draw_label_dsc_t* dsc,
    const lv_point_t* pos_p, lv_font_glyph_dsc_t* g, uint32_t letter);

static void* freetype_outline_event_cb(lv_freetype_event_dsc_t* event_dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_letter_gpu_init(lv_draw_ctx_t* draw_ctx)
{
    lv_freetype_outline_add_generator(freetype_outline_event_cb, draw_ctx);
}

lv_res_t lv_draw_letter_gpu(
    lv_draw_ctx_t* draw_ctx,
    const lv_draw_label_dsc_t* dsc,
    const lv_point_t* pos_p,
    uint32_t letter)
{
    lv_font_glyph_dsc_t g;
    bool g_ret = lv_font_get_glyph_dsc(dsc->font, &g, letter, '\0');
    if (!g_ret) {
        /*Add warning if the dsc is not found
         *but do not print warning for non printable ASCII chars (e.g. '\n')*/
        if (letter >= 0x20 && letter != 0xf8ff && /*LV_SYMBOL_DUMMY*/
            letter != 0x200c) { /*ZERO WIDTH NON-JOINER*/
            LV_LOG_INFO("lv_draw_letter: glyph dsc. not found for U+%" LV_PRIX32, letter);

#if LV_USE_FONT_PLACEHOLDER
            /* draw placeholder */
            lv_area_t glyph_coords;
            lv_draw_rect_dsc_t glyph_dsc;
            lv_coord_t begin_x = pos_p->x + g.ofs_x;
            lv_coord_t begin_y = pos_p->y + g.ofs_y;
            lv_area_set(&glyph_coords, begin_x, begin_y, begin_x + g.box_w, begin_y + g.box_h);
            lv_draw_rect_dsc_init(&glyph_dsc);
            glyph_dsc.bg_opa = LV_OPA_MIN;
            glyph_dsc.outline_opa = LV_OPA_MIN;
            glyph_dsc.shadow_opa = LV_OPA_MIN;
            glyph_dsc.bg_img_opa = LV_OPA_MIN;
            glyph_dsc.border_color = dsc->color;
            glyph_dsc.border_width = 1;
            draw_ctx->draw_rect(draw_ctx, &glyph_dsc, &glyph_coords);
#endif
        }
        return LV_RES_OK;
    }

    /*Don't draw anything if the character is empty. E.g. space*/
    if ((g.box_h == 0) || (g.box_w == 0))
        return LV_RES_OK;

    lv_coord_t real_h;
#if LV_USE_IMGFONT
    if (g.bpp == LV_IMGFONT_BPP) {
        /*Center imgfont's drawing position*/
        real_h = (dsc->font->line_height - g.box_h) / 2;
    } else
#endif
    {
        real_h = (dsc->font->line_height - dsc->font->base_line) - g.box_h;
    }

    lv_point_t gpos;
    gpos.x = pos_p->x + g.ofs_x;
    gpos.y = pos_p->y + real_h - g.ofs_y;

    /*If the letter is completely out of mask don't draw it*/
    if (gpos.x + g.box_w < draw_ctx->clip_area->x1 || gpos.x > draw_ctx->clip_area->x2
        || gpos.y + g.box_h < draw_ctx->clip_area->y1 || gpos.y > draw_ctx->clip_area->y2) {
        return LV_RES_OK;
    }

    return draw_letter_normal(draw_ctx, dsc, &gpos, &g, letter);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void matrix_flip_y(vg_lite_matrix_t* matrix)
{
    matrix->m[1][1] = -matrix->m[1][1];
}

static point_data_t matrix_get_transform_point(const vg_lite_matrix_t* matrix, const point_data_t* point)
{
    point_data_t p;
    p.x = point->x * matrix->m[0][0] + point->y * matrix->m[0][1] + matrix->m[0][2];
    p.y = point->x * matrix->m[1][0] + point->y * matrix->m[1][1] + matrix->m[1][2];
    return p;
}

/* Get the inversion of a matrix. */
static bool matrix_inverse(vg_lite_matrix_t* result, vg_lite_matrix_t* matrix)
{
    vg_lite_float_t det00, det01, det02;
    vg_lite_float_t d;
    bool is_affine;

    /* Test for identity matrix. */
    if (matrix == NULL) {
        result->m[0][0] = 1.0f;
        result->m[0][1] = 0.0f;
        result->m[0][2] = 0.0f;
        result->m[1][0] = 0.0f;
        result->m[1][1] = 1.0f;
        result->m[1][2] = 0.0f;
        result->m[2][0] = 0.0f;
        result->m[2][1] = 0.0f;
        result->m[2][2] = 1.0f;

        /* Success. */
        return true;
    }

    det00 = (matrix->m[1][1] * matrix->m[2][2]) - (matrix->m[2][1] * matrix->m[1][2]);
    det01 = (matrix->m[2][0] * matrix->m[1][2]) - (matrix->m[1][0] * matrix->m[2][2]);
    det02 = (matrix->m[1][0] * matrix->m[2][1]) - (matrix->m[2][0] * matrix->m[1][1]);

    /* Compute determinant. */
    d = (matrix->m[0][0] * det00) + (matrix->m[0][1] * det01) + (matrix->m[0][2] * det02);

    /* Return 0 if there is no inverse matrix. */
    if (d == 0.0f)
        return false;

    /* Compute reciprocal. */
    d = 1.0f / d;

    /* Determine if the matrix is affine. */
    is_affine = (matrix->m[2][0] == 0.0f) && (matrix->m[2][1] == 0.0f) && (matrix->m[2][2] == 1.0f);

    result->m[0][0] = d * det00;
    result->m[0][1] = d * ((matrix->m[2][1] * matrix->m[0][2]) - (matrix->m[0][1] * matrix->m[2][2]));
    result->m[0][2] = d * ((matrix->m[0][1] * matrix->m[1][2]) - (matrix->m[1][1] * matrix->m[0][2]));
    result->m[1][0] = d * det01;
    result->m[1][1] = d * ((matrix->m[0][0] * matrix->m[2][2]) - (matrix->m[2][0] * matrix->m[0][2]));
    result->m[1][2] = d * ((matrix->m[1][0] * matrix->m[0][2]) - (matrix->m[0][0] * matrix->m[1][2]));
    result->m[2][0] = is_affine ? 0.0f : d * det02;
    result->m[2][1] = is_affine ? 0.0f : d * ((matrix->m[2][0] * matrix->m[0][1]) - (matrix->m[0][0] * matrix->m[2][1]));
    result->m[2][2] = is_affine ? 1.0f : d * ((matrix->m[0][0] * matrix->m[1][1]) - (matrix->m[1][0] * matrix->m[0][1]));

    /* Success. */
    return true;
}

static vg_lite_color_t vg_color_make(lv_color_t color, lv_opa_t opa)
{
    lv_color32_t c32;
    c32.full = lv_color_to32(color);
    if (opa < LV_OPA_MAX) {
        c32.ch.red = LV_UDIV255(c32.ch.red * opa);
        c32.ch.green = LV_UDIV255(c32.ch.green * opa);
        c32.ch.blue = LV_UDIV255(c32.ch.blue * opa);
    }
    return (uint32_t)opa << 24 | (uint32_t)c32.ch.blue << 16 | (uint32_t)c32.ch.green << 8 | c32.ch.red;
}

static lv_res_t draw_letter_normal(lv_draw_ctx_t* draw_ctx, const lv_draw_label_dsc_t* dsc,
    const lv_point_t* pos_p, lv_font_glyph_dsc_t* g, uint32_t letter)
{
    vg_lite_error_t vgerr;
    vg_lite_outline_t* outline;
    const lv_font_t* font_p = g->resolved_font;

    outline = (vg_lite_outline_t*)lv_freetype_outline_lookup((lv_font_t*)font_p, letter);

    if (!outline) {
        LV_LOG_WARN("get outline failed");
        return LV_RES_INV;
    }

    /* calc convert matrix */
    float scale = FT_F26Dot6_TO_PATH_SCALE(lv_freetype_outline_get_scale(font_p));
    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);

    /* convert to vg-lite coordinate */
    vg_lite_translate(pos_p->x - g->ofs_x, pos_p->y + g->box_h + g->ofs_y, &matrix);

    /* scale size */
    vg_lite_scale(scale, scale, &matrix);

    /* Cartesian coordinates to LCD coordinates */
    matrix_flip_y(&matrix);

    /* get clip area */
    lv_area_t glpyh_area;
    lv_area_set(&glpyh_area,
        pos_p->x, pos_p->y,
        pos_p->x + g->box_w, pos_p->y + g->box_h);

    lv_area_t path_clip_area;
    bool union_ok = _lv_area_intersect(&path_clip_area, draw_ctx->clip_area, &glpyh_area);
    if (!union_ok) {
        return LV_RES_OK;
    }

    /* vg-lite bounding_box will crop the pixels on the edge, so +1px is needed here */
    path_clip_area.x2++;
    path_clip_area.y2++;

    /* calc inverse matrix */
    vg_lite_matrix_t result;
    if (!matrix_inverse(&result, &matrix)) {
        LV_LOG_ERROR("no inverse matrix");
        return LV_RES_INV;
    }

    point_data_t p1 = { path_clip_area.x1, path_clip_area.y1 };
    point_data_t p1_res = matrix_get_transform_point(&result, &p1);

    point_data_t p2 = { path_clip_area.x2, path_clip_area.y2 };
    point_data_t p2_res = matrix_get_transform_point(&result, &p2);

    /* Since the font uses Cartesian coordinates, the y coordinates need to be reversed */
    outline->path.bounding_box[0] = p1_res.x;
    outline->path.bounding_box[1] = p2_res.y;
    outline->path.bounding_box[2] = p2_res.x;
    outline->path.bounding_box[3] = p1_res.y;

    /* Avoid errors when vg-lite bonding_box area is 0 */
    if (p2_res.x - p1_res.x < FLT_EPSILON || p1_res.y - p2_res.y < FLT_EPSILON) {
        LV_LOG_ERROR("path_clip_area: %d, %d, %d, %d",
            (int)path_clip_area.x1,
            (int)path_clip_area.y1,
            (int)path_clip_area.x2,
            (int)path_clip_area.y2);
        LV_LOG_ERROR("path.bounding_box: %f, %f, %f, %f",
            outline->path.bounding_box[0],
            outline->path.bounding_box[1],
            outline->path.bounding_box[2],
            outline->path.bounding_box[3]);
        return LV_RES_INV;
    }

    /* Move to the position relative to the first address of the buffer */
    vg_lite_translate(-draw_ctx->buf_area->x1 / scale, draw_ctx->buf_area->y1 / scale, &matrix);

    vg_lite_buffer_t dest_vg_buf;

    if (!init_vg_buf(
            &dest_vg_buf,
            lv_area_get_width(draw_ctx->buf_area),
            lv_area_get_height(draw_ctx->buf_area),
            lv_area_get_width(draw_ctx->buf_area) * sizeof(lv_color_t),
            draw_ctx->buf,
            BPP_TO_VG_FMT(LV_COLOR_DEPTH),
            false)) {
        LV_LOG_WARN("dest_vg_buf init failed");
        return LV_RES_INV;
    }

    CHECK_ERROR(vg_lite_draw(
        &dest_vg_buf, &outline->path, VG_LITE_FILL_EVEN_ODD,
        &matrix, VG_LITE_BLEND_SRC_OVER, vg_color_make(dsc->color, dsc->opa)));
    CHECK_ERROR(gpu_flush());

    return LV_RES_OK;
}

static lv_freetype_outline_t* vg_lite_outline_create(lv_freetype_event_dsc_t* event_dsc)
{
    vg_lite_error_t vgerr;

    int path_data_len = event_dsc->param.outline_create.counter.cmd
        + event_dsc->param.outline_create.counter.vector * 2;
    if (path_data_len <= 0) {
        LV_LOG_ERROR("path_data_len = %d", path_data_len);
        return NULL;
    }

    vg_lite_outline_t* outline = lv_mem_alloc(sizeof(vg_lite_outline_t));
    LV_ASSERT_MALLOC(outline);
    if (!outline) {
        LV_LOG_WARN("outline alloc failed");
        return NULL;
    }
    memset(outline, 0, sizeof(vg_lite_outline_t));

    size_t path_data_size = path_data_len * sizeof(path_data_t);

    path_data_t* path_data = lv_mem_alloc(path_data_size);
    LV_ASSERT_MALLOC(path_data);
    if (!path_data) {
        LV_LOG_WARN("path_data alloc failed");
        lv_mem_free(outline);
        return NULL;
    }
    lv_memset_00(path_data, path_data_size);

    outline->cur_ptr = path_data;

    CHECK_ERROR(vg_lite_init_path(
        &outline->path,
        PATH_DATA_COORD_FORMAT,
        PATH_QUALITY,
        path_data_size,
        path_data,
        0, 0, 0, 0));

    return &outline->base;
}

static void vg_lite_outline_delete(lv_freetype_event_dsc_t* event_dsc)
{
    vg_lite_outline_t* outline = (vg_lite_outline_t*)event_dsc->param.outline_delete.outline;
    LV_ASSERT_NULL(outline);
    LV_ASSERT_NULL(outline->path.path);
    if (outline) {
        lv_mem_free(outline->path.path);
        lv_mem_free(outline);
    }
}

static void vg_lite_outline_push(lv_freetype_event_dsc_t* event_dsc)
{
    vg_lite_outline_t* outline = (vg_lite_outline_t*)event_dsc->param.outline_push.outline;
    LV_ASSERT_NULL(outline);
    LV_ASSERT_NULL(outline->path.path);

    int path_data_len = outline->path.path_length / sizeof(path_data_t);
    int offset = outline->cur_ptr - ((path_data_t*)outline->path.path);

    if (offset >= path_data_len) {
        LV_LOG_ERROR("offset = %d, path_data_len = %d, overflow!",
            offset, path_data_len);
        return;
    }

    lv_freetype_outline_type_t type = event_dsc->param.outline_push.type;
    switch (type) {
    case LV_FREETYPE_OUTLINE_END:
        VLC_SET_OP_CODE(*outline->cur_ptr++, VLC_OP_END);
        break;
    case LV_FREETYPE_OUTLINE_MOVE_TO:
        VLC_SET_OP_CODE(*outline->cur_ptr++, VLC_OP_MOVE);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.y);
        break;
    case LV_FREETYPE_OUTLINE_LINE_TO:
        VLC_SET_OP_CODE(*outline->cur_ptr++, VLC_OP_LINE);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.y);
        break;
    case LV_FREETYPE_OUTLINE_CUBIC_TO:
        VLC_SET_OP_CODE(*outline->cur_ptr++, VLC_OP_CUBIC);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.y);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control1.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control1.y);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control2.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control2.y);
        break;
    case LV_FREETYPE_OUTLINE_CONIC_TO:
        VLC_SET_OP_CODE(*outline->cur_ptr++, VLC_OP_QUAD);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control1.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.control1.y);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.x);
        *outline->cur_ptr++ = FT_F26Dot6_TO_PATH_DATA(event_dsc->param.outline_push.to.y);
        break;
    default:
        LV_LOG_WARN("unknown point type: %d", type);
        break;
    }

    offset = outline->cur_ptr - ((path_data_t*)outline->path.path);
    LV_ASSERT(offset <= path_data_len);
}

static void* freetype_outline_event_cb(lv_freetype_event_dsc_t* event_dsc)
{
    void* retval = NULL;
    switch (event_dsc->code) {
    case LV_FREETYPE_EVENT_OUTLINE_CREATE:
        retval = vg_lite_outline_create(event_dsc);
        break;
    case LV_FREETYPE_EVENT_OUTLINE_DELETE:
        vg_lite_outline_delete(event_dsc);
        break;
    case LV_FREETYPE_EVENT_OUTLINE_PUSH:
        vg_lite_outline_push(event_dsc);
        break;
    default:
        LV_LOG_WARN("unknown event code: %d", event_dsc->code);
        break;
    }
    return retval;
}

#endif /* CONFIG_LV_GPU_DRAW_LETTER */
