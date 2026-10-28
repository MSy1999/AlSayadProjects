/** @file AnalogOutput.h @brief AlSayad reusable PWM output. @author Mohamad Sayadi */
#pragma once
#include <stdbool.h>
#include "driver/ledc.h"
#include "esp_err.h"
typedef struct { int pin; ledc_channel_t channel; ledc_timer_t timer; bool initialized; } as_analog_output_t;
/* Timer ownership belongs to caller; channels sharing a timer share frequency. */
esp_err_t as_analog_timer_init(ledc_timer_t timer, uint32_t frequency_hz);
esp_err_t as_analog_output_init(as_analog_output_t *output, int pin, ledc_channel_t channel, ledc_timer_t timer);
esp_err_t as_analog_output_write(as_analog_output_t *output, float normalized);
esp_err_t as_analog_output_stop(as_analog_output_t *output);
