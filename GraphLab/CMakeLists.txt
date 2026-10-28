cmake_minimum_required(VERSION 3.15)
project(GraphLab LANGUAGES C)
include(GenerateG3A)
find_package(Gint 2.11 REQUIRED)
set(SOURCES
  src/main.c src/input.c src/navigation.c src/navigation_input.c src/expr.c src/layout.c src/editor_meta.c src/plot.c src/plot2d.c src/ray.c src/voxel.c src/fractal.c src/storage.c
)
add_executable(graphlab ${SOURCES})
target_compile_features(graphlab PRIVATE c_std_11)
target_compile_options(graphlab PRIVATE -Wall -Wextra -Os)
target_link_libraries(graphlab Gint::Gint)
target_link_options(graphlab PRIVATE "-Wl,-Map,graphlab.map")
if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
  generate_g3a(TARGET graphlab OUTPUT "${CMAKE_SOURCE_DIR}/GraphLab.g3a"
    NAME "GraphLab" VERSION "01.06.0000" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
