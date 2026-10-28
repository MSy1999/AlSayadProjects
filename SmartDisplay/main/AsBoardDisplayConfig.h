/**
 * @file AsBoardDisplayConfig.h
 * @brief Declare the working CYD board profile and orientation.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_BOARD_DISPLAY_CONFIG_H
#define AS_BOARD_DISPLAY_CONFIG_H

#include "as_display_config.h"
#include "lvgl.h"

#define AS_BOARD_DISPLAY_READY_BRIGHTNESS 75
#define AS_BOARD_DISPLAY_ROTATION LV_DISPLAY_ROTATION_90

/* Board profile matched to the supplied working ST7789 reference. */
as_display_config_t as_board_display_config(void);

#endif /* AS_BOARD_DISPLAY_CONFIG_H */
