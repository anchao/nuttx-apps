/**
 * @file lv_gpu_manager.h
 *
 */

#ifndef LV_GPU_MANAGER_H
#define LV_GPU_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl/lvgl.h>

struct _lv_gpu_ctx_t;
struct _lv_gpu_manager_t;

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_gpu_manager_init(lv_disp_drv_t * disp_drv);

void lv_gpu_manager_add_gpu(struct _lv_gpu_manager_t * manager, struct _lv_gpu_ctx_t * gpu);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_MANAGER_H*/
