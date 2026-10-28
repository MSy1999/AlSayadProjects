/** @file main.c @brief Temporary transistor bench test and flight entry point. */
/* Set to 0 to restore the normal drone firmware. */
#define AS_TRANSISTOR_TEST 1

#if AS_TRANSISTOR_TEST
#include "AnalogOutput.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TRANSISTOR_GPIO GPIO_NUM_0
#define TRANSISTOR_PWM_HZ 20000

void app_main(void)
{
    /* Release the previous test output and keep it OFF. */
    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_4));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 0));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT));

    /* GPIO 0 is exclusively owned by LEDC; the battery ADC is not started. */
    ESP_ERROR_CHECK(gpio_reset_pin(TRANSISTOR_GPIO));
    ESP_ERROR_CHECK(gpio_pulldown_en(TRANSISTOR_GPIO));
    as_analog_output_t transistor;
    ESP_ERROR_CHECK(as_analog_timer_init(LEDC_TIMER_0, TRANSISTOR_PWM_HZ));
    ESP_ERROR_CHECK(as_analog_output_init(&transistor, TRANSISTOR_GPIO,
                                         LEDC_CHANNEL_0, LEDC_TIMER_0));
    ESP_LOGW("transistor_test", "GPIO 0 will stay at 100%% in 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Hold LEDC output HIGH for true 100% with no PWM low pulses. */
    ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1));
    ESP_LOGI("transistor_test", "GPIO 0 constant 100%% (HIGH)");
}
#else
#include "AnalogOutput.h"
#include "WifiManager.h"
#include "as_flight.h"
#include "board.h"
#include "imu.h"
#include "inputs.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <math.h>
static as_analog_output_t outputs[4];static bool output_enabled;
static void stop(void){if(output_enabled)for(int i=0;i<4;i++)as_analog_output_stop(&outputs[i]);}
void app_main(void){
 const int pins[]=AS_MOTOR_PINS;
 for(int i=0;i<4;i++){gpio_set_level(pins[i],0);gpio_set_direction(pins[i],GPIO_MODE_OUTPUT);}
 /* Explicit hardware opt-in; outputs remain zero until safety allows arming. */
#ifdef CONFIG_AS_ENABLE_MOTORS
 ESP_ERROR_CHECK(as_analog_timer_init(LEDC_TIMER_0,20000));
 for(int i=0;i<4;i++)ESP_ERROR_CHECK(as_analog_output_init(&outputs[i],pins[i],(ledc_channel_t)i,LEDC_TIMER_0));
 output_enabled=true;
#endif
 ESP_ERROR_CHECK(as_wifi_start("AlSayad-MiniDrone",CONFIG_AS_WIFI_PASSWORD,AS_CHANNEL));
 QueueHandle_t input=xQueueCreate(1,sizeof(as_event_t)),events=xQueueCreate(8,sizeof(as_event_t));
 if(!input||!events){ESP_LOGE("drone","Queue allocation failed");return;}
 ESP_ERROR_CHECK(as_inputs_start(input,events));
 adc_oneshot_unit_handle_t adc;adc_oneshot_unit_init_cfg_t ac={.unit_id=ADC_UNIT_1};ESP_ERROR_CHECK(adc_oneshot_new_unit(&ac,&adc));
 adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};ESP_ERROR_CHECK(adc_oneshot_config_channel(adc,ADC_CHANNEL_0,&cc));
 adc_cali_handle_t calibration;adc_cali_curve_fitting_config_t cal={.unit_id=ADC_UNIT_1,.chan=ADC_CHANNEL_0,.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};
 bool adc_ok=adc_cali_create_scheme_curve_fitting(&cal,&calibration)==ESP_OK;
 ESP_LOGW("drone","Keep flat and stationary for MPU6050 calibration; outputs %s",output_enabled?"ENABLED":"DISABLED");
 bool imu_ok=as_imu_init()==ESP_OK;
 as_flight_t flight;as_flight_init(&flight);if(!imu_ok)as_flight_fault(&flight);
 esp_task_wdt_config_t wd={.timeout_ms=100,.idle_core_mask=0,.trigger_panic=true};esp_err_t we=esp_task_wdt_init(&wd);if(we==ESP_ERR_INVALID_STATE)we=esp_task_wdt_reconfigure(&wd);ESP_ERROR_CHECK(we);ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
 TickType_t tick=xTaskGetTickCount();int64_t previous=esp_timer_get_time();float battery=0;unsigned cycles=0;
 for(;;){
  vTaskDelayUntil(&tick,pdMS_TO_TICKS(4));int64_t now=esp_timer_get_time();float dt=(now-previous)*1e-6f;previous=now;
  if(cycles++%25==0){int raw,mv;if(!adc_ok||adc_oneshot_read(adc,ADC_CHANNEL_0,&raw)!=ESP_OK||adc_cali_raw_to_voltage(calibration,raw,&mv)!=ESP_OK)battery=0;else {float v=mv*.002f;battery=battery==0?v:.8f*battery+.2f*v;}}
  as_sample_t sample;bool sensor=imu_ok&&as_imu_read(&sample)==ESP_OK;
  bool healthy=sensor&&battery>3.4f&&battery<4.35f&&dt>=.001f&&dt<=.012f;
  as_event_t e;/* Latest input first, then explicit actions. Timestamp freshness checked locally. */
  if(xQueueReceive(input,&e,0)==pdTRUE&&now-e.received_us<=100000)as_flight_command(&flight,&e.command,e.source,e.received_us,healthy&&output_enabled);
  for(int n=0;n<8&&xQueueReceive(events,&e,0)==pdTRUE;n++)if(now-e.received_us<=100000)as_flight_command(&flight,&e.command,e.source,e.received_us,healthy&&output_enabled);
  float motors[4];as_flight_step(&flight,sensor?&sample:NULL,dt,now,healthy,motors);
  if(output_enabled){bool failed=false;for(int i=0;i<4;i++)if(as_analog_output_write(&outputs[i],motors[i])!=ESP_OK)failed=true;if(failed){as_flight_fault(&flight);stop();}}
  if(cycles%25==0)as_telemetry_publish(flight.state,battery,flight.roll,flight.pitch);
  esp_task_wdt_reset();
 }
}
#endif
