/**
 * @file main.c
 * @brief Connect room monitor components, initialize the display, receive measurements, and manage their state.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsRoomRuntime.h"
#include "AsRoomUi.h"
#include "AsMeasurementJson.h"
#include "AsWifiTransport.h"
#include "AsSdHistory.h"
#include "AsDisplayBrand.h"
#include "AsBoardDisplayConfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <string.h>

/* Measurements and brightness save requests share one queue; the main loop processes them sequentially. */
typedef enum { AS_ROOM_RECEIVE, AS_ROOM_BRIGHTNESS } AsRoomEventType;
typedef struct {
    AsRoomEventType eType;
    AsMeasurement st_Measurement;
    uint64_t u64ReceivedMs;
    uint8_t u8Brightness;
} AsRoomEvent;
/* The measurement store is independent of the transport; the mutex protects it during copies and updates. */
static AsMeasurementStore st_Store;
static AsHistoryStorage st_Storage;
static QueueHandle_t event_queue;
static SemaphoreHandle_t store_mutex;
static bool bConnected;
static bool bTransportReady;
static uint8_t u8Brightness = 75;

/* Local monotonic time in milliseconds, rather than wall-clock time; the two boards do not need synchronized clocks. */
static uint64_t as_room_now(void) {
    return (uint64_t)esp_timer_get_time() / 1000U;
}
/* Common data entry point: any protocol can submit prepared measurements without going through JSON. */
esp_err_t as_room_submit(const AsMeasurement* pst_Measurement) {
    if (!as_measurement_valid(pst_Measurement)
        || strcmp(pst_Measurement->acDeviceId, CONFIG_AS_ROOM_DEVICE_ID) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Copy the data and its arrival time; do not retain a pointer to memory the receiver may free. */
    AsRoomEvent st_Event = {.eType = AS_ROOM_RECEIVE, .st_Measurement = *pst_Measurement,
                             .u64ReceivedMs = as_room_now()};
    /* Nonblocking sends prevent a full queue from blocking the network receive task. */
    return xQueueSend(event_queue, &st_Event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* This callback only decodes and validates the message; state updates and rendering are handled elsewhere. */
static esp_err_t as_room_frame(const char* pcFrame, size_t length, void* pContext) {
    (void)pContext;
    AsMeasurement st_Measurement;
    esp_err_t error = as_measurement_decode(pcFrame, length, CONFIG_AS_ROOM_DEVICE_ID, &st_Measurement);
    return error == ESP_OK ? as_room_submit(&st_Measurement) : error;
}
/* Provide the UI with a consistent snapshot so it cannot read the store during a partial update. */
esp_err_t as_room_snapshot(AsRoomSnapshot* pst_Output, AsHistoryRecord* pst_History) {
    if (pst_Output == NULL || store_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(store_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    as_measurements_snapshot(&st_Store, as_room_now(), &pst_Output->st_Measurement, &pst_Output->u64AgeMs);
    pst_Output->bConnected = bConnected;
    pst_Output->bTransportReady = bTransportReady;
    pst_Output->u8Brightness = u8Brightness;
    pst_Output->history_count = pst_History != NULL
        ? as_measurements_history(&st_Store, pst_History, AS_HISTORY_COUNT) : 0;
    xSemaphoreGive(store_mutex);
    /* Release the measurement mutex before requesting storage status to minimize lock duration. */
    if (st_Storage.snapshot != NULL) {
        st_Storage.snapshot(st_Storage.pContext, &pst_Output->st_Storage, NULL);
    }
    return ESP_OK;
}
/* Called by the UI when brightness adjustment ends; persistence happens outside the UI callback. */
esp_err_t as_room_save_brightness(uint8_t u8Percent) {
    if (u8Percent < 1 || u8Percent > 100 || event_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    AsRoomEvent st_Event = {.eType = AS_ROOM_BRIGHTNESS, .u8Brightness = u8Percent};
    return xQueueSend(event_queue, &st_Event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
/* Reading a historical window is asynchronous; the request ID links the result to the user selection. */
bool as_room_history_request(uint64_t u64Session, uint64_t u64StartMs, uint32_t u32Request) {
    return st_Storage.request_window != NULL
        && st_Storage.request_window(st_Storage.pContext, u64Session, u64StartMs, u32Request);
}
/* Copy the latest available result without reading a file directly from the UI. */
void as_room_history_result(AsHistoryWindow* pst_Output) {
    if (st_Storage.snapshot != NULL) {
        st_Storage.snapshot(st_Storage.pContext, NULL, pst_Output);
    }
}
static void as_room_log_error(const char* pcAction, esp_err_t error) {
    if (error != ESP_OK) {
        ESP_LOGE("RoomMonitor", "%s: %s", pcAction, esp_err_to_name(error));
    }
}
/* Store brightness in NVS so it survives restarts, then commit the write. */
static void as_room_persist(uint8_t u8Percent) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("room-monitor", NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        error = nvs_set_u8(handle, "brightness", u8Percent);
        if (error == ESP_OK) {
            error = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    as_room_log_error("Save brightness", error);
}
/* Each history bucket spans five seconds; close completed buckets and pass them to optional storage. */
static void as_room_close_buckets(uint64_t u64NowMs) {
    AsHistoryRecord st_Closed;
    while (as_measurements_tick(&st_Store, u64NowMs, &st_Closed)) {
        if (st_Storage.append != NULL && !st_Storage.append(st_Storage.pContext, &st_Closed)) {
            ESP_LOGW("RoomMonitor", "Recording queue full; chart continues in RAM");
        }
    }
}
/* ESP-IDF entry point: initialize and connect components, then continuously supervise application state. */
void app_main(void) {
    /* Do not erase NVS automatically on failure, to preserve existing settings. */
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        as_room_log_error("NVS initialization (not erased automatically)", error);
        return;
    }
    /* Keep the default brightness if no saved setting exists, and correct any out-of-range value. */
    nvs_handle_t handle;
    error = nvs_open("room-monitor", NVS_READONLY, &handle);
    if (error == ESP_OK) {
        error = nvs_get_u8(handle, "brightness", &u8Brightness);
        nvs_close(handle);
    }
    if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) {
        as_room_log_error("Read brightness", error);
    }
    if (u8Brightness < 1 || u8Brightness > 100) {
        u8Brightness = 75;
    }
    /* The mutex protects the shared store, and the queue separates event reception from processing. */
    store_mutex = xSemaphoreCreateMutex();
    event_queue = xQueueCreate(8, sizeof(AsRoomEvent));
    if (store_mutex == NULL || event_queue == NULL) {
        ESP_LOGE("RoomMonitor", "Cannot allocate application synchronization");
        return;
    }
    as_measurements_init(&st_Store, as_room_now());
    /* Use the working board configuration, then set the orientation before displaying content. */
    const as_display_config_t st_Display = as_board_display_config();
    error = as_display_init_with_config(&st_Display);
    if (error == ESP_OK) {
        error = as_display_rotate(AS_BOARD_DISPLAY_ROTATION);
    }
    if (error == ESP_OK) {
        error = as_display_set_brightness(75);
    }
    if (error == ESP_OK) {
        /* The logo uses a nonblocking timer, allowing initialization to continue while it is displayed. */
        error = as_display_show_boot_logo(2000);
    }
    if (error != ESP_OK) {
        as_room_log_error("Display startup", error);
        as_room_log_error("Display cleanup", as_display_deinit());
        return;
    }
    /* The display and touch controller reserve both SPI buses on this board. Use RAM history
     * if SD conflicts, until a compatible resource allocation or storage adapter is available. */
    AsSdHistoryConfig st_SdConfig = {
        #ifdef CONFIG_AS_ROOM_SD_ENABLED
        .bEnabled = true,
#else
        .bEnabled = false,
#endif
        .i32Host = CONFIG_AS_ROOM_SD_HOST,
        .i32Mosi = CONFIG_AS_ROOM_SD_MOSI, .i32Miso = CONFIG_AS_ROOM_SD_MISO,
        .i32Sclk = CONFIG_AS_ROOM_SD_SCLK, .i32Cs = CONFIG_AS_ROOM_SD_CS,
        .u32ReservedHosts = (1U << st_Display.lcd_host)
            | (st_Display.touch_enabled ? (1U << st_Display.touch_host) : 0U),
        .budget_bytes = (size_t)CONFIG_AS_ROOM_SD_BUDGET_MIB * 1024U * 1024U,
    };
    /* Tell the SD adapter which pins are reserved so it does not reinitialize pins used by the display or touch controller. */
    const gpio_num_t aePins[] = {st_Display.lcd_mosi, st_Display.lcd_miso, st_Display.lcd_sclk,
        st_Display.lcd_cs, st_Display.lcd_dc, st_Display.lcd_reset, st_Display.backlight_pin,
        st_Display.touch_enabled ? st_Display.touch_mosi : GPIO_NUM_NC,
        st_Display.touch_enabled ? st_Display.touch_miso : GPIO_NUM_NC,
        st_Display.touch_enabled ? st_Display.touch_sclk : GPIO_NUM_NC,
        st_Display.touch_enabled ? st_Display.touch_cs : GPIO_NUM_NC,
        st_Display.touch_interrupt ? st_Display.touch_irq : GPIO_NUM_NC};
    for (size_t index = 0; index < sizeof(aePins) / sizeof(aePins[0]); ++index) {
        if (aePins[index] >= 0 && aePins[index] < 64) {
            st_SdConfig.u64ReservedGpios |= UINT64_C(1) << aePins[index];
        }
    }
    /* The adapter determines storage availability; the UI accesses it through a generic interface independent of SD. */
    AsSdHistory* pst_Sd = NULL;
    error = as_sd_history_start(&st_SdConfig, &pst_Sd);
    if (error == ESP_OK) {
        st_Storage = as_sd_history_interface(pst_Sd);
    } else {
        as_room_log_error("History worker", error);
    }
    /* The UI starts its own refresh timer; it does not receive data or write files. */
    error = as_room_ui_start();
    if (error != ESP_OK) {
        as_room_log_error("UI setup", error);
        /* Stop the UI timer before deleting the display so it cannot use invalid handles. */
        esp_err_t cleanup = as_room_ui_stop();
        as_room_log_error("UI stop", cleanup);
        if (cleanup == ESP_OK) {
            as_room_log_error("Display cleanup", as_display_deinit());
        }
        return;
    }
    /* Wi-Fi details are confined to the adapter; replacing it does not change the measurement model or rendering. */
    AsWifiTransport* pst_Wifi = NULL;
    const AsWifiTransportConfig st_WifiConfig = {
        .pcSsid = CONFIG_AS_ROOM_AP_SSID, .pcPassword = CONFIG_AS_ROOM_AP_PASSWORD, .u8Channel = 1,
    };
    error = as_wifi_transport_create(&st_WifiConfig, &pst_Wifi);
    AsTransport st_Transport = as_wifi_transport_interface(pst_Wifi);
    if (error == ESP_OK) {
        error = st_Transport.start(st_Transport.pContext, as_room_frame, NULL);
    }
    if (xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        bTransportReady = error == ESP_OK;
        xSemaphoreGive(store_mutex);
    }
    as_room_log_error("Wi-Fi receiver", error);
    ESP_LOGI("RoomMonitor", "AP %s; receiver http://192.168.4.1/api/v1/measurements; free heap=%u",
             CONFIG_AS_ROOM_AP_SSID, (unsigned)esp_get_free_heap_size());
    uint64_t u64LastLinkMs = 0;
    uint64_t u64BootLogoEndMs = as_room_now() + 2000U;
    bool bBrightnessRestored = false;
    /* The processing loop receives events and updates state; rendering runs in a separate LVGL task. */
    for (;;) {
        AsRoomEvent st_Event;
        bool bEvent = xQueueReceive(event_queue, &st_Event, pdMS_TO_TICKS(100)) == pdTRUE;
        uint64_t u64NowMs = as_room_now();
        if (xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
            if (bEvent && st_Event.eType == AS_ROOM_RECEIVE) {
                /* Close earlier buckets before placing the measurement in the bucket in which it arrived. */
                as_room_close_buckets(st_Event.u64ReceivedMs);
                if (!as_measurements_accept(&st_Store, &st_Event.st_Measurement, st_Event.u64ReceivedMs)) {
                    ESP_LOGW("RoomMonitor", "Rejected queued measurement");
                }
            }
            /* History continues to advance during data interruptions, so missing periods appear as gaps. */
            as_room_close_buckets(u64NowMs);
            if (bEvent && st_Event.eType == AS_ROOM_BRIGHTNESS) {
                u8Brightness = st_Event.u8Brightness;
            }
            xSemaphoreGive(store_mutex);
        }
        /* Write to NVS after releasing the store mutex to avoid delaying data snapshots for the UI. */
        if (bEvent && st_Event.eType == AS_ROOM_BRIGHTNESS) {
            as_room_persist(st_Event.u8Brightness);
        }
        /* Restore the saved brightness after the boot logo display period. */
        if (!bBrightnessRestored && u64NowMs >= u64BootLogoEndMs) {
            as_room_log_error("Restore brightness", as_display_set_brightness(u8Brightness));
            bBrightnessRestored = true;
        }
        /* Check the connection once per second; a connected device does not imply valid measurements are arriving. */
        if (bTransportReady && u64NowMs - u64LastLinkMs >= 1000U) {
            bool bLink = st_Transport.connected(st_Transport.pContext);
            if (xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
                bConnected = bLink;
                xSemaphoreGive(store_mutex);
            }
            u64LastLinkMs = u64NowMs;
        }
    }
}
