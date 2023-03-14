// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include <cstdio>
#include <cstring>
#include <ostream>
#include <sstream>
#include <string>
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "mp_audio_stream.h"
#include <iostream>
#include <unordered_map>

#define DEVICE_FORMAT ma_format_f32

static char device_name[256] = "";

struct UserBuffer {
  int id = -1;
  ma_uint32 buffer_size;

  float *buffer;
  ma_uint32 buf_end = 0;
  ma_uint32 buf_start = 0;

  bool is_exhausted = false;
  ma_uint32 minimum_size_to_recover;

  // Buffer exhaustion count (when buffer does not contain at least a frame but
  // it's copied anyway)
  ma_uint32 exhaust_count = 0;

  // Buffer full count
  ma_uint32 full_count = 0;
};

static ma_device *device = NULL;

UserBuffer *globalUserBuffer = NULL;

std::unordered_map<int, UserBuffer> activeSpeakers;

unsigned long long data_callback_counter = 0;

static void printInfo(const std::unordered_map<int, UserBuffer> &map) {
  std::printf("\033[H\033[2J");
  std::printf("Callbacks: %llu\tDevice name: %s\n", data_callback_counter,
              device_name);
  auto mapSize = (unsigned int)map.size();
  for (auto &keypair : map) {
    auto size = keypair.second.buf_end - keypair.second.buf_start;
    auto isFull = "\x1b[32m";
    if (size == keypair.second.buffer_size) {
      isFull = "\x1b[31m";
    } else if (size > (keypair.second.buffer_size / 2)) {
      isFull = "\x1b[33m";
    }
    std::printf("[%d]\t filled: %s%d\x1b[0m\texhaust_count: "
                "%d\tlast_playable_size: %d\tfull_count: %d\n",
                keypair.first, isFull, size, keypair.second.exhaust_count, size,
                keypair.second.full_count);
  }
}

void addToBuffer(float *dest, float *src, int frame_count) {
  for (int i = 0; i < frame_count; i++) {
    // dest[i] represents a float, so setting bytes directly with
    // frame_count*sizeof(float) as limit for i is not correct.
    dest[i] += src[i];
  }
}

float *tempBuffer = NULL;

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frame_count) {
  float *out = (float *)pOutput;
#ifdef PARLO_DEBUG
  printInfo(activeSpeakers);
#endif
  data_callback_counter++;
  for (auto &speaker : activeSpeakers) {
    auto &userBuffer = speaker.second;
    ma_uint32 playable_size = userBuffer.buf_end - userBuffer.buf_start;

    if (userBuffer.is_exhausted &&
        playable_size < userBuffer.minimum_size_to_recover) {
      // Empty sound
      // messages.emplace_back("userBuffer exhausted");

      // auto empty = (float *)calloc(frame_count, sizeof(float));
      // addToBuffer(out, empty, frame_count);
      userBuffer.exhaust_count++;
      // free(empty);

    } else {

      userBuffer.is_exhausted = false;

      if (playable_size < frame_count) {
        memcpy(tempBuffer, &userBuffer.buffer[userBuffer.buf_start],
               playable_size * sizeof(float));
        memset(&tempBuffer[playable_size], 0,
               (frame_count - playable_size) * sizeof(float));

        addToBuffer(out, tempBuffer, frame_count);

        userBuffer.buf_start = userBuffer.buf_end;
        userBuffer.is_exhausted = true;
        userBuffer.exhaust_count++;

      } else {
        auto start = &userBuffer.buffer[userBuffer.buf_start];
        addToBuffer(out, start, frame_count);

        userBuffer.buf_start += frame_count;
      }
    }
  }
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
    newUser.buffer_size = globalUserBuffer->buffer_size;
    newUser.minimum_size_to_recover = globalUserBuffer->minimum_size_to_recover;
    newUser.buffer =
        (float *)calloc(globalUserBuffer->buffer_size, sizeof(float));
    userBuffer = &newUser;
  }

  // ignore if no buffer remains
  if (userBuffer->buf_end - userBuffer->buf_start + length >
      userBuffer->buffer_size) {
    userBuffer->full_count++;
    return -1;
  }

  // move the waiting buffer to the head of the buffer, if needed
  if (userBuffer->buf_end + length > userBuffer->buffer_size) {
    // messages.emplace_back("Moved waiting buffer");
    memmove(userBuffer->buffer, &(userBuffer->buffer[userBuffer->buf_start]),
            (userBuffer->buf_end - userBuffer->buf_start) * sizeof(float));
    userBuffer->buf_end -= userBuffer->buf_start;
    userBuffer->buf_start = 0;
  }

  memcpy(&userBuffer->buffer[userBuffer->buf_end], buf, length * sizeof(float));
  userBuffer->buf_end += length;

  return 0;
}

ma_uint32 ma_stream_stat_exhaust_count() {
  return globalUserBuffer->exhaust_count;
}

ma_uint32 ma_stream_stat_full_count() { return globalUserBuffer->full_count; }

void ma_stream_stat_reset() {
  globalUserBuffer->full_count = 0;
  globalUserBuffer->exhaust_count = 0;
}

void ma_stream_uninit() {
  ma_device_uninit(device);

  for (auto &buffer : activeSpeakers) {
    auto buf = buffer.second.buffer;
    if (buf != NULL) {
      free(buf);
    }
  }

  activeSpeakers.clear();

  if (tempBuffer != NULL) {
    free(tempBuffer);
  }
}

int ma_stream_init(int max_buffer_size, int keep_buffer_size, int channels,
                   int sample_rate) {

  device = (ma_device *)calloc(1, sizeof(ma_device));
  tempBuffer = (float *)calloc(max_buffer_size, sizeof(float));
  if (globalUserBuffer == NULL) {
    globalUserBuffer = (UserBuffer *)calloc(1, sizeof(UserBuffer));

    globalUserBuffer->buffer_size = 128 * 1024;
    globalUserBuffer->buffer = NULL;
    globalUserBuffer->buf_end = 0;
    globalUserBuffer->buf_start = 0;
    globalUserBuffer->is_exhausted = false;
    globalUserBuffer->minimum_size_to_recover = 10 * 1024;
    globalUserBuffer->exhaust_count = 0;
    globalUserBuffer->full_count = 0;
  } else {
    ma_device_uninit(device);
  }

  ma_device_config deviceConfig;

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = DEVICE_FORMAT;
  deviceConfig.playback.channels = channels;
  deviceConfig.sampleRate = sample_rate;
  deviceConfig.dataCallback = data_callback;

  if (ma_device_init(NULL, &deviceConfig, device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    return -4;
  }

  strcpy(device_name, device->playback.name);

  globalUserBuffer->buffer_size = max_buffer_size;
  globalUserBuffer->minimum_size_to_recover = keep_buffer_size;

  if (globalUserBuffer->buffer != NULL) {
    free(globalUserBuffer->buffer);
  }

  globalUserBuffer->buffer =
      (float *)calloc(globalUserBuffer->buffer_size, sizeof(float));
  globalUserBuffer->buf_end = 0;
  globalUserBuffer->buf_start = 0;

  if (ma_device_start(device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(device);
    return -5;
  }

  activeSpeakers = {};

  return 0;
}

void parlo_remove_user(int userId) {
  if (activeSpeakers.find(userId) != activeSpeakers.end()) {
    UserBuffer &existingUser = activeSpeakers.at(userId);
    if(existingUser.buffer != NULL) {
      free(existingUser.buffer);
    }
    int res = activeSpeakers.erase(userId);
  }
}
