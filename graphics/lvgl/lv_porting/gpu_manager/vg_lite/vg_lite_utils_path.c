/**
 * @file vg_lite_utils_path.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "vg_lite_utils.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "lvgl/src/misc/lv_gc.h"
#include "vg_lite_internal.h"
#include "vg_lite_utils_path.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/

#define RECT_PATH_LEN 13 /* 3(MOVE) + 3(LINE) * 3 + 1(CLOSE/END) */
#define POINT_PATH_LEN 41 /* 3(MOVE) + 3(LINE) * 3 + 7(CUBIC) * 4 + 1(CLOSE/END) */

#define ARC_MAX_POINTS 25
#define PI_DEG (M_PI / 180.0f)

#define __PF(x, y) ((vg_lite_fpoint_t) { (x), (y) }) /* float */
#define __PT(p, dx, dy) __PF((p)->x + (dy), (p)->y + (dx)) /* top */
#define __PB(p, dx, dy) __PF((p)->x - (dy), (p)->y - (dx)) /* bottom */
#define __PL(p, dx, dy) __PF((p)->x - (dx), (p)->y + (dy)) /* left */
#define __PR(p, dx, dy) __PF((p)->x + (dx), (p)->y - (dy)) /* right */
#define __PO(p, dx, dy) __PF((p)->x + (dx), (p)->y + (dy)) /* offset */
#define __PP(p, r, c, s) __PF((p)->x + (r) * (c), (p)->y + (r) * (s)) /*polar*/

#define __SIGN(x) ((x) > 0 ? 1 : ((x < 0) ? -1 : 0))

#define SINF(deg) sinf((deg)*PI_DEG)
#define COSF(deg) cosf((deg)*PI_DEG)
#define ASINF(x) asinf(x)
#define FABS(x) fabs(x)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static uint32_t curve_get_length(const vg_lite_curve_dsc_t * curve_dsc);
static void curve_fill_path(float * path, const vg_lite_curve_dsc_t * curve_dsc, lv_area_t * area);
static void curve_fill_path_f(float * path, const vg_lite_curve_dsc_t * curve_dsc, lv_area_t * area);
static uint16_t curve_fill_round_rect_path(float * path, const lv_area_t * rect, lv_coord_t radius);
static uint16_t curve_fill_line_path(float * path, const vg_lite_fpoint_t * points, const lv_draw_line_dsc_t * dsc);
static uint16_t curve_fill_polygon_path(float * path, const lv_point_t * points, lv_coord_t num);
static uint16_t curve_fill_arc_path(float * path, const lv_point_t * center, const vg_lite_arc_dsc_t * dsc);

static bool curve_fill_color(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc);

static bool curve_fill_image(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc);

static bool curve_fill_linear_gradient(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc);

static bool curve_fill_radial_gradient(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

/* Magic number from https://spencermortensen.com/articles/bezier-circle/ */
static const float g_arc_magic = 0.55191502449351f;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_curve_fill_dsc_init(vg_lite_curve_fill_dsc_t * fill_dsc)
{
    lv_memset_00(fill_dsc, sizeof(vg_lite_curve_fill_dsc_t));
    fill_dsc->type = CURVE_FILL_COLOR;
    fill_dsc->fill_rule = VG_LITE_FILL_NON_ZERO;
}

bool vg_lite_draw_curve(lv_gpu_dest_buf_t * dest, const vg_lite_curve_dsc_t * curve_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(dest->buf);
    LV_ASSERT_NULL(curve_dsc);
    LV_ASSERT_NULL(curve_dsc->op);
    LV_ASSERT(curve_dsc->points || curve_dsc->fpoints);
    LV_ASSERT_NULL(curve_dsc->fill_dsc);

    if(curve_dsc->fill_dsc->type == CURVE_FILL_LINEAR_GRADIENT && !curve_dsc->fill_dsc->grad_dsc) {
        LV_GPU_LOG_ERROR("Invalid gradient argument");
        return false;
    }

    uint32_t path_length = curve_get_length(curve_dsc);

    float * path = lv_mem_buf_get(path_length);
    LV_ASSERT_MALLOC(path);
    if(!path) {
        LV_GPU_LOG_ERROR("out of memory");
        return false;
    }

    lv_memset_00(path, path_length);

    vg_lite_curve_fill_dsc_t fill_dsc = *curve_dsc->fill_dsc;

    /* Convert to vglite path */
    lv_area_t grad_area;
    lv_area_t * grad_area_p = NULL;
    if(fill_dsc.type == CURVE_FILL_LINEAR_GRADIENT && !fill_dsc.grad_area) {
        fill_dsc.grad_area = grad_area_p = &grad_area;
    }

    if(curve_dsc->points) {
        curve_fill_path(path, curve_dsc, grad_area_p);
    }
    else {
        curve_fill_path_f(path, curve_dsc, grad_area_p);
    }

    bool ret = vg_lite_draw_path(dest, path, path_length, &fill_dsc);

    lv_mem_buf_release(path);

    return ret;
}

bool vg_lite_draw_path(
    lv_gpu_dest_buf_t * dest,
    float * path_data,
    uint32_t path_length,
    const vg_lite_curve_fill_dsc_t * fill_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(path_data);
    LV_ASSERT(path_length > 0);
    LV_ASSERT_NULL(fill_dsc);

    const lv_area_t * buf_area = dest->buf_area;
    LV_ASSERT_NULL(buf_area);

    LV_GPU_LOG_TRACE("start");

    bool retval = false;

    /* Init destination vglite buffer */
    vg_lite_buffer_t dest_vg_buf;
    if(!vg_lite_gpu_buf_to_vg_buf(&dest_vg_buf, dest)) {
        LV_GPU_LOG_TRACE("dest_vg_buf init failed");
        return false;
    }

    lv_area_t clip_area;
    if(!_lv_area_intersect(&clip_area, buf_area, dest->clip_area)) {
        LV_GPU_LOG_TRACE("not in clip_area");
        return false;
    }

    /* Set last OP to VLC_OP_END */

    path_data[path_length / sizeof(float) - 1] = VLC_OP_END;

    vg_lite_path_t vg_path;
    vg_lite_init_path(
        &vg_path,
        VG_LITE_FP32,
        VG_LITE_HIGH,
        path_length,
        path_data,
        clip_area.x1,
        clip_area.y1,
        clip_area.x2 + 1,
        clip_area.y2 + 1);

    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);
    vg_lite_translate(-buf_area->x1, -buf_area->y1, &matrix);
    VG_LITE_DUMP_MATRIX_INFO(&matrix);

    LV_GPU_LOG_TRACE("fill_dsc->type = 0x%x", fill_dsc->type);
    VG_LITE_DUMP_BUFFER_INFO(&dest_vg_buf);

    switch(fill_dsc->type) {
        case CURVE_FILL_COLOR: {
                retval = curve_fill_color(&dest_vg_buf, &vg_path, dest, &matrix, fill_dsc);
            }
            break;
        case CURVE_FILL_IMAGE: {
                retval = curve_fill_image(&dest_vg_buf, &vg_path, dest, &matrix, fill_dsc);
            }
            break;
        case CURVE_FILL_LINEAR_GRADIENT: {
                retval = curve_fill_linear_gradient(&dest_vg_buf, &vg_path, dest, &matrix, fill_dsc);
            }
            break;
        case CURVE_FILL_RADIAL_GRADIENT: {
                retval = curve_fill_radial_gradient(&dest_vg_buf, &vg_path, dest, &matrix, fill_dsc);
            }
            break;
        default:
            LV_GPU_LOG_WARN("unsupport type: %d", fill_dsc->type);
            break;
    }

    if(!retval) {
        LV_GPU_LOG_WARN("fill failed");
        vg_lite_dump_path_info(&vg_path);
    }

    vg_lite_clear_path(&vg_path);

    LV_GPU_LOG_TRACE("finish");

    return retval;
}

