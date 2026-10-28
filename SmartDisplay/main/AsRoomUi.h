/**
 * @file AsRoomUi.h
 * @brief Room Monitor application composition.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#ifndef AS_ROOM_UI_H
#define AS_ROOM_UI_H
#include "esp_err.h"
esp_err_t as_room_ui_start(void);
/* Stop the application timer before display teardown. */
esp_err_t as_room_ui_stop(void);
#endif
