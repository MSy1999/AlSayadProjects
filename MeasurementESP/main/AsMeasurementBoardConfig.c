/**
 * @file AsMeasurementBoardConfig.c
 * @brief MeasurementESP application wiring.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsMeasurementBoardConfig.h"
#include "AsDht11.h"
#include "TimeManager.h"
#include "AsAnalogLight.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>

#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
static AsDht11 st_Dht;
static bool bPairAttempted;
static esp_err_t pair_error;
static float f32TemperatureC;
static float f32HumidityRh;
static uint32_t u32DhtSuccesses;
static uint64_t u64NextDhtWarningUs;

static esp_err_t as_board_dht_init(void* context, uint32_t u32TimeoutMs) {
    (void)context;
    (void)u32TimeoutMs;
#ifdef CONFIG_AS_SENDER_DHT_GPIO_CAPTURE
    esp_err_t error = as_dht11_init_gpio(&st_Dht, GPIO_NUM_27);
#else
    esp_err_t error = as_dht11_init(&st_Dht, GPIO_NUM_27);
#endif
    if (error != ESP_OK) {
        ESP_LOGW("AsBoard", "DHT11 init GPIO27: %s", esp_err_to_name(error));
    }
    return error;
}

static esp_err_t as_board_dht_pair(uint32_t u32TimeoutMs) {
    if (!bPairAttempted) {
        bPairAttempted = true;
        pair_error = as_dht11_read(&st_Dht, u32TimeoutMs, &f32TemperatureC, &f32HumidityRh);
        if (pair_error != ESP_OK) {
            uint64_t u64NowUs = (uint64_t)esp_timer_get_time();
            if (u64NowUs >= u64NextDhtWarningUs) {
                u64NextDhtWarningUs = u64NowUs + 10000000U;
                ESP_LOGW("AsBoard", "DHT11 GPIO27 transaction: %s; both readings error",
                         esp_err_to_name(pair_error));
            }
        } else if (u32DhtSuccesses++ % 5U == 0) {
            ESP_LOGI("AsBoard", "DHT11 temperature=%.1f C humidity=%.1f %%RH",
                     (double)f32TemperatureC, (double)f32HumidityRh);
        }
    }
    return pair_error;
}

static esp_err_t as_board_temperature(void* context, uint32_t u32TimeoutMs, float* pf32Value) {
    (void)context;
    esp_err_t error = as_board_dht_pair(u32TimeoutMs);
    if (error == ESP_OK) {
        *pf32Value = f32TemperatureC;
    }
    return error;
}

static esp_err_t as_board_humidity(void* context, uint32_t u32TimeoutMs, float* pf32Value) {
    (void)context;
    esp_err_t error = as_board_dht_pair(u32TimeoutMs);
    if (error == ESP_OK) {
        *pf32Value = f32HumidityRh;
    }
    return error;
}
#endif

#if defined(CONFIG_AS_SENDER_LIGHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
static AsAnalogLight st_Light;
static uint32_t u32LightSamples;
static esp_err_t as_board_light_init(void* context, uint32_t u32TimeoutMs) {
    (void)context;
    (void)u32TimeoutMs;
    esp_err_t error = as_analog_light_init(&st_Light, 34);
    if (error != ESP_OK) {
        ESP_LOGW("AsBoard", "KY-018 ADC init GPIO34: %s", esp_err_to_name(error));
    }
    return error;
}
static esp_err_t as_board_light_read(void* context, uint32_t u32TimeoutMs, float* pf32Value) {
    (void)context;
    float f32Raw = 0;
    esp_err_t error = as_analog_light_read(&st_Light, u32TimeoutMs, &f32Raw);
    if (error != ESP_OK) {
        return error;
    }
    bool bLogRaw = true;
#ifdef CONFIG_AS_SENDER_LIGHT_CALIBRATED
    bLogRaw = u32LightSamples++ % 5U == 0;
#else
    ++u32LightSamples;
#endif
    if (bLogRaw) {
        ESP_LOGI("AsBoard", "KY-018 raw average=%.2f (GPIO34 ADC1/6)", (double)f32Raw);
    }
#ifdef CONFIG_AS_SENDER_LIGHT_CALIBRATED
    return as_measurement_light_convert(f32Raw, CONFIG_AS_SENDER_LIGHT_DARK_RAW,
                                         CONFIG_AS_SENDER_LIGHT_BRIGHT_RAW, pf32Value);
#else
    (void)pf32Value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
#endif

void as_measurement_board_begin_sample(void) {
#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    bPairAttempted = false;
    pair_error = ESP_ERR_INVALID_STATE;
    f32TemperatureC = 0;
    f32HumidityRh = 0;
#endif
}

bool as_measurement_board_config_valid(void) {
#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    if (CONFIG_AS_SENDER_PERIOD_MS < 2000) {
        return false;
    }
#endif
#if defined(CONFIG_AS_SENDER_LIGHT_CALIBRATED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    if (CONFIG_AS_SENDER_LIGHT_DARK_RAW == CONFIG_AS_SENDER_LIGHT_BRIGHT_RAW) {
        return false;
    }
#endif
    return true;
}

uint32_t as_measurement_board_startup_ms(void) {
#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    return 2000;
#else
    return 0;
#endif
}

void as_measurement_board_configure(AsMeasurementSensor* pst_Sensors) {
    if (pst_Sensors == NULL) {
        return;
    }
    memset(pst_Sensors, 0, sizeof(*pst_Sensors) * AS_METRIC_COUNT);
#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    pst_Sensors[AS_METRIC_TEMPERATURE] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_dht_init, .read = as_board_temperature, .u32TimeoutMs = 100};
#ifdef CONFIG_AS_SENDER_DHT_GPIO_CAPTURE
    ESP_LOGI("AsBoard", "DHT11 GPIO27 capture=GPIO edge-sync=2; retry transactions each sampling period");
#else
    ESP_LOGI("AsBoard", "DHT11 GPIO27 capture=RMT; retry transactions each sampling period");
#endif
    pst_Sensors[AS_METRIC_HUMIDITY] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_dht_init, .read = as_board_humidity, .u32TimeoutMs = 100};
#endif
#if defined(CONFIG_AS_SENDER_LIGHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    pst_Sensors[AS_METRIC_LIGHT] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_light_init, .read = as_board_light_read, .u32TimeoutMs = 100};
#ifdef CONFIG_AS_SENDER_LIGHT_CALIBRATED
    ESP_LOGI("AsBoard", "KY-018 enabled on GPIO34 with relative-light calibration");
#else
    ESP_LOGI("AsBoard", "KY-018 enabled on GPIO34: raw logging; percent unavailable until calibrated");
#endif
#elif !defined(CONFIG_AS_SENDER_SIMULATED)
    ESP_LOGI("AsBoard", "KY-018 disabled in menuconfig; brightness unavailable");
#endif
}
