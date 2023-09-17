#include "utils.h"
#include <chrono>
#include <iostream>
#include <cstring>
#include <ratio>
#include <thread>

void add_to_buffer(float *dest, float *src, int frame_count) {
  for (int i = 0; i < frame_count; i++) {
    // dest[i] represents a float, so setting bytes directly with
    // frame_count*sizeof(float) as limit for i is not correct.
    dest[i] += src[i];
    //std::cout << " a " << dest[i] << " src " << src[i] << "|\t";
  }
}

void print_info(const std::unordered_map<int, UserBuffer> &map, unsigned long long data_callback_counter, const char device_name[256]) {
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

void consume_from_buffer(float *output, UserBuffer *user_buffer,
                         int frame_count) {
  ma_uint32 playable_size = user_buffer->buf_end - user_buffer->buf_start;

  if (user_buffer->is_exhausted &&
      playable_size < user_buffer->minimum_size_to_recover) {
    user_buffer->exhaust_count++;
  } else {

    user_buffer->is_exhausted = false;

    if (playable_size < frame_count) {
      add_to_buffer(output, &user_buffer->buffer[user_buffer->buf_start], playable_size);

      user_buffer->buf_start = user_buffer->buf_end;
      user_buffer->is_exhausted = true;
      user_buffer->exhaust_count++;

    } else {
      auto start = &user_buffer->buffer[user_buffer->buf_start];
      add_to_buffer(output, start, frame_count);

      user_buffer->buf_start += frame_count;
    }
  }
}

void consume_from_buffer_mic(float *output, UserBuffer *user_buffer,
                         int frame_count) {
  ma_uint32 playable_size = user_buffer->buf_end - user_buffer->buf_start;


  while(playable_size < frame_count) {
    user_buffer->is_exhausted = true;
    user_buffer->exhaust_count++;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  user_buffer->is_exhausted = false;
  auto start = &user_buffer->buffer[user_buffer->buf_start];
  add_to_buffer(output, start, frame_count);

  user_buffer->buf_start += frame_count;
}

int buffer_push(UserBuffer * userBuffer, float* src, int length) {
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

  memcpy(&userBuffer->buffer[userBuffer->buf_end], src, length * sizeof(float));
  userBuffer->buf_end += length;

  return 0;
}