#pragma once
#include "user_buffer.h"
#define NOMINMAX
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdint.h>
#include <unordered_map>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG__INT(fmt, ...)                                                     \
  __android_log_print(ANDROID_LOG_INFO, "native_audio", fmt "%s", __VA_ARGS__);
#define LOG(...) LOG__INT(__VA_ARGS__, "\n")
#elif __linux__
#define LOG__INT(fmt, ...)                                                     \
  std::printf("\x1b[90m[native_audio - INFO] \x1b[0m" fmt "%s", __VA_ARGS__);
#define LOG(...) LOG__INT(__VA_ARGS__, "\n")
#elif _WIN32
#define LOG(fmt, ...)                                                     \
  std::printf("\x1b[90m[native_audio - INFO] \x1b[0m" fmt, __VA_ARGS__); std::printf("\n");
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define ERROR__INT(fmt, ...)                                                     \
  __android_log_print(ANDROID_LOG_ERROR, "native_audio", fmt "%s", __VA_ARGS__);
#define ERROR(...) ERROR__INT(__VA_ARGS__, "\n")
#elif __linux__
#define ERROR__INT(fmt, ...)                                                     \
  std::fprintf(stderr, "\x1b[90m[native_audio - ERROR] \x1b[31m" fmt "%s", __VA_ARGS__);
#define ERROR(...) ERROR__INT(__VA_ARGS__, "\n")
#elif _WIN32
#define ERROR(fmt, ...)                                                     \
  std::fprintf(stderr, "\x1b[90m[native_audio - ERROR] \x1b[31m" fmt, __VA_ARGS__); std::fprintf(stderr, "\n");
#endif

struct UserBuffer {
  int id = -1;
  uint32_t buffer_size;

  float *buffer = nullptr;
  uint32_t buf_end = 0;
  uint32_t buf_start = 0;

  bool is_exhausted = false;
  uint32_t minimum_size_to_recover;

  // Buffer exhaustion count (when buffer does not contain at least a frame but
  // it's copied anyway)
  uint32_t exhaust_count = 0;

  // Buffer full count
  uint32_t full_count = 0;
};

void consume_from_buffer(float *output, UserBuffer *user_buffer,
                         int frame_count);

void consume_from_buffer_mic(float *output, UserBuffer *user_buffer,
                             int frame_count);

void print_info(const std::unordered_map<int, Buffer> &map,
                unsigned long long data_callback_counter,
                const char device_name[256]);
                
void print_mic_level(uint64_t counter, float *input, uint32_t frame_count, bool speaking);

int buffer_push(UserBuffer *userBuffer, float *src, int length);

inline float calculate_mic_level(float *input, uint32_t frame_count) {
  float acc = 0;
  for (int i = 0; i < frame_count; i++) {
    acc += input[i] * input[i];
  }
  // https://dsp.stackexchange.com/questions/2951/loudness-of-pcm-stream/2953#2953
  // Root mean square calculates the average loudness
  float average_loudness = std::sqrt(acc / frame_count);

  // Convert to logarithmic scale. -96db is lowest value
  float db = 20.0 * std::log10(average_loudness);
  float level = (db > -96.0f ? db : -96.0f) / 96.0f + 1.0f;

  return level;
}

void add_to_buffer(float *dest, float *src, int frame_count);

class Stopwatch {
public:
  Stopwatch();
  void start();
  double elapsed();

private:
  std::chrono::time_point<std::chrono::steady_clock> start_time;
};