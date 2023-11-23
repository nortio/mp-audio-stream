# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(mp_audio_stream_library VERSION 0.0.1 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mp_audio_stream SHARED
  "mp_audio_stream.cc"
  "user_buffer.cpp"
  "dart-sdk-include/dart_api_dl.c"
  "utils.cc"
)

if(ANDROID)
  find_library(
    log-lib
    log
  )

  target_link_libraries(mp_audio_stream PUBLIC ${log-lib})
endif()

#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")
if(LINUX)
add_compile_options(-O3 -Wall -Wextra -fPIC)
elseif(WIN32)
message("BUILD TYPE: ${CMAKE_BUILD_TYPE}")
add_compile_options($<$<NOT:$<CONFIG:Debug>>:/O2> /Wall)
endif()
#set(OPUS_BUILD_SHARED_LIBRARY ON CACHE BOOL "" FORCE)
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/opus")
add_subdirectory("../3rdparty/opus-1.4" "${CMAKE_CURRENT_BINARY_DIR}/opus" EXCLUDE_FROM_ALL)
target_link_libraries(mp_audio_stream PUBLIC opus)

set_target_properties(mp_audio_stream PROPERTIES
  PUBLIC_HEADER mp_audio_stream.h
  OUTPUT_NAME "mp_audio_stream"
)

target_compile_definitions(mp_audio_stream PUBLIC DART_SHARED_LIB)
