/**
 * @file main.c
 * @brief MeasurementESP application wiring.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsMeasurementBoardConfig.h"
#include "AsMeasurementSender.h"
#include "AsMeasurementHttp.h"
#include "AsWifiStation.h"
#include "TimeManager.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>

/** One-slot queue owns copies; no pointer to a mutable snapshot crosses tasks. */
typedef struct {
    AsMeasurement st_Measurement;
    uint64_t u64AcquiredMs;
} AsSenderSnapshot;
static QueueHandle_t snapshot_queue;
static const char* pcTag = "MeasurementESP";

/* Clock boundary: pure service/policy APIs accept time rather than reading clocks. */
static uint64_t as_sender_now_ms(void) {
    return (uint64_t)MICRO_SEC_TO_MS(esp_timer_get_time());
}

static void as_sender_sampling_worker(void* context) {
    (void)context;
    AsMeasurementSensor ast_Sensors[AS_METRIC_COUNT];
    /* Application wiring: sensor drivers only need their pin/config and a read budget. */
    AsMeasurementBoardConfig st_Config = {
        .st_Dht = AS_DHT11_CONFIG_DEFAULT(GPIO_NUM_27),
        .st_Light = AS_ANALOG_LIGHT_CONFIG_DEFAULT(34),
        .u32DhtTimeoutMs = 100,
        .u32LightTimeoutMs = 100
    };
#ifndef CONFIG_AS_SENDER_DHT_GPIO_CAPTURE
    st_Config.st_Dht.eCapture = AS_DHT11_CAPTURE_RMT;
#endif
    esp_err_t configure_error = as_measurement_board_configure(ast_Sensors, &st_Config);
    if (configure_error != ESP_OK) {
        ESP_LOGE(pcTag, "sensor configuration: %s", esp_err_to_name(configure_error));
        vTaskDelete(NULL);
        return;
    }
    uint64_t u64DeadlineMs = as_sender_now_ms() + as_measurement_board_startup_ms();
    uint64_t au64Errors[AS_METRIC_COUNT] = {0};
    uint32_t u32Samples = 0;
    for (;;) {
        uint64_t u64NowMs = as_sender_now_ms();
        if (u64NowMs < u64DeadlineMs) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        AsSenderSnapshot st_Snapshot = {.u64AcquiredMs = u64NowMs};
        bool bSimulated = false;
#ifdef CONFIG_AS_SENDER_SIMULATED
        bSimulated = true;
#endif
        as_measurement_board_begin_sample();
        esp_err_t error = as_measurement_sensors_sample(ast_Sensors,
            CONFIG_AS_SENDER_DEVICE_ID, bSimulated, u64NowMs, &st_Snapshot.st_Measurement);
        if (error == ESP_OK) {
            for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
                if (st_Snapshot.st_Measurement.ast_Readings[index].eStatus == AS_READING_ERROR) {
                    ++au64Errors[index];
                }
            }
            if (xQueueOverwrite(snapshot_queue, &st_Snapshot) != pdPASS) {
                ESP_LOGE(pcTag, "snapshot publication failed");
            }
        } else {
            ESP_LOGE(pcTag, "acquisition: %s", esp_err_to_name(error));
        }
        if (u32Samples++ % 30U == 0) {
            ESP_LOGI(pcTag, "sensor errors T/H/L=%" PRIu64 "/%" PRIu64 "/%" PRIu64
                "; minimum heap=%lu", au64Errors[0], au64Errors[1], au64Errors[2],
                (unsigned long)esp_get_minimum_free_heap_size());
        }
        u64DeadlineMs = as_measurement_sender_next_period(u64DeadlineMs,
            as_sender_now_ms(), CONFIG_AS_SENDER_PERIOD_MS);
    }
}

static const char* as_sender_status_name(AsReadingStatus eStatus) {
    switch (eStatus) {
        case AS_READING_VALID: return "valid";
        case AS_READING_ERROR: return "error";
        case AS_READING_UNAVAILABLE: return "unavailable";
        default: return "invalid";
    }
}

