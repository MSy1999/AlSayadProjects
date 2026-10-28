/**
 * @file AsRoomRuntime.h
 * @brief Room Monitor application composition.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_ROOM_RUNTIME_H
#define AS_ROOM_RUNTIME_H
#include "AsHistoryStorage.h"
#include "esp_err.h"
typedef struct {
    AsMeasurement st_Measurement;
    uint64_t u64AgeMs;
    bool bConnected;
    bool bTransportReady;
    uint8_t u8Brightness;
    AsStorageInfo st_Storage;
    size_t history_count;
} AsRoomSnapshot;
/* Data/UI boundary; no networking or filesystem types. */
esp_err_t as_room_submit(const AsMeasurement* pst_Measurement);
esp_err_t as_room_snapshot(AsRoomSnapshot* pst_Output, AsHistoryRecord* pst_History);
esp_err_t as_room_save_brightness(uint8_t u8Percent);
bool as_room_history_request(uint64_t u64Session, uint64_t u64StartMs, uint32_t u32Request);
void as_room_history_result(AsHistoryWindow* pst_Output);
#endif
