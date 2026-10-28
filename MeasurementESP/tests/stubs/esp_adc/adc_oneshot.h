/** @file adc_oneshot.h @brief Host sensor SDK shim. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_ADC_H
#define AS_TEST_ADC_H
#include "esp_err.h"
#include <stdint.h>
typedef struct AsFakeAdc* adc_oneshot_unit_handle_t;
typedef int adc_unit_t;
typedef int adc_channel_t;
#define ADC_UNIT_1 1
#define ADC_ATTEN_DB_12 12
#define ADC_BITWIDTH_12 12
typedef struct { adc_unit_t unit_id; } adc_oneshot_unit_init_cfg_t;
typedef struct { int atten, bitwidth; } adc_oneshot_chan_cfg_t;
esp_err_t adc_oneshot_io_to_channel(int pin, adc_unit_t* unit, adc_channel_t* channel);
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* pst_Config, adc_oneshot_unit_handle_t* output);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t unit, adc_channel_t channel, const adc_oneshot_chan_cfg_t* pst_Config);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t unit, adc_channel_t channel, int* raw);
esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t unit);
#endif
