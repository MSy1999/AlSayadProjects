/** @file gpio.h @brief Host sensor SDK shim. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_GPIO_H
#define AS_TEST_GPIO_H
#include "esp_err.h"
#include <stdint.h>
typedef int gpio_num_t;
#define GPIO_NUM_27 27
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin) >= 0 && (pin) <= 33)
#define GPIO_MODE_INPUT_OUTPUT_OD 1
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
typedef struct {
    uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
} gpio_config_t;
esp_err_t gpio_set_direction(gpio_num_t pin, int mode);
int gpio_get_level(gpio_num_t pin);
esp_err_t gpio_config(const gpio_config_t* pst_Config);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
#endif
