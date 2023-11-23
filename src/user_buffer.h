#pragma once
#include <cstdint>

class Buffer {
public:
  Buffer(int id, uint32_t buffer_size, uint32_t minimum_size_to_recover);
  Buffer(const Buffer& source);
  Buffer(Buffer&& source);
  Buffer& operator=(Buffer&& source);
  Buffer& operator=(const Buffer& source);
  virtual ~Buffer();
  int get_id() { return id; }
  int push(float* src, int length);
  void consume(float *output, int frame_count);
  bool is_ready(uint32_t length);
  float level(uint32_t frame_count);

  uint32_t get_buf_end() const {return buf_end;};
  uint32_t get_buf_start() const { return buf_start;};
  uint32_t get_exhaustion_count() const { return exhaust_count;};
  uint32_t get_buffer_size() const { return max_buffer_size; };
  uint32_t get_full_count() const { return full_count; };

  friend void add_to_buffer(float *dest, float *src, int frame_count);
private:
  int id;
  uint32_t max_buffer_size;

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