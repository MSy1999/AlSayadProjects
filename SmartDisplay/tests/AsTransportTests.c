/**
 * @file AsTransportTests.c
 * @brief Actual Wi-Fi/HTTP adapter tests with SDK boundary mocks, no radio claims.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsWifiTransport.h"
#include "AsMeasurementJson.h"
#include "AsWifiSdk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static esp_netif_t st_Netif;
static httpd_uri_t st_Uri;
static const char* pcBody;
static size_t offset;
static size_t body_length;
static int response_status;
static char acResponse[128];
static bool bBusy;
static bool bBorrowLoop;
static bool bRegisterFail;
static bool bSlow;
static unsigned loop_deletes;
static unsigned wifi_deletes;
static unsigned server_stops;
static int64_t i64NowUs;
static AsMeasurement st_Accepted;
esp_err_t esp_netif_init(void) { return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { return bBorrowLoop ? ESP_ERR_INVALID_STATE : ESP_OK; }
esp_err_t esp_event_loop_delete_default(void) { ++loop_deletes; return ESP_OK; }
esp_netif_t* esp_netif_create_default_wifi_ap(void) { return &st_Netif; }
void esp_netif_destroy_default_wifi(esp_netif_t* pst_Netif) { assert(pst_Netif == &st_Netif); }
esp_err_t esp_netif_dhcps_stop(esp_netif_t* pst_Netif) {
    (void)pst_Netif;
    return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED;
}
esp_err_t esp_netif_dhcps_start(esp_netif_t* pst_Netif) {
    (void)pst_Netif;
    return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
}
esp_err_t esp_netif_set_ip_info(esp_netif_t* pst_Netif, const esp_netif_ip_info_t* pst_Ip) {
    (void)pst_Netif;
    assert(pst_Ip->ip.addr == UINT32_C(0xc0a80401));
    return ESP_OK;
}
esp_err_t esp_wifi_init(const wifi_init_config_t* pst_Config) { (void)pst_Config; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { ++wifi_deletes; return ESP_OK; }
esp_err_t esp_wifi_stop(void) { return ESP_OK; }
esp_err_t esp_wifi_start(void) { return ESP_OK; }
esp_err_t esp_wifi_set_storage(int mode) { assert(mode == WIFI_STORAGE_RAM); return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode == WIFI_MODE_AP); return ESP_OK; }
esp_err_t esp_wifi_set_config(int mode, const wifi_config_t* pst_Config) {
    assert(mode == WIFI_IF_AP && pst_Config->ap.ssid_len == 12);
    assert(pst_Config->ap.authmode == WIFI_AUTH_WPA2_PSK);
    return ESP_OK;
}
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t* pst_Output) { pst_Output->num = 1; return ESP_OK; }
esp_err_t httpd_start(httpd_handle_t* server, const httpd_config_t* pst_Config) {
    assert(pst_Config->stack_size >= 6144 && pst_Config->recv_wait_timeout == 2);
    *server = &st_Uri;
    return ESP_OK;
}
esp_err_t httpd_stop(httpd_handle_t server) { (void)server; ++server_stops; return ESP_OK; }
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t* pst_Config) {
    (void)server;
    assert(strcmp(pst_Config->uri, "/api/v1/measurements") == 0);
    st_Uri = *pst_Config;
    return bRegisterFail ? ESP_FAIL : ESP_OK;
}
int httpd_req_recv(httpd_req_t* pst_Request, char* pcOutput, size_t length) {
    (void)pst_Request;
    if (offset >= body_length) {
        return 0;
    }
    size_t count = body_length - offset;
    if (count > length) { count = length; }
    if (count > 7) { count = 7; }
    memcpy(pcOutput, pcBody + offset, count);
    offset += count;
    i64NowUs += bSlow ? 3000000 : 100;
    return (int)count;
}
esp_err_t httpd_resp_set_status(httpd_req_t* pst_Request, const char* pcStatus) {
    (void)pst_Request;
    assert(sscanf(pcStatus, "%d", &response_status) == 1);
    return ESP_OK;
}
esp_err_t httpd_resp_set_type(httpd_req_t* pst_Request, const char* pcType) {
    (void)pst_Request; assert(strcmp(pcType, "application/json") == 0); return ESP_OK;
}
esp_err_t httpd_resp_sendstr(httpd_req_t* pst_Request, const char* pcResponse) {
    (void)pst_Request;
    snprintf(acResponse, sizeof(acResponse), "%s", pcResponse);
    return ESP_OK;
}
esp_err_t httpd_resp_send_err(httpd_req_t* pst_Request, int status, const char* pcResponse) {
    response_status = status;
    return httpd_resp_sendstr(pst_Request, pcResponse);
}
int64_t esp_timer_get_time(void) { return i64NowUs; }
static esp_err_t as_test_receive(const char* pcFrame, size_t length, void* pContext) {
    assert(pContext == &st_Accepted);
    esp_err_t error = as_measurement_decode(pcFrame, length, "measurement-01", &st_Accepted);
    return error != ESP_OK ? error : (bBusy ? ESP_ERR_TIMEOUT : ESP_OK);
}
static int as_test_request(const char* pcInput, size_t length) {
    pcBody = pcInput;
    body_length = pcInput != NULL ? strlen(pcInput) : 0;
    offset = 0;
    response_status = 0;
    acResponse[0] = 0;
    httpd_req_t st_Request = {.content_len = length, .user_ctx = st_Uri.user_ctx};
    assert(st_Uri.handler(&st_Request) == ESP_OK);
    return response_status;
}
void as_test_transport(const char* pcValidFrame) {
    AsWifiTransportConfig st_Config = {.pcSsid = "AlSayad-Room", .pcPassword = "short", .u8Channel = 1};
    AsWifiTransport* pst_Wifi = NULL;
    assert(as_wifi_transport_create(&st_Config, &pst_Wifi) == ESP_ERR_INVALID_ARG && pst_Wifi == NULL);
    st_Config.pcPassword = "AlSayadRoom";
    assert(as_wifi_transport_create(&st_Config, &pst_Wifi) == ESP_OK);
    AsTransport st_Transport = as_wifi_transport_interface(pst_Wifi);
    assert(st_Transport.start(st_Transport.pContext, as_test_receive, &st_Accepted) == ESP_OK);
    assert(st_Transport.connected(st_Transport.pContext));
    assert(as_test_request(pcValidFrame, strlen(pcValidFrame)) == 202);
    assert(strcmp(acResponse, "{\"accepted\":true}") == 0);
    assert(st_Accepted.ast_Readings[0].eStatus == AS_READING_VALID);
    bBusy = true;
    assert(as_test_request(pcValidFrame, strlen(pcValidFrame)) == 503);
    bBusy = false;
    assert(as_test_request("{}", 2) == 400);
    assert(as_test_request(NULL, 1025) == 413);
    assert(as_test_request("", 0) == 413);
    assert(as_test_request("", 20) == 400);
    bSlow = true;
    assert(as_test_request(pcValidFrame, strlen(pcValidFrame)) == 408);
    bSlow = false;
    assert(as_wifi_transport_destroy(pst_Wifi) == ESP_OK);
    assert(loop_deletes == 1 && wifi_deletes == 1 && server_stops == 1);
    bBorrowLoop = true;
    bRegisterFail = true;
    assert(as_wifi_transport_create(&st_Config, &pst_Wifi) == ESP_OK);
    st_Transport = as_wifi_transport_interface(pst_Wifi);
    assert(st_Transport.start(st_Transport.pContext, as_test_receive, &st_Accepted) == ESP_FAIL);
    assert(as_wifi_transport_destroy(pst_Wifi) == ESP_OK);
    assert(loop_deletes == 1 && wifi_deletes == 2 && server_stops == 2);
    puts("PASS: AP lifecycle/cleanup, borrowed loop, partial HTTP reads, 202/400/408/413/503 mappings");
}
