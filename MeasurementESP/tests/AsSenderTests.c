/**
 * @file AsSenderTests.c
 * @brief Host tests of real sender services with injected time, sensors and allocation faults.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsMeasurementSensors.h"
#include "AsMeasurementSender.h"
#include "AsMeasurementJson.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float f32Value;
    esp_err_t error;
    esp_err_t init_error;
    uint32_t u32Initializations;
} AsFakeSensor;
static esp_err_t as_test_init(void* context, uint32_t u32TimeoutMs) {
    AsFakeSensor* pst_Sensor = context;
    assert(u32TimeoutMs == 100);
    ++pst_Sensor->u32Initializations;
    return pst_Sensor->init_error;
}
static esp_err_t as_test_read(void* context, uint32_t u32TimeoutMs, float* pf32Value) {
    AsFakeSensor* pst_Sensor = context;
    assert(u32TimeoutMs == 100);
    *pf32Value = pst_Sensor->f32Value;
    return pst_Sensor->error;
}
static void as_test_roundtrip(const AsMeasurement* pst_Frame) {
    char acFrame[AS_MEASUREMENT_FRAME_BYTES + 1];
    size_t length = 0;
    assert(as_measurement_encode(pst_Frame, acFrame, sizeof(acFrame), &length) == ESP_OK);
    assert(length == strlen(acFrame) && length <= AS_MEASUREMENT_FRAME_BYTES);
    AsMeasurement st_Decoded;
    assert(as_measurement_decode(acFrame, length, pst_Frame->acDeviceId, &st_Decoded) == ESP_OK);
    assert(strcmp(st_Decoded.acDeviceId, pst_Frame->acDeviceId) == 0);
    assert(st_Decoded.bSimulated == pst_Frame->bSimulated);
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        assert(st_Decoded.ast_Readings[index].eStatus == pst_Frame->ast_Readings[index].eStatus);
        if (pst_Frame->ast_Readings[index].eStatus == AS_READING_VALID) {
            assert(st_Decoded.ast_Readings[index].f32Value == pst_Frame->ast_Readings[index].f32Value);
        }
    }
}
static void as_test_sensors(void) {
    AsMeasurementSensor ast_Sensors[AS_METRIC_COUNT] = {0};
    AsMeasurement st_Frame;
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 0, &st_Frame) == ESP_OK);
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        assert(st_Frame.ast_Readings[index].eStatus == AS_READING_UNAVAILABLE);
    }
    as_test_roundtrip(&st_Frame);
    assert(as_measurement_sensors_sample(ast_Sensors, "test", true, 0, &st_Frame) == ESP_OK);
    assert(st_Frame.bSimulated && st_Frame.ast_Readings[2].f32Value == 0);
    as_test_roundtrip(&st_Frame);
    AsFakeSensor ast_Fake[AS_METRIC_COUNT] = {{.f32Value = 23.4f},
        {.f32Value = 55}, {.f32Value = 0}};
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        ast_Sensors[index] = (AsMeasurementSensor){.context = &ast_Fake[index],
            .init = as_test_init, .read = as_test_read, .bEnabled = true, .u32TimeoutMs = 100};
    }
    assert(as_measurement_sensors_sample(ast_Sensors, "a\"\\\n", false, 0, &st_Frame) == ESP_OK);
    as_test_roundtrip(&st_Frame);
    for (size_t failed = 0; failed < AS_METRIC_COUNT; ++failed) {
        ast_Fake[failed].error = ESP_ERR_TIMEOUT;
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 20000 * (failed + 1), &st_Frame) == ESP_OK);
        for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
            assert(st_Frame.ast_Readings[index].eStatus ==
                (index == failed ? AS_READING_ERROR : AS_READING_VALID));
        }
        assert(st_Frame.ast_Readings[failed].f32Value == 0);
        as_test_roundtrip(&st_Frame);
        ast_Fake[failed].error = ESP_OK;
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false,
                                             20000 * (failed + 1) + 2000, &st_Frame) == ESP_OK);
        assert(st_Frame.ast_Readings[failed].eStatus == AS_READING_VALID);
    }
    const float af32Bad[] = {NAN, INFINITY, -INFINITY, 126, -41};
    for (size_t index = 0; index < sizeof(af32Bad) / sizeof(af32Bad[0]); ++index) {
        ast_Fake[0].f32Value = af32Bad[index];
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 100000, &st_Frame) == ESP_OK);
        assert(st_Frame.ast_Readings[0].eStatus == AS_READING_ERROR);
        as_test_roundtrip(&st_Frame);
    }
    for (size_t metric = 0; metric < AS_METRIC_COUNT; ++metric) {
        ast_Fake[metric].f32Value = metric == 0 ? -40 : 0;
    }
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 100000, &st_Frame) == ESP_OK);
    as_test_roundtrip(&st_Frame);
    for (size_t metric = 0; metric < AS_METRIC_COUNT; ++metric) {
        ast_Fake[metric].f32Value = metric == 0 ? 125 : 100;
    }
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 100000, &st_Frame) == ESP_OK);
    as_test_roundtrip(&st_Frame);
    for (size_t metric = 1; metric < AS_METRIC_COUNT; ++metric) {
        ast_Fake[metric].f32Value = -1;
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 100000, &st_Frame) == ESP_OK);
        assert(st_Frame.ast_Readings[metric].eStatus == AS_READING_ERROR);
        ast_Fake[metric].f32Value = 101;
        assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 100000, &st_Frame) == ESP_OK);
        assert(st_Frame.ast_Readings[metric].eStatus == AS_READING_ERROR);
        ast_Fake[metric].f32Value = 50;
    }
    ast_Sensors[0].bReady = false;
    ast_Fake[0].init_error = ESP_FAIL;
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 200000, &st_Frame) == ESP_OK);
    uint32_t u32Calls = ast_Fake[0].u32Initializations;
    assert(st_Frame.ast_Readings[0].eStatus == AS_READING_ERROR);
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 202000, &st_Frame) == ESP_OK);
    assert(ast_Fake[0].u32Initializations == u32Calls);
    ast_Fake[0].init_error = ESP_OK;
    assert(as_measurement_sensors_sample(ast_Sensors, "test", false, 210000, &st_Frame) == ESP_OK);
    assert(st_Frame.ast_Readings[0].eStatus == AS_READING_VALID);
    float f32Percent = 0;
    assert(as_measurement_light_convert(25, 0, 100, &f32Percent) == ESP_OK && f32Percent == 25);
    assert(as_measurement_light_convert(25, 100, 0, &f32Percent) == ESP_OK && f32Percent == 75);
    assert(as_measurement_light_convert(-25, 0, 100, &f32Percent) == ESP_OK && f32Percent == 0);
    assert(as_measurement_light_convert(125, 0, 100, &f32Percent) == ESP_OK && f32Percent == 100);
    assert(as_measurement_light_convert(0, 1, 1, &f32Percent) != ESP_OK);
    assert(as_measurement_light_convert(NAN, 0, 100, &f32Percent) != ESP_OK);
}
static size_t allocation_count;
static size_t failure_at;
static size_t outstanding;
static void* as_test_allocate(size_t size) {
    if (allocation_count++ == failure_at) {
        return NULL;
    }
    void* memory = malloc(size);
    if (memory != NULL) {
        ++outstanding;
    }
    return memory;
}
static void as_test_free(void* memory) {
    if (memory != NULL) {
        assert(outstanding > 0);
        --outstanding;
        free(memory);
    }
}
static void as_test_encoder_faults(void) {
    AsMeasurementSensor ast_Sensors[AS_METRIC_COUNT] = {0};
    AsMeasurement st_Frame;
    assert(as_measurement_sensors_sample(ast_Sensors, "test", true, 0, &st_Frame) == ESP_OK);
    char acFrame[AS_MEASUREMENT_FRAME_BYTES + 1];
    size_t length = 123;
    assert(as_measurement_encode(&st_Frame, acFrame, 8, &length) != ESP_OK && length == 0);
    st_Frame.ast_Readings[0].eStatus = AS_READING_STALE;
    assert(as_measurement_encode(&st_Frame, acFrame, sizeof(acFrame), &length) != ESP_OK);
    st_Frame.ast_Readings[0].eStatus = AS_READING_VALID;
    st_Frame.ast_Readings[0].f32Value = NAN;
    assert(as_measurement_encode(&st_Frame, acFrame, sizeof(acFrame), &length) != ESP_OK);
    st_Frame.ast_Readings[0].f32Value = 0;
    cJSON_Hooks st_Hooks = {.malloc_fn = as_test_allocate, .free_fn = as_test_free};
    cJSON_InitHooks(&st_Hooks);
    bool bSuccess = false;
    for (failure_at = 0; failure_at < 200; ++failure_at) {
        allocation_count = 0;
        esp_err_t error = as_measurement_encode(&st_Frame, acFrame, sizeof(acFrame), &length);
        assert(outstanding == 0);
        if (error == ESP_OK) {
            bSuccess = true;
            break;
        }
        assert(length == 0);
    }
    assert(bSuccess);
    cJSON_InitHooks(NULL);
    /* Maximum escaped device ID still fits protocol size; larger identities are rejected. */
    memset(st_Frame.acDeviceId, '\1', AS_DEVICE_ID_BYTES - 1);
    st_Frame.acDeviceId[AS_DEVICE_ID_BYTES - 1] = 0;
    as_test_roundtrip(&st_Frame);
    memset(st_Frame.acDeviceId, 'x', AS_DEVICE_ID_BYTES);
    assert(as_measurement_encode(&st_Frame, acFrame, sizeof(acFrame), &length) != ESP_OK);
}
static void as_test_policy(void) {
    assert(as_measurement_sender_config_valid("AlSayad-Room", "AlSayadRoom", "test",
        "http://192.168.4.1/api/v1/measurements", 2000, 3000));
    assert(!as_measurement_sender_config_valid("", "short", "test", "http://x", 2000, 3000));
    assert(!as_measurement_sender_config_valid("x", "12345678", "test", "https://x", 2000, 3000));
    assert(!as_measurement_sender_config_valid("x", "12345678", "test", "http://x", 0, 3000));
    AsMeasurementSender st_State = {.u32PeriodMs = 2000};
    assert(!as_measurement_sender_ready(&st_State, false, 10000, 10000));
    assert(!as_measurement_sender_ready(&st_State, true, 10000, 5999));
    assert(!as_measurement_sender_ready(&st_State, true, 10000, 10001));
    assert(as_measurement_sender_ready(&st_State, true, 10000, 10000));
    as_measurement_sender_attempt(&st_State, 10000);
    as_measurement_sender_result(&st_State, 10100, 202);
    assert(st_State.u64DueMs == 12000);
    assert(!as_measurement_sender_ready(&st_State, true, 12000, 10000));
    assert(as_measurement_sender_ready(&st_State, true, 12000, 12000));
    as_measurement_sender_attempt(&st_State, 12000);
    as_measurement_sender_result(&st_State, 19100, 202);
    assert(st_State.u64DueMs == 20000); /* slow request skips missed periods */
    const int32_t ai32Status[] = {400, 408, 413, 503, 0, 500, 200, 302};
    for (size_t index = 0; index < sizeof(ai32Status) / sizeof(ai32Status[0]); ++index) {
        st_State.u32Failures = 0;
        as_measurement_sender_result(&st_State, 30000, ai32Status[index]);
        assert(st_State.u64DueMs >= 32000 && st_State.u64DueMs <= 40000);
        if (ai32Status[index] == 400 || ai32Status[index] == 413) {
            assert(st_State.u64DueMs == 40000);
        }
    }
    for (uint32_t u32Failure = 0; u32Failure < 1000; ++u32Failure) {
        as_measurement_sender_result(&st_State, 100000, 0);
        assert(st_State.u64DueMs <= 110000 && st_State.u64DueMs >= 102000);
    }
    assert(as_measurement_sender_backoff(0) == 1000);
    assert(as_measurement_sender_backoff(1) == 2000);
    assert(as_measurement_sender_backoff(2) == 4000);
    assert(as_measurement_sender_backoff(3) == 8000);
    assert(as_measurement_sender_backoff(UINT32_MAX) == 10000);
    assert(as_measurement_sender_next_period(2000, 9900, 2000) == 10000);
    assert(as_measurement_sender_next_period(2000, 2000, 2000) == 4000);
    /* Model a single-slot handoff through a minute-long outage: only newest is eligible. */
    uint64_t u64LatestMs = 0;
    st_State = (AsMeasurementSender){.u32PeriodMs = 2000};
    for (uint64_t u64NowMs = 0; u64NowMs <= 60000; u64NowMs += 2000) {
        u64LatestMs = u64NowMs;
        assert(!as_measurement_sender_ready(&st_State, false, u64NowMs, u64LatestMs));
    }
    assert(as_measurement_sender_ready(&st_State, true, 60000, u64LatestMs));
    assert(!as_measurement_sender_ready(&st_State, true, 65000, u64LatestMs));
}
void as_test_hardware(void);
void as_test_dht11(void);
void as_test_http(void);
int main(void) {
    as_test_http();
    as_test_dht11();
    as_test_hardware();
    as_test_sensors();
    as_test_encoder_faults();
    as_test_policy();
    puts("PASS: sender sensor, encoder allocation/round-trip, calibration and scheduling checks");
    return 0;
}
