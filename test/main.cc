// SPDX-License-Identifier: MIT
/*
Copyright (c) 2022 reki2000
Copyright (c) 2023 nortio
*/

#include "../src/miniaudio.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <thread>
#include <stdlib.h>
#include "../src/mp_audio_stream.h"
#include "../src/utils.hpp"
#define PI 3.14159265

static bool continueRunning = true;
const int sample_rate = 48000;
const int channels = 1;
const int max_buffer_size_ms = 3000;
const int wait_buffer_size_ms = 40;
const int emulated_lag_range = 10;
const int min_lag = 10;

void closeSignal(int) { continueRunning = false; }


int main() {
  srand(time(NULL));
  const int max_buffer_size = (max_buffer_size_ms * sample_rate) / 1000;
  const int wait_buffer_size = (wait_buffer_size_ms * sample_rate) / 1000;

  ma_stream_init(max_buffer_size, wait_buffer_size, channels, sample_rate);

  int bufLength = 960; // 20ms packet at 44100 hz

  float *buf = (float *)calloc(bufLength, sizeof(float));
  float *buf2 = (float *)calloc(bufLength, sizeof(float));

  int freq = 1000;

  signal(SIGINT, closeSignal);

  //std::cout << "\x1b[2J" << std::endl;

  int i = 0;
  while (continueRunning) {
    auto lag = rand() % emulated_lag_range + min_lag;

    for (int j = 0; j < bufLength; j++) {
      buf[j] = (float)sin(2 * PI * ((j * freq) % sample_rate) / sample_rate);
      buf2[j] = (float)sin(2 * PI * ((j * freq) % sample_rate) / sample_rate + (PI));
    }

    // They should cancel out for the first 100 packets
    ma_stream_push(buf, bufLength, 0);
/*     if(i < 100) {
      ma_stream_push(buf2, bufLength, 1);
    } */
    // ma_stream_push(buf, bufLength, false, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(lag));
    i++;
  }
  LOG("Closing");
  ma_stream_uninit();
  free(buf);
  free(buf2);
}
