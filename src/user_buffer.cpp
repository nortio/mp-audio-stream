#include "user_buffer.h"
#include "utils.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

Buffer::Buffer(int id, uint32_t buffer_size, uint32_t minimum_size_to_recover)
    : id(id), max_buffer_size(buffer_size),
      minimum_size_to_recover(minimum_size_to_recover) {
  buffer = (float *)calloc(max_buffer_size, sizeof(float));
}

Buffer::Buffer(const Buffer &source)
    : id(source.id), max_buffer_size(source.max_buffer_size),
      minimum_size_to_recover(source.minimum_size_to_recover) {

  buffer = (float *)calloc(max_buffer_size, sizeof(float));
  memcpy(buffer, source.buffer, max_buffer_size * sizeof(float));
}

Buffer::Buffer(Buffer &&source)
    : id(source.id), max_buffer_size(source.max_buffer_size),
      minimum_size_to_recover(source.minimum_size_to_recover) {

  buffer = source.buffer;
  source.buffer = nullptr;
}

Buffer &Buffer::operator=(const Buffer &source) {
  if (this != &source) {
    if (buffer) {
      free(buffer);
    }
    buffer = (float *)calloc(max_buffer_size, sizeof(float));
    memcpy(buffer, source.buffer, max_buffer_size * sizeof(float));
  }
  return *this;
}

Buffer &Buffer::operator=(Buffer &&source) {
  if (this != &source) {
    buffer = source.buffer;
    source.buffer = nullptr;
  }
  return *this;
}

Buffer::~Buffer() {
  if (buffer) {
    free(buffer);
  }
}

/**
 * @brief Pushes new data into buffer
 * 
 * @param src Source
 * @param length Length
 * @return int Status: -1 if the buffer is full, 0 if push was successful
 */
int Buffer::push(float *src, int length) {
  // ignore if no buffer remains
  if (buf_end - buf_start + length > max_buffer_size) {
    full_count++;
    return -1;
  }

  // move the waiting buffer to the head of the buffer, if needed
  if (buf_end + length > max_buffer_size) {
    memmove(buffer, &(buffer[buf_start]),
            (buf_end - buf_start) * sizeof(float));
    buf_end -= buf_start;
    buf_start = 0;
  }

  memcpy(&buffer[buf_end], src, length * sizeof(float));
  buf_end += length;

  return 0;
}

/**
 * @brief Consumes from buffer and mixes with output (adding samples)
 * 
 * @param output Pointer to output buffer
 * @param frame_count Number of samples to consume
 */
void Buffer::consume(float *output, int frame_count) {
  uint32_t playable_size = buf_end - buf_start;

  if (is_exhausted && playable_size < minimum_size_to_recover) {
    exhaust_count++;
  } else {

    is_exhausted = false;

    if (playable_size < frame_count) {
      add_to_buffer(output, &buffer[buf_start], playable_size);

      buf_start = buf_end;
      is_exhausted = true;
      exhaust_count++;

    } else {
      auto start = &buffer[buf_start];
      add_to_buffer(output, start, frame_count);

      buf_start += frame_count;
    }
  }
}

bool Buffer::is_ready(uint32_t length) {
  return (buf_end - buf_start) >= length;
}
