/** @file main.c @brief Motors at full speed and MPU6050 serial monitor. */
#include <stdbool.h>

#include "board.h"
#include "driver/ledc.h"
#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MPU6050";
static const char *MOTOR_TAG = "MOTOR_TEST";

static const int motor_pins[] = AS_MOTOR_PINS;
#define MOTOR_COUNT (sizeof(motor_pins) / sizeof(motor_pins[0]))
#define MOTOR_TEST_MAX_PERCENT 100U

static esp_err_t motors_set_percent(unsigned percent)
{
    if (percent > MOTOR_TEST_MAX_PERCENT) {
        return ESP_ERR_INVALID_ARG;
    }
    /* A 10-bit LEDC period is 1024 ticks; duty 1024 gives fully-on output. */
    const uint32_t duty = (1024U * percent + 50U) / 100U;
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, duty);
        if (err != ESP_OK) {
            return err;
        }
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

/* Direct PWM for active-high brushed-motor transistor drivers, not ESC signals. */
static void motor_test_task(void *arg)
{
    (void)arg;
    unsigned initialized_channels = 0;
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        goto failed;
    }

    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
        const ledc_channel_config_t channel = {
            .gpio_num = motor_pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        err = ledc_channel_config(&channel);
        if (err != ESP_OK) {
            goto failed;
        }
        ++initialized_channels;
    }

    err = motors_set_percent(100);
    if (err != ESP_OK) {
        goto failed;
    }
    ESP_LOGI(MOTOR_TAG, "All motors: held at 100%% PWM duty; automatic ramp disabled");
    vTaskDelete(NULL);
    return;

failed:
    /* Stop every configured output before ending the test on a driver error. */
    for (unsigned i = 0; i < initialized_channels; ++i) {
        esp_err_t stop_err = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, 0);
        if (stop_err != ESP_OK) {
            ESP_LOGE(MOTOR_TAG, "Could not stop GPIO%d: %s", motor_pins[i],
                     esp_err_to_name(stop_err));
        }
    }
    ESP_LOGE(MOTOR_TAG, "Motor test ended: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
}

void app_main(void)
{
    if (xTaskCreate(motor_test_task, "motor_test", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(MOTOR_TAG, "Could not create motor test task; motors not started");
    }
    bool sensor_ready = false;

    for (;;) {
        if (!sensor_ready) {
            esp_err_t err = mpu6050_init();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Initialization failed (%s); retrying in 1 second",
                         esp_err_to_name(err));
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            sensor_ready = true;
        }

        mpu6050_data_t data;
        esp_err_t err = mpu6050_read(&data);
        if (err != ESP_OK) {
            sensor_ready = false;
            ESP_LOGW(TAG, "Communication failed (%s); reinitializing in 1 second",
                     esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "ACCEL X: %.2f g | Y: %.2f g | Z: %.2f g",
                 data.accel_x, data.accel_y, data.accel_z);
        ESP_LOGI(TAG, "GYRO  X: %.2f °/s | Y: %.2f °/s | Z: %.2f °/s",
                 data.gyro_x, data.gyro_y, data.gyro_z);
        ESP_LOGI(TAG, "TEMP  %.2f °C", data.temperature);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
