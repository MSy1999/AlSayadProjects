#include "mpu6050.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_I2C_HZ       100000
#define MPU6050_TIMEOUT_MS   100
#define MPU6050_WHO_AM_I     0x75
#define MPU6050_EXPECTED_ID  0x68
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_PWR_MGMT_2   0x6C
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B

static const char *TAG = "MPU6050";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static bool initialized;

static esp_err_t log_error(const char *operation, esp_err_t err)
{
    ESP_LOGE(TAG, "%s: %s (0x%x)", operation, esp_err_to_name(err), (unsigned)err);
    if (err == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "I2C timed out; check SDA/SCL wiring and pull-ups to 3.3 V");
    }
    return err;
}

static esp_err_t read_registers(uint8_t address, uint8_t *buffer, size_t length)
{
    esp_err_t err = i2c_master_transmit_receive(device, &address, 1, buffer,
                                               length, MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read register 0x%02x failed", address);
        return log_error("I2C read", err);
    }
    return ESP_OK;
}

static esp_err_t write_register(uint8_t address, uint8_t value)
{
    const uint8_t buffer[] = {address, value};
    esp_err_t err = i2c_master_transmit(device, buffer, sizeof(buffer),
                                       MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Write register 0x%02x failed", address);
        return log_error("I2C write", err);
    }
    return ESP_OK;
}

static int16_t decode_int16(const uint8_t *bytes)
{
    /* Explicit sign extension avoids an out-of-range unsigned-to-signed cast. */
    int32_t value = ((uint32_t)bytes[0] << 8) | bytes[1];
    if (value >= 0x8000) {
        value -= 0x10000;
    }
    return (int16_t)value;
}

esp_err_t mpu6050_init(void)
{
    initialized = false;
    esp_err_t err;

    if (bus == NULL) {
        const i2c_master_bus_config_t config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AS_SDA,
            .scl_io_num = AS_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        err = i2c_new_master_bus(&config, &bus);
        if (err != ESP_OK) {
            return log_error("Create I2C master bus", err);
        }
        ESP_LOGI(TAG, "I2C SDA=GPIO%d SCL=GPIO%d, %d Hz",
                 AS_SDA, AS_SCL, MPU6050_I2C_HZ);
        ESP_LOGI(TAG, "Internal pull-ups enabled; external pull-ups to 3.3 V may be required");
    } else {
        /* Recover the controller/bus without allocating handles on each retry. */
        err = i2c_master_bus_reset(bus);
        if (err != ESP_OK) {
            return log_error("Reset I2C master bus", err);
        }
    }

    err = i2c_master_probe(bus, AS_IMU_ADDRESS, MPU6050_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 not detected at 0x%02x; check 3.3 V, GND, "
                 "SDA=GPIO%d, SCL=GPIO%d and AD0=GND",
                 AS_IMU_ADDRESS, AS_SDA, AS_SCL);
        return log_error("Probe MPU6050", err);
    }

    if (device == NULL) {
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AS_IMU_ADDRESS,
            .scl_speed_hz = MPU6050_I2C_HZ,
        };
        err = i2c_master_bus_add_device(bus, &config, &device);
        if (err != ESP_OK) {
            return log_error("Add MPU6050 I2C device", err);
        }
    }

    uint8_t identity;
    err = read_registers(MPU6050_WHO_AM_I, &identity, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (identity != MPU6050_EXPECTED_ID) {
        ESP_LOGE(TAG, "Unexpected WHO_AM_I=0x%02x; expected 0x%02x",
                 identity, MPU6050_EXPECTED_ID);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* SLEEP=0, CYCLE=0, TEMP_DIS=0; use the X gyro PLL clock. */
    err = write_register(MPU6050_PWR_MGMT_1, 0x01);
    if (err != ESP_OK) {
        return err;
    }
    /* Allow the oscillator and sensor outputs to settle after waking. */
    vTaskDelay(pdMS_TO_TICKS(100));
    err = write_register(MPU6050_PWR_MGMT_2, 0x00); /* Enable all axes. */
    if (err != ESP_OK) {
        return err;
    }
    err = write_register(MPU6050_ACCEL_CONFIG, 0x00); /* +/-2 g, self-test off. */
    if (err != ESP_OK) {
        return err;
    }
    err = write_register(MPU6050_GYRO_CONFIG, 0x00); /* +/-250 deg/s, self-test off. */
    if (err != ESP_OK) {
        return err;
    }

    initialized = true;
    ESP_LOGI(TAG, "Detected MPU6050 at 0x%02x (WHO_AM_I=0x%02x); "
             "accelerometer +/-2 g, gyroscope +/-250 deg/s", AS_IMU_ADDRESS, identity);
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t frame[14];
    esp_err_t err = read_registers(MPU6050_ACCEL_XOUT_H, frame, sizeof(frame));
    if (err != ESP_OK) {
        initialized = false;
        return err;
    }

    const mpu6050_data_t sample = {
        .accel_x = decode_int16(&frame[0]) / 16384.0f,
        .accel_y = decode_int16(&frame[2]) / 16384.0f,
        .accel_z = decode_int16(&frame[4]) / 16384.0f,
        .temperature = decode_int16(&frame[6]) / 340.0f + 36.53f,
        .gyro_x = decode_int16(&frame[8]) / 131.0f,
        .gyro_y = decode_int16(&frame[10]) / 131.0f,
        .gyro_z = decode_int16(&frame[12]) / 131.0f,
    };
    *data = sample;
    return ESP_OK;
}
