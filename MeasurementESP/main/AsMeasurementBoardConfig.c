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

static AsMeasurementBoardConfig st_BoardConfig;

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
    esp_err_t error = as_dht11_init_config(&st_Dht, &st_BoardConfig.st_Dht);
    if (error != ESP_OK) {
        ESP_LOGW("AsBoard", "DHT11 init GPIO%d: %s",
                 (int)st_BoardConfig.st_Dht.ePin, esp_err_to_name(error));
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
                ESP_LOGW("AsBoard", "DHT11 GPIO%d transaction: %s; both readings error",
                         (int)st_BoardConfig.st_Dht.ePin, esp_err_to_name(pair_error));
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
    esp_err_t error = as_analog_light_init_config(&st_Light, &st_BoardConfig.st_Light);
    if (error != ESP_OK) {
        ESP_LOGW("AsBoard", "KY-018 ADC init GPIO%ld: %s",
                 (long)st_BoardConfig.st_Light.i32Pin, esp_err_to_name(error));
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
        ESP_LOGI("AsBoard", "KY-018 raw average=%.2f (GPIO%ld)",
                 (double)f32Raw, (long)st_BoardConfig.st_Light.i32Pin);
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

esp_err_t as_measurement_board_configure(AsMeasurementSensor* pst_Sensors,
                                         const AsMeasurementBoardConfig* pst_Config) {
    if (pst_Sensors == NULL || pst_Config == NULL
        || pst_Config->u32DhtTimeoutMs < 50 || pst_Config->u32DhtTimeoutMs > 1000
        || pst_Config->u32LightTimeoutMs < 10 || pst_Config->u32LightTimeoutMs > 1000) {
        return ESP_ERR_INVALID_ARG;
    }
    st_BoardConfig = *pst_Config;
    memset(pst_Sensors, 0, sizeof(*pst_Sensors) * AS_METRIC_COUNT);
#if defined(CONFIG_AS_SENDER_DHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    pst_Sensors[AS_METRIC_TEMPERATURE] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_dht_init, .read = as_board_temperature, .u32TimeoutMs = pst_Config->u32DhtTimeoutMs};
    ESP_LOGI("AsBoard", "DHT11 GPIO%d capture=%s; retry transactions each sampling period",
        (int)pst_Config->st_Dht.ePin,
        pst_Config->st_Dht.eCapture == AS_DHT11_CAPTURE_GPIO ? "GPIO edge-sync=2" : "RMT");
    pst_Sensors[AS_METRIC_HUMIDITY] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_dht_init, .read = as_board_humidity, .u32TimeoutMs = pst_Config->u32DhtTimeoutMs};
#endif
#if defined(CONFIG_AS_SENDER_LIGHT_ENABLED) && !defined(CONFIG_AS_SENDER_SIMULATED)
    pst_Sensors[AS_METRIC_LIGHT] = (AsMeasurementSensor){.bEnabled = true,
        .init = as_board_light_init, .read = as_board_light_read, .u32TimeoutMs = pst_Config->u32LightTimeoutMs};
#ifdef CONFIG_AS_SENDER_LIGHT_CALIBRATED
    ESP_LOGI("AsBoard", "KY-018 enabled on GPIO%ld with relative-light calibration",
             (long)pst_Config->st_Light.i32Pin);
#else
    ESP_LOGI("AsBoard", "KY-018 enabled on GPIO%ld: raw logging; percent unavailable until calibrated",
             (long)pst_Config->st_Light.i32Pin);
#endif
#elif !defined(CONFIG_AS_SENDER_SIMULATED)
    ESP_LOGI("AsBoard", "KY-018 disabled in menuconfig; brightness unavailable");
#endif
    return ESP_OK;
}
