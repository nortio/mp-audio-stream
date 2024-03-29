// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#ifdef __cplusplus
    #ifdef WIN32
        #define EXPORT extern "C" __declspec(dllexport)
    #else
        #define EXPORT extern "C" __attribute__((visibility("default"))) __attribute__((used))
    #endif // WIN32
    #include <cstdint>
#else // __cplusplus - Objective-C or other C platform
    #define EXPORT extern
    #include <stdint.h>
    #include <stdbool.h>
#endif

EXPORT
int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels, int sample_rate);

EXPORT
void ma_stream_uninit(void);

EXPORT
int ma_stream_push(float*, int, int);

EXPORT
int push_opus(uint8_t*, int, int);

EXPORT
void ma_stream_stat_reset(void);

EXPORT
void parlo_remove_user(int);

#ifndef SELFTEST
EXPORT
intptr_t init_dart_api_dl(void* data);

EXPORT
void init_port(int64_t, int64_t);
#endif

EXPORT
void set_threshold(double);