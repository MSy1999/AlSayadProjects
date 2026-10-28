#include "inputs.h"
#include "as_protocol.h"
#include "sdkconfig.h"
#include "esp_now.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>
static QueueHandle_t input_queue,event_queue;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static as_state_t state;static float voltage,roll,pitch;
void as_telemetry_publish(as_state_t s,float v,float r,float p){portENTER_CRITICAL(&lock);state=s;voltage=v;roll=r;pitch=p;portEXIT_CRITICAL(&lock);}
static bool ingest(const uint8_t*p,size_t n,uint32_t source){
 as_event_t e={.source=source,.received_us=esp_timer_get_time()};if(!as_protocol_decode(p,n,&e.command))return false;
 if(e.command.kind==AS_INPUT)return xQueueOverwrite(input_queue,&e)==pdTRUE;
 /* A dedicated STOP slot cannot be displaced by ordinary events. */
 if(e.command.kind==AS_STOP){xQueueReset(event_queue);return xQueueSendToFront(event_queue,&e,0)==pdTRUE;}
 return xQueueSend(event_queue,&e,0)==pdTRUE;
}
#ifdef CONFIG_AS_ESPNOW
static uint8_t peer_mac[6];
static void receive(const esp_now_recv_info_t*info,const uint8_t*data,int len){if(info&&memcmp(info->src_addr,peer_mac,6)==0&&len==AS_PACKET_SIZE)ingest(data,len,1);}
#endif
#ifndef CONFIG_AS_ESPNOW
static esp_err_t page(httpd_req_t*r){extern const char html_start[] asm("_binary_controller_html_start");httpd_resp_set_type(r,"text/html");return httpd_resp_send(r,html_start,HTTPD_RESP_USE_STRLEN);}
static esp_err_t status(httpd_req_t*r){char b[160];portENTER_CRITICAL(&lock);as_state_t s=state;float v=voltage,rr=roll,pp=pitch;portEXIT_CRITICAL(&lock);snprintf(b,sizeof(b),"{\"state\":%d,\"battery\":%.2f,\"roll\":%.3f,\"pitch\":%.3f}",s,(double)v,(double)rr,(double)pp);httpd_resp_set_type(r,"application/json");return httpd_resp_send(r,b,HTTPD_RESP_USE_STRLEN);}
static esp_err_t websocket(httpd_req_t*r){
 if(r->method==HTTP_GET)return ESP_OK;
 uint8_t data[AS_PACKET_SIZE];httpd_ws_frame_t frame={0};esp_err_t e=httpd_ws_recv_frame(r,&frame,0);if(e!=ESP_OK)return e;
 if(frame.type!=HTTPD_WS_TYPE_BINARY||frame.len!=AS_PACKET_SIZE)return ESP_FAIL;
 frame.payload=data;if((e=httpd_ws_recv_frame(r,&frame,sizeof(data)))!=ESP_OK)return e;
 /* Phone mode supports one AP client. Session arbitration lives in flight core. */
 bool accepted=ingest(data,sizeof(data),2);uint8_t ack=accepted;httpd_ws_frame_t reply={.type=HTTPD_WS_TYPE_BINARY,.payload=&ack,.len=1};return httpd_ws_send_frame(r,&reply);
}
#endif
esp_err_t as_inputs_start(QueueHandle_t input,QueueHandle_t events){input_queue=input;event_queue=events;
#ifdef CONFIG_AS_ESPNOW
 unsigned m[6];if(sscanf(CONFIG_AS_REMOTE_MAC,"%x:%x:%x:%x:%x:%x",m,m+1,m+2,m+3,m+4,m+5)!=6)return ESP_ERR_INVALID_ARG;
 bool nonzero=false;for(int i=0;i<6;i++){if(m[i]>255)return ESP_ERR_INVALID_ARG;peer_mac[i]=m[i];nonzero|=m[i]!=0;}if(!nonzero||(peer_mac[0]&1))return ESP_ERR_INVALID_ARG;
 esp_err_t e=esp_now_init();if(e!=ESP_OK)return e;
 uint8_t key[16]; bool key_set=false;
 if(strlen(CONFIG_AS_ESPNOW_KEY)!=32)return ESP_ERR_INVALID_ARG;
 for(int i=0;i<16;i++){unsigned value;if(sscanf(CONFIG_AS_ESPNOW_KEY+2*i,"%2x",&value)!=1)return ESP_ERR_INVALID_ARG;key[i]=value;key_set|=value!=0;}
 if(!key_set)return ESP_ERR_INVALID_ARG;
 if((e=esp_now_set_pmk(key))!=ESP_OK)return e;
 esp_now_peer_info_t peer={.channel=6,.ifidx=WIFI_IF_STA,.encrypt=true};memcpy(peer.peer_addr,peer_mac,6);memcpy(peer.lmk,key,16);
 if((e=esp_now_add_peer(&peer))!=ESP_OK)return e;
 return esp_now_register_recv_cb(receive);
#else
 httpd_handle_t server;httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.max_uri_handlers=4;config.recv_wait_timeout=1;config.send_wait_timeout=1;
 esp_err_t e=httpd_start(&server,&config);if(e!=ESP_OK)return e;
 httpd_uri_t home={.uri="/",.method=HTTP_GET,.handler=page};httpd_uri_t ws={.uri="/control",.method=HTTP_GET,.handler=websocket,.is_websocket=true};httpd_uri_t telemetry={.uri="/status",.method=HTTP_GET,.handler=status};
 if((e=httpd_register_uri_handler(server,&home))!=ESP_OK)return e;
 if((e=httpd_register_uri_handler(server,&ws))!=ESP_OK)return e;
 return httpd_register_uri_handler(server,&telemetry);
#endif
}
