/**
 * @file AsDht11Tests.c
 * @brief Test real DHT11 pulse/checksum/range decoder using captured-level fixtures.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsDht11Decode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void as_test_dht_fixture(const uint8_t* pu8Bytes, AsDht11Pulse* pst_Pulses) {
    pst_Pulses[0] = (AsDht11Pulse){30, 1};
    pst_Pulses[1] = (AsDht11Pulse){80, 0};
    pst_Pulses[2] = (AsDht11Pulse){80, 1};
    for (size_t bit = 0; bit < 40; ++bit) {
        pst_Pulses[3 + 2 * bit] = (AsDht11Pulse){50, 0};
        pst_Pulses[4 + 2 * bit] = (AsDht11Pulse){
            (pu8Bytes[bit / 8] & (1U << (7 - bit % 8))) ? 70 : 27, 1};
    }
}
static void as_test_dht_reason(const AsDht11Pulse* pst_Pulses, size_t count,
                               const char* pcExpectedReason) {
    AsDht11DecodeInfo st_Info;
    float f32Temperature = -1;
    float f32Humidity = -1;
    esp_err_t error = as_dht11_decode_diagnose(pst_Pulses, count,
        &f32Temperature, &f32Humidity, &st_Info);
    assert(strcmp(st_Info.pcReason, pcExpectedReason) == 0);
    if (strcmp(pcExpectedReason, "ok") != 0) {
        assert(error != ESP_OK && f32Temperature == -1 && f32Humidity == -1);
    }
}
void as_test_dht11(void) {
    uint8_t au8Bytes[5] = {55, 0, 23, 0, 78};
    AsDht11Pulse ast_Pulses[83];
    as_test_dht_fixture(au8Bytes, ast_Pulses);
    float f32Temperature = -1;
    float f32Humidity = -1;
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 23 && f32Humidity == 55);
    as_test_dht_reason(ast_Pulses, 83, "ok");
    as_test_dht_reason(ast_Pulses, 82, "response_preamble");
    as_test_dht_reason(ast_Pulses, 2, "incomplete_response");
    as_test_dht_reason(NULL, 0, "arguments");
    assert(as_dht11_decode(ast_Pulses + 1, 82, &f32Temperature, &f32Humidity) == ESP_OK);
    ast_Pulses[1].u16DurationUs = 30; /* sensor reply was already LOW when observed */
    assert(as_dht11_decode(ast_Pulses + 1, 82, &f32Temperature, &f32Humidity) == ESP_OK);
    ast_Pulses[1].u16DurationUs = 80;
    assert(as_dht11_decode(ast_Pulses, 82, &f32Temperature, &f32Humidity) != ESP_OK);
    au8Bytes[4] = 77;
    as_test_dht_fixture(au8Bytes, ast_Pulses);
    f32Temperature = -1;
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) != ESP_OK);
    assert(f32Temperature == -1);
    as_test_dht_reason(ast_Pulses, 83, "checksum");
    au8Bytes[4] = 78;
    as_test_dht_fixture(au8Bytes, ast_Pulses);
    ast_Pulses[1].u16DurationUs = 10;
    as_test_dht_reason(ast_Pulses, 83, "response_preamble");
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) != ESP_OK);
    as_test_dht_fixture(au8Bytes, ast_Pulses);
    ast_Pulses[4].u16DurationUs = 48; /* ambiguous bit width */
    as_test_dht_reason(ast_Pulses, 83, "bit_timing");
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) != ESP_OK);
    const uint8_t au8Minimum[] = {20, 0, 0, 0, 20};
    as_test_dht_fixture(au8Minimum, ast_Pulses);
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 0 && f32Humidity == 20);
    const uint8_t au8Maximum[] = {90, 0, 50, 0, 140};
    as_test_dht_fixture(au8Maximum, ast_Pulses);
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) == ESP_OK);
    const uint8_t au8BadRange[] = {91, 0, 51, 0, 142};
    as_test_dht_fixture(au8BadRange, ast_Pulses);
    as_test_dht_reason(ast_Pulses, 83, "sensor_range");
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) != ESP_OK);
    const uint8_t au8Decimal[] = {55, 5, 23, 4, 87};
    as_test_dht_fixture(au8Decimal, ast_Pulses);
    assert(as_dht11_decode(ast_Pulses, 83, &f32Temperature, &f32Humidity) == ESP_OK);
    assert(f32Temperature == 23.4f && f32Humidity == 55.5f);
    puts("PASS: DHT11 pulse framing, checksum, timing, decimals and selected module ranges");
}
