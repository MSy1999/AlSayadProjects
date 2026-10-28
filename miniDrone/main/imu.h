#pragma once
#include "as_flight.h"
#include "esp_err.h"
esp_err_t as_imu_init(void);
esp_err_t as_imu_read(as_sample_t *sample);
