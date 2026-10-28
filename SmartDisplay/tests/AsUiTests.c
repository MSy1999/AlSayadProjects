/**
 * @file AsUiTests.c
 * @brief Headless tests and real LVGL render captures of the application UI.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsRoomUi.h"
#include "AsRoomRuntime.h"
#include "AsDisplayBrand.h"
#include "AsDisplayIcons.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static lv_display_t* pst_Display;
static uint8_t au8Frame[320U * 240U * 2U];
static uint8_t au8Buffer[320U * 30U * 2U];
static bool bData;
static bool bSd;
static bool bFault;
static bool bStale;
static AsHistoryWindow st_TestArchive;
static uint32_t u32Clicks;
static uint8_t u8Brightness;
static int32_t i32ChangedValue;
static void as_test_flush(lv_display_t* pst_Disp, const lv_area_t* pst_Area, uint8_t* pu8Data) {
    size_t width = (size_t)(pst_Area->x2 - pst_Area->x1 + 1);
    for (int32_t i32Y = pst_Area->y1; i32Y <= pst_Area->y2; ++i32Y) {
        memcpy(au8Frame + ((size_t)i32Y * 320U + (size_t)pst_Area->x1) * 2U,
            pu8Data + (size_t)(i32Y - pst_Area->y1) * width * 2U, width * 2U);
    }
    lv_display_flush_ready(pst_Disp);
}
lv_display_t* as_display_get_lvgl_display(void) { return pst_Display; }
esp_err_t as_display_set_brightness(uint8_t u8Percent) {
    u8Brightness = u8Percent;
    return ESP_OK;
}
bool lvgl_port_lock(uint32_t u32TimeoutMs) { (void)u32TimeoutMs; return true; }
void lvgl_port_unlock(void) {}
esp_err_t as_room_snapshot(AsRoomSnapshot* pst_Output, AsHistoryRecord* pst_History) {
    *pst_Output = (AsRoomSnapshot){.u64AgeMs = bData ? 2000 : UINT64_MAX,
        .bConnected = bData, .bTransportReady = true, .u8Brightness = 75,
        .st_Storage = {.eState = AS_STORAGE_UNSUPPORTED}, .history_count = 120};
    if (bSd) {
        pst_Output->st_Storage = (AsStorageInfo){.eState = AS_STORAGE_RECORDING,
            .session_count = 1, .ast_Sessions = {{.u64Session = 1, .u64EndMs = 1200000}}};
    }
    const float af32Values[] = {24.6f, 53.0f, 72.0f};
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        pst_Output->st_Measurement.ast_Readings[index] = (AsReading){.f32Value = af32Values[index],
            .eStatus = bData ? AS_READING_VALID : AS_READING_UNAVAILABLE};
        if (bFault && index == AS_METRIC_TEMPERATURE) {
            pst_Output->st_Measurement.ast_Readings[index].eStatus = AS_READING_ERROR;
        }
        if (bStale) {
            pst_Output->st_Measurement.ast_Readings[index].eStatus = AS_READING_STALE;
            pst_Output->u64AgeMs = 15000;
            pst_Output->bConnected = false;
        }
    }
    if (pst_History != NULL) {
        for (size_t index = 0; index < AS_HISTORY_COUNT; ++index) {
            pst_History[index].u64ElapsedMs = index * 5000U;
            for (size_t metric = 0; metric < AS_METRIC_COUNT; ++metric) {
                pst_History[index].ast_Readings[metric] = (AsReading){
                    .f32Value = af32Values[metric] + sinf((float)index / 10.0f) * 3,
                    .eStatus = index % 37U == 0 ? AS_READING_UNAVAILABLE : AS_READING_VALID};
            }
        }
    }
    return ESP_OK;
}
esp_err_t as_room_save_brightness(uint8_t u8Percent) {
    u8Brightness = u8Percent;
    return ESP_OK;
}
bool as_room_history_request(uint64_t u64Session, uint64_t u64StartMs, uint32_t u32Request) {
    st_TestArchive = (AsHistoryWindow){.u64Session = u64Session, .u64StartMs = u64StartMs,
        .u32Request = u32Request, .bFound = true};
    for (size_t index = 0; index < AS_HISTORY_COUNT; ++index) {
        st_TestArchive.ast_Records[index].u64ElapsedMs = u64StartMs + index * 5000;
        st_TestArchive.ast_Records[index].bSimulated = true;
        for (size_t metric = 0; metric < AS_METRIC_COUNT; ++metric) {
            st_TestArchive.ast_Records[index].ast_Readings[metric] = (AsReading){
                .eStatus = AS_READING_VALID, .f32Value = 20 + (float)index / 100};
        }
    }
    return true;
}
void as_room_history_result(AsHistoryWindow* pst_Output) { *pst_Output = st_TestArchive; }
static void as_test_capture(const char* pcRoot, const char* pcName) {
    lv_tick_inc(250);
    lv_timer_handler();
    lv_refr_now(pst_Display);
    lv_obj_t* pst_Screen = lv_display_get_screen_active(pst_Display);
    assert(lv_obj_get_scroll_y(pst_Screen) == 0);
    for (uint32_t u32Index = 0; u32Index < lv_obj_get_child_count(pst_Screen); ++u32Index) {
        lv_obj_t* pst_Page = lv_obj_get_child(pst_Screen, (int32_t)u32Index);
        if (lv_obj_get_width(pst_Page) == 320 && lv_obj_get_height(pst_Page) == 138) {
            assert(lv_obj_get_y(pst_Page) == 32 && lv_obj_get_scroll_y(pst_Page) == 0);
        }
        if (lv_obj_get_width(pst_Page) == 320 && lv_obj_get_height(pst_Page) == 190) {
            assert(lv_obj_get_y(pst_Page) == 48 && lv_obj_get_scroll_y(pst_Page) == 0);
        }
    }
    char acPath[512];
    snprintf(acPath, sizeof(acPath), "%s/%s.rgb565", pcRoot, pcName);
    FILE* file = fopen(acPath, "wb");
    assert(file != NULL && fwrite(au8Frame, 1, sizeof(au8Frame), file) == sizeof(au8Frame));
    assert(fclose(file) == 0);
}
static lv_obj_t* as_test_button_at(lv_obj_t* pst_Parent, int32_t i32X, int32_t i32Y);
static void as_test_nav(int32_t i32X) {
    lv_obj_t* pst_Screen = lv_display_get_screen_active(pst_Display);
    for (uint32_t u32Index = 0; u32Index < lv_obj_get_child_count(pst_Screen); ++u32Index) {
        lv_obj_t* pst_Child = lv_obj_get_child(pst_Screen, (int32_t)u32Index);
        if (lv_obj_check_type(pst_Child, &lv_button_class) && lv_obj_get_x(pst_Child) == i32X
            && lv_obj_get_y(pst_Child) == 198 && !lv_obj_has_flag(pst_Child, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_send_event(pst_Child, LV_EVENT_CLICKED, NULL);
            return;
        }
    }
    /* Dashboard navigation is through the sensor cards and header settings shortcut. */
    lv_obj_t* pst_Target = as_test_button_at(pst_Screen, i32X == 112 ? 56 : 21,
                                            i32X == 112 ? 110 : 40);
    assert(pst_Target != NULL);
    lv_obj_send_event(pst_Target, LV_EVENT_CLICKED, NULL);
}
static lv_obj_t* as_test_button_at(lv_obj_t* pst_Parent, int32_t i32X, int32_t i32Y) {
    for (int32_t i32Index = (int32_t)lv_obj_get_child_count(pst_Parent) - 1; i32Index >= 0; --i32Index) {
        lv_obj_t* pst_Child = lv_obj_get_child(pst_Parent, i32Index);
        if (lv_obj_has_flag(pst_Child, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        lv_area_t st_Area;
        lv_obj_get_coords(pst_Child, &st_Area);
        if (lv_obj_has_flag(pst_Child, LV_OBJ_FLAG_CLICKABLE) && i32X >= st_Area.x1
            && i32X <= st_Area.x2 && i32Y >= st_Area.y1 && i32Y <= st_Area.y2) {
            return pst_Child;
        }
        lv_obj_t* pst_Found = as_test_button_at(pst_Child, i32X, i32Y);
        if (pst_Found != NULL) {
            return pst_Found;
        }
    }
    return NULL;
}
static void as_test_click(as_widget_t* pst_Widget, void* pContext) {
    assert(pContext == &u32Clicks);
    ++u32Clicks;
    assert(as_display_widget_set_text(pst_Widget, "Clicked") == ESP_OK);
}
static void as_test_delete_click(as_widget_t* pst_Widget, void* pContext) {
    (void)pContext;
    ++u32Clicks;
    assert(as_display_widget_delete(pst_Widget) == ESP_OK);
}
static void as_test_change(as_widget_t* pst_Widget, int32_t i32Value, void* pContext) {
    (void)pst_Widget;
    assert(pContext == &i32ChangedValue);
    i32ChangedValue = i32Value;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    lv_init();
    pst_Display = lv_display_create(320, 240);
    assert(pst_Display != NULL);
    lv_display_set_color_format(pst_Display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(pst_Display, au8Buffer, NULL, sizeof(au8Buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(pst_Display, as_test_flush);
    assert(as_display_show_boot_logo(2000) == ESP_OK);
    assert(as_room_ui_start() == ESP_OK);
    as_test_capture(argv[1], "boot");
    assert(as_display_hide_boot_logo() == ESP_OK);
    as_test_capture(argv[1], "waiting");
    bData = true;
    as_test_capture(argv[1], "dashboard");
    bFault = true;
    as_test_capture(argv[1], "sensor-error");
    bFault = false;
    bStale = true;
    as_test_capture(argv[1], "stale");
    bStale = false;
    as_test_nav(112);
    as_test_capture(argv[1], "chart");
    as_test_nav(8);
    as_test_capture(argv[1], "settings");
    bSd = true;
    as_test_nav(112);
    as_test_capture(argv[1], "chart-sd-live");
    lv_obj_t* pst_Previous = as_test_button_at(lv_display_get_screen_active(pst_Display), 264, 157);
    assert(pst_Previous != NULL);
    assert(lv_obj_check_type(pst_Previous, &lv_button_class));
    lv_obj_send_event(pst_Previous, LV_EVENT_CLICKED, NULL);
    as_test_capture(argv[1], "archive-loading");
    as_test_capture(argv[1], "archive");
    assert(st_TestArchive.u64StartMs == 600000);
    bool bMarked = false;
    lv_obj_t* pst_Root = lv_display_get_screen_active(pst_Display);
    for (uint32_t u32Index = 0; u32Index < lv_obj_get_child_count(pst_Root); ++u32Index) {
        lv_obj_t* pst_Child = lv_obj_get_child(pst_Root, (int32_t)u32Index);
        if (lv_obj_check_type(pst_Child, &lv_label_class)
            && strchr(lv_label_get_text(pst_Child), '(') != NULL) {
            bMarked = true;
        }
    }
    assert(bMarked); /* archived test data stays marked */
    as_button_config_t st_Button = as_button_default_config();
    st_Button.on_click = as_test_click;
    st_Button.user_data = &u32Clicks;
    as_widget_t* pst_Button = NULL;
    assert(as_display_add_button(&st_Button, &pst_Button) == ESP_OK);
    assert(lv_obj_send_event(pst_Button, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK);
    assert(u32Clicks == 1);
    assert(as_display_widget_set_value(pst_Button, 5) == ESP_ERR_INVALID_ARG);
    assert(as_display_widget_delete(pst_Button) == ESP_OK);
    for (size_t index = 0; index < 64; ++index) {
        st_Button.on_click = as_test_delete_click;
        assert(as_display_add_button(&st_Button, &pst_Button) == ESP_OK);
        assert(lv_obj_send_event(pst_Button, LV_EVENT_CLICKED, NULL) == LV_RESULT_INVALID);
    }
    as_slider_config_t st_Slider = as_slider_default_config();
    st_Slider.on_change = as_test_change;
    st_Slider.user_data = &i32ChangedValue;
    as_widget_t* pst_Slider = NULL;
    assert(as_display_add_slider(&st_Slider, &pst_Slider) == ESP_OK);
    assert(as_display_widget_set_value(pst_Slider, 42) == ESP_OK);
    assert(i32ChangedValue == 0); /* programmatic updates do not invoke the handler */
    lv_obj_send_event(pst_Slider, LV_EVENT_VALUE_CHANGED, NULL);
    assert(i32ChangedValue == 42);
    assert(as_display_widget_set_value(pst_Slider, 101) == ESP_ERR_INVALID_ARG);
    assert(as_display_widget_delete(pst_Slider) == ESP_OK);
    as_switch_config_t st_Switch = as_switch_default_config();
    st_Switch.on_change = as_test_change;
    st_Switch.user_data = &i32ChangedValue;
    as_widget_t* pst_Switch = NULL;
    assert(as_display_add_switch(&st_Switch, &pst_Switch) == ESP_OK);
    assert(as_display_widget_set_value(pst_Switch, 1) == ESP_OK);
    lv_obj_send_event(pst_Switch, LV_EVENT_VALUE_CHANGED, NULL);
    assert(i32ChangedValue == 1);
    assert(as_display_widget_delete(pst_Switch) == ESP_OK);
    AsDisplayChartConfig st_ExtraChart = as_display_chart_default_config();
    as_widget_t* pst_ExtraChart = NULL;
    assert(as_display_add_chart(&st_ExtraChart, &pst_ExtraChart) == ESP_OK);
    assert(as_display_widget_set_text(pst_ExtraChart, "invalid") == ESP_ERR_INVALID_ARG);
    assert(as_display_widget_set_value(pst_ExtraChart, 5) == ESP_ERR_INVALID_ARG);
    assert(as_display_widget_set_hidden(pst_ExtraChart, true) == ESP_OK);
    assert(as_display_widget_set_enabled(pst_ExtraChart, false) == ESP_OK);
    assert(as_display_widget_delete(pst_ExtraChart) == ESP_OK);
    AsImageConfig st_Image = as_display_image_default_config();
    as_widget_t* pst_Image = NULL;
    assert(as_display_add_image(&st_Image, &pst_Image) == ESP_ERR_INVALID_ARG);
    st_Image.pst_Source = &AS_ICON_TEMPERATURE;
    assert(as_display_add_image(&st_Image, &pst_Image) == ESP_OK);
    assert(!lv_obj_has_flag(pst_Image, LV_OBJ_FLAG_CLICKABLE));
    assert(as_display_image_set_source(pst_Image, &AS_ICON_LIGHT) == ESP_OK);
    assert(as_display_widget_set_text(pst_Image, "invalid") == ESP_ERR_INVALID_ARG);
    assert(as_display_widget_delete(pst_Image) == ESP_OK);
    assert(as_display_show_boot_logo(1) == ESP_OK);
    lv_tick_inc(10);
    lv_timer_handler();
    assert(as_display_hide_boot_logo() == ESP_OK);
    lv_mem_monitor_t st_Memory;
    lv_mem_monitor(&st_Memory);
    printf("LVGL host heap: used=%zu, free=%zu, total=%zu, peak=%zu bytes\n",
           st_Memory.total_size - st_Memory.free_size, st_Memory.free_size,
           st_Memory.total_size, st_Memory.max_used);
    assert(as_room_ui_stop() == ESP_OK);
    lv_display_delete(pst_Display);
    pst_Display = NULL;
    lv_deinit();
    puts("PASS: real LVGL renders, navigation, button callbacks, type validation, deletion, splash cleanup");
    return 0;
}
