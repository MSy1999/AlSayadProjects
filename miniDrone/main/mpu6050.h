#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Measurements in the sensor's native axes, without calibration or fusion. */
typedef struct {
    float accel_x;     /**< Acceleration in g, configured range +/-2 g. */
    float accel_y;
    float accel_z;
    float gyro_x;      /**< Angular velocity in degrees/s, range +/-250. */
    float gyro_y;
    float gyro_z;
    float temperature; /**< Sensor die temperature in degrees Celsius. */
} mpu6050_data_t;

/**
 * Initialize/reinitialize the MPU6050 at 0x68 on GPIO4/GPIO5, 100 kHz.
 * Owns I2C port 0 for the lifetime of the application and reuses its handles
 * across retries. Call these functions from one task, or serialize externally.
 * Returns ESP_ERR_NOT_FOUND for a missing device, ESP_ERR_INVALID_RESPONSE for
 * an unexpected WHO_AM_I, or the underlying ESP-IDF error on other failures.
 */
esp_err_t mpu6050_init(void);

/**
 * Read one 14-byte measurement frame. Output is unchanged on failure.
 * Returns ESP_ERR_INVALID_ARG for NULL, ESP_ERR_INVALID_STATE until initialized
 * (or after a communication failure), or an underlying ESP-IDF error.
 * After a communication failure, call mpu6050_init() before reading again.
 */
esp_err_t mpu6050_read(mpu6050_data_t *data);

#ifdef __cplusplus
}
#endif
