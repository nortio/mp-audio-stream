// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include <cstdint>
#ifdef __cplusplus
#ifdef WIN32
    #define EXPORT extern "C" __declspec(dllexport)
#else
    #define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif // WIN32
    #include <cstdio>
    #include <cstdlib>
    #include <cstring>
#else // __cplusplus - Objective-C or other C platform
    #define EXPORT extern
    #include "stdio.h"
    #include "stdlib.h"
    #include "string.h"
#endif

#include "miniaudio.h"
#include "dart-sdk-include/dart_api_dl.h"

EXPORT
int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate);

EXPORT
void ma_stream_uninit(void);

EXPORT
int ma_stream_push(float*, int, int);

EXPORT
float * get_mic_data(int);
/* EXPORT
ma_uint32 ma_stream_stat_exhaust_count(void); 

EXPORT
ma_uint32 ma_stream_stat_full_count(void);  */

EXPORT
void ma_stream_stat_reset(void);

EXPORT
void parlo_remove_user(int);

EXPORT
bool is_mic_ready(int);

EXPORT
intptr_t init_dart_api_dl(void* data);

EXPORT
void init_port(Dart_Port);