cmake_minimum_required(VERSION 3.0.0)
project(parlo_stream_test VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(parlo_stream_test main.cc ../src/mp_audio_stream.cc)

add_compile_definitions(PARLO_DEBUG)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
