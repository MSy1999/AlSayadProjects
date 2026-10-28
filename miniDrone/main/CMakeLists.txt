idf_component_register(SRCS "main.c" "imu.c" "inputs.c" INCLUDE_DIRS "."
    REQUIRES AlSayadLib as_flight esp_driver_i2c esp_adc esp_timer esp_http_server esp_wifi
    EMBED_TXTFILES "controller.html")
