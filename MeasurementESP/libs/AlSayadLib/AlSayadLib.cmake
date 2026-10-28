set(ALSAYAD_LIB_ROOT "D:/GitHub/AlSayad/AlSayadLib/ESP32" CACHE PATH "Shared ESP32 library root")
foreach(as_module AsMeasurements AsMeasurementJson AsMeasurementSensors AsMeasurementSender
                  AsWifiStation AsMeasurementHttp TimeManager sensors/DHT11 sensors/Light)
    if(NOT EXISTS "${ALSAYAD_LIB_ROOT}/${as_module}/CMakeLists.txt")
        message(FATAL_ERROR "Missing shared ESP-IDF component: ${ALSAYAD_LIB_ROOT}/${as_module}")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS "${ALSAYAD_LIB_ROOT}/${as_module}")
endforeach()
