#include "as_protocol.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static float axis(int raw){float v=(raw-2048)/1800.f;if(fabsf(v)<.06f)return 0;return fminf(1,fmaxf(-1,v));}
void app_main(void){
 uint8_t mac[6],key[16];unsigned parsed[6];ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
 if(sscanf(CONFIG_AS_DRONE_MAC,"%x:%x:%x:%x:%x:%x",parsed,parsed+1,parsed+2,parsed+3,parsed+4,parsed+5)!=6)return;
 bool valid=false;for(int i=0;i<6;i++){if(parsed[i]>255)return;mac[i]=parsed[i];valid|=mac[i]!=0;}if(!valid||(mac[0]&1)||strlen(CONFIG_AS_PAIR_KEY)!=32)return;
 valid=false;for(int i=0;i<16;i++){unsigned k;if(sscanf(CONFIG_AS_PAIR_KEY+2*i,"%2x",&k)!=1)return;key[i]=k;valid|=k!=0;}if(!valid)return;
 esp_netif_create_default_wifi_sta();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE));ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
 ESP_ERROR_CHECK(esp_now_init());ESP_ERROR_CHECK(esp_now_set_pmk(key));esp_now_peer_info_t peer={.channel=6,.ifidx=WIFI_IF_STA,.encrypt=true};memcpy(peer.peer_addr,mac,6);memcpy(peer.lmk,key,16);ESP_ERROR_CHECK(esp_now_add_peer(&peer));
 /* C3 remote: ADC joystick GPIO0,1,3,4; active-low buttons GPIO5 arm, GPIO6 disarm, GPIO7 STOP. */
 gpio_config_t buttons={.pin_bit_mask=(1ULL<<5)|(1ULL<<6)|(1ULL<<7),.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};ESP_ERROR_CHECK(gpio_config(&buttons));
 adc_oneshot_unit_handle_t adc;adc_oneshot_unit_init_cfg_t ac={.unit_id=ADC_UNIT_1};ESP_ERROR_CHECK(adc_oneshot_new_unit(&ac,&adc));int channels[]={0,1,3,4};adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};for(int i=0;i<4;i++)ESP_ERROR_CHECK(adc_oneshot_config_channel(adc,channels[i],&cc));
 as_command_t cmd={.session=esp_random()|1};bool was_arm=false,was_disarm=false;uint8_t packet[AS_PACKET_SIZE];
 for(;;){int raw[4];bool ok=true;for(int i=0;i<4;i++)if(adc_oneshot_read(adc,channels[i],raw+i)!=ESP_OK)ok=false;
 bool arm=!gpio_get_level(5),disarm=!gpio_get_level(6),stop=!gpio_get_level(7);
 cmd.kind=AS_INPUT;cmd.throttle=ok?fminf(1,fmaxf(0,(raw[0]-100)/3800.f)):0;cmd.yaw=ok?axis(raw[1]):0;cmd.roll=ok?axis(raw[2]):0;cmd.pitch=ok?axis(raw[3]):0;
 if(stop||!ok)cmd.kind=AS_STOP;else if(disarm&&!was_disarm)cmd.kind=AS_DISARM;else if(arm&&!was_arm&&cmd.throttle<=.02f)cmd.kind=AS_ARM;
 was_arm=arm;was_disarm=disarm;cmd.sequence++;as_protocol_encode(&cmd,packet);esp_now_send(mac,packet,sizeof(packet));vTaskDelay(pdMS_TO_TICKS(20));}
}
