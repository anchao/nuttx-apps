/**
 * @file vg_lite_draw_line.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "vg_lite_utils_path.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/

#define PLAIN_LINE_NUM 10

#define P(p) ((vg_lite_fpoint_t) { (p)->x, (p)->y })
#define PR(p) ((vg_lite_fpoint_t) { (p)->x + w2_dx, (p)->y - w2_dy })
#define PL(p) ((vg_lite_fpoint_t) { (p)->x - w2_dx, (p)->y + w2_dy })
#define PB(p) ((vg_lite_fpoint_t) { (p)->x - w2_dy, (p)->y - w2_dx })
#define PT(p) ((vg_lite_fpoint_t) { (p)->x + w2_dy, (p)->y + w2_dx })

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_draw_line(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_line_dsc_t * dsc,
    const lv_point_t * point1,
    const lv_point_t * point2)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;

    LV_GPU_LOG_TRACE("point: P1(%d, %d) -> P2(%d, %d)",
                     point1->x, point1->y, point2->x, point2->y);

    if(point1->x == point2->x || point1->y == point2->y) {
        LV_GPU_LOG_TRACE("horizontal/vertical line skipped");
        return;
    }

    lv_coord_t w = dsc->width;
    lv_coord_t dash_width = dsc->dash_width;
    lv_coord_t dash_gap = dsc->dash_gap;
    lv_coord_t dash_l = dash_width + dash_gap;

    float dx = point2->x - point1->x;
    float dy = point2->y - point1->y;
    float inv_dl = lv_gpu_fast_inv_sqrt(dx * dx + dy * dy);
    float w_dx = w * dy * inv_dl;
    float w_dy = w * dx * inv_dl;
    float w2_dx = w_dx / 2;
    float w2_dy = w_dy / 2;
    lv_coord_t num = 4;

    if(dsc->round_end) {
        num += 3;
    }

    if(dsc->round_start) {
        num += 3;
    }

    lv_coord_t ndash = 0;
    if(dash_width && dash_l * inv_dl < 1.0f) {
        ndash = (1.0f / inv_dl + dash_l - 1) / dash_l;
        num += ndash * 4;
    }

    vg_lite_fpoint_t points_tmp[PLAIN_LINE_NUM];
    vg_lite_curve_op_t op_tmp[PLAIN_LINE_NUM];

    vg_lite_fpoint_t * points = points_tmp;
    vg_lite_curve_op_t * op = op_tmp;

    if(num > PLAIN_LINE_NUM) {
        points = lv_mem_buf_get(sizeof(vg_lite_fpoint_t) * num);
        op = lv_mem_buf_get(sizeof(vg_lite_curve_op_t) * num);
    }

    points[0] = PR(point1);
    op[0] = CURVE_LINE;

    lv_coord_t i = 1;
    if(dsc->round_start) {
        op[0] = CURVE_ARC_90;
        op[i] = CURVE_NOP;
        points[i++] = P(point1);
        op[i] = CURVE_ARC_90;
        points[i++] = PB(point1);
        op[i] = CURVE_NOP;
        points[i++] = P(point1);
    }

    op[i] = CURVE_LINE;
    points[i++] = PL(point1);

    for(lv_coord_t j = 0; j < ndash; j++) {
        op[i] = CURVE_LINE;
        points[i++] = (vg_lite_fpoint_t) {
            point1->x - w2_dx + dx * (j * dash_l + dash_width) * inv_dl,
                   point1->y + w2_dy + dy * (j * dash_l + dash_width) * inv_dl
        };
        op[i] = CURVE_CLOSE;
        points[i++] = (vg_lite_fpoint_t) {
            point1->x + w2_dx + dx * (j * dash_l + dash_width) * inv_dl,
                   point1->y - w2_dy + dy * (j * dash_l + dash_width) * inv_dl
        };
        op[i] = CURVE_LINE;
        points[i++] = (vg_lite_fpoint_t) {
            point1->x + w2_dx + dx * (j + 1) * dash_l * inv_dl,
                   point1->y - w2_dy + dy * (j + 1) * dash_l * inv_dl
        };
        op[i] = CURVE_LINE;
        points[i++] = (vg_lite_fpoint_t) {
            point1->x - w2_dx + dx * (j + 1) * dash_l * inv_dl,
                   point1->y + w2_dy + dy * (j + 1) * dash_l * inv_dl
        };
    }

    op[i] = CURVE_LINE;

    points[i++] = PL(point2);

    if(dsc->round_end) {
        op[i - 1] = CURVE_ARC_90;
        op[i] = CURVE_NOP;
        points[i++] = P(point2);
        op[i] = CURVE_ARC_90;
        points[i++] = PT(point2);
        op[i] = CURVE_NOP;
        points[i++] = P(point2);
    }

    op[i] = CURVE_CLOSE;
    points[i] = PR(point2);

    vg_lite_curve_fill_dsc_t fill_dsc;
    vg_lite_curve_fill_dsc_init(&fill_dsc);
    fill_dsc.color = dsc->color;
    fill_dsc.opa = dsc->opa;
    fill_dsc.type = CURVE_FILL_COLOR;

    vg_lite_curve_dsc_t curve_dsc;
    lv_memset_00(&curve_dsc, sizeof(curve_dsc));
    curve_dsc.fpoints = points;
    curve_dsc.num = num;
    curve_dsc.op = op;
    curve_dsc.fill_dsc = &fill_dsc;

    lv_gpu_dest_buf_t gpu_buf = {
        .buf = draw_ctx->buf,
        .buf_area = draw_ctx->buf_area,
        .clip_area = (lv_area_t *)draw_ctx->clip_area,
        .cf = LV_IMG_CF_TRUE_COLOR
    };
    LV_GPU_DUMP_DEST_BUFFER_INFO(&gpu_buf);

    gpu->draw_ok = vg_lite_draw_curve(&gpu_buf, &curve_dsc);

    if(num > PLAIN_LINE_NUM) {
        lv_mem_buf_release(points);
        lv_mem_buf_release(op);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
