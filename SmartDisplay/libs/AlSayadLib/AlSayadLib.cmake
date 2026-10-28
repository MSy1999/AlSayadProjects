set(ALSAYAD_LIB_ROOT "D:/GitHub/AlSayad/AlSayadLib/ESP32" CACHE PATH "Shared ESP32 library root")
foreach(as_module as_display AsMeasurements AsMeasurementJson AsTransport AsWifiTransport AsHistoryStorage AsSdHistory)
    list(APPEND EXTRA_COMPONENT_DIRS "${ALSAYAD_LIB_ROOT}/${as_module}")
endforeach()
