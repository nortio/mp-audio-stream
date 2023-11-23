#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <ostream>
#include <thread>
#include <unordered_map>

#include "../3rdparty/rnnoise/rnnoise-src/include/rnnoise.h"
#include "../src/utils.hpp"
#include "../src/mp_audio_stream.h"

const int sample_rate = 48000;
const int channels = 1;
const int max_buffer_size = (3000 * sample_rate) / 1000;
const int wait_buffer_size = (50 * sample_rate) / 1000;

volatile bool running = true;

void stopRunning(int signal) { running = false; }

int main() {
  ma_stream_init(max_buffer_size, wait_buffer_size, channels, sample_rate);
  signal(SIGINT, stopRunning);
  while (running) {

  }

  ma_stream_uninit();
}