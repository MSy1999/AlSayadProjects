#include "AnalogOutput.h"
#include "driver/gpio.h"
#include <math.h>
esp_err_t as_analog_timer_init(ledc_timer_t timer, uint32_t hz) {
    if (!hz || timer >= LEDC_TIMER_MAX) return ESP_ERR_INVALID_ARG;
    ledc_timer_config_t c = {.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=timer,.freq_hz=hz,.clk_cfg=LEDC_AUTO_CLK};
    return ledc_timer_config(&c);
}
esp_err_t as_analog_output_init(as_analog_output_t *o,int pin,ledc_channel_t ch,ledc_timer_t timer) {
    if (!o || !GPIO_IS_VALID_OUTPUT_GPIO(pin) || ch>=LEDC_CHANNEL_MAX || timer>=LEDC_TIMER_MAX) return ESP_ERR_INVALID_ARG;
    *o=(as_analog_output_t){.pin=pin,.channel=ch,.timer=timer};
    ledc_channel_config_t c={.gpio_num=pin,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=ch,.timer_sel=timer,.duty=0,.hpoint=0};
    esp_err_t e=ledc_channel_config(&c); o->initialized=e==ESP_OK; return e;
}
esp_err_t as_analog_output_write(as_analog_output_t *o,float value) {
    if (!o || !o->initialized) return ESP_ERR_INVALID_STATE;
    if (!isfinite(value) || value<0 || value>1) return ESP_ERR_INVALID_ARG;
    /* Each channel has one task owner. Direct duty updates need no fade service. */
    esp_err_t e=ledc_set_duty(LEDC_LOW_SPEED_MODE,o->channel,(uint32_t)lroundf(value*1023));
    if (e!=ESP_OK) return e;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE,o->channel);
}
esp_err_t as_analog_output_stop(as_analog_output_t *o) { return as_analog_output_write(o,0); }
