/**
 * @file esp_err.h
 * @brief AlSayad esp_err module.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_TEST_ESP_ERR_H
#define AS_TEST_ESP_ERR_H
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_TIMEOUT 0x107
static inline const char* esp_err_to_name(esp_err_t error) {
    return error == ESP_OK ? "ESP_OK" : "test error";
}
#endif
