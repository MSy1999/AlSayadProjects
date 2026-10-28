/**
 * @file AsWifiSdk.h
 * @brief AlSayad AsWifiSdk module.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_TEST_WIFI_SDK_H
#define AS_TEST_WIFI_SDK_H
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED 0x501
#define ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED 0x502
#define WIFI_AUTH_WPA2_PSK 3
#define WIFI_MODE_AP 2
#define WIFI_IF_AP 1
#define WIFI_STORAGE_RAM 1
#define HTTP_POST 1
#define HTTPD_408_REQ_TIMEOUT 408
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_SOCK_ERR_TIMEOUT -1
typedef struct { uint32_t addr; } AsTestIp;
typedef struct { AsTestIp ip, gw, netmask; } esp_netif_ip_info_t;
typedef struct { int unused; } esp_netif_t;
#define IP4_ADDR(ip,a,b,c,d) ((ip)->addr=((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(d))
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct { struct { uint8_t ssid[32], password[64], ssid_len, channel, max_connection; int authmode; } ap; } wifi_config_t;
typedef struct { int num; } wifi_sta_list_t;
typedef void* httpd_handle_t;
typedef struct { size_t content_len; void* user_ctx; } httpd_req_t;
typedef struct { const char* uri; int method; esp_err_t (*handler)(httpd_req_t*); void* user_ctx; } httpd_uri_t;
typedef struct { size_t stack_size; unsigned recv_wait_timeout; bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_loop_delete_default(void);
esp_netif_t* esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy_default_wifi(esp_netif_t* pst_Netif);
esp_err_t esp_netif_dhcps_stop(esp_netif_t* pst_Netif);
esp_err_t esp_netif_dhcps_start(esp_netif_t* pst_Netif);
esp_err_t esp_netif_set_ip_info(esp_netif_t* pst_Netif, const esp_netif_ip_info_t* pst_Ip);
esp_err_t esp_wifi_init(const wifi_init_config_t* pst_Config);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_set_storage(int mode);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_set_config(int mode, const wifi_config_t* pst_Config);
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t* pst_Output);
esp_err_t httpd_start(httpd_handle_t* server, const httpd_config_t* pst_Config);
esp_err_t httpd_stop(httpd_handle_t server);
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t* pst_Uri);
int httpd_req_recv(httpd_req_t* pst_Request, char* pcOutput, size_t length);
esp_err_t httpd_resp_set_status(httpd_req_t* pst_Request, const char* pcStatus);
esp_err_t httpd_resp_set_type(httpd_req_t* pst_Request, const char* pcType);
esp_err_t httpd_resp_sendstr(httpd_req_t* pst_Request, const char* pcBody);
esp_err_t httpd_resp_send_err(httpd_req_t* pst_Request, int status, const char* pcBody);
int64_t esp_timer_get_time(void);
#endif
