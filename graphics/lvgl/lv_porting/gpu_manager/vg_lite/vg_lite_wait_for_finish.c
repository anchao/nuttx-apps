/**
 * @file vg_lite_wait_for_finish.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

/*********************
 *      DEFINES
 *********************/

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

void vg_lite_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    vg_lite_error_t error;
    LV_GPU_PERF_START();
    VG_LITE_WAIT_FINISH();
error_handler:
    return;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
