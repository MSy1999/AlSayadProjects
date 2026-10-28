/**
 * @file AsCppSmoke.cpp
 * @brief Compile-check shared C interfaces and the compatible C++ display facade.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsDisplay.hpp"
#include "AsDisplayIcons.h"
#include "AsMeasurementJson.h"
#include "AsWifiTransport.h"
#include "AsSdHistory.h"

esp_err_t as_cpp_smoke() {
    AsDisplay display;
    auto st_Button = as_button_default_config();
    st_Button.on_click = [](as_widget_t* pst_Widget, void* pContext) {
        (void)pContext;
        (void)as_display_widget_set_text(pst_Widget, "C++ callback");
    };
    auto st_Chart = as_display_chart_default_config();
    as_widget_t* pst_Chart = nullptr;
    esp_err_t error = display.addChart(st_Chart, &pst_Chart);
    if (error == ESP_OK) {
        const int32_t ai32Points[] = {0, LV_CHART_POINT_NONE, 100};
        error = display.setChartPoints(pst_Chart, ai32Points, 3, 0, 100);
    }
    if (error == ESP_OK) {
        error = display.addButton(st_Button);
    }
    if (error == ESP_OK) {
        auto st_Image = as_display_image_default_config();
        st_Image.pst_Source = &AS_ICON_TEMPERATURE;
        as_widget_t* pst_Image = nullptr;
        error = display.addImage(st_Image, &pst_Image);
        if (error == ESP_OK) {
            error = display.setImageSource(pst_Image, &AS_ICON_HUMIDITY);
        }
    }
    return error;
}
