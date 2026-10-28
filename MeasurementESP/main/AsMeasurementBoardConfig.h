/**
 * @file AsMeasurementBoardConfig.h
 * @brief MeasurementESP application wiring.
 * @author As
 * @date 2026-10-28
 * @version 1.0.0
 */
#ifndef AS_MEASUREMENT_BOARD_CONFIG_H
#define AS_MEASUREMENT_BOARD_CONFIG_H
#include "AsMeasurementSensors.h"
/** Fill caller-owned array of AS_METRIC_COUNT callbacks for the documented DevKitC wiring.
 * Registrations borrow static board-owned contexts, accessed only by sampling task.
 */
void as_measurement_board_configure(AsMeasurementSensor* pst_Sensors);
/** Begin a fresh application snapshot. Invalidate paired DHT data before callbacks run. */
void as_measurement_board_begin_sample(void);
/** Startup/config validation: DHT needs >=2000 ms, calibrated endpoints must differ. */
bool as_measurement_board_config_valid(void);
/** Minimum boot settling time in milliseconds (zero in simulation or with DHT disabled). */
uint32_t as_measurement_board_startup_ms(void);
#endif
