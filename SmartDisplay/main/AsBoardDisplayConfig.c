/**
 * @file AsBoardDisplayConfig.c
 * @brief Working ST7789 CYD board profile.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsBoardDisplayConfig.h"

as_display_config_t as_board_display_config(void)
{
    as_display_config_t st_Config = as_display_default_config();

    /* Match esp32-2432S028R-main with CYD_ILI9341 undefined. */
    st_Config.controller = AS_DISPLAY_CONTROLLER_ST7789;
    st_Config.lcd_miso = AS_TFT_MISO;
    st_Config.lcd_reset = GPIO_NUM_4;
    st_Config.lcd_clock_hz = 40000000;
    st_Config.bgr = false;
    st_Config.invert_colors = false;
    st_Config.panel_mirror_x = false;
    st_Config.panel_mirror_y = false;
    st_Config.draw_buffer_lines = 30;
    st_Config.double_buffer = true;
    st_Config.touch_clock_hz = 1000000;
    st_Config.touch_interrupt = false;
    st_Config.backlight_active_high = true;
    st_Config.backlight_pwm_hz = 5000;
    st_Config.backlight_timer = LEDC_TIMER_1;
    st_Config.backlight_channel = LEDC_CHANNEL_1;
    st_Config.initial_brightness = 0;
    st_Config.initialize_lvgl_port = true;

    /* Equivalent direction to the reference's driver conversion + mirror X.
     * Keep raw ADC mode so measured endpoints can replace these full-scale
     * defaults. Mapping and clamping use valid pixel indices (239/319). */
    st_Config.calibration = (as_touch_calibration_t) {
        .x_start = 4095, .x_end = 0,
        .y_start = 0, .y_end = 4095,
        .swap_xy = false,
    };
    return st_Config;
}
