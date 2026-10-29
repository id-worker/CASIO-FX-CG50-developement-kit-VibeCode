cmake_minimum_required(VERSION 3.15)
project(ObjView LANGUAGES C)

include(GenerateG3A)
include(Fxconv)
find_package(Gint 2.9 REQUIRED)

set(SOURCES
  src/main.c
)

add_executable(objview ${SOURCES})
target_compile_options(objview PRIVATE -Wall -Wextra -O3 -fomit-frame-pointer -fno-math-errno)
target_link_libraries(objview Gint::Gint -lm -lc)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
  generate_g3a(TARGET objview OUTPUT "${CMAKE_SOURCE_DIR}/ObjView.g3a"
    NAME "ObjView" VERSION "02.23.0000"
    ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
