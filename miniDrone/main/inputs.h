#pragma once
#include "as_flight.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
typedef struct {as_command_t command; uint32_t source; int64_t received_us;} as_event_t;
esp_err_t as_inputs_start(QueueHandle_t input,QueueHandle_t events);
void as_telemetry_publish(as_state_t state,float battery,float roll,float pitch);
