// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>
#include <ratio>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
//#define MA_NO_PULSEAUDIO
#include "../3rdparty/opus-1.4/include/opus.h"
#include "miniaudio.h"
#include "mp_audio_stream.h"
#include "user_buffer.h"
#include "utils.hpp"
#include "values.hpp"

#define DEVICE_FORMAT ma_format_f32

static char device_name[256] = "";
static ma_device device_playback;
static ma_device device_capture;
static ma_context context;
unsigned long long data_callback_counter = 0;
static int sample_rate = 48000;
static int channels = 1;
static int max_buffer_size = (3000 * sample_rate) / 1000;
static int wait_buffer_size = (50 * sample_rate) / 1000;
static int mic_number_of_samples_per_packet = 0.02 * sample_rate;
OpusEncoder *opus_encoder;
OpusDecoder *opus_decoder;

#define max_data_bytes_opus 1275 * 3
uint8_t encoded_packet[max_data_bytes_opus];
Buffer input_buffer(-1, mic_number_of_samples_per_packet * 10, 0);
std::unordered_map<int, Buffer> active_speakers;

std::thread encoding_thread;

#ifndef SELFTEST
#include "dart-sdk-include/dart_api.h"
#include "dart-sdk-include/dart_api_dl.h"

Dart_Port data_callback_dart_port;
Dart_Port level_dart_port;

void notify_dart(Dart_Port port, int64_t number) {
    const bool res = Dart_PostInteger_DL(port, number);
    if (!res) {
        LOG("Could not notify dart (integer)");
    }
}

void notify_dart_encoded(Dart_Port port, uint8_t *data, uint32_t len) {
    Dart_CObject request;
    request.type = Dart_CObject_kTypedData;
    request.value.as_typed_data.type = Dart_TypedData_kUint8;
    request.value.as_typed_data.length = len;
    request.value.as_typed_data.values = data;

    const bool res = Dart_PostCObject_DL(port, &request);
    if (!res) {
        LOG("Could not post encoded data to dart isolate");
    }
}

void notify_dart_mic_level(double level) {
    Dart_CObject request;
    request.type = Dart_CObject_kDouble;
    request.value.as_double = level;

    const bool res = Dart_PostCObject_DL(level_dart_port, &request);
    if (!res) {
        LOG("Could not post mic level to dart isolate");
    }
}

intptr_t init_dart_api_dl(void *data) { return Dart_InitializeApiDL(data); }

void init_port(Dart_Port port, Dart_Port l_port) {
    data_callback_dart_port = port;
    level_dart_port = l_port;
}
#endif

float input_array[960];

volatile static float treshold = 0.65f;
volatile bool is_transmitting = false;

void set_threshold(double t) {
    LOG("VAD threshold set to %f", t);
    treshold = t;
}

Stopwatch st;

void data_callback(ma_device *device, void *p_output, const void *p_input,
                   ma_uint32 frame_count) {

    // LOG("t: %f", st.elapsed())
    float *output = (float *)p_output;

    data_callback_counter++;

#ifdef PARLO_DEBUG
    print_info(active_speakers, data_callback_counter, device_name);
#endif

    for (auto &[_, buffer] : active_speakers) {
        buffer.consume(output, frame_count);
    }

    // st.start();
}

auto last_activation = std::chrono::steady_clock::now();

void data_callback_capture(ma_device *device, void *p_output,
                           const void *p_input, ma_uint32 frame_count) {
    float *input = (float *)p_input;
#ifdef PARLO_MIC_DEBUG
    print_mic_level(data_callback_counter, input, frame_count, is_transmitting);
#endif
    int encoded_bytes = 0;
    if (frame_count == mic_number_of_samples_per_packet) {
        //memcpy(input_array, p_input, frame_count*sizeof(float));
        auto level =
            calculate_mic_level(input, mic_number_of_samples_per_packet);
#ifndef SELFTEST
        notify_dart_mic_level(level);
#endif
        bool is_over_treshold = level > treshold;
        if (is_over_treshold || is_transmitting) {
            if (!is_over_treshold) {
                if (std::chrono::steady_clock::now() - last_activation >
                    Duration::ms500) {
                    is_transmitting = false;
                    return;
                }
            } else {
                is_transmitting = true;
                last_activation = std::chrono::steady_clock::now();
            }

            encoded_bytes = opus_encode_float(
                opus_encoder, input, mic_number_of_samples_per_packet,
                encoded_packet, max_data_bytes_opus);
            if (encoded_bytes < 0) {
                LOG("ERROR ENCODING OPUS PACKET: %s",
                    opus_strerror(encoded_bytes));
            }

#ifndef SELFTEST
            notify_dart_encoded(data_callback_dart_port, encoded_packet,
                                encoded_bytes);
#endif
            //push_opus(encoded_packet, encoded_bytes, 10);
        } else {
            is_transmitting = false;
        }
    } else {
        LOG("frame_count mismatch: got %d, expected %d", frame_count, mic_number_of_samples_per_packet);
    }

}