bool vg_lite_draw_mask_to_path(vg_lite_path_t * vg_path, const lv_area_t * coords)
{
    LV_ASSERT_NULL(vg_path);
    LV_ASSERT_NULL(coords);

    bool masked = false;
    for(uint8_t i = 0; i < _LV_MASK_MAX_NUM; i++) {
        _lv_draw_mask_common_dsc_t * comm = LV_GC_ROOT(_lv_draw_mask_list[i]).param;
        if(!comm) {
            continue;
        }

        if(comm->type == LV_DRAW_MASK_TYPE_RADIUS) {
            if(masked) {
                LV_GPU_LOG_WARN("multiple mask unsupported");
                lv_mem_buf_release(vg_path->path);
                vg_path->path = NULL;
                break;
            }

            lv_draw_mask_radius_param_t * r = (lv_draw_mask_radius_param_t *)comm;
            lv_coord_t w = lv_area_get_width(&r->cfg.rect);
            lv_coord_t h = lv_area_get_height(&r->cfg.rect);

            if((r->cfg.outer && !_lv_area_is_out(coords, &r->cfg.rect, r->cfg.radius))
               || !_lv_area_is_in(coords, &r->cfg.rect, r->cfg.radius)) {
                masked = true;
                uint16_t length = POINT_PATH_LEN + r->cfg.outer * RECT_PATH_LEN;
                uint16_t path_length = length * sizeof(float);

                float * path = lv_mem_buf_get(path_length);
                if(!path) {
                    LV_GPU_LOG_ERROR("out of memory");
                    return false;
                }

                vg_path->path = path;
                vg_path->path_length = path_length;
                lv_coord_t r_short = LV_MIN(w, h) >> 1;
                lv_coord_t radius = LV_MIN(r->cfg.radius, r_short);
                uint16_t len = curve_fill_round_rect_path(path, &r->cfg.rect, radius);

                if(r->cfg.outer) {
                    curve_fill_round_rect_path(path + len, coords, 0);
                }

                *(uint8_t *)(path + length - 1) = VLC_OP_END;
            }
        }
        else {
            LV_GPU_LOG_INFO("mask type %d unsupported", comm->type);
            masked = true;
            vg_path->path = NULL;
        }
    }
    return masked;
}

void vg_lite_draw_mask_clean_path(vg_lite_path_t * vg_path)
{
    LV_ASSERT_NULL(vg_path);
    lv_mem_buf_release(vg_path->path);
}

