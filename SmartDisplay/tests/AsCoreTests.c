/**
 * @file AsCoreTests.c
 * @brief Host tests for the actual decoder, state/history and CSV implementation.
 * @author AlSayad
 * @date 2026-10-25
 * @version 1.0.0
 * @copyright Copyright (c) 2026 AlSayad. All rights reserved.
 */
#include "AsMeasurements.h"
#include "AsMeasurementJson.h"
#include "AsHistoryFiles.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
void as_test_transport(const char* pcValidFrame);
static void* as_test_allocation_failure(size_t size) {
    (void)size;
    return NULL;
}
static const char acValid[] = "{\"version\":1,\"device_id\":\"measurement-01\",\"readings\":{"
    "\"temperature_c\":{\"value\":0,\"status\":\"valid\"},"
    "\"humidity_rh\":{\"value\":50,\"status\":\"valid\"},"
    "\"brightness_pct\":{\"value\":0,\"status\":\"valid\"}}}";
static void as_test_decoder(void) {
    AsMeasurement st_Output = {0};
    assert(as_measurement_decode(acValid, strlen(acValid), "measurement-01", &st_Output) == ESP_OK);
    assert(st_Output.ast_Readings[0].f32Value == 0 && st_Output.ast_Readings[0].eStatus == AS_READING_VALID);
    AsMeasurement st_Before = st_Output;
    assert(as_measurement_decode(acValid, strlen(acValid), "other", &st_Output) != ESP_OK);
    assert(memcmp(&st_Before, &st_Output, sizeof(st_Output)) == 0);
    char acFrame[1200];
    snprintf(acFrame, sizeof(acFrame), "%s garbage", acValid);
    assert(as_measurement_decode(acFrame, strlen(acFrame), "measurement-01", &st_Output) != ESP_OK);
    snprintf(acFrame, sizeof(acFrame), "%s \r\n", acValid);
    assert(as_measurement_decode(acFrame, strlen(acFrame), "measurement-01", &st_Output) == ESP_OK);
    snprintf(acFrame, sizeof(acFrame), "%s", acValid);
    char* pcVersion = strstr(acFrame, "\"version\":1");
    pcVersion[10] = '2';
    assert(as_measurement_decode(acFrame, strlen(acFrame), "measurement-01", &st_Output) != ESP_OK);
    assert(as_measurement_decode("{}", 2, "measurement-01", &st_Output) != ESP_OK);
    assert(as_measurement_decode(acValid, 1025, "measurement-01", &st_Output) != ESP_OK);
    const char* pcError = "{\"version\":1,\"device_id\":\"measurement-01\",\"readings\":{"
        "\"temperature_c\":{\"value\":null,\"status\":\"error\"},"
        "\"humidity_rh\":{\"value\":50,\"status\":\"valid\"},"
        "\"brightness_pct\":{\"value\":null,\"status\":\"unavailable\"}}}";
    assert(as_measurement_decode(pcError, strlen(pcError), "measurement-01", &st_Output) == ESP_OK);
    assert(st_Output.ast_Readings[0].eStatus == AS_READING_ERROR);
    assert(st_Output.ast_Readings[1].eStatus == AS_READING_VALID);
    assert(st_Output.ast_Readings[2].eStatus == AS_READING_UNAVAILABLE);
    for (size_t length = 0; length < strlen(acValid); ++length) {
        assert(as_measurement_decode(acValid, length, "measurement-01", &st_Output) != ESP_OK);
    }
    const char* pcDuplicate = "{\"version\":1,\"version\":1,\"device_id\":\"measurement-01\",\"readings\":{}}";
    assert(as_measurement_decode(pcDuplicate, strlen(pcDuplicate), "measurement-01", &st_Output) != ESP_OK);
    const char* pcNul = "{\"version\":1,\"device_id\":\"measurement-01\\u0000evil\",\"readings\":{}}";
    assert(as_measurement_decode(pcNul, strlen(pcNul), "measurement-01", &st_Output) != ESP_OK);
    char acNested[81];
    memset(acNested, '[', 40);
    memset(acNested + 40, ']', 40);
    acNested[80] = '\0';
    assert(as_measurement_decode(acNested, 80, "measurement-01", &st_Output) != ESP_OK);
    cJSON_Hooks st_Hooks = {.malloc_fn = as_test_allocation_failure, .free_fn = free};
    cJSON_InitHooks(&st_Hooks);
    assert(as_measurement_decode(acValid, strlen(acValid), "measurement-01", &st_Output) != ESP_OK);
    cJSON_InitHooks(NULL);
}
static void as_test_store(void) {
    AsMeasurementStore st_Store;
    as_measurements_init(&st_Store, 100);
    AsMeasurement st_Measurement;
    uint64_t u64Age = 0;
    as_measurements_snapshot(&st_Store, 100, &st_Measurement, &u64Age);
    assert(u64Age == UINT64_MAX && st_Measurement.ast_Readings[0].eStatus == AS_READING_UNAVAILABLE);
    assert(as_measurement_decode(acValid, strlen(acValid), "measurement-01", &st_Measurement) == ESP_OK);
    st_Measurement.bSimulated = true;
    assert(as_measurements_accept(&st_Store, &st_Measurement, 200));
    assert(!as_measurements_accept(&st_Store, &st_Measurement, 199));
    as_measurements_snapshot(&st_Store, 10199, &st_Measurement, &u64Age);
    assert(st_Measurement.ast_Readings[0].eStatus == AS_READING_VALID);
    as_measurements_snapshot(&st_Store, 10200, &st_Measurement, &u64Age);
    assert(st_Measurement.ast_Readings[0].eStatus == AS_READING_STALE);
    AsHistoryRecord st_Closed;
    assert(!as_measurements_tick(&st_Store, 5099, &st_Closed));
    assert(as_measurements_tick(&st_Store, 5100, &st_Closed));
    assert(st_Closed.ast_Readings[0].eStatus == AS_READING_VALID && st_Closed.u64ElapsedMs == 0);
    assert(st_Closed.bSimulated);
    assert(as_measurements_tick(&st_Store, 10100, &st_Closed));
    assert(st_Closed.ast_Readings[0].eStatus == AS_READING_UNAVAILABLE);
    for (uint64_t u64Now = 15100; u64Now <= 650100; u64Now += 5000) {
        assert(as_measurements_tick(&st_Store, u64Now, &st_Closed));
    }
    AsHistoryRecord ast_Output[AS_HISTORY_COUNT];
    assert(as_measurements_history(&st_Store, ast_Output, AS_HISTORY_COUNT) == AS_HISTORY_COUNT);
    assert(ast_Output[119].u64ElapsedMs - ast_Output[0].u64ElapsedMs == 595000);
    assert(!as_measurements_accept(&st_Store, &st_Measurement, 200));
    st_Measurement.ast_Readings[0] = (AsReading){.f32Value = 23, .eStatus = AS_READING_VALID};
    st_Measurement.ast_Readings[1] = (AsReading){.eStatus = AS_READING_ERROR};
    st_Measurement.ast_Readings[2] = (AsReading){.f32Value = 0, .eStatus = AS_READING_VALID};
    assert(as_measurements_accept(&st_Store, &st_Measurement, 650200));
    as_measurements_snapshot(&st_Store, 650201, &st_Measurement, &u64Age);
    assert(st_Measurement.ast_Readings[1].eStatus == AS_READING_ERROR);
    st_Measurement.ast_Readings[2].f32Value = 101;
    assert(!as_measurement_valid(&st_Measurement));
    st_Measurement.ast_Readings[2].f32Value = NAN;
    assert(!as_measurement_valid(&st_Measurement));
}
static void as_test_files(const char* pcRoot) {
    char acUnrelated[512];
    snprintf(acUnrelated, sizeof(acUnrelated), "%s/keep.txt", pcRoot);
    FILE* unrelated = fopen(acUnrelated, "w");
    assert(unrelated != NULL && fputs("preserve", unrelated) >= 0 && fclose(unrelated) == 0);
    AsHistoryFiles* pst_Files = as_history_files_open(pcRoot, 1600, 512);
    assert(pst_Files != NULL);
    uint64_t u64Session = as_history_files_session(pst_Files);
    for (size_t index = 0; index < 100; ++index) {
        AsHistoryRecord st_Record = {.u64ElapsedMs = index * 5000};
        st_Record.bSimulated = index >= 50;
        st_Record.ast_Readings[0] = (AsReading){.f32Value = 24, .eStatus = AS_READING_VALID};
        st_Record.ast_Readings[1].eStatus = AS_READING_ERROR;
        st_Record.ast_Readings[2] = (AsReading){.f32Value = 0, .eStatus = AS_READING_VALID};
        assert(as_history_files_append(pst_Files, &st_Record));
    }
    assert(as_history_files_sync(pst_Files));
    AsHistoryWindow st_Window;
    assert(as_history_files_window(pst_Files, u64Session, 0, &st_Window));
    assert(st_Window.bFound);
    assert(st_Window.ast_Records[99].ast_Readings[1].eStatus == AS_READING_ERROR);
    assert(st_Window.ast_Records[99].bSimulated);
    assert(st_Window.ast_Records[0].ast_Readings[0].eStatus == AS_READING_UNAVAILABLE); /* rotated out */
    AsHistorySession ast_Sessions[4];
    assert(as_history_files_sessions(pst_Files, ast_Sessions, 4) == 1);
    assert(ast_Sessions[0].u64EndMs == 500000);
    assert(as_history_files_close(pst_Files));
    char acTail[512];
    snprintf(acTail, sizeof(acTail), "%s/rm-%020llu-0000.csv", pcRoot,
             (unsigned long long)(u64Session + 1U));
    FILE* tail = fopen(acTail, "a");
    assert(tail != NULL && fputs("1,2,0,23,0,50,0,10,0", tail) >= 0 && fclose(tail) == 0);
    pst_Files = as_history_files_open(pcRoot, 1600, 512);
    assert(pst_Files != NULL);
    assert(as_history_files_window(pst_Files, u64Session + 1U, 0, &st_Window));
    assert(!st_Window.bFound); /* incomplete power-loss tail is not a saved sample */
    assert(as_history_files_close(pst_Files));
    pst_Files = as_history_files_open(pcRoot, 1600, 512);
    assert(pst_Files != NULL && as_history_files_session(pst_Files) == u64Session + 3U);
    assert(as_history_files_close(pst_Files));
    unrelated = fopen(acUnrelated, "r");
    assert(unrelated != NULL && fgetc(unrelated) == 'p' && fclose(unrelated) == 0);
    assert(as_history_files_open("Z:/nonexistent-room-monitor-test", 1600, 512) == NULL);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    as_test_decoder();
    as_test_transport(acValid);
    as_test_store();
    as_test_files(argv[1]);
    puts("PASS: decoder, state/stale/recovery, 120-bucket rollover/gaps, CSV rotation/sessions/preservation");
    return 0;
}
