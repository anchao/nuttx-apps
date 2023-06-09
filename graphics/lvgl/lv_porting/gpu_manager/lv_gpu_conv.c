/**
 * @file lv_gpu_conv.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_gpu_conv.h"
#include "lv_gpu_utils.h"
#include "mve/mve_conv_imp.h"
#include "sw/sw_conv_imp.h"
#include <nuttx/config.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_gpu_conv_callback_t imps[_LV_GPU_CONV_TYPE_LAST];
} lv_gpu_conv_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_gpu_conv_t g_conv;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_gpu_conv_init(void)
{
    memset(&g_conv, 0, sizeof(g_conv));
    sw_conv_init();
#ifdef CONFIG_LV_GPU_USE_ARM_MVE
    mve_conv_init();
#endif
}

void lv_gpu_conv_deinit(void)
{
    memset(&g_conv, 0, sizeof(g_conv));
}

void lv_gpu_conv_set_callback(lv_gpu_conv_type_t type, lv_gpu_conv_callback_t callback)
{
    if(type >= _LV_GPU_CONV_TYPE_LAST) {
        return;
    }

    g_conv.imps[type] = callback;
}

lv_gpu_conv_res_t lv_gpu_conv_start(lv_gpu_conv_type_t type, void * dsc)
{
    LV_ASSERT_NULL(dsc);

    if(type >= _LV_GPU_CONV_TYPE_LAST) {
        LV_GPU_LOG_ERROR("type: %d error", type);
        return LV_GPU_CONV_RES_PARAM_ERROR;
    }

    lv_gpu_conv_callback_t callback = g_conv.imps[type];

    if(callback == NULL) {
        LV_GPU_LOG_WARN("type: %d is no implementation", type);
        return LV_GPU_CONV_RES_NO_IMPLEMENTATION;
    }

    LV_GPU_LOG_TRACE("start, type = %d, callback = %p, dsc = %p", type, callback, dsc);
    lv_gpu_conv_res_t res = callback(dsc);
    LV_GPU_LOG_TRACE("finish");

    if(res != LV_GPU_CONV_RES_OK) {
        LV_GPU_LOG_ERROR("type: %d error: %d", type, res);
    }

    return res;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
