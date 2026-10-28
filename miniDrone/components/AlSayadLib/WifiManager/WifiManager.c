#include "WifiManager.h"
#include <string.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_check.h"
esp_err_t as_wifi_start(const char *ssid,const char *password,unsigned channel) {
    if (!ssid || !password || strlen(ssid)>32 || strlen(password)<8 || strlen(password)>63 || channel<1 || channel>11) return ESP_ERR_INVALID_ARG;
    /* Never erase NVS automatically: credentials/settings must survive errors. */
    ESP_RETURN_ON_ERROR(nvs_flash_init(),"wifi","NVS");
    ESP_RETURN_ON_ERROR(esp_netif_init(),"wifi","netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(),"wifi","events");
    if (!esp_netif_create_default_wifi_ap() || !esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init),"wifi","init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM),"wifi","storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA),"wifi","mode");
    wifi_config_t c={0}; memcpy(c.ap.ssid,ssid,strlen(ssid)); memcpy(c.ap.password,password,strlen(password));
    c.ap.ssid_len=strlen(ssid); c.ap.channel=channel; c.ap.max_connection=1; c.ap.authmode=WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP,&c),"wifi","config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(),"wifi","start");
    return esp_wifi_set_ps(WIFI_PS_NONE);
}
