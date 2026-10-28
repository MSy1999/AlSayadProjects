/** @file esp_log.h @brief Host logging shim. @author As @date 2026-10-28 @version 1.0 */
#ifndef AS_TEST_ESP_LOG_H
#define AS_TEST_ESP_LOG_H
static inline void as_test_log(const char* tag, const char* format, ...) {
    (void)tag;
    (void)format;
}
#define ESP_LOGI(...) as_test_log(__VA_ARGS__)
#define ESP_LOGE(...) as_test_log(__VA_ARGS__)
#define ESP_LOGW(...) as_test_log(__VA_ARGS__)
#endif
