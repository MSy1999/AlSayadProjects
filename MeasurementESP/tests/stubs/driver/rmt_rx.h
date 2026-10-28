/** @file rmt_rx.h @brief Host sensor SDK shim. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_RMT_H
#define AS_TEST_RMT_H
#include "gpio.h"
#include <stdbool.h>
#include <stddef.h>
#define RMT_CLK_SRC_DEFAULT 0
typedef struct AsFakeRmt* rmt_channel_handle_t;
typedef struct { uint16_t duration0, level0, duration1, level1; } rmt_symbol_word_t;
typedef struct { size_t num_symbols; } rmt_rx_done_event_data_t;
typedef struct {
    gpio_num_t gpio_num;
    int clk_src;
    uint32_t resolution_hz;
    size_t mem_block_symbols;
} rmt_rx_channel_config_t;
typedef struct { uint32_t signal_range_min_ns, signal_range_max_ns; } rmt_receive_config_t;
typedef struct {
    bool (*on_recv_done)(rmt_channel_handle_t, const rmt_rx_done_event_data_t*, void*);
} rmt_rx_event_callbacks_t;
esp_err_t rmt_new_rx_channel(const rmt_rx_channel_config_t* pst_Config, rmt_channel_handle_t* output);
esp_err_t rmt_rx_register_event_callbacks(rmt_channel_handle_t channel, const rmt_rx_event_callbacks_t* pst_Callbacks, void* context);
esp_err_t rmt_enable(rmt_channel_handle_t channel);
esp_err_t rmt_disable(rmt_channel_handle_t channel);
esp_err_t rmt_del_channel(rmt_channel_handle_t channel);
esp_err_t rmt_receive(rmt_channel_handle_t channel, void* buffer, size_t bytes, const rmt_receive_config_t* pst_Config);
#endif
