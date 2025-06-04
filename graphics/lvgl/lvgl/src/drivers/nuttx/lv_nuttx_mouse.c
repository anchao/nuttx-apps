/**
 * @file lv_nuttx_mouse.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_nuttx_mouse.h"

#if LV_USE_NUTTX_MOUSE

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/input/mouse.h>
#include "../../lvgl_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int fd;
    lv_indev_state_t last_state;
    lv_indev_t * indev_drv;
} lv_nuttx_mouse_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void mouse_read(lv_indev_t * drv, lv_indev_data_t * data);
static void mouse_delete_cb(lv_event_t * e);
static lv_indev_t * mouse_init(int fd);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_indev_t * lv_nuttx_mouse_create(const char * dev_path)
{
    lv_indev_t * indev;
    int fd;

    LV_ASSERT_NULL(dev_path);
    LV_LOG_USER("mouse %s opening", dev_path);
    fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if(fd < 0) {
        LV_LOG_ERROR("Error: cannot open mouse device");
        return NULL;
    }

    LV_LOG_USER("mouse %s open success", dev_path);

    indev = mouse_init(fd);

    if(indev == NULL) {
        close(fd);
    }

    return indev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void mouse_read(lv_indev_t * drv, lv_indev_data_t * data)
{
    FAR lv_nuttx_mouse_t * mouse = drv->driver_data;
    struct mouse_report_s sample;

    /* Read one sample */

    int nbytes = read(mouse->fd, &sample, sizeof(struct mouse_report_s));

    /* Handle unexpected return values */

    if(nbytes == sizeof(struct mouse_report_s)) {
        lv_display_t * disp = lv_indev_get_display(drv);
        int32_t hor_max = lv_display_get_horizontal_resolution(disp) - 1;
        int32_t ver_max = lv_display_get_vertical_resolution(disp) - 1;

        data->point.x =
            LV_CLAMP(0,
                     data->point.x + (sample.x * CONFIG_LV_MOUSE_RATE),
                     hor_max);
        data->point.y =
            LV_CLAMP(0,
                     data->point.y + (sample.y * CONFIG_LV_MOUSE_RATE),
                     ver_max);

        uint8_t mouse_buttons = sample.buttons;

        if(mouse_buttons & MOUSE_BUTTON_1 || mouse_buttons & MOUSE_BUTTON_2 ||
           mouse_buttons & MOUSE_BUTTON_3) {
            mouse->last_state = LV_INDEV_STATE_PRESSED;
        }
        else {
            mouse->last_state = LV_INDEV_STATE_RELEASED;
        }
    }

    data->state = mouse->last_state;
}

static void mouse_delete_cb(lv_event_t * e)
{
    lv_indev_t * indev = (lv_indev_t *) lv_event_get_user_data(e);
    lv_nuttx_mouse_t * mouse = lv_indev_get_driver_data(indev);
    if(mouse) {
        lv_indev_set_driver_data(indev, NULL);
        lv_indev_set_read_cb(indev, NULL);

        if(mouse->fd >= 0) {
            close(mouse->fd);
            mouse->fd = -1;
        }
        lv_free(mouse);
        LV_LOG_USER("done");
    }
}

static void mouse_set_cursor(FAR lv_indev_t * indev)
{
    FAR lv_obj_t * cursor_obj = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(cursor_obj);

    int32_t size = 20;
    lv_obj_set_size(cursor_obj, size, size);
    lv_obj_set_style_translate_x(cursor_obj, -size / 2, 0);
    lv_obj_set_style_translate_y(cursor_obj, -size / 2, 0);
    lv_obj_set_style_radius(cursor_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(cursor_obj, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(cursor_obj, lv_color_black(), 0);
    lv_obj_set_style_border_width(cursor_obj, 2, 0);
    lv_obj_set_style_border_color(cursor_obj,
                                  lv_palette_main(LV_PALETTE_GREY), 0);
    lv_indev_set_cursor(indev, cursor_obj);
}

static lv_indev_t * mouse_init(int fd)
{
    lv_nuttx_mouse_t * mouse;
    lv_indev_t * indev = NULL;

    mouse = lv_malloc_zeroed(sizeof(lv_nuttx_mouse_t));
    if(mouse == NULL) {
        LV_LOG_ERROR("mouse_s malloc failed");
        return NULL;
    }

    mouse->fd = fd;
    mouse->last_state = LV_INDEV_STATE_RELEASED;
    mouse->indev_drv = indev = lv_indev_create();

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, mouse_read);
    lv_indev_set_driver_data(indev, mouse);
    lv_indev_add_event_cb(indev, mouse_delete_cb, LV_EVENT_DELETE, indev);

    /* Set cursor icon */
    mouse_set_cursor(indev);
    return indev;
}

#endif /*LV_USE_NUTTX_MOUSE*/
