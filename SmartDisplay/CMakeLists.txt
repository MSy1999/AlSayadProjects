# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include(${CMAKE_CURRENT_LIST_DIR}/libs/AlSayadLib/AlSayadLib.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SmartDisplay)

# LVGL 9.3 computes chart coordinates for INT32_MAX gap sentinels before
# skipping missing points. Define signed wrapping for that upstream arithmetic.
idf_component_get_property(as_lvgl_target lvgl__lvgl COMPONENT_LIB)
target_compile_options(${as_lvgl_target} PRIVATE -fwrapv)

# Print ESP-IDF's memory summary after every build, including incremental builds.
# Reuse the SDK target so chip-specific capacities and section accounting stay
# consistent with `idf.py size`.
add_dependencies(size app)
add_custom_target(memory_usage ALL DEPENDS size)
