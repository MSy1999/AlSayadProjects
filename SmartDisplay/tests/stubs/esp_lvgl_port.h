/**
 * @file esp_lvgl_port.h
 * @brief AlSayad esp_lvgl_port module.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool lvgl_port_lock(uint32_t u32TimeoutMs);
void lvgl_port_unlock(void);
