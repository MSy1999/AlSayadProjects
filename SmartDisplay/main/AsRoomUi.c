/**
 * @file AsRoomUi.c
 * @brief Arabic Room Monitor dashboard, charts and settings.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsRoomUi.h"
#include "AsRoomRuntime.h"
#include "AsDisplayBrand.h"
#include "AsDisplayIcons.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum { AS_PAGE_DASHBOARD, AS_PAGE_CHART, AS_PAGE_SETTINGS } AsRoomPage;
static as_widget_t* apst_Pages[3];
static as_widget_t* apst_Values[AS_METRIC_COUNT];
static as_widget_t* apst_Status[AS_METRIC_COUNT];
static as_widget_t* apst_StatusIcons[AS_METRIC_COUNT];
static as_widget_t* apst_SensorIcons[AS_METRIC_COUNT];
static as_widget_t* apst_DashboardHeader[5];
static as_widget_t* pst_Overview;
static as_widget_t* pst_OverviewIcon;
static as_widget_t* pst_UpdateAge;
static as_widget_t* pst_DashboardFrame;
static as_widget_t* pst_Header;
static as_widget_t* pst_Footer;
static as_widget_t* pst_Chart;
static as_widget_t* pst_ChartCaption;
static as_widget_t* pst_ChartScale;
static as_widget_t* pst_ArchiveRange;
static as_widget_t* pst_ArchiveUnit;
static as_widget_t* pst_Connection;
static as_widget_t* pst_StorageLabel;
static as_widget_t* pst_BrightnessLabel;
static as_widget_t* pst_Previous;
static as_widget_t* pst_Next;
static as_widget_t* pst_Session;
static as_widget_t* apst_Navigation[3];
static lv_timer_t* pst_UiTimer;
static AsRoomPage ePage;
static AsMetric eMetric;
static AsRoomSnapshot st_Snapshot;
static AsHistoryRecord ast_History[AS_HISTORY_COUNT];
static AsHistoryWindow st_Archive;
static int32_t ai32Points[AS_HISTORY_COUNT];
static uint64_t u64SelectedSession;
static uint64_t u64WindowStartMs;
static uint32_t u32Request;
static bool bArchive;
static bool bArchivePending;
static size_t session_index;
static esp_err_t setup_error;
static const char* const apcMetricNames[] = {"الحرارة", "الرطوبة", "الإضاءة النسبية"};
static const char* const apcUnits[] = {"°C", "%RH", "%"};

static void as_ui_error(esp_err_t error) {
    if (error != ESP_OK) {
        ESP_LOGE("AsRoomUi", "%s", esp_err_to_name(error));
        if (setup_error == ESP_OK) {
            setup_error = error;
        }
    }
}
static as_widget_t* as_ui_label(as_widget_t* pst_Parent, const char* pcText,
                                int32_t i32X, int32_t i32Y, int32_t i32Width,
                                const lv_font_t* pst_Font, uint32_t u32Color, bool bLtr) {
    as_label_config_t st_Label = as_label_default_config();
    st_Label.layout = (as_widget_layout_t){.parent = pst_Parent, .align = LV_ALIGN_TOP_LEFT,
        .x = i32X, .y = i32Y, .width = i32Width};
    st_Label.text = pcText;
    st_Label.style.text_color = u32Color;
    st_Label.style.font = pst_Font;
    st_Label.text_align = LV_TEXT_ALIGN_CENTER;
    as_widget_t* pst_Label = NULL;
    esp_err_t error = as_display_add_label(&st_Label, &pst_Label);
    as_ui_error(error);
    if (pst_Label != NULL) {
        lv_obj_set_style_base_dir(pst_Label, bLtr ? LV_BASE_DIR_LTR : LV_BASE_DIR_RTL, 0);
    }
    return pst_Label;
}
/* الصور مستقلة عن النصوص والقياسات، وبياناتها ثابتة في ذاكرة البرنامج. */
static as_widget_t* as_ui_image(as_widget_t* pst_Parent, const lv_image_dsc_t* pst_Source,
                                int32_t i32X, int32_t i32Y, int32_t i32Width, int32_t i32Height) {
    AsImageConfig st_Image = as_display_image_default_config();
    st_Image.st_Layout = (as_widget_layout_t){.parent = pst_Parent, .align = LV_ALIGN_TOP_LEFT,
        .x = i32X, .y = i32Y, .width = i32Width, .height = i32Height};
    st_Image.pst_Source = pst_Source;
    as_widget_t* pst_Image = NULL;
    as_ui_error(as_display_add_image(&st_Image, &pst_Image));
    return pst_Image;
}

