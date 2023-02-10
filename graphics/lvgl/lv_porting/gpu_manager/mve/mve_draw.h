/**
 * @file mve_draw.h
 *
 */

#ifndef MVE_DRAW_H
#define MVE_DRAW_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <nuttx/config.h>

#ifdef CONFIG_LV_GPU_USE_ARM_MVE

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_gpu_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

struct _lv_gpu_ctx_t * mve_draw_ctx_create(void);

/**********************
 *      MACROS
 **********************/

#endif /* CONFIG_LV_GPU_USE_ARM_MVE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MVE_DRAW_H*/
