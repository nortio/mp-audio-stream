# SPDX-License-Identifier: MIT
# Copyright (c) 2023 nortio

cmake_minimum_required(VERSION 3.0.0)
project(parlo_stream_test VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

include(CTest)
enable_testing()

add_executable(parlo_stream_test main.cc ../src/mp_audio_stream.cc ../src/user_buffer.cpp ../src/utils.cc ../src/dart-sdk-include/dart_api_dl.c)
add_executable(parlo_mic_test mic.cc ../src/utils.cc)

add_subdirectory("../3rdparty/rnnoise" "${CMAKE_CURRENT_BINARY_DIR}/rnnoise" EXCLUDE_FROM_ALL)
target_link_libraries(parlo_mic_test PRIVATE rnnoise)

add_compile_options(-O3 -Wall -Wextra)
set(OPUS_BUILD_SHARED_LIBRARY ON CACHE BOOL "" FORCE)
add_subdirectory("../3rdparty/opus-1.4" "${CMAKE_CURRENT_BINARY_DIR}/opus" EXCLUDE_FROM_ALL)
target_link_libraries(parlo_stream_test PRIVATE opus)


if(WIN32)
# Shared library on Windows (e.g. ".dll")
set_target_properties(rnnoise PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
else()
# Shared library on UNIX (e.g. ".so")
set_target_properties(rnnoise PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_compile_definitions(PARLO_DEBUG)
add_compile_definitions(SELFTEST)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
