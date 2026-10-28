/** @file WifiManager.h @brief Radio-only AlSayadLib variant. */
#pragma once
#include "esp_err.h"
esp_err_t as_wifi_start(const char *ssid,const char *password,unsigned channel);
