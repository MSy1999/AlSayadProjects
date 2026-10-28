/** @file esp_http_client.h @brief Injected HTTP SDK. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_ESP_HTTP_CLIENT_H
#define AS_TEST_ESP_HTTP_CLIENT_H
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct AsFakeClient* esp_http_client_handle_t;
enum {HTTP_METHOD_POST};
typedef struct {
    const char* url;
    int method;
    int timeout_ms;
    bool disable_auto_redirect;
    int buffer_size;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* pst_Config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char* key, const char* value);
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int length);
int esp_http_client_write(esp_http_client_handle_t client, const char* data, int length);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
#endif
