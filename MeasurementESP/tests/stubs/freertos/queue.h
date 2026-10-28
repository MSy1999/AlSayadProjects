/** @file queue.h @brief Host sensor SDK shim. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_QUEUE_H
#define AS_TEST_QUEUE_H
#include "FreeRTOS.h"
#include <stddef.h>
typedef struct AsFakeQueue* QueueHandle_t;
QueueHandle_t xQueueCreate(size_t count, size_t size);
BaseType_t xQueueOverwriteFromISR(QueueHandle_t queue, const void* item, BaseType_t* woken);
BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t ticks);
BaseType_t xQueueReset(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);
#endif
