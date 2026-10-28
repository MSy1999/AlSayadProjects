/**
 * @file AsHttpTests.c
 * @brief Exercise actual HTTP adapter resource paths through injected SDK calls.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#include "AsMeasurementHttp.h"
#include "esp_http_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct AsFakeClient { int unused; };
static struct AsFakeClient st_Client;
static int active;
static int failure;
static int status;
static int calls;
static size_t expected_length;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* pst_Config) {
    ++calls;
    assert(active == 0);
    assert(pst_Config->method == HTTP_METHOD_POST && pst_Config->timeout_ms == 3000);
    assert(pst_Config->disable_auto_redirect && pst_Config->buffer_size == 512);
    if (failure == 1) {
        return NULL;
    }
    ++active;
    return &st_Client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char* key,
                                     const char* value) {
    assert(client == &st_Client && active == 1);
    assert(strcmp(key, "Content-Type") == 0 && strcmp(value, "application/json") == 0);
    return failure == 2 ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int length) {
    assert(client == &st_Client && (size_t)length == expected_length);
    return failure == 3 ? ESP_ERR_TIMEOUT : ESP_OK;
}
int esp_http_client_write(esp_http_client_handle_t client, const char* data, int length) {
    assert(client == &st_Client && (size_t)length == strlen(data));
    return failure == 4 ? length - 1 : length;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    assert(client == &st_Client);
    return failure == 5 ? -1 : 17;
}
int esp_http_client_get_status_code(esp_http_client_handle_t client) {
    assert(client == &st_Client);
    return status;
}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
    assert(client == &st_Client && active == 1);
    --active;
    return failure == 6 ? ESP_FAIL : ESP_OK;
}
void as_test_http(void) {
    const char* pcFrame = "{\"test\":true}";
    expected_length = strlen(pcFrame);
    int32_t i32Status = 0;
    const int aiStatus[] = {202, 400, 408, 413, 503, 500, 302};
    for (size_t index = 0; index < sizeof(aiStatus) / sizeof(aiStatus[0]); ++index) {
        status = aiStatus[index];
        for (failure = 0; failure <= 6; ++failure) {
            for (size_t repeat = 0; repeat < 100; ++repeat) {
                esp_err_t error = as_measurement_http_post("http://192.168.4.1/api/v1/measurements",
                    3000, pcFrame, expected_length, &i32Status);
                assert(active == 0);
                assert((error == ESP_OK) == (failure == 0));
                if (error == ESP_OK) {
                    assert(i32Status == status);
                }
            }
        }
    }
    calls = 0;
    assert(as_measurement_http_post("http://x", 3000, pcFrame, 1025, &i32Status) != ESP_OK);
    assert(as_measurement_http_post("http://x", 3000, pcFrame, 0, &i32Status) != ESP_OK);
    assert(calls == 0 && active == 0);
    puts("PASS: HTTP statuses, fixed body length, partial writes, 4900 resource/failure cycles");
}