uint16_t vg_lite_fill_path(
    float * path_data,
    vg_lite_shape_path_type_t type,
    const lv_point_t * points,
    const void * dsc)
{
    LV_ASSERT_NULL(path_data);
    LV_ASSERT_NULL(points);
    LV_ASSERT_NULL(dsc);
    LV_GPU_LOG_TRACE("type = %d", type);

    uint16_t len = 0;
    switch(type) {
        case VG_LITE_POINT_PATH: {
                /* point path */
                vg_lite_fpoint_t * point_delta = (vg_lite_fpoint_t *)dsc;
                lv_coord_t dx = point_delta->x;
                lv_coord_t dy = point_delta->y;
                lv_area_t point_area = {
                    points[0].x - dx,
                    points[0].y - dy,
                    points[1].x + dx + 1,
                    points[1].y + dy + 1
                };
                len = curve_fill_round_rect_path(path_data, &point_area, LV_MIN(dx, dy));
            }
            break;

        case VG_LITE_LINE_PATH: {
                /* line path */
                const lv_draw_line_dsc_t * line_dsc = dsc;
                vg_lite_fpoint_t fpoints[2] = {
                    __PR(points, 0.5f, 0),
                    __PR(points + 1, 0.5f, 0)
                };
                len = curve_fill_line_path(path_data, fpoints, line_dsc);
            }
            break;

        case VG_LITE_RECT_PATH: {
                /* rect path */
                const lv_draw_rect_dsc_t * rect_dsc = dsc;
                lv_area_t * coords = (lv_area_t *)points;
                lv_coord_t w = lv_area_get_width(coords);
                lv_coord_t h = lv_area_get_height(coords);
                lv_coord_t r_short = LV_MIN(w, h) >> 1;
                lv_coord_t radius = LV_MIN(rect_dsc->radius, r_short);
                len = curve_fill_round_rect_path(path_data, coords, radius);
            }
            break;

        case VG_LITE_POLYGON_PATH: {
                /* polygon path */
                const vg_lite_polygon_dsc_t * poly_dsc = dsc;
                len = curve_fill_polygon_path(path_data, points, poly_dsc->num);
            }
            break;

        case VG_LITE_ARC_PATH: {
                /* arc path */
                const vg_lite_arc_dsc_t * arc_dsc = dsc;
                len = curve_fill_arc_path(path_data, points, arc_dsc);
            }
            break;

        default:
            /* TODO: add other path type fill function as needed */
            LV_GPU_LOG_WARN("unsupport type: %d", type);
            break;
    }

    return len;
}

uint32_t vg_lite_calculate_path_length(vg_lite_shape_path_type_t type, const void * dsc)
{
    LV_ASSERT_NULL(dsc);
    uint32_t len = 0;
    switch(type) {
        case VG_LITE_ARC_PATH: {
                vg_lite_arc_dsc_t * arc_dsc = (vg_lite_arc_dsc_t *)dsc;
                float angle = arc_dsc->end_angle - arc_dsc->start_angle;
                while(angle > 360.0f) {
                    angle -= 360.0f;
                }
                while(angle < -VG_LITE_ANGLE_RES) {
                    angle += 360.0f;
                }
                bool circle = FABS(angle) < VG_LITE_ANGLE_RES;
                uint32_t right_angles = (uint32_t)floorf(angle / 90.0f + 1);
                len = circle ? 65 : 11 + arc_dsc->dsc.rounded * 22 + right_angles * 14;
            }
            break;
        default:
            LV_GPU_LOG_WARN("unsupport type: %d", type);
            break;
    }

    return len * sizeof(float);
}