static void as_sender_http_worker(void* context) {
    (void)context;
    AsMeasurementSender st_Policy = {.u32PeriodMs = CONFIG_AS_SENDER_PERIOD_MS};
    uint64_t u64LastAcceptedMs = 0;
    uint64_t u64Failures = 0;
    uint64_t u64LastLogMs = 0;
    bool bAccepted = false;
    for (;;) {
        AsSenderSnapshot st_Snapshot;
        if (xQueuePeek(snapshot_queue, &st_Snapshot, pdMS_TO_TICKS(100)) == pdPASS
            && as_measurement_sender_ready(&st_Policy, as_wifi_station_ready(),
                as_sender_now_ms(), st_Snapshot.u64AcquiredMs)) {
            char acFrame[AS_MEASUREMENT_FRAME_BYTES + 1U];
            size_t length = 0;
            as_measurement_sender_attempt(&st_Policy, st_Snapshot.u64AcquiredMs);
            esp_err_t error = as_measurement_encode(&st_Snapshot.st_Measurement,
                                                    acFrame, sizeof(acFrame), &length);
            int32_t i32Status = 0;
            /* Recheck the live handoff immediately before starting the request. */
            uint64_t u64NowMs = as_sender_now_ms();
            if (error == ESP_OK && as_wifi_station_ready()
                && u64NowMs - st_Snapshot.u64AcquiredMs <= 2ULL * CONFIG_AS_SENDER_PERIOD_MS) {
                error = as_measurement_http_post(CONFIG_AS_SENDER_URL,
                    CONFIG_AS_SENDER_HTTP_TIMEOUT_MS, acFrame, length, &i32Status);
            } else if (error == ESP_OK) {
                error = ESP_ERR_INVALID_STATE;
            }
            u64NowMs = as_sender_now_ms();
            as_measurement_sender_result(&st_Policy, u64NowMs,
                                          error == ESP_OK ? i32Status : 0);
            if (error == ESP_OK && i32Status == 202) {
                u64LastAcceptedMs = u64NowMs;
                bAccepted = true;
                ESP_LOGI(pcTag, "HTTP 202 accepted (%u bytes) T=%s H=%s L=%s", (unsigned)length,
                    as_sender_status_name(st_Snapshot.st_Measurement.ast_Readings[0].eStatus),
                    as_sender_status_name(st_Snapshot.st_Measurement.ast_Readings[1].eStatus),
                    as_sender_status_name(st_Snapshot.st_Measurement.ast_Readings[2].eStatus));
            } else {
                ++u64Failures;
                if (u64Failures == 1 || u64NowMs - u64LastLogMs >= 10000U) {
                    ESP_LOGW(pcTag, "HTTP=%ld error=%s failures=%" PRIu64
                        " acceptance age=%" PRIu64 " ms (never accepted=%d)",
                        (long)i32Status, esp_err_to_name(error), u64Failures,
                        bAccepted ? u64NowMs - u64LastAcceptedMs : 0, !bAccepted);
                    u64LastLogMs = u64NowMs;
                }
            }
        }
        /* Queue remains populated. Always yield so offline/error paths cannot spin. */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void) {
    if (!as_measurement_sender_config_valid(CONFIG_AS_SENDER_WIFI_SSID,
        CONFIG_AS_SENDER_WIFI_PASSWORD, CONFIG_AS_SENDER_DEVICE_ID, CONFIG_AS_SENDER_URL,
        CONFIG_AS_SENDER_PERIOD_MS, CONFIG_AS_SENDER_HTTP_TIMEOUT_MS)
        || !as_measurement_board_config_valid()) {
        ESP_LOGE(pcTag, "Invalid sender configuration; check menuconfig");
        return;
    }
    snapshot_queue = xQueueCreate(1, sizeof(AsSenderSnapshot));
    if (snapshot_queue == NULL) {
        ESP_LOGE(pcTag, "snapshot queue allocation failed");
        return;
    }
    if (xTaskCreate(as_sender_sampling_worker, "AsSampling", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(pcTag, "sampling task allocation failed");
        vQueueDelete(snapshot_queue);
        snapshot_queue = NULL;
        return;
    }
    esp_err_t error = nvs_flash_init();
    if (error == ESP_OK) {
        error = as_wifi_station_start(CONFIG_AS_SENDER_WIFI_SSID, CONFIG_AS_SENDER_WIFI_PASSWORD);
    }
    if (error != ESP_OK) {
        ESP_LOGE(pcTag, "network initialization: %s; sampling continues; NVS preserved",
                 esp_err_to_name(error));
        return;
    }
    if (xTaskCreate(as_sender_http_worker, "AsHttp", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(pcTag, "HTTP task allocation failed; sampling continues");
    }
}
