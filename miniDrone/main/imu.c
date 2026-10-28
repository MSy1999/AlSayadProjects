#include "imu.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
static i2c_master_dev_handle_t device;
static float bias[3];
static esp_err_t reg(uint8_t address,uint8_t value){uint8_t b[]={address,value};return i2c_master_transmit(device,b,2,10);}
static int16_t word(const uint8_t*p){return (int16_t)(((uint16_t)p[0]<<8)|p[1]);}
static esp_err_t raw(as_sample_t*s){
 uint8_t a=0x3b,b[14];esp_err_t e=i2c_master_transmit_receive(device,&a,1,b,14,3);if(e!=ESP_OK)return e;
 for(int i=0;i<3;i++){float sign=i==0?1:-1;s->accel[i]=sign*word(b+2*i)*(9.80665f/8192);s->gyro[i]=sign*word(b+8+2*i)*(.01745329252f/65.5f);}
 return ESP_OK;
}
esp_err_t as_imu_read(as_sample_t*s){esp_err_t e=raw(s);if(e==ESP_OK)for(int i=0;i<3;i++)s->gyro[i]-=bias[i];return e;}
esp_err_t as_imu_init(void){
 i2c_master_bus_handle_t bus;i2c_master_bus_config_t c={.i2c_port=0,.sda_io_num=AS_SDA,.scl_io_num=AS_SCL,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};
 esp_err_t e=i2c_new_master_bus(&c,&bus);if(e!=ESP_OK)return e;
 i2c_device_config_t d={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=AS_IMU_ADDRESS,.scl_speed_hz=400000};
 if((e=i2c_master_bus_add_device(bus,&d,&device))!=ESP_OK)return e;
 uint8_t a=0x75,id;if((e=i2c_master_transmit_receive(device,&a,1,&id,1,10))!=ESP_OK)return e;
 if(id!=0x68)return ESP_ERR_NOT_SUPPORTED;
 if((e=reg(0x6b,0x80))!=ESP_OK)return e;
 vTaskDelay(pdMS_TO_TICKS(100));
 if((e=reg(0x6b,1))!=ESP_OK)return e;
 if((e=reg(0x1a,3))!=ESP_OK)return e;
 if((e=reg(0x19,3))!=ESP_OK)return e;
 if((e=reg(0x1b,8))!=ESP_OK)return e;
 if((e=reg(0x1c,8))!=ESP_OK)return e;
 vTaskDelay(pdMS_TO_TICKS(100));
 /* Reject movement and nonlevel placement during calibration. */
 for(int n=0;n<500;n++){as_sample_t s;if((e=raw(&s))!=ESP_OK)return e;
  if(fabsf(s.accel[0])>1||fabsf(s.accel[1])>1||fabsf(s.accel[2]+9.80665f)>1)return ESP_ERR_INVALID_STATE;
  for(int i=0;i<3;i++){if(fabsf(s.gyro[i])>.15f)return ESP_ERR_INVALID_STATE;bias[i]+=s.gyro[i]/500;}
  vTaskDelay(pdMS_TO_TICKS(4));}
 return ESP_OK;
}