void vg_lite_curve_fill_dsc_dump_info(const vg_lite_curve_fill_dsc_t * dsc)
{
    if(!dsc) {
        LV_GPU_LOG_WARN("dsc is NULL");
        return;
    }

    LV_GPU_LOG_INFO(
        "type: %d, color: #%08" PRIX32 ", opa: %d",
        dsc->type,
        lv_color_to32(dsc->color),
        dsc->opa);
    if(dsc->type == CURVE_FILL_IMAGE) {
        if(dsc->img_dsc) {
            LV_GPU_LOG_INFO(
                "img_dsc: {img_dsc: %p, draw_dsc: %p, area: {%d, %d, %d, %d}}",
                dsc->img_dsc->img_dsc,
                dsc->img_dsc->draw_dsc,
                dsc->img_dsc->area->x1,
                dsc->img_dsc->area->y1,
                dsc->img_dsc->area->x2,
                dsc->img_dsc->area->y2);
        }
        else {
            LV_GPU_LOG_WARN("dsc->img_dsc is NULL");
        }
    }

    if(dsc->type == CURVE_FILL_LINEAR_GRADIENT || dsc->type == CURVE_FILL_RADIAL_GRADIENT) {
        if(dsc->grad_dsc) {
            LV_GPU_LOG_INFO(
                "grad_dsc: {stops_count: %d, dir: %d, dither: %d}",
                dsc->grad_dsc->stops_count,
                dsc->grad_dsc->dir,
                dsc->grad_dsc->dither);
        }
        else {
            LV_GPU_LOG_WARN("dsc->grad_dsc is NULL");
        }

        if(dsc->grad_area) {
            LV_GPU_LOG_INFO(
                "grad_area: {%d, %d, %d, %d}",
                dsc->grad_area->x1,
                dsc->grad_area->y1,
                dsc->grad_area->x2,
                dsc->grad_area->y2);
        }
        else {
            LV_GPU_LOG_WARN("dsc->grad_area is NULL");
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static float curve_get_angle(float ux, float uy, float vx, float vy)
{
    float det = ux * vy - uy * vx;
    float norm2 = (ux * ux + uy * uy) * (vx * vx + vy * vy);
    float angle = ASINF(det * lv_gpu_fast_inv_sqrt(norm2));
    return angle;
}

static vg_lite_fpoint_t curve_get_rotated(
    const lv_point_t * center,
    float radius,
    float cos_val,
    float sin_val,
    uint8_t j)
{
    switch(j & 3) {
        case 3:
            return __PP(center, radius, sin_val, -cos_val);
        case 2:
            return __PP(center, radius, -cos_val, -sin_val);
        case 1:
            return __PP(center, radius, -sin_val, cos_val);
        case 0:
        default:
            return __PP(center, radius, cos_val, sin_val);
    }
}

static void curve_update_area(lv_area_t * a, float x, float y)
{
    if(x < a->x1) {
        a->x1 = (lv_coord_t)x;
    }
    else if(x > a->x2) {
        a->x2 = (lv_coord_t)x;
    }
    if(y < a->y1) {
        a->y1 = (lv_coord_t)y;
    }
    else if(y > a->y2) {
        a->y2 = (lv_coord_t)y;
    }
}

static uint32_t curve_get_length(const vg_lite_curve_dsc_t * curve_dsc)
{
    uint32_t sum = 1; /* VLC_OP_END */

    for(uint32_t i = 0; i < curve_dsc->num; i++) {
        /* the value of curve_op is designed to be the same as actual size */
        if(curve_dsc->op[i] >= CURVE_ARC_90) {
            sum += CURVE_CUBIC;
        }
        else {
            sum += curve_dsc->op[i];
        }
    }

    if(curve_dsc->op[curve_dsc->num - 1] != CURVE_CLOSE) {
        sum += 3;
    }

    return sum * sizeof(float);
}

static void curve_fill_path(float * path, const vg_lite_curve_dsc_t * curve_dsc, lv_area_t * area)
{
    uint32_t i = 0;
    int32_t dx1, dy1, dx2, dy2;
    float c;
    *(uint8_t *)path++ = VLC_OP_MOVE;
    *path++ = curve_dsc->points[0].x;
    *path++ = curve_dsc->points[0].y;
    if(area) {
        area->x1 = area->x2 = curve_dsc->points[0].x;
        area->y1 = area->y2 = curve_dsc->points[0].y;
    }
    while(i < curve_dsc->num) {
        switch(curve_dsc->op[i]) {
            case CURVE_END:
                *(uint8_t *)path++ = VLC_OP_END;
                i = curve_dsc->num;
                break;
            case CURVE_LINE:
                if(i < curve_dsc->num - 1) {
                    *(uint8_t *)path++ = VLC_OP_LINE;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_CLOSE:
                *(uint8_t *)path++ = VLC_OP_CLOSE;
                if(i++ < curve_dsc->num - 3) {
                    *(uint8_t *)path++ = VLC_OP_MOVE;
                    *path++ = curve_dsc->points[i].x;
                    *path++ = curve_dsc->points[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_QUAD:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_QUAD;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_CUBIC:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    *path++ = curve_dsc->points[++i].x;
                    *path++ = curve_dsc->points[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_ARC_90:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    dx1 = curve_dsc->points[i + 1].x - curve_dsc->points[i].x;
                    dy1 = curve_dsc->points[i + 1].y - curve_dsc->points[i].y;
                    dx2 = curve_dsc->points[i + 2].x - curve_dsc->points[i + 1].x;
                    dy2 = curve_dsc->points[i + 2].y - curve_dsc->points[i + 1].y;
                    c = __SIGN(dx1 * dy2 - dx2 * dy1) * g_arc_magic;
                    *path++ = curve_dsc->points[i].x - c * dy1;
                    *path++ = curve_dsc->points[i].y + c * dx1;
                    *path++ = curve_dsc->points[i + 2].x - c * dy2;
                    *path++ = curve_dsc->points[i + 2].y + c * dx2;
                    *path++ = curve_dsc->points[i + 2].x;
                    *path++ = curve_dsc->points[i + 2].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                    i += 2;
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_ARC_ACUTE:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    dx1 = curve_dsc->points[i + 1].x - curve_dsc->points[i].x;
                    dy1 = curve_dsc->points[i + 1].y - curve_dsc->points[i].y;
                    dx2 = curve_dsc->points[i + 2].x - curve_dsc->points[i + 1].x;
                    dy2 = curve_dsc->points[i + 2].y - curve_dsc->points[i + 1].y;
                    float theta = curve_get_angle(dx1, dy1, dx2, dy2);
                    c = 1.3333333f * tanf(theta * 0.25f);
                    *path++ = curve_dsc->points[i].x - c * dy1;
                    *path++ = curve_dsc->points[i].y + c * dx1;
                    *path++ = curve_dsc->points[i + 2].x - c * dy2;
                    *path++ = curve_dsc->points[i + 2].y + c * dx2;
                    *path++ = curve_dsc->points[i + 2].x;
                    *path++ = curve_dsc->points[i + 2].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                    i += 2;
                }
                else {
                    i = curve_dsc->num;
                }
                break;

                break;
            default:
                break;
        }
    }
    *(uint8_t *)path++ = VLC_OP_END;
}

static void curve_fill_path_f(float * path, const vg_lite_curve_dsc_t * curve_dsc, lv_area_t * area)
{
    uint32_t i = 0;
    int32_t dx1, dy1, dx2, dy2;
    float c;
    *(uint8_t *)path++ = VLC_OP_MOVE;
    *path++ = curve_dsc->fpoints[0].x;
    *path++ = curve_dsc->fpoints[0].y;
    if(area) {
        area->x1 = area->x2 = curve_dsc->fpoints[0].x;
        area->y1 = area->y2 = curve_dsc->fpoints[0].y;
    }
    while(i < curve_dsc->num) {
        switch(curve_dsc->op[i]) {
            case CURVE_END:
                *(uint8_t *)path++ = VLC_OP_END;
                i = curve_dsc->num;
                break;
            case CURVE_LINE:
                if(i < curve_dsc->num - 1) {
                    *(uint8_t *)path++ = VLC_OP_LINE;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_CLOSE:
                *(uint8_t *)path++ = VLC_OP_CLOSE;
                if(i++ < curve_dsc->num - 3) {
                    *(uint8_t *)path++ = VLC_OP_MOVE;
                    *path++ = curve_dsc->fpoints[i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_QUAD:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_QUAD;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_CUBIC:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    *path++ = curve_dsc->fpoints[++i].x;
                    *path++ = curve_dsc->fpoints[i].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_ARC_90:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    dx1 = curve_dsc->fpoints[i + 1].x - curve_dsc->fpoints[i].x;
                    dy1 = curve_dsc->fpoints[i + 1].y - curve_dsc->fpoints[i].y;
                    dx2 = curve_dsc->fpoints[i + 2].x - curve_dsc->fpoints[i + 1].x;
                    dy2 = curve_dsc->fpoints[i + 2].y - curve_dsc->fpoints[i + 1].y;
                    c = __SIGN(dx1 * dy2 - dx2 * dy1) * g_arc_magic;
                    *path++ = curve_dsc->fpoints[i].x - c * dy1;
                    *path++ = curve_dsc->fpoints[i].y + c * dx1;
                    *path++ = curve_dsc->fpoints[i + 2].x - c * dy2;
                    *path++ = curve_dsc->fpoints[i + 2].y + c * dx2;
                    *path++ = curve_dsc->fpoints[i + 2].x;
                    *path++ = curve_dsc->fpoints[i + 2].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                    i += 2;
                }
                else {
                    i = curve_dsc->num;
                }
                break;
            case CURVE_ARC_ACUTE:
                if(i < curve_dsc->num - 2) {
                    *(uint8_t *)path++ = VLC_OP_CUBIC;
                    dx1 = curve_dsc->fpoints[i + 1].x - curve_dsc->fpoints[i].x;
                    dy1 = curve_dsc->fpoints[i + 1].y - curve_dsc->fpoints[i].y;
                    dx2 = curve_dsc->fpoints[i + 2].x - curve_dsc->fpoints[i + 1].x;
                    dy2 = curve_dsc->fpoints[i + 2].y - curve_dsc->fpoints[i + 1].y;
                    float theta = curve_get_angle(dx1, dy1, dx2, dy2);
                    c = 1.3333333f * tanf(theta * 0.25f);
                    *path++ = curve_dsc->fpoints[i].x - c * dy1;
                    *path++ = curve_dsc->fpoints[i].y + c * dx1;
                    *path++ = curve_dsc->fpoints[i + 2].x - c * dy2;
                    *path++ = curve_dsc->fpoints[i + 2].y + c * dx2;
                    *path++ = curve_dsc->fpoints[i + 2].x;
                    *path++ = curve_dsc->fpoints[i + 2].y;
                    if(area) {
                        curve_update_area(area, *(path - 2), *(path - 1));
                        curve_update_area(area, *(path - 4), *(path - 3));
                        curve_update_area(area, *(path - 6), *(path - 5));
                    }
                    i += 2;
                }
                else {
                    i = curve_dsc->num;
                }
                break;

                break;
            default:
                break;
        }
    }
    *(uint8_t *)path++ = VLC_OP_END;
}

static uint32_t curve_get_grad_hash(const lv_grad_dsc_t * grad)
{
    uint32_t hash = lv_color_to16(grad->stops[0].color) ^ grad->stops[0].frac;
    return hash << 16 | (lv_color_to16(grad->stops[1].color) ^ grad->stops[1].frac);
}

static uint16_t curve_fill_round_rect_path(float * path, const lv_area_t * rect, lv_coord_t radius)
{
    if(!radius) {
        *(uint8_t *)path = VLC_OP_MOVE;
        *(uint8_t *)(path + 3) = *(uint8_t *)(path + 6) = *(uint8_t *)(path + 9)
                                                          = VLC_OP_LINE;
        *(uint8_t *)(path + 12) = VLC_OP_CLOSE;
        path[1] = path[4] = rect->x1;
        path[7] = path[10] = rect->x2 + 1;
        path[2] = path[11] = rect->y1;
        path[5] = path[8] = rect->y2 + 1;
        return RECT_PATH_LEN;
    }

    float r = radius;
    float c = g_arc_magic * r;
    float cx0 = rect->x1 + r;
    float cx1 = rect->x2 - r;
    float cy0 = rect->y1 + r;
    float cy1 = rect->y2 - r;
    *(uint8_t *)path++ = VLC_OP_MOVE;
    *path++ = cx0 - r;
    *path++ = cy0;
    *(uint8_t *)path++ = VLC_OP_CUBIC;
    *path++ = cx0 - r;
    *path++ = cy0 - c;
    *path++ = cx0 - c;
    *path++ = cy0 - r;
    *path++ = cx0;
    *path++ = cy0 - r;
    *(uint8_t *)path++ = VLC_OP_LINE;
    *path++ = cx1;
    *path++ = cy0 - r;
    *(uint8_t *)path++ = VLC_OP_CUBIC;
    *path++ = cx1 + c;
    *path++ = cy0 - r;
    *path++ = cx1 + r;
    *path++ = cy0 - c;
    *path++ = cx1 + r;
    *path++ = cy0;
    *(uint8_t *)path++ = VLC_OP_LINE;
    *path++ = cx1 + r;
    *path++ = cy1;
    *(uint8_t *)path++ = VLC_OP_CUBIC;
    *path++ = cx1 + r;
    *path++ = cy1 + c;
    *path++ = cx1 + c;
    *path++ = cy1 + r;
    *path++ = cx1;
    *path++ = cy1 + r;
    *(uint8_t *)path++ = VLC_OP_LINE;
    *path++ = cx0;
    *path++ = cy1 + r;
    *(uint8_t *)path++ = VLC_OP_CUBIC;
    *path++ = cx0 - c;
    *path++ = cy1 + r;
    *path++ = cx0 - r;
    *path++ = cy1 + c;
    *path++ = cx0 - r;
    *path++ = cy1;
    *(uint8_t *)path++ = VLC_OP_CLOSE;

    return POINT_PATH_LEN;
}

static uint16_t curve_fill_line_path(float * path, const vg_lite_fpoint_t * points, const lv_draw_line_dsc_t * dsc)
{
    float dx = points[1].x - points[0].x;
    float dy = points[1].y - points[0].y;
    float dl_inv = lv_gpu_fast_inv_sqrt(dx * dx + dy * dy);
    float r = dsc->width * 0.5f;
    float tmp = r * dl_inv;
    float w2_dx = tmp * dy;
    float w2_dy = tmp * dx;
    float c_dx = g_arc_magic * w2_dx;
    float c_dy = g_arc_magic * w2_dy;
    float * p = path;
    vg_lite_fpoint_t tmp_p = __PL(points, w2_dx, w2_dy);
    *(uint8_t *)p++ = VLC_OP_MOVE;
    *(vg_lite_fpoint_t *)p++ = tmp_p;
    p++;
    if(!dsc->round_start) {
        *(uint8_t *)p++ = VLC_OP_LINE;
        *(vg_lite_fpoint_t *)p++ = __PR(points, w2_dx, w2_dy);
        p++;
    }
    else {
        *(uint8_t *)p++ = VLC_OP_CUBIC;
        *(vg_lite_fpoint_t *)p++ = __PB(&tmp_p, c_dx, c_dy);
        p++;
        tmp_p = __PB(points, w2_dx, w2_dy);
        *(vg_lite_fpoint_t *)p++ = __PL(&tmp_p, c_dx, c_dy);
        p++;
        *(vg_lite_fpoint_t *)p++ = tmp_p;
        p++;
        *(uint8_t *)p++ = VLC_OP_CUBIC;
        *(vg_lite_fpoint_t *)p++ = __PR(&tmp_p, c_dx, c_dy);
        p++;
        tmp_p = __PR(points, w2_dx, w2_dy);
        *(vg_lite_fpoint_t *)p++ = __PB(&tmp_p, c_dx, c_dy);
        p++;
        *(vg_lite_fpoint_t *)p++ = tmp_p;
        p++;
    }
    points++;
    tmp_p = __PR(points, w2_dx, w2_dy);
    *(uint8_t *)p++ = VLC_OP_LINE;
    *(vg_lite_fpoint_t *)p++ = tmp_p;
    p++;
    if(!dsc->round_end) {
        *(uint8_t *)p++ = VLC_OP_LINE;
        *(vg_lite_fpoint_t *)p++ = __PL(points, w2_dx, w2_dy);
        p++;
    }
    else {
        *(uint8_t *)p++ = VLC_OP_CUBIC;
        *(vg_lite_fpoint_t *)p++ = __PT(&tmp_p, c_dx, c_dy);
        p++;
        tmp_p = __PT(points, w2_dx, w2_dy);
        *(vg_lite_fpoint_t *)p++ = __PR(&tmp_p, c_dx, c_dy);
        p++;
        *(vg_lite_fpoint_t *)p++ = tmp_p;
        p++;
        *(uint8_t *)p++ = VLC_OP_CUBIC;
        *(vg_lite_fpoint_t *)p++ = __PL(&tmp_p, c_dx, c_dy);
        p++;
        tmp_p = __PL(points, w2_dx, w2_dy);
        *(vg_lite_fpoint_t *)p++ = __PT(&tmp_p, c_dx, c_dy);
        p++;
        *(vg_lite_fpoint_t *)p++ = tmp_p;
        p++;
    }
    *(uint8_t *)p++ = VLC_OP_CLOSE;
    return p - path;
}

static uint16_t curve_fill_polygon_path(float * path, const lv_point_t * points, lv_coord_t num)
{
    *(uint8_t *)path++ = VLC_OP_MOVE;
    *path++ = points[0].x;
    *path++ = points[0].y;
    for(lv_coord_t i = 1; i < num; i++) {
        *(uint8_t *)path++ = VLC_OP_LINE;
        *path++ = points[i].x;
        *path++ = points[i].y;
    }
    *(uint8_t *)path++ = VLC_OP_CLOSE;
    return num * 3 + 1;
}

static uint16_t curve_fill_arc_path(float * path, const lv_point_t * center, const vg_lite_arc_dsc_t * dsc)
{
    LV_ASSERT_NULL(path);
    LV_ASSERT_NULL(center);
    LV_ASSERT_NULL(dsc);

    vg_lite_fpoint_t arc_points[ARC_MAX_POINTS];
    vg_lite_curve_op_t arc_op[ARC_MAX_POINTS];
    vg_lite_curve_dsc_t arc_curve = {
        .fpoints = arc_points,
        .op = arc_op,
        .num = 0
    };

    float start_angle = dsc->start_angle;
    float end_angle = dsc->end_angle;
    float radius = dsc->radius;
    vg_lite_fpoint_t * points = arc_points;
    vg_lite_curve_op_t * op = arc_op;
    lv_memset_00(arc_op, sizeof(arc_op));
    float angle = end_angle - start_angle;
    if(FABS(angle) < VG_LITE_ANGLE_RES) {
        op[0] = CURVE_ARC_90;
        points[0] = __PF(center->x + radius, center->y);
        points[1] = __PF(center->x, center->y);
        op[2] = CURVE_ARC_90;
        points[2] = __PF(center->x, center->y + radius);
        points[3] = points[1];
        op[4] = CURVE_ARC_90;
        points[4] = __PF(center->x - radius, center->y);
        points[5] = points[1];
        op[6] = CURVE_ARC_90;
        points[6] = __PF(center->x, center->y - radius);
        points[7] = points[1];
        op[8] = CURVE_CLOSE;
        points[8] = points[0];
        arc_curve.num = 8;
        if(dsc->dsc.width - radius < VG_LITE_ANGLE_RES) {
            float inner_radius = radius - dsc->dsc.width;
            op[9] = CURVE_ARC_90;
            points[9] = __PF(center->x + inner_radius, center->y);
            points[10] = points[1];
            op[11] = CURVE_ARC_90;
            points[11] = __PF(center->x, center->y + inner_radius);
            points[12] = points[1];
            op[13] = CURVE_ARC_90;
            points[13] = __PF(center->x - inner_radius, center->y);
            points[14] = points[1];
            op[15] = CURVE_ARC_90;
            points[15] = __PF(center->x, center->y - inner_radius);
            points[16] = points[1];
            op[17] = CURVE_CLOSE;
            points[17] = points[9];
            arc_curve.num = 18;
        }
    }
    else {
        float st_sin = SINF(start_angle);
        float st_cos = COSF(start_angle);
        float ed_sin = SINF(end_angle);
        float ed_cos = COSF(end_angle);
        float width = LV_MIN(dsc->dsc.width, radius);
        points[0] = __PP(center, radius - width, st_cos, st_sin);
        op[0] = CURVE_LINE;
        lv_coord_t i = 1;
        if(dsc->dsc.rounded) {
            op[i - 1] = CURVE_ARC_90;
            points[i++] = __PP(center, radius - width * 0.5f, st_cos, st_sin);
            vg_lite_fpoint_t * mid = &points[i - 1];
            op[i] = CURVE_ARC_90;
            points[i++] = __PO(mid, width * 0.5f * st_sin, -width * 0.5f * st_cos);
            points[i++] = *mid;
        }
        uint8_t j = 0;
        while(angle > -VG_LITE_ANGLE_RES) {
            op[i] = angle < 90.0f ? CURVE_ARC_ACUTE : CURVE_ARC_90;
            points[i++] = curve_get_rotated(center, radius, st_cos, st_sin, j++);
            points[i++] = __PF(center->x, center->y);
            angle -= 90.0f;
        }
        op[i] = CURVE_LINE;
        points[i++] = __PP(center, radius, ed_cos, ed_sin);
        if(dsc->dsc.rounded) {
            op[i - 1] = CURVE_ARC_90;
            points[i++] = __PP(center, radius - width * 0.5f, ed_cos, ed_sin);
            vg_lite_fpoint_t * mid = &points[i - 1];
            op[i] = CURVE_ARC_90;
            points[i++] = __PO(mid, -width * 0.5f * ed_sin, width * 0.5f * ed_cos);
            points[i++] = *mid;
        }
        if(dsc->dsc.width < radius) {
            angle = end_angle - start_angle;
            op[i] = angle < 90.0f ? CURVE_ARC_ACUTE : CURVE_ARC_90;
            j = 4;
            while(angle > -VG_LITE_ANGLE_RES) {
                op[i] = angle < 90.0f ? CURVE_ARC_ACUTE : CURVE_ARC_90;
                points[i++] = curve_get_rotated(center, radius - width, ed_cos, ed_sin, j--);
                points[i++] = __PF(center->x, center->y);
                angle -= 90.0f;
            }
        }
        op[i] = CURVE_CLOSE;
        points[i++] = __PP(center, radius - width, st_cos, st_sin);
        arc_curve.num = i;
    }

    curve_fill_path_f(path, &arc_curve, NULL);

    return curve_get_length(&arc_curve) / sizeof(float);
}

static bool curve_fill_color(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(path);
    LV_ASSERT_NULL(gpu_buf);
    LV_ASSERT_NULL(fill_dsc);

    vg_lite_error_t error;

    LV_GPU_LOG_TRACE("fill_rule: 0x%x (%s)",
                     fill_dsc->fill_rule,
                     vg_lite_get_fill_rule_string(fill_dsc->fill_rule));

    VG_LITE_CHECK_ERROR(vg_lite_draw(
                            dest,
                            path,
                            fill_dsc->fill_rule,
                            (vg_lite_matrix_t *)matrix,
                            VG_LITE_BLEND_SRC_OVER,
                            vg_lite_lv_color_to_vg_color(fill_dsc->color, fill_dsc->opa)));

    VG_LITE_CHECK_ERROR(vg_lite_flush());
    return true;

error_handler:
    vg_lite_dump_path_info(path);
    return false;
}

static bool curve_fill_image(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(path);
    LV_ASSERT_NULL(gpu_buf);
    LV_ASSERT_NULL(fill_dsc);

    const vg_lite_img_dsc_t * img = fill_dsc->img_dsc;
    LV_ASSERT_NULL(img);

    const uint8_t * img_data = img->img_dsc->data;
    const lv_draw_img_dsc_t * draw_dsc = img->draw_dsc;
    LV_ASSERT_NULL(draw_dsc);

    vg_lite_error_t error;

    vg_lite_buffer_t * vg_buf = vg_lite_img_data_to_vg_buf((void *)img_data);
    vg_lite_buffer_t src_vg_buf;

    bool allocated_src = false;
    bool retval = false;

    if(!vg_buf) {
        lv_color32_t recolor = {
            .full = lv_color_to32(draw_dsc->recolor)
        };

        LV_COLOR_SET_A32(recolor, draw_dsc->recolor_opa);

        if(!vg_lite_create_vg_buf_from_img_data(
               &src_vg_buf,
               img_data,
               &img->img_dsc->header,
               NULL,
               recolor,
               false)) {
            return false;
        }

        allocated_src = true;
        vg_buf = &src_vg_buf;
    }

    if(VG_LITE_IS_INDEX_FMT(vg_buf->format)) {
        uint32_t * palette = (uint32_t *)(img_data + sizeof(vg_lite_img_header_t) + vg_buf->stride * vg_buf->height);
        uint32_t palette_size = vg_lite_get_palette_size(vg_buf->format);
        LV_ASSERT(palette_size > 0);
        vg_lite_set_CLUT(palette_size, palette);
    }

    lv_area_t coords = {
        .x1 = fill_dsc->img_dsc->area->x1,
        .y1 = fill_dsc->img_dsc->area->y1
    };

    coords.x1 -= gpu_buf->buf_area->x1;
    coords.y1 -= gpu_buf->buf_area->y1;

    vg_lite_matrix_t matrix_src;
    vg_lite_img_trasnfrom_to_matrix(&matrix_src, draw_dsc, &coords);

    lv_opa_t opa = fill_dsc->opa;

    if(opa < LV_OPA_MAX) {
        vg_lite_color_t multiply_color = opa << 24 | opa << 16 | opa << 8 | opa;
        vg_lite_set_multiply_color(multiply_color);
        vg_buf->image_mode = VG_LITE_MULTIPLY_IMAGE_MODE;
    }
    else {
        vg_buf->image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    }

    vg_lite_color_t color = vg_lite_lv_color_to_vg_color(fill_dsc->color, LV_OPA_COVER);

    VG_LITE_CHECK_ERROR(
        vg_lite_draw_pattern(
            dest,
            path,
            fill_dsc->fill_rule,
            (vg_lite_matrix_t *)matrix,
            vg_buf,
            &matrix_src,
            VG_LITE_BLEND_SRC_OVER,
            VG_LITE_PATTERN_COLOR,
            color,
            VG_LITE_FILTER_BI_LINEAR));

    VG_LITE_CHECK_ERROR(vg_lite_flush());
    retval = true;

error_handler:
    if(allocated_src) {
        lv_mem_free(vg_buf->memory);
    }
    return retval;
}

static bool curve_fill_linear_gradient(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(path);
    LV_ASSERT_NULL(gpu_buf);
    LV_ASSERT_NULL(fill_dsc);
    LV_ASSERT_NULL(fill_dsc->grad_dsc);
    LV_ASSERT_NULL(fill_dsc->grad_area);

    vg_lite_error_t error;
    static uint32_t grad_mem[VLC_GRADIENT_BUFFER_WIDTH];

    vg_lite_linear_gradient_t vg_grad;
    vg_lite_custom_buffer_init(&vg_grad.image, grad_mem, VLC_GRADIENT_BUFFER_WIDTH, 1, VG_LITE_BGRA8888);
    VG_LITE_DUMP_BUFFER_INFO(&vg_grad.image);

    const lv_grad_dsc_t * grad_dsc = fill_dsc->grad_dsc;
    vg_grad.count = grad_dsc->stops_count;
    lv_opa_t opa = fill_dsc->opa;

    if(opa < LV_OPA_MAX) {
        for(int i = 0; i < vg_grad.count; i++) {
            lv_color_t color = grad_dsc->stops[i].color;
            color = lv_color_mix(color, lv_color_black(), opa);
            LV_COLOR_SET_A(color, opa);
            vg_grad.colors[i] = lv_color_to32(color);
            vg_grad.stops[i] = grad_dsc->stops[i].frac;
        }
    }
    else {
        for(int i = 0; i < vg_grad.count; i++) {
            lv_color_t color = grad_dsc->stops[i].color;
            vg_grad.colors[i] = lv_color_to32(color);
            vg_grad.stops[i] = grad_dsc->stops[i].frac;
        }
    }

    static uint32_t last_grad_hash = 0;
    uint32_t grad_hash = curve_get_grad_hash(grad_dsc);

    if(grad_hash != last_grad_hash) {
        vg_lite_update_grad(&vg_grad);
        last_grad_hash = grad_hash;
    }

    const lv_area_t * grad_area = fill_dsc->grad_area;
    vg_lite_identity(&vg_grad.matrix);
    vg_lite_translate(grad_area->x1, grad_area->y1, &vg_grad.matrix);
    vg_lite_translate(-gpu_buf->buf_area->x1, -gpu_buf->buf_area->y1, &vg_grad.matrix);

    if(grad_dsc->dir == LV_GRAD_DIR_VER) {
        vg_lite_scale(1.0f, lv_area_get_height(grad_area) / 256.0f, &vg_grad.matrix);
        vg_lite_rotate(90.0f, &vg_grad.matrix);
    }
    else {
        vg_lite_scale(lv_area_get_width(grad_area) / 256.0f, 1.0f, &vg_grad.matrix);
    }

    VG_LITE_CHECK_ERROR(vg_lite_draw_gradient(
                            dest,
                            path,
                            fill_dsc->fill_rule,
                            (vg_lite_matrix_t *)matrix,
                            &vg_grad,
                            VG_LITE_BLEND_SRC_OVER));

    VG_LITE_CHECK_ERROR(vg_lite_flush());

    return true;

error_handler:
    return false;
}

static bool curve_fill_radial_gradient(
    vg_lite_buffer_t * dest,
    vg_lite_path_t * path,
    const lv_gpu_dest_buf_t * gpu_buf,
    const vg_lite_matrix_t * matrix,
    const vg_lite_curve_fill_dsc_t * fill_dsc)
{
    LV_ASSERT_NULL(dest);
    LV_ASSERT_NULL(path);
    LV_ASSERT_NULL(gpu_buf);
    LV_ASSERT_NULL(fill_dsc);
    LV_GPU_LOG_WARN("Radial gradient unsupported at the moment");
    return false;
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