// TODO: function to remove users that have disconnected from channel

int ma_stream_push(float *buf, int length, int user_id) {
    Buffer *user_buffer;

    if (active_speakers.find(user_id) != active_speakers.end()) {
        Buffer &existingUser = active_speakers.at(user_id);
        user_buffer = &existingUser;
    } else {
        auto res = active_speakers.emplace(
            user_id, Buffer(user_id, max_buffer_size, wait_buffer_size));
        user_buffer = &res.first->second;
    }

    return user_buffer->push(buf, length);
}

static float decoded[5760];

int push_opus(uint8_t *data, int length, int user_id) {
    int n_decoded =
        opus_decode_float(opus_decoder, data, length, decoded, 5760, 0);
    if (n_decoded < 0) {
        ERROR("decoder failed: %s", opus_strerror(n_decoded));
        return -1;
    } else {
        ma_stream_push(decoded, n_decoded, user_id);
        return 0;
    }
}

void ma_stream_uninit() {
    LOG("Uninitializing audio module");

    ma_device_uninit(&device_playback);
    ma_device_uninit(&device_capture);
    ma_context_uninit(&context);

    LOG("Miniaudio unitialized correctly");

    active_speakers.clear();

    opus_decoder_destroy(opus_decoder);
    opus_encoder_destroy(opus_encoder);

    LOG("Destroyed opus encoder and decoder")
}

int ma_stream_init(int max_buffer_size_p, int keep_buffer_size_p,
                   int channels_p, int sample_rate_p) {

    // OPUS
    int err;
    opus_encoder =
        opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err) {
        ERROR("failed to create opus encoder: %s", opus_strerror(err));
        return -1;
    }

    opus_decoder = opus_decoder_create(sample_rate, channels, &err);
    if (err) {
        ERROR("failed to create opus encoder: %s", opus_strerror(err))
        return -1;
    }

    err = opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(40000));
    if (err) {
        ERROR("failed to set bitrate: %s", opus_strerror(err));
        return -1;
    }
    err = opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    if (err) {
        ERROR("failed to set bitrate: %s", opus_strerror(err));
        return -1;
    }

    LOG("Opus initialized correctly");

    channels = channels_p;
    sample_rate = sample_rate_p;
    mic_number_of_samples_per_packet = 0.02 * sample_rate;

    auto state = ma_device_get_state(&device_playback);
    if (state != ma_device_state_uninitialized) {
        ma_device_uninit(&device_playback);
    }

    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        ERROR("Error initializing miniaudio context");
        return -1;
    }

    ma_device_config device_config_playback;

    device_config_playback = ma_device_config_init(ma_device_type_playback);
    device_config_playback.playback.format = DEVICE_FORMAT;
    device_config_playback.playback.channels = channels;
    device_config_playback.sampleRate = sample_rate;
    device_config_playback.dataCallback = data_callback;
    device_config_playback.performanceProfile =
        ma_performance_profile_low_latency;

    if (ma_device_init(&context, &device_config_playback, &device_playback) !=
        MA_SUCCESS) {
        ERROR("Failed to init playback device.\n");
        return -4;
    }

    ma_device_config device_config_capture;

    device_config_capture = ma_device_config_init(ma_device_type_capture);
    device_config_capture.capture.format = DEVICE_FORMAT;
    device_config_capture.capture.channels = channels;
    device_config_capture.sampleRate = sample_rate;
    device_config_capture.dataCallback = data_callback_capture;
    device_config_capture.periodSizeInFrames = 960; // 20ms
    device_config_capture.performanceProfile =
        ma_performance_profile_low_latency;

    if (ma_device_init(&context, &device_config_capture, &device_capture) !=
        MA_SUCCESS) {
        ERROR("Failed to init capture device.\n");
        return -4;
    }

    strcpy(device_name, device_playback.playback.name);

    max_buffer_size = max_buffer_size_p;
    wait_buffer_size = keep_buffer_size_p;

    if (ma_device_start(&device_playback) != MA_SUCCESS) {
        ERROR("Failed to start playback device.\n");
        ma_device_uninit(&device_playback);
        return -5;
    }

    if (ma_device_start(&device_capture) != MA_SUCCESS) {
        ERROR("Failed to start capture device.\n");
        ma_device_uninit(&device_capture);
        return -5;
    }

    active_speakers = {};

    LOG("Native audio module initialized");

    return 0;
}

void parlo_remove_user(int userId) {
    /*   if (active_speakers.find(userId) != active_speakers.end()) {
        UserBuffer &existingUser = active_speakers.at(userId);
        if (existingUser.buffer != NULL) {
          free(existingUser.buffer);
        }
        int res = active_speakers.erase(userId);
      } */
}
