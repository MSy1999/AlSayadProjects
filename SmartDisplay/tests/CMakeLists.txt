cmake_minimum_required(VERSION 3.22)
project(AsRoomUiTests C)
set(CMAKE_C_STANDARD 11)
set(AS_LIBRARY_ROOT "D:/GitHub/AlSayad/AlSayadLib/ESP32" CACHE PATH "Shared libraries")
set(AS_LVGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../managed_components/lvgl__lvgl")
file(GLOB_RECURSE as_lvgl_sources "${AS_LVGL_ROOT}/src/*.c")
add_library(AsHostLvgl OBJECT ${as_lvgl_sources})
target_include_directories(AsHostLvgl PUBLIC stubs "${AS_LVGL_ROOT}")
target_compile_definitions(AsHostLvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
# LVGL 9.3 computes coordinates for INT32_MAX gap sentinels before skipping
# their lines. Define signed wrapping for this upstream arithmetic.
target_compile_options(AsHostLvgl PRIVATE -fwrapv)
add_executable(AsUiTests AsUiTests.c ../main/AsRoomUi.c
    "${AS_LIBRARY_ROOT}/as_display/src/as_display_widgets.c"
    "${AS_LIBRARY_ROOT}/as_display/src/AsDisplayBrand.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsBrandLogo.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsCairo16.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsCairo20.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsCairo28.c")
target_sources(AsUiTests PRIVATE
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsCairo12.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsCairo14.c"
    "${AS_LIBRARY_ROOT}/as_display/src/generated/AsDisplayIcons.c")
target_include_directories(AsUiTests PRIVATE ../main
    "${AS_LIBRARY_ROOT}/AsMeasurements" "${AS_LIBRARY_ROOT}/AsHistoryStorage"
    "${AS_LIBRARY_ROOT}/as_display/include")
target_compile_options(AsUiTests PRIVATE -include "${CMAKE_CURRENT_LIST_DIR}/stubs/AsDisplayHost.h")
target_link_libraries(AsUiTests PRIVATE AsHostLvgl)
