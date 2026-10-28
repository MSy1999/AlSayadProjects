idf_component_register(SRCS "main.c" "AsMeasurementBoardConfig.c" INCLUDE_DIRS "."
    REQUIRES AsMeasurementSensors AsMeasurementSender AsWifiStation AsMeasurementHttp
             AsDht11 AsAnalogLight TimeManager nvs_flash esp_system log freertos)
set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
get_target_property(as_sources ${COMPONENT_LIB} SOURCES)
set_property(SOURCE ${as_sources} APPEND PROPERTY COMPILE_OPTIONS "-std=gnu11")
