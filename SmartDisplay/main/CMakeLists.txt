idf_component_register(SRCS "main.c" "AsBoardDisplayConfig.c" "AsRoomUi.c"
    INCLUDE_DIRS "."
    REQUIRES as_display AsMeasurements AsHistoryStorage
    PRIV_REQUIRES AsMeasurementJson AsWifiTransport AsSdHistory nvs_flash esp_timer freertos log esp_system)
set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
get_target_property(as_sources ${COMPONENT_LIB} SOURCES)
set_property(SOURCE ${as_sources} APPEND PROPERTY COMPILE_OPTIONS "-std=gnu11")
