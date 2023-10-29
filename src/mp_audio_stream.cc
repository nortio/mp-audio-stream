// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include "dart-sdk-include/dart_native_api.h"
#include "opus_defines.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <ratio>
#include <sstream>
#include <string>
#include <thread>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "../3rdparty/opus-1.4/include/opus.h"
#include "dart-sdk-include/dart_api.h"
#include "dart-sdk-include/dart_api_dl.h"
#include "mp_audio_stream.h"
#include "user_buffer.h"
#include "utils.h"
#include <atomic>
#include <iostream>
#include <unordered_map>

#define DEVICE_FORMAT ma_format_f32

static char device_name[256] = "";
static ma_device *device = NULL;
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
UserBuffer micBuffer;
std::unordered_map<int, Buffer> active_speakers;
Dart_Port data_callback_dart_port;
uint8_t example_data[3] = {0x01, 0x02, 0x03};
float *mic_ready_packet_buffer = nullptr;
std::thread encoding_thread;

void notify_dart(Dart_Port port, int64_t number) {
  const bool res = Dart_PostInteger_DL(port, number);
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

/* void ready() {
  std::cout << "Initializing notification thread" << std::endl;
  while(notify_loop.load()) {
    if (!notified.load() && is_mic_ready(mic_number_of_samples_per_packet)) {
      notify_dart(data_callback_dart_port, data_callback_counter);
      notified = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  std::cout << "Destroying notify loop thread" << std::endl;
} */

float input_array[960];

std::atomic<bool> ready{false};
std::atomic<bool> continue_running{true};

void encode_thread_func() {
  using namespace std::literals;
  Stopwatch data_callback_stopwatch;

  auto end = std::chrono::steady_clock::now();
  auto start = std::chrono::steady_clock::now();
  std::chrono::duration<int64_t, std::nano> need_to_wait{0};
  LOG("Initializing thread");

  while (continue_running) {

    std::this_thread::sleep_for(need_to_wait);
    //LOG("TIME FROM LAST CALLBACK: %f", data_callback_stopwatch.elapsed());

    start = std::chrono::steady_clock::now();

    memset(input_array, 0, sizeof(input_array));

    if (input_buffer.is_ready(mic_number_of_samples_per_packet)) {
      input_buffer.consume(input_array, mic_number_of_samples_per_packet);
    }


    int encoded_bytes = opus_encode_float(opus_encoder, input_array,
                                          mic_number_of_samples_per_packet,
                                          encoded_packet, max_data_bytes_opus);
    if (encoded_bytes < 0) {
      LOG("ERROR ENCODING OPUS PACKET: %s", opus_strerror(encoded_bytes));
    }

#ifndef SELFTEST
    notify_dart_encoded(data_callback_dart_port, encoded_packet, encoded_bytes);
#endif

    end = std::chrono::steady_clock::now();

    std::chrono::duration<int64_t, std::nano> duration = end - start;
    // LOG("Encoding + notification duration: %f", duration.count());

    need_to_wait = 20000000ns - duration;

    data_callback_stopwatch.start();
  }
}

Stopwatch st;

void data_callback(ma_device *device, void *p_output, const void *p_input,
                   ma_uint32 frame_count) {

  // LOG("t: %f", st.elapsed())
  float *output = (float *)p_output;
  float *input = (float *)p_input;

  data_callback_counter++;

#ifdef PARLO_DEBUG
  // print_info(active_speakers, data_callback_counter, device_name);
#endif

  input_buffer.push(input, frame_count);
  ready = true;

  for (auto &speaker : active_speakers) {
    auto &userBuffer = speaker.second;
    userBuffer.consume(output, frame_count);
  }

  st.start();
}

float *get_mic_data(int length) {
  // float *out = (float *)calloc(length, sizeof(float));
  memset(mic_ready_packet_buffer, 0,
         mic_number_of_samples_per_packet * sizeof(float));
  consume_from_buffer(mic_ready_packet_buffer, &micBuffer, length);

  // notified = false;
  return mic_ready_packet_buffer;
};

bool is_mic_ready(uint32_t length) {
  return (micBuffer.buf_end - micBuffer.buf_start) >= length;
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


int push_opus(uint8_t* data, int length, int user_id) {
  int n_decoded = opus_decode_float(opus_decoder, data, length, decoded, 5760, 0);
  if(n_decoded < 0) {
    fprintf(stderr, "decoder failed: %s\n", opus_strerror(n_decoded));
    return -1;
  } else {
    ma_stream_push(decoded, n_decoded, user_id);
    return 0;
  }
}

void ma_stream_uninit() {
  LOG("Uninitializing audio module");

  ma_device_uninit(device);

  if (mic_ready_packet_buffer) {
    free(mic_ready_packet_buffer);
  }

  /*   for (auto &buffer : active_speakers) {
      std::cout << "buffer "<< buffer.first << std::endl;
      auto buf = buffer.second.buffer;
      if (buf) {
        std::cout << "removing buffer " << buffer.first << std::endl;
        free(buf);
      }
    } */

  active_speakers.clear();
  if (micBuffer.buffer) {
    free(micBuffer.buffer);
  }

  // notify_loop = false;
  continue_running = false;
  ready = true;
  encoding_thread.join();

  opus_decoder_destroy(opus_decoder);
  opus_encoder_destroy(opus_encoder);
}

intptr_t init_dart_api_dl(void *data) { return Dart_InitializeApiDL(data); }

void init_port(Dart_Port port) { data_callback_dart_port = port; }

int ma_stream_init(int max_buffer_size_p, int keep_buffer_size_p,
                   int channels_p, int sample_rate_p) {

  // OPUS
  int err;
  opus_encoder =
      opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
  if (err) {
    std::cerr << "failed to create opus encoder: " << opus_strerror(err)
              << std::endl;
    return -1;
  }

  opus_decoder = opus_decoder_create(sample_rate, channels, &err);
  if (err) {
    std::cerr << "failed to create opus encoder: " << opus_strerror(err)
              << std::endl;
    return -1;
  }

  err = opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(40000));
  if (err) {
    std::cerr << "failed to set bitrate: " << opus_strerror(err) << std::endl;
    return -1;
  }
  err = opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  if (err) {
    std::cerr << "failed to set bitrate: " << opus_strerror(err) << std::endl;
    return -1;
  }

  LOG("Opus initialized correctly");

  channels = channels_p;
  sample_rate = sample_rate_p;
  mic_number_of_samples_per_packet = 0.02 * sample_rate;

  device = (ma_device *)calloc(1, sizeof(ma_device));

  auto state = ma_atomic_device_state_get(&(device->state));
  if (state != ma_device_state_uninitialized) {
    ma_device_uninit(device);
  }

  ma_device_config device_config;

  device_config = ma_device_config_init(ma_device_type_duplex);
  device_config.playback.format = DEVICE_FORMAT;
  device_config.playback.channels = channels;
  device_config.capture.format = DEVICE_FORMAT;
  device_config.capture.channels = channels;
  device_config.sampleRate = sample_rate;
  device_config.dataCallback = data_callback;

  device_config.periodSizeInFrames = 960; // 20ms
  // device_config.periodSizeInMilliseconds = 20;
  // deviceConfig.periods = 1;
  device_config.performanceProfile = ma_performance_profile_low_latency;

  if (ma_device_init(NULL, &device_config, device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    return -4;
  }

  strcpy(device_name, device->playback.name);

  max_buffer_size = max_buffer_size_p;
  wait_buffer_size = keep_buffer_size_p;

  micBuffer.id = 0;
  micBuffer.buffer_size = max_buffer_size;
  micBuffer.minimum_size_to_recover = wait_buffer_size;
  micBuffer.buffer = (float *)calloc(max_buffer_size, sizeof(float));

  if (ma_device_start(device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(device);
    return -5;
  }

  active_speakers = {};

  mic_ready_packet_buffer =
      (float *)calloc(mic_number_of_samples_per_packet, sizeof(float));

  LOG("Native audio module initialized");

  /*   notify_loop = true;*/
  encoding_thread = std::thread(encode_thread_func);

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

// TODO: reimplement these
/* ma_uint32 ma_stream_stat_exhaust_count() {
  return globalUserBuffer->exhaust_count;
}

ma_uint32 ma_stream_stat_full_count() { return globalUserBuffer->full_count; }

void ma_stream_stat_reset() {
  globalUserBuffer->full_count = 0;
  globalUserBuffer->exhaust_count = 0;
} */
