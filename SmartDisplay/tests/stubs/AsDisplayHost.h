/**
 * @file AsDisplayHost.h
 * @brief AlSayad AsDisplayHost module.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_DISPLAY_HOST_H
#define AS_DISPLAY_HOST_H
#define AS_CYD_DISPLAY_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
lv_display_t* as_display_get_lvgl_display(void);
esp_err_t as_display_set_brightness(uint8_t u8Percent);
#endif
