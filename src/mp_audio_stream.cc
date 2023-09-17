// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
//#include <android/log.h>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "mp_audio_stream.h"
#include "utils.h"
#include <iostream>
#include <unordered_map>
#include <atomic>

#include "dart-sdk-include/dart_api.h"
#include "dart-sdk-include/dart_api_dl.h"

#define DEVICE_FORMAT ma_format_f32

static char device_name[256] = "";
static ma_device *device = NULL;
unsigned long long data_callback_counter = 0;
static int sample_rate = 48000;
static int channels = 1;
static int max_buffer_size = (3000 * sample_rate) / 1000;
static int wait_buffer_size = (50 * sample_rate) / 1000;
static int mic_number_of_samples_per_packet = 0.02 * sample_rate;

UserBuffer micBuffer;
std::unordered_map<int, UserBuffer> activeSpeakers;
Dart_Port data_callback_dart_port;

float * mic_ready_packet_buffer = nullptr;
std::atomic<bool> notify_loop(true);
std::atomic<bool> notified(false);
std::thread notification_thread;

void notify_dart(Dart_Port port, int64_t number) {
  const bool res = Dart_PostInteger_DL(port, number);
}

void ready() {
  std::cout << "Initializing notification thread" << std::endl;
  while(notify_loop.load()) {
    if (!notified.load() && is_mic_ready(mic_number_of_samples_per_packet)) {
      notify_dart(data_callback_dart_port, data_callback_counter);
      notified = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  std::cout << "Destroying notify loop thread" << std::endl;
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frame_count) {
  float *output = (float *)pOutput;
  float *input = (float *)pInput;

  data_callback_counter++;

  //__android_log_print(ANDROID_LOG_DEBUG, "flutter", "%u - count: %lu", frame_count, data_callback_counter );

  //std::printf("count: %u\n",frame_count);
#ifdef PARLO_DEBUG
  print_info(activeSpeakers, data_callback_counter, device_name);
#endif

#ifndef SELFTEST

#endif

  buffer_push(&micBuffer, input, frame_count);

  if(is_mic_ready(mic_number_of_samples_per_packet)) {
    notify_dart(data_callback_dart_port, data_callback_counter);
  }

  for (auto &speaker : activeSpeakers) {
    auto &userBuffer = speaker.second;
    consume_from_buffer(output, &userBuffer, frame_count);
  }
  // consume_from_buffer(out, micBuffer, frame_count);
}

float *get_mic_data(int length) {
  //float *out = (float *)calloc(length, sizeof(float));
  memset(mic_ready_packet_buffer, 0, mic_number_of_samples_per_packet * sizeof(float));
  consume_from_buffer(mic_ready_packet_buffer, &micBuffer, length);

  //notified = false;
  return mic_ready_packet_buffer;
};

inline bool is_mic_ready(int length) {
  return (micBuffer.buf_end - micBuffer.buf_start) >= length;
}

// TODO: function to remove users that have disconnected from channel

int ma_stream_push(float *buf, int length, int userId) {
  UserBuffer *userBuffer;

  if (activeSpeakers.find(userId) != activeSpeakers.end()) {
    UserBuffer &existingUser = activeSpeakers.at(userId);
    userBuffer = &existingUser;
  } else {
    UserBuffer &newUser = activeSpeakers[userId];
    newUser.id = userId;
    newUser.buffer_size = max_buffer_size;
    newUser.minimum_size_to_recover = wait_buffer_size;
    newUser.buffer = (float *)calloc(max_buffer_size, sizeof(float));
    userBuffer = &newUser;
  }

  return buffer_push(userBuffer, buf, length);
}

void ma_stream_uninit() {
  ma_device_uninit(device);

  if(mic_ready_packet_buffer) {
    free(mic_ready_packet_buffer);
  }

  for (auto &buffer : activeSpeakers) {
    auto buf = buffer.second.buffer;
    if (buf != NULL) {
      free(buf);
    }
  }

  activeSpeakers.clear();
  if (micBuffer.buffer != NULL) {
    free(micBuffer.buffer);
  }

  //notify_loop = false;
  //notification_thread.join();
}

intptr_t init_dart_api_dl(void *data) { return Dart_InitializeApiDL(data); }

void init_port(Dart_Port port) { data_callback_dart_port = port; }

int ma_stream_init(int max_buffer_size_p, int keep_buffer_size_p,
                   int channels_p, int sample_rate_p) {
  
  channels = channels_p;
  sample_rate = sample_rate_p;
  mic_number_of_samples_per_packet = 0.02 * sample_rate;

  device = (ma_device *)calloc(1, sizeof(ma_device));

  auto state = ma_atomic_device_state_get(&(device->state));
  if (state != ma_device_state_uninitialized) {
    ma_device_uninit(device);
  }

  ma_device_config deviceConfig;

  deviceConfig = ma_device_config_init(ma_device_type_duplex);
  deviceConfig.playback.format = DEVICE_FORMAT;
  deviceConfig.playback.channels = channels;
  deviceConfig.capture.format = DEVICE_FORMAT;
  deviceConfig.capture.channels = channels;
  deviceConfig.sampleRate = sample_rate;
  deviceConfig.dataCallback = data_callback;
  
  deviceConfig.periodSizeInFrames = 960; // 20ms

  if (ma_device_init(NULL, &deviceConfig, device) != MA_SUCCESS) {
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

  activeSpeakers = {};

  mic_ready_packet_buffer = (float *)calloc(mic_number_of_samples_per_packet, sizeof(float));
  
/*   notify_loop = true;
  notification_thread = std::thread(ready); */
  
  return 0;
}

void parlo_remove_user(int userId) {
  if (activeSpeakers.find(userId) != activeSpeakers.end()) {
    UserBuffer &existingUser = activeSpeakers.at(userId);
    if (existingUser.buffer != NULL) {
      free(existingUser.buffer);
    }
    int res = activeSpeakers.erase(userId);
  }
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
