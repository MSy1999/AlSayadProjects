/**
 * @file AsHardwareTests.c
 * @brief Test real board/driver code with injected RMT, ADC, clock and queue SDK boundaries.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsDht11.h"
#include "AsAnalogLight.h"
#include "AsMeasurementBoardConfig.h"
#include "sdkconfig.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct AsFakeQueue { size_t value; bool bPending; };
struct AsFakeRmt { rmt_rx_event_callbacks_t st_Callbacks; void* context; bool bEnabled; };
struct AsFakeAdc { int unused; };
static uint64_t u64NowUs = 3000000;
static uint64_t u64LastStartUs;
static uint32_t u32Starts;
static uint32_t u32Transactions;
static uint32_t u32Queues;
static uint32_t u32Channels;
static uint32_t u32Units;
static bool bRmtTimeout;
static bool bBadChecksum;
static bool bRmtInitFailure;
static bool bAdcConfigFailure;
static bool bAdcReadFailure;
static int adc_raw = 2000;
static uint32_t u32GpioLevel = 1;
static bool bGpioStuckHigh;
static bool bGpioStuckLow;
static uint32_t u32GpioRiseDelayUs = 3;
static uint32_t u32GpioReleaseObservationDelayUs;
static bool bGpioFixture;
static bool bGpioFixtureActive;
static uint64_t u64GpioReleaseUs;
void esp_rom_delay_us(uint32_t u32DelayUs) { u64NowUs += u32DelayUs; }
int64_t esp_timer_get_time(void) { return (int64_t)u64NowUs; }
void vTaskDelay(TickType_t ticks) {
    /* Exercise wakes near the next tick boundary, not just exact whole-tick delays. */
    u64NowUs += ticks > 0 ? ticks * 10000ULL - 9000ULL : 0;
}
QueueHandle_t xQueueCreate(size_t count, size_t size) {
    assert(count == 1 && size == sizeof(size_t));
    ++u32Queues;
    return calloc(1, sizeof(struct AsFakeQueue));
}
BaseType_t xQueueOverwriteFromISR(QueueHandle_t queue, const void* item, BaseType_t* woken) {
    memcpy(&queue->value, item, sizeof(size_t));
    queue->bPending = true;
    *woken = pdTRUE;
    return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t ticks) {
    if (!queue->bPending) {
        vTaskDelay(ticks);
        return pdFALSE;
    }
    memcpy(item, &queue->value, sizeof(size_t));
    queue->bPending = false;
    return pdPASS;
}
BaseType_t xQueueReset(QueueHandle_t queue) { queue->bPending = false; return pdPASS; }
void vQueueDelete(QueueHandle_t queue) { --u32Queues; free(queue); }
esp_err_t gpio_config(const gpio_config_t* pst_Config) {
    assert(pst_Config->pin_bit_mask == (1ULL << 27));
    assert(pst_Config->mode == GPIO_MODE_INPUT_OUTPUT_OD);
    return ESP_OK;
}
esp_err_t gpio_set_direction(gpio_num_t pin, int mode) {
    assert(pin == 27 && (mode == GPIO_MODE_INPUT || mode == GPIO_MODE_INPUT_OUTPUT_OD));
    return ESP_OK;
}
int gpio_get_level(gpio_num_t pin) {
    assert(pin == 27);
    if (bGpioStuckHigh) {
        return 1;
    }
    if (bGpioStuckLow) {
        return 0;
    }
    if (!bGpioFixture || !bGpioFixtureActive || bRmtTimeout) {
        return (int)u32GpioLevel;
    }
    uint64_t u64ElapsedUs = u64NowUs - u64GpioReleaseUs;
    /* Open-drain release has a finite pull-up rise time, not an instantaneous edge.
     * Ignoring this can miscount the release transient as the response preamble. */
    if (u64ElapsedUs < u32GpioRiseDelayUs) { return 0; }
    if (u64ElapsedUs < 30) { return 1; }
    u64ElapsedUs -= 30;
    if (u64ElapsedUs < 80) { return 0; }
    u64ElapsedUs -= 80;
    if (u64ElapsedUs < 80) { return 1; }
    u64ElapsedUs -= 80;
    uint8_t au8Bytes[] = {55, 0, 23, 0, bBadChecksum ? 0 : 78};
    for (size_t bit = 0; bit < 40; ++bit) {
        if (u64ElapsedUs < 50) { return 0; }
        u64ElapsedUs -= 50;
        uint32_t u32HighUs = (au8Bytes[bit / 8] & (1U << (7 - bit % 8))) ? 70 : 27;
        if (u64ElapsedUs < u32HighUs) { return 1; }
        u64ElapsedUs -= u32HighUs;
    }
    return u64ElapsedUs < 50 ? 0 : 1;
}
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
    assert(pin == 27 && level <= 1);
    if (level == 1 && u32GpioLevel == 0 && bGpioFixture) {
        u64GpioReleaseUs = u64NowUs;
        bGpioFixtureActive = true;
        u64NowUs += u32GpioReleaseObservationDelayUs;
    }
    u32GpioLevel = level;
    if (level == 0) {
        bGpioFixtureActive = false;
        assert(u32Starts == 0 || u64NowUs - u64LastStartUs >= 2000000);
        u64LastStartUs = u64NowUs;
        ++u32Starts;
    }
    return ESP_OK;
}
esp_err_t rmt_new_rx_channel(const rmt_rx_channel_config_t* pst_Config, rmt_channel_handle_t* output) {
    assert(pst_Config->gpio_num == 27 && pst_Config->resolution_hz == 1000000);
    if (bRmtInitFailure) {
        return ESP_ERR_NO_MEM;
    }
    ++u32Channels;
    *output = calloc(1, sizeof(struct AsFakeRmt));
    return ESP_OK;
}
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t channel,
    const rmt_rx_event_callbacks_t* pst_Callbacks, void* context) {
    channel->st_Callbacks = *pst_Callbacks;
    channel->context = context;
    return ESP_OK;
}
esp_err_t rmt_enable(rmt_channel_handle_t channel) {
    assert(!channel->bEnabled);
    channel->bEnabled = true;
    return ESP_OK;
}
esp_err_t rmt_disable(rmt_channel_handle_t channel) {
    assert(channel->bEnabled);
    channel->bEnabled = false;
    return ESP_OK;
}
esp_err_t rmt_del_channel(rmt_channel_handle_t channel) {
    assert(!channel->bEnabled);
    --u32Channels;
    free(channel);
    return ESP_OK;
}
esp_err_t rmt_receive(rmt_channel_handle_t channel, void* buffer, size_t bytes,
                       const rmt_receive_config_t* pst_Config) {
    assert(channel->bEnabled && bytes >= 43 * sizeof(rmt_symbol_word_t));
    assert(pst_Config->signal_range_max_ns > 90000 && pst_Config->signal_range_max_ns <= 2000000);
    ++u32Transactions;
    if (bRmtTimeout) {
        return ESP_OK;
    }
    uint8_t au8Bytes[] = {55, 0, 23, 0, bBadChecksum ? 0 : 78};
    AsDht11Pulse ast_Pulses[86] = {{20, 1}, {80, 0}, {80, 1}};
    for (size_t bit = 0; bit < 40; ++bit) {
        ast_Pulses[3 + bit * 2] = (AsDht11Pulse){50, 0};
        ast_Pulses[4 + bit * 2] = (AsDht11Pulse){
            (au8Bytes[bit / 8] & (1 << (7 - bit % 8))) ? 70 : 27, 1};
    }
    ast_Pulses[83] = (AsDht11Pulse){50, 0};
    ast_Pulses[84] = (AsDht11Pulse){200, 1};
    rmt_symbol_word_t* pst_Symbols = buffer;
    for (size_t index = 0; index < 43; ++index) {
        pst_Symbols[index] = (rmt_symbol_word_t){ast_Pulses[2 * index].u16DurationUs,
            ast_Pulses[2 * index].u8Level, ast_Pulses[2 * index + 1].u16DurationUs,
            ast_Pulses[2 * index + 1].u8Level};
    }
    rmt_rx_done_event_data_t st_Event = {.num_symbols = 43};
    channel->st_Callbacks.on_recv_done(channel, &st_Event, channel->context);
    return ESP_OK;
}
esp_err_t adc_oneshot_io_to_channel(int pin, adc_unit_t* unit, adc_channel_t* channel) {
    *unit = pin == 34 ? 1 : 2;
    *channel = 6;
    return ESP_OK;
}
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* pst_Config,
                               adc_oneshot_unit_handle_t* output) {
    assert(pst_Config->unit_id == ADC_UNIT_1);
    ++u32Units;
    *output = calloc(1, sizeof(struct AsFakeAdc));
    return ESP_OK;
}
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t unit, adc_channel_t channel,
    const adc_oneshot_chan_cfg_t* pst_Config) {
    assert(unit != NULL && channel == 6 && pst_Config->atten == ADC_ATTEN_DB_12
        && pst_Config->bitwidth == ADC_BITWIDTH_12);
    return bAdcConfigFailure ? ESP_FAIL : ESP_OK;
}
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t unit, adc_channel_t channel, int* raw) {
    assert(unit != NULL && channel == 6);
    *raw = adc_raw;
    u64NowUs += 50;
    return bAdcReadFailure ? ESP_ERR_TIMEOUT : ESP_OK;
}
esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t unit) {
    --u32Units;
    free(unit);
    return ESP_OK;
}
void as_test_hardware(void) {
    AsDht11 st_Dht = {0};
    bRmtInitFailure = true;
    assert(as_dht11_init(&st_Dht, 27) == ESP_ERR_NO_MEM && u32Queues == 1);
    bRmtInitFailure = false;
    assert(as_dht11_init(&st_Dht, 27) == ESP_OK && u32Queues == 1 && u32Channels == 1);
    float f32Temperature = 0;
    float f32Humidity = 0;
    u64NowUs = 0;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_TIMEOUT);
    assert(u32Transactions == 0);
    u64NowUs = 3000000;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 23 && f32Humidity == 55);
    u64NowUs = st_Dht.u64NextAttemptUs - 5000U;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    uint32_t u32Before = u32Transactions;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_TIMEOUT);
    assert(u32Transactions == u32Before); /* refuses rapid attempts, even on error */
    u64NowUs += 2000000;
    bRmtTimeout = true;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_TIMEOUT);
    assert(!st_Dht.bEnabled);
    bRmtTimeout = false;
    u64NowUs += 2000000;
    bBadChecksum = true;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) != ESP_OK);
    bBadChecksum = false;
    u64NowUs += 2000000;
    bGpioStuckHigh = true;
    u32Before = u32Transactions;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_INVALID_STATE);
    assert(u32Transactions == u32Before && u32GpioLevel == 1 && !st_Dht.bEnabled);
    bGpioStuckHigh = false;
    u64NowUs += 2000000;
    assert(as_dht11_read(&st_Dht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(as_dht11_deinit(&st_Dht) == ESP_OK && u32Queues == 0 && u32Channels == 0);
    /* Exercise the alternate capture path using time-driven GPIO pulses, no RMT callback. */
    bGpioFixture = true;
    AsDht11 st_GpioDht = {0};
    u64NowUs += 2000000;
    assert(as_dht11_init_gpio(&st_GpioDht, 27) == ESP_OK);
    assert(u32Queues == 0 && u32Channels == 0);
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 23 && f32Humidity == 55);
    const uint32_t au32RiseDelaysUs[] = {0, 3, 10};
    for (size_t index = 0; index < 3; ++index) {
        u64NowUs += 2000000;
        u32GpioRiseDelayUs = au32RiseDelaysUs[index];
        assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
        assert(f32Temperature == 23 && f32Humidity == 55);
    }
    u32GpioRiseDelayUs = 3;
    /* The first CPU observation may occur after the sensor has already begun LOW. */
    u32GpioReleaseObservationDelayUs = 70;
    u64NowUs += 2000000;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 23 && f32Humidity == 55);
    /* Missing the response HIGH must still fail rather than accepting data as preamble. */
    u32GpioReleaseObservationDelayUs = 180;
    u64NowUs += 2000000;
    f32Temperature = -123;
    f32Humidity = -456;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) != ESP_OK);
    assert(f32Temperature == -123 && f32Humidity == -456);
    u32GpioReleaseObservationDelayUs = 0;
    u64NowUs += 2000000;
    bGpioStuckLow = true;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_TIMEOUT);
    bGpioStuckLow = false;
    u64NowUs += 2000000;
    bRmtTimeout = true;
    uint64_t u64GpioStartedUs = u64NowUs;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_ERR_TIMEOUT);
    assert(u64NowUs - u64GpioStartedUs <= 40000);
    bRmtTimeout = false;
    u64NowUs += 2000000;
    bBadChecksum = true;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) != ESP_OK);
    bBadChecksum = false;
    u64NowUs += 2000000;
    assert(as_dht11_read(&st_GpioDht, 100, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(as_dht11_deinit(&st_GpioDht) == ESP_OK);
    bGpioFixture = false;
    AsAnalogLight st_Light = {0};
    assert(as_analog_light_init(&st_Light, 27) != ESP_OK && u32Units == 0);
    bAdcConfigFailure = true;
    assert(as_analog_light_init(&st_Light, 34) != ESP_OK && u32Units == 1);
    bAdcConfigFailure = false;
    assert(as_analog_light_init(&st_Light, 34) == ESP_OK && u32Units == 1);
    float f32Raw = -1;
    uint64_t u64StartedUs = u64NowUs;
    assert(as_analog_light_read(&st_Light, 100, &f32Raw) == ESP_OK && f32Raw == 2000);
    assert(u64NowUs - u64StartedUs >= 5000);
    bAdcReadFailure = true;
    assert(as_analog_light_read(&st_Light, 100, &f32Raw) != ESP_OK);
    bAdcReadFailure = false;
    adc_raw = 4096;
    assert(as_analog_light_read(&st_Light, 100, &f32Raw) != ESP_OK);
    adc_raw = 2000;
    assert(as_analog_light_deinit(&st_Light) == ESP_OK && u32Units == 0);
    /* Exercise actual board registration and shared snapshot service together. */
#ifdef CONFIG_AS_SENDER_DHT_GPIO_CAPTURE
    bGpioFixture = true;
#endif
    AsMeasurementSensor ast_Sensors[AS_METRIC_COUNT];
    AsMeasurement st_Frame;
    as_measurement_board_configure(ast_Sensors);
    assert(as_measurement_board_config_valid() && as_measurement_board_startup_ms() == 2000);
    for (size_t sample = 0; sample < 3; ++sample) {
        u64NowUs += 2000000;
        u32Before = u32Starts;
        as_measurement_board_begin_sample();
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false, u64NowUs / 1000, &st_Frame) == ESP_OK);
        assert(u32Starts == u32Before + 1);
        assert(st_Frame.ast_Readings[0].eStatus == AS_READING_VALID);
        assert(st_Frame.ast_Readings[1].eStatus == AS_READING_VALID);
#ifdef AS_TEST_CALIBRATED
        assert(st_Frame.ast_Readings[2].eStatus == AS_READING_VALID);
        assert(st_Frame.ast_Readings[2].f32Value == 50);
#else
        assert(st_Frame.ast_Readings[2].eStatus == AS_READING_UNAVAILABLE);
        assert(ast_Sensors[2].bReady); /* no repeated init for missing calibration */
#endif
    }
    u64NowUs += 2000000;
    bBadChecksum = true;
    as_measurement_board_begin_sample();
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, u64NowUs / 1000, &st_Frame) == ESP_OK);
    assert(st_Frame.ast_Readings[0].eStatus == AS_READING_ERROR
        && st_Frame.ast_Readings[1].eStatus == AS_READING_ERROR);
    assert(st_Frame.ast_Readings[2].eStatus != AS_READING_ERROR);
    bBadChecksum = false;
    u64NowUs += 2000000;
    bAdcReadFailure = true;
    as_measurement_board_begin_sample();
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, u64NowUs / 1000, &st_Frame) == ESP_OK);
    assert(st_Frame.ast_Readings[0].eStatus == AS_READING_VALID
        && st_Frame.ast_Readings[1].eStatus == AS_READING_VALID
        && st_Frame.ast_Readings[2].eStatus == AS_READING_ERROR);
    bAdcReadFailure = false;
    u64NowUs += 2000000;
    as_measurement_board_begin_sample();
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, u64NowUs / 1000, &st_Frame) == ESP_OK);
    assert(st_Frame.ast_Readings[0].eStatus == AS_READING_VALID
        && st_Frame.ast_Readings[1].eStatus == AS_READING_VALID
        && st_Frame.ast_Readings[2].eStatus != AS_READING_ERROR);
    puts("PASS: real RMT/ADC/board code, startup/retry, transaction spacing, paired faults, calibration gating");
}