static as_widget_t* as_ui_panel(as_widget_t* pst_Parent, int32_t i32X, int32_t i32Y,
                                int32_t i32Width, int32_t i32Height, bool bCard) {
    as_panel_config_t st_Panel = as_panel_default_config();
    st_Panel.layout = (as_widget_layout_t){.parent = pst_Parent, .align = LV_ALIGN_TOP_LEFT,
        .x = i32X, .y = i32Y, .width = i32Width, .height = i32Height};
    st_Panel.style.background_color = bCard ? AS_COLOR_CARD : AS_COLOR_BACKGROUND;
    st_Panel.style.padding = 0;
    st_Panel.style.radius = bCard ? 8 : 0;
    st_Panel.style.border_width = bCard ? 1 : 0;
    st_Panel.style.border_color = AS_COLOR_BORDER;
    as_widget_t* pst_Panel = NULL;
    as_ui_error(as_display_add_panel(&st_Panel, &pst_Panel));
    return pst_Panel;
}
static as_widget_t* as_ui_button(as_widget_t* pst_Parent, const char* pcText,
                                 int32_t i32X, int32_t i32Y, int32_t i32Width,
                                 int32_t i32Height, as_click_handler_t handler, void* pContext) {
    as_button_config_t st_Button = as_button_default_config();
    st_Button.layout = (as_widget_layout_t){.parent = pst_Parent, .align = LV_ALIGN_TOP_LEFT,
        .x = i32X, .y = i32Y, .width = i32Width, .height = i32Height};
    st_Button.text = pcText;
    st_Button.style.background_color = AS_COLOR_SURFACE;
    st_Button.style.text_color = AS_COLOR_TEXT;
    st_Button.style.font = &AsCairo16;
    st_Button.style.border_color = AS_COLOR_BORDER;
    st_Button.style.border_width = 1;
    st_Button.style.padding = 1;
    st_Button.pressed_color = AS_COLOR_BORDER;
    st_Button.on_click = handler;
    st_Button.user_data = pContext;
    as_widget_t* pst_Button = NULL;
    as_ui_error(as_display_add_button(&st_Button, &pst_Button));
    if (pst_Button != NULL) {
        lv_obj_set_style_base_dir(pst_Button, LV_BASE_DIR_RTL, 0);
    }
    return pst_Button;
}
static void as_ui_navigate(as_widget_t* pst_Button, void* pContext) {
    (void)pst_Button;
    ePage = (AsRoomPage)(uintptr_t)pContext;
    bool bDashboard = ePage == AS_PAGE_DASHBOARD;
    for (size_t index = 0; index < 3; ++index) {
        as_ui_error(as_display_widget_set_hidden(apst_Pages[index], index != (size_t)ePage));
        as_ui_error(as_display_widget_set_hidden(apst_Navigation[index], bDashboard));
        lv_obj_set_style_bg_color(apst_Navigation[index],
            lv_color_hex(index == (size_t)ePage ? AS_COLOR_GOLD : AS_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(apst_Navigation[index],
            lv_color_hex(index == (size_t)ePage ? AS_COLOR_BACKGROUND : AS_COLOR_TEXT), 0);
    }
    for (size_t index = 0; index < 5; ++index) {
        as_ui_error(as_display_widget_set_hidden(apst_DashboardHeader[index], !bDashboard));
    }
    as_ui_error(as_display_widget_set_hidden(pst_Footer, bDashboard));
    lv_obj_set_pos(pst_Header, bDashboard ? 74 : 8, bDashboard ? 27 : 0);
    lv_obj_set_width(pst_Header, bDashboard ? 168 : 304);
    lv_obj_set_y(pst_DashboardFrame, bDashboard ? 18 : 0);
    lv_obj_set_height(pst_DashboardFrame, bDashboard ? 205 : 240);
}

/* لمس البطاقة يفتح مخطط قياسها؛ لا نضيف شريط تنقل يزاحم لوحة القراءات. */
static void as_ui_card_clicked(as_widget_t* pst_Card, void* pContext) {
    (void)pst_Card;
    eMetric = (AsMetric)(uintptr_t)pContext;
    bArchive = false;
    bArchivePending = false;
    as_ui_navigate(NULL, (void*)(uintptr_t)AS_PAGE_CHART);
}

static void as_ui_metric(as_widget_t* pst_Button, void* pContext) {
    (void)pst_Button;
    eMetric = (AsMetric)(uintptr_t)pContext;
}
static void as_ui_archive_query(void) {
    ++u32Request;
    bArchivePending = as_room_history_request(u64SelectedSession, u64WindowStartMs, u32Request);
    if (!bArchivePending) {
        as_ui_error(as_display_widget_set_text(pst_ChartCaption, "تعذر طلب السجل"));
    }
}
static void as_ui_previous(as_widget_t* pst_Button, void* pContext) {
    (void)pst_Button;
    (void)pContext;
    if (st_Snapshot.st_Storage.session_count == 0) {
        return;
    }
    if (!bArchive) {
        session_index = 0;
        u64SelectedSession = st_Snapshot.st_Storage.ast_Sessions[0].u64Session;
        uint64_t u64EndMs = st_Snapshot.st_Storage.ast_Sessions[0].u64EndMs;
        u64WindowStartMs = u64EndMs > 600000U ? u64EndMs - 600000U : 0;
        u64WindowStartMs -= u64WindowStartMs % AS_HISTORY_BUCKET_MS;
        bArchive = true;
    } else {
        u64WindowStartMs = u64WindowStartMs >= 600000U ? u64WindowStartMs - 600000U : 0;
    }
    as_ui_archive_query();
}
static void as_ui_next(as_widget_t* pst_Button, void* pContext) {
    (void)pst_Button;
    (void)pContext;
    if (!bArchive) {
        return;
    }
    uint64_t u64EndMs = st_Snapshot.st_Storage.ast_Sessions[session_index].u64EndMs;
    if (u64WindowStartMs + 600000U >= u64EndMs) {
        bArchive = false;
        bArchivePending = false;
    } else {
        u64WindowStartMs += 600000U;
        as_ui_archive_query();
    }
}
static void as_ui_session(as_widget_t* pst_Button, void* pContext) {
    (void)pst_Button;
    (void)pContext;
    size_t count = st_Snapshot.st_Storage.session_count;
    if (count == 0) {
        return;
    }
    session_index = bArchive ? (session_index + 1U) % count : 0;
    bArchive = true;
    u64SelectedSession = st_Snapshot.st_Storage.ast_Sessions[session_index].u64Session;
    uint64_t u64EndMs = st_Snapshot.st_Storage.ast_Sessions[session_index].u64EndMs;
    u64WindowStartMs = u64EndMs > 600000U ? u64EndMs - 600000U : 0;
    u64WindowStartMs -= u64WindowStartMs % AS_HISTORY_BUCKET_MS;
    as_ui_archive_query();
}
static void as_ui_brightness(as_widget_t* pst_Slider, int32_t i32Value, void* pContext) {
    (void)pst_Slider;
    (void)pContext;
    as_ui_error(as_display_set_brightness((uint8_t)i32Value));
    char acText[64];
    snprintf(acText, sizeof(acText), "سطوع الشاشة  %ld%%", (long)i32Value);
    as_ui_error(as_display_widget_set_text(pst_BrightnessLabel, acText));
}
static void as_ui_brightness_released(lv_event_t* pst_Event) {
    as_widget_t* pst_Slider = lv_event_get_current_target(pst_Event);
    as_ui_error(as_room_save_brightness((uint8_t)lv_slider_get_value(pst_Slider)));
}
static const char* as_ui_status(AsReadingStatus eStatus) {
    switch (eStatus) {
        case AS_READING_VALID: return "قراءة صالحة";
        case AS_READING_ERROR: return "خطأ الحساس";
        case AS_READING_STALE: return "قراءة قديمة";
        case AS_READING_UNAVAILABLE: return "غير متاح";
        default: return "غير متاح";
    }
}
static void as_ui_refresh(lv_timer_t* pst_Timer) {
    (void)pst_Timer;
    esp_err_t error = as_room_snapshot(&st_Snapshot, ast_History);
    if (error != ESP_OK) {
        return;
    }
    bool bSimulated = st_Snapshot.st_Measurement.bSimulated;
    if (ePage == AS_PAGE_CHART && bArchive && st_Archive.u32Request == u32Request) {
        bSimulated = false;
        for (size_t index = 0; index < AS_HISTORY_COUNT; ++index) {
            bSimulated = bSimulated || st_Archive.ast_Records[index].bSimulated;
        }
    }
    as_ui_error(as_display_widget_set_text(pst_Header,
        bSimulated ? "مراقبة الغرفة (تجريبي)" : "مراقبة الغرفة"));
    lv_obj_set_style_text_font(pst_Header,
        ePage == AS_PAGE_DASHBOARD && bSimulated ? &AsCairo14 : &AsCairo20, 0);
    bool bAllValid = true;
    bool bAnyError = false;
    bool bAnyStale = false;
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        const AsReading* pst_Reading = &st_Snapshot.st_Measurement.ast_Readings[index];
        char acText[32] = "--";
        if (pst_Reading->eStatus == AS_READING_VALID) {
            snprintf(acText, sizeof(acText), index == AS_METRIC_TEMPERATURE ? "%.1f" : "%.0f",
                     (double)pst_Reading->f32Value);
        }
        lv_obj_set_style_text_font(apst_Values[index], strlen(acText) > 4 ? &AsCairo20 : &AsCairo28, 0);
        as_ui_error(as_display_widget_set_text(apst_Values[index], acText));
        as_ui_error(as_display_widget_set_text(apst_Status[index], as_ui_status(pst_Reading->eStatus)));
        bAllValid = bAllValid && pst_Reading->eStatus == AS_READING_VALID;
        bAnyError = bAnyError || pst_Reading->eStatus == AS_READING_ERROR;
        bAnyStale = bAnyStale || pst_Reading->eStatus == AS_READING_STALE;
        uint32_t u32Color = AS_COLOR_MUTED;
        if (pst_Reading->eStatus == AS_READING_VALID) {
            u32Color = AS_COLOR_CYAN;
        } else if (pst_Reading->eStatus == AS_READING_ERROR) {
            u32Color = AS_COLOR_ERROR;
        } else if (pst_Reading->eStatus == AS_READING_STALE) {
            u32Color = AS_COLOR_GOLD;
        }
        const lv_image_dsc_t* pst_StateIcon = &AS_ICON_WAITING;
        uint32_t u32Fill = 0x0A1E2B;
        uint32_t u32Border = 0x244B63;
        if (pst_Reading->eStatus == AS_READING_VALID) {
            pst_StateIcon = &AS_ICON_CHECK;
            u32Fill = 0x062B24;
            u32Border = 0x00CC84;
            u32Color = 0xBAD8E9;
        } else if (pst_Reading->eStatus == AS_READING_ERROR) {
            pst_StateIcon = &AS_ICON_ERROR;
            u32Fill = 0x291318;
            u32Border = AS_COLOR_ERROR;
        } else if (pst_Reading->eStatus == AS_READING_STALE) {
            u32Fill = 0x2A2111;
            u32Border = AS_COLOR_GOLD;
        }
        as_ui_error(as_display_image_set_source(apst_StatusIcons[index], pst_StateIcon));
        lv_obj_set_style_text_color(apst_Status[index], lv_color_hex(u32Color), 0);
        lv_obj_set_style_bg_color(apst_Status[index], lv_color_hex(u32Fill), 0);
        lv_obj_set_style_border_color(apst_Status[index], lv_color_hex(u32Border), 0);
        lv_obj_set_style_image_opa(apst_SensorIcons[index],
            pst_Reading->eStatus == AS_READING_VALID ? LV_OPA_COVER : LV_OPA_50, 0);
    }
    const char* pcOverview = bAllValid ? "القراءات صالحة"
        : (bAnyError ? "خطأ في الحساسات" : (bAnyStale ? "البيانات قديمة" : "انتظار القراءات"));
    as_ui_error(as_display_widget_set_text(pst_Overview, pcOverview));
    as_ui_error(as_display_image_set_source(pst_OverviewIcon,
        bAllValid ? &AS_ICON_CHECK : (bAnyError ? &AS_ICON_ERROR : &AS_ICON_WAITING)));
    char acAge[64];
    if (st_Snapshot.u64AgeMs == UINT64_MAX) {
        snprintf(acAge, sizeof(acAge), "آخر تحديث\n--");
    } else {
        snprintf(acAge, sizeof(acAge), "آخر تحديث\n%" PRIu64 " ث", st_Snapshot.u64AgeMs / 1000U);
    }
    as_ui_error(as_display_widget_set_text(pst_UpdateAge, acAge));
    char acFooter[112];
    if (!st_Snapshot.bTransportReady) {
        snprintf(acFooter, sizeof(acFooter), "تعذر بدء الاتصال");
    } else if (st_Snapshot.u64AgeMs == UINT64_MAX) {
        snprintf(acFooter, sizeof(acFooter), "%s / انتظار البيانات",
                  st_Snapshot.bConnected ? "جهاز متصل" : "لا جهاز");
    } else {
        snprintf(acFooter, sizeof(acFooter), "%s / عمر البيانات %" PRIu64 " ث",
            st_Snapshot.bConnected ? "جهاز متصل" : "لا جهاز", st_Snapshot.u64AgeMs / 1000U);
    }
    as_ui_error(as_display_widget_set_text(pst_Footer, acFooter));
    const char* pcStorage = "RAM / آخر عشر دقائق";
    if (st_Snapshot.st_Storage.eState == AS_STORAGE_RECORDING) {
        pcStorage = "SD / تسجيل القراءات";
    } else if (st_Snapshot.st_Storage.eState == AS_STORAGE_UNSUPPORTED) {
        pcStorage = "RAM / ناقل SD غير متاح";
    } else if (st_Snapshot.st_Storage.eState == AS_STORAGE_ERROR) {
        pcStorage = "RAM / خطأ تخزين SD";
    }
    char acStorage[128];
    snprintf(acStorage, sizeof(acStorage), "%s / فقد %" PRIu32,
              pcStorage, st_Snapshot.st_Storage.u32Dropped);
    as_ui_error(as_display_widget_set_text(pst_StorageLabel, acStorage));
    if (ePage != AS_PAGE_CHART) {
        return;
    }
    bool bHasArchive = st_Snapshot.st_Storage.eState == AS_STORAGE_RECORDING
        && st_Snapshot.st_Storage.session_count > 0;
    as_ui_error(as_display_widget_set_enabled(pst_Previous, bHasArchive));
    as_ui_error(as_display_widget_set_enabled(pst_Next, bArchive));
    as_ui_error(as_display_widget_set_enabled(pst_Session, bHasArchive));
    const AsHistoryRecord* pst_Records = ast_History;
    size_t count = st_Snapshot.history_count;
    if (bArchive) {
        as_room_history_result(&st_Archive);
        if (st_Archive.u32Request == u32Request) {
            bArchivePending = false;
        }
        if (bArchivePending || st_Archive.u32Request != u32Request || !st_Archive.bFound) {
            count = 0;
        } else {
            pst_Records = st_Archive.ast_Records;
            count = AS_HISTORY_COUNT;
        }
    }
    int32_t i32Min = eMetric == AS_METRIC_TEMPERATURE ? -100 : 0;
    int32_t i32Max = eMetric == AS_METRIC_TEMPERATURE ? 500 : 1000;
    size_t offset = AS_HISTORY_COUNT - count;
    for (size_t index = 0; index < AS_HISTORY_COUNT; ++index) {
        ai32Points[index] = LV_CHART_POINT_NONE;
        if (index >= offset) {
            const AsReading* pst_Reading = &pst_Records[index - offset].ast_Readings[eMetric];
            if (pst_Reading->eStatus == AS_READING_VALID) {
                ai32Points[index] = (int32_t)lroundf(pst_Reading->f32Value * 10.0f);
                if (ai32Points[index] < i32Min) {
                    i32Min = ai32Points[index] - 50;
                }
                if (ai32Points[index] > i32Max) {
                    i32Max = ai32Points[index] + 50;
                }
            }
        }
    }
    as_ui_error(as_display_chart_set_points(pst_Chart, ai32Points, AS_HISTORY_COUNT, i32Min, i32Max));
    char acScale[48];
    snprintf(acScale, sizeof(acScale), "%.0f..%.0f %s",
             (double)i32Min / 10.0, (double)i32Max / 10.0, apcUnits[eMetric]);
    as_ui_error(as_display_widget_set_text(pst_ChartScale, acScale));
    char acCaption[192];
    as_ui_error(as_display_widget_set_hidden(pst_ArchiveRange, !bArchive));
    as_ui_error(as_display_widget_set_hidden(pst_ArchiveUnit, !bArchive));
    if (bArchive) {
        lv_obj_set_width(pst_ChartCaption, 88);
        lv_obj_set_x(pst_ChartCaption, 228);
        char acRange[48];
        snprintf(acRange, sizeof(acRange), "%" PRIu64 "..%" PRIu64,
                 u64WindowStartMs / 60000U, (u64WindowStartMs + 600000U) / 60000U);
        as_ui_error(as_display_widget_set_text(pst_ArchiveRange, acRange));
        if (bArchivePending || count == 0) {
            snprintf(acCaption, sizeof(acCaption), "%s", bArchivePending ? "تحميل" : "غير متاح");
        } else {
            snprintf(acCaption, sizeof(acCaption), "جلسة %" PRIu64, u64SelectedSession);
        }
    } else {
        lv_obj_set_width(pst_ChartCaption, 200);
        lv_obj_set_x(pst_ChartCaption, 116);
        snprintf(acCaption, sizeof(acCaption), "آخر 10 دقائق");
    }
    as_ui_error(as_display_widget_set_text(pst_ChartCaption, acCaption));
}
esp_err_t as_room_ui_start(void) {
    if (pst_UiTimer != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lvgl_port_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    setup_error = ESP_OK;
    lv_obj_t* pst_Screen = lv_display_get_screen_active(as_display_get_lvgl_display());
    lv_obj_remove_flag(pst_Screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to(pst_Screen, 0, 0, LV_ANIM_OFF);
    as_ui_error(as_display_set_background(0x06121D));
    as_widget_t* pst_Frame = as_ui_panel(NULL, 0, 18, 320, 205, false);
    pst_DashboardFrame = pst_Frame;
    if (pst_Frame != NULL) {
        lv_obj_set_style_bg_color(pst_Frame, lv_color_hex(0x06121D), 0);
        lv_obj_set_style_bg_grad_color(pst_Frame, lv_color_hex(0x030C13), 0);
        lv_obj_set_style_bg_grad_dir(pst_Frame, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(pst_Frame, lv_color_hex(0x174766), 0);
        lv_obj_set_style_border_width(pst_Frame, 1, 0);
        lv_obj_set_style_radius(pst_Frame, 5, 0);
    }
    apst_DashboardHeader[0] = as_ui_image(NULL, &AS_HEADER_CIRCUIT, 0, 18, 320, 48);
    as_widget_layout_t st_Logo = {.align = LV_ALIGN_TOP_LEFT, .x = 249, .y = 31,
                                  .width = 65, .height = 23};
    as_ui_error(as_display_add_brand_logo(&st_Logo, &apst_DashboardHeader[1]));
    pst_Header = as_ui_label(NULL, "مراقبة الغرفة", 74, 27, 168, &AsCairo20, AS_COLOR_TEXT, false);
    apst_DashboardHeader[2] = as_ui_label(NULL, "AlSayad", 90, 48, 52,
                                          &AsCairo12, AS_COLOR_GOLD, true);
    apst_DashboardHeader[3] = as_ui_label(NULL, "Room Monitor", 136, 48, 88,
                                          &AsCairo12, 0x9BC9E7, true);
    apst_DashboardHeader[4] = as_ui_button(NULL, "", 7, 26, 28, 28,
                                          as_ui_navigate, (void*)(uintptr_t)AS_PAGE_SETTINGS);
    if (apst_DashboardHeader[4] != NULL) {
        lv_obj_set_style_bg_opa(apst_DashboardHeader[4], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(apst_DashboardHeader[4], lv_color_hex(0x1A526D), 0);
        lv_obj_set_style_pad_all(apst_DashboardHeader[4], 0, 0);
        as_ui_image(apst_DashboardHeader[4], &AS_ICON_SETTINGS, 4, 4, 18, 18);
    }
    pst_Footer = as_ui_label(NULL, "انتظار البيانات", 4, 170, 312,
                             &AsCairo16, AS_COLOR_MUTED, false);
    for (size_t index = 0; index < 3; ++index) {
        apst_Pages[index] = as_ui_panel(NULL, 0, index == 0 ? 48 : 32,
                                       320, index == 0 ? 190 : 138, false);
        if (apst_Pages[index] == NULL) {
            lvgl_port_unlock();
            return setup_error;
        }
        lv_obj_set_style_bg_opa(apst_Pages[index], LV_OPA_TRANSP, 0);
    }
    const lv_image_dsc_t* apst_Icons[] = {&AS_ICON_TEMPERATURE, &AS_ICON_HUMIDITY, &AS_ICON_LIGHT};
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        as_panel_config_t st_Card = as_panel_default_config();
        st_Card.layout = (as_widget_layout_t){.parent = apst_Pages[0], .align = LV_ALIGN_TOP_LEFT,
            .x = 7 + (int32_t)index * 104, .y = 18, .width = 98, .height = 116};
        st_Card.style.background_color = 0x0B1A27;
        st_Card.style.border_color = 0x17628C;
        st_Card.style.border_width = 1;
        st_Card.style.padding = 0;
        st_Card.style.radius = 5;
        st_Card.on_click = as_ui_card_clicked;
        st_Card.user_data = (void*)(uintptr_t)index;
        as_widget_t* pst_Card = NULL;
        as_ui_error(as_display_add_panel(&st_Card, &pst_Card));
        if (pst_Card == NULL) {
            lvgl_port_unlock();
            return setup_error;
        }
        lv_obj_set_style_bg_grad_color(pst_Card, lv_color_hex(0x04121C), 0);
        lv_obj_set_style_bg_grad_dir(pst_Card, LV_GRAD_DIR_VER, 0);
        as_widget_t* pst_CardHeader = as_ui_label(pst_Card, apcMetricNames[index], 3, 5, 90,
                                                 &AsCairo12, AS_COLOR_TEXT, false);
        if (pst_CardHeader != NULL) {
            lv_obj_set_height(pst_CardHeader, 23);
            lv_obj_set_style_bg_color(pst_CardHeader, lv_color_hex(0x0B2537), 0);
            lv_obj_set_style_bg_opa(pst_CardHeader, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(pst_CardHeader, lv_color_hex(0x143D55), 0);
            lv_obj_set_style_border_width(pst_CardHeader, 1, 0);
            lv_obj_set_style_radius(pst_CardHeader, 4, 0);
            lv_obj_set_style_pad_top(pst_CardHeader, 2, 0);
        }
        apst_SensorIcons[index] = as_ui_image(pst_Card, apst_Icons[index], 2, 26, 31, 55);
        apst_Values[index] = as_ui_label(pst_Card, "--", 32, 34, 61,
                                        &AsCairo28, 0xF5FAFF, true);
        as_ui_label(pst_Card, apcUnits[index], 32, 68, 61, &AsCairo16, AS_COLOR_CYAN, true);
        apst_Status[index] = as_ui_label(pst_Card, "غير متاح", 3, 90, 90,
                                        &AsCairo12, AS_COLOR_MUTED, false);
        if (apst_Status[index] != NULL) {
            lv_obj_set_height(apst_Status[index], 20);
            lv_obj_set_style_bg_opa(apst_Status[index], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(apst_Status[index], lv_color_hex(0x0A1E2B), 0);
            lv_obj_set_style_border_color(apst_Status[index], lv_color_hex(0x244B63), 0);
            lv_obj_set_style_border_width(apst_Status[index], 1, 0);
            lv_obj_set_style_radius(apst_Status[index], 4, 0);
            lv_obj_set_style_pad_left(apst_Status[index], 18, 0);
            lv_obj_set_style_pad_top(apst_Status[index], 1, 0);
        }
        apst_StatusIcons[index] = as_ui_image(pst_Card, &AS_ICON_WAITING, 6, 93, 14, 14);
    }
    as_widget_t* pst_StatusBar = as_ui_panel(apst_Pages[0], 7, 139, 306, 36, true);
    if (pst_StatusBar == NULL) {
        lvgl_port_unlock();
        return setup_error;
    }
    lv_obj_set_style_bg_color(pst_StatusBar, lv_color_hex(0x071925), 0);
    lv_obj_set_style_border_color(pst_StatusBar, lv_color_hex(0x185276), 0);
    lv_obj_set_style_radius(pst_StatusBar, 5, 0);
    as_ui_image(pst_StatusBar, &AS_ICON_CHIP, 6, 9, 18, 18);
    as_ui_label(pst_StatusBar, "ESP-IDF", 25, 9, 44, &AsCairo12, AS_COLOR_CYAN, true);
    as_widget_t* pst_Divider = as_ui_panel(pst_StatusBar, 75, 7, 1, 21, false);
    if (pst_Divider != NULL) {
        lv_obj_set_style_bg_color(pst_Divider, lv_color_hex(0x175779), 0);
    }
    pst_Overview = as_ui_label(pst_StatusBar, "انتظار القراءات", 80, 9, 109,
                                &AsCairo12, 0xBCD9E8, false);
    pst_OverviewIcon = as_ui_image(pst_StatusBar, &AS_ICON_WAITING, 191, 11, 13, 13);
    pst_Divider = as_ui_panel(pst_StatusBar, 209, 7, 1, 21, false);
    if (pst_Divider != NULL) {
        lv_obj_set_style_bg_color(pst_Divider, lv_color_hex(0x175779), 0);
    }
    as_ui_image(pst_StatusBar, &AS_ICON_CLOCK, 216, 10, 16, 16);
    pst_UpdateAge = as_ui_label(pst_StatusBar, "آخر تحديث\n--", 234, 0, 65,
                                 &AsCairo12, 0xBCD9E8, false);
    for (size_t index = 0; index < AS_METRIC_COUNT; ++index) {
        as_ui_button(apst_Pages[1], index == AS_METRIC_LIGHT ? "الإضاءة" : apcMetricNames[index],
                      216 - (int32_t)index * 104, 0, 96, 27, as_ui_metric, (void*)(uintptr_t)index);
    }
    AsDisplayChartConfig st_Chart = as_display_chart_default_config();
    st_Chart.st_Layout = (as_widget_layout_t){.parent = apst_Pages[1], .align = LV_ALIGN_TOP_LEFT,
        .x = 8, .y = 30, .width = 304, .height = 58};
    as_ui_error(as_display_add_chart(&st_Chart, &pst_Chart));
    pst_ChartScale = as_ui_label(apst_Pages[1], "°C", 4, 89, 108,
                                 &AsCairo16, AS_COLOR_CYAN, true);
    pst_ChartCaption = as_ui_label(apst_Pages[1], "آخر 10 دقائق", 116, 89, 200,
                                   &AsCairo16, AS_COLOR_MUTED, false);
    pst_ArchiveRange = as_ui_label(apst_Pages[1], "", 136, 89, 88,
                                   &AsCairo16, AS_COLOR_MUTED, true);
    pst_ArchiveUnit = as_ui_label(apst_Pages[1], "د", 116, 89, 20,
                                  &AsCairo16, AS_COLOR_MUTED, false);
    as_ui_error(as_display_widget_set_hidden(pst_ArchiveRange, true));
    as_ui_error(as_display_widget_set_hidden(pst_ArchiveUnit, true));
    pst_Previous = as_ui_button(apst_Pages[1], "أقدم", 216, 112, 96, 26, as_ui_previous, NULL);
    pst_Session = as_ui_button(apst_Pages[1], "الجلسة", 112, 112, 96, 26, as_ui_session, NULL);
    pst_Next = as_ui_button(apst_Pages[1], "أحدث", 8, 112, 96, 26, as_ui_next, NULL);
    char acConnection[112];
    snprintf(acConnection, sizeof(acConnection), "%s\n192.168.4.1", CONFIG_AS_ROOM_AP_SSID);
    pst_Connection = as_ui_label(apst_Pages[2], acConnection, 8, 0, 304,
                                 &AsCairo16, AS_COLOR_CYAN, true);
    pst_StorageLabel = as_ui_label(apst_Pages[2], "RAM / آخر عشر دقائق", 4, 47, 312,
                                   &AsCairo16, AS_COLOR_MUTED, false);
    if (as_room_snapshot(&st_Snapshot, NULL) != ESP_OK) {
        st_Snapshot.u8Brightness = 75;
    }
    char acBrightness[64];
    snprintf(acBrightness, sizeof(acBrightness), "سطوع الشاشة  %u%%", st_Snapshot.u8Brightness);
    pst_BrightnessLabel = as_ui_label(apst_Pages[2], acBrightness, 8, 74, 304,
                                      &AsCairo16, AS_COLOR_TEXT, false);
    as_slider_config_t st_Slider = as_slider_default_config();
    st_Slider.layout = (as_widget_layout_t){.parent = apst_Pages[2], .align = LV_ALIGN_TOP_LEFT,
        .x = 24, .y = 112, .width = 272, .height = 12};
    st_Slider.minimum = 1;
    st_Slider.value = st_Snapshot.u8Brightness;
    st_Slider.style.background_color = AS_COLOR_CARD;
    st_Slider.indicator_color = AS_COLOR_GOLD;
    st_Slider.on_change = as_ui_brightness;
    as_widget_t* pst_Slider = NULL;
    as_ui_error(as_display_add_slider(&st_Slider, &pst_Slider));
    if (pst_Slider != NULL && lv_obj_add_event_cb(pst_Slider, as_ui_brightness_released,
                                                 LV_EVENT_RELEASED, NULL) == NULL) {
        as_ui_error(ESP_ERR_NO_MEM);
    }
    const char* apcPages[] = {"الرئيسية", "المخطط", "الإعدادات"};
    for (size_t index = 0; index < 3; ++index) {
        apst_Navigation[index] = as_ui_button(NULL, apcPages[index], 216 - (int32_t)index * 104,
            198, 96, 40, as_ui_navigate, (void*)(uintptr_t)index);
    }
    if (setup_error == ESP_OK) {
        as_ui_navigate(NULL, (void*)(uintptr_t)AS_PAGE_DASHBOARD);
        pst_UiTimer = lv_timer_create(as_ui_refresh, 250, NULL);
        if (pst_UiTimer == NULL) {
            as_ui_error(ESP_ERR_NO_MEM);
        }
    }
    lvgl_port_unlock();
    return setup_error;
}
esp_err_t as_room_ui_stop(void) {
    if (!lvgl_port_lock(1000)) {
        ESP_LOGE("AsRoomUi", "Cannot stop UI timer: lock timeout");
        return ESP_ERR_TIMEOUT;
    }
    if (pst_UiTimer != NULL) {
        lv_timer_delete(pst_UiTimer);
        pst_UiTimer = NULL;
    }
    lvgl_port_unlock();
    return ESP_OK;
}
