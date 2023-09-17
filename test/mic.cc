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

#define MINIAUDIO_IMPLEMENTATION
#include "../3rdparty/rnnoise/rnnoise-src/include/rnnoise.h"
#include "../src/miniaudio.h"
#include "../src/utils.h"

const int sample_rate = 48000;
const int channels = 1;
const int max_buffer_size = (3000 * sample_rate) / 1000;
const int wait_buffer_size = (50 * sample_rate) / 1000;
const int bar_size = 80;
char bar[bar_size + 1];

UserBuffer *micBuffer;
std::unordered_map<int, UserBuffer> activeSpeakers;

// short *tempInputBuffer = NULL;
// DenoiseState *st;

int push_stream_buffer(float *buf, int length, int userId) {
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

#define RNNOISE_FRAME_SIZE 480

void multiplyBuffer(float *buf, float mult, int n) {
  for (int i = 0; i < n; i++) {
    buf[i] *= mult;
  }
}

void divideBuffer(float *buf, float div, int n) {
  for (int i = 0; i < n; i++) {
    buf[i] /= div;
  }
}

/// Clip the given float value to a range that can be safely converted into a
/// short (without causing integer overflow)
static short clampFloatSample(float v) {
  return static_cast<short>(std::min(
      std::max(v, static_cast<float>(std::numeric_limits<short>::min())),
      static_cast<float>(std::numeric_limits<short>::max())));
}

bool first = true;
uint64_t counter = 0;

bool isZeroed(float *src, int length) {
  bool isZero = true;
  for (int i = 0; i < length; i++) {
    if (src[i] != 0) {
      isZero = false;
    }
  }
  return isZero;
}

void print_mic_level(uint64_t counter, float *input, uint32_t frame_count) {
  float level = calculate_mic_level(input, frame_count);

  int bar_i = floor(level * bar_size);
  bar[bar_size] = '\0';
  memset(bar, ' ', bar_size * sizeof(char));
  for (int i = 0; i < bar_i; i++) {
    bar[i] = 'x';
  }
  std::printf("[%s]\tlev:%f\t\t%lu\r", bar, level, counter);
  
  std::flush(std::cout);
}

void data_callback_capture(ma_device *device, void *output, const void *input,
                           ma_uint32 frame_count) {
  float *float_input = (float *)input;
  float *float_output = (float *)output;
  counter++;

  // printInfo(activeSpeakers, counter, "deez");

  /* int chunks = frame_count / RNNOISE_FRAME_SIZE;
  int remainder = frame_count % RNNOISE_FRAME_SIZE;

  std::cout << frame_count << std::endl;

  // RNNoise expects a float with max value 32768
  // multiplyBuffer(tempInputBuffer, 32768.f, frame_count);

  memset(tempInputBuffer, 0, max_buffer_size * sizeof(short));

  // ma_pcm_f32_to_s16(tempInputBuffer, float_input, frame_count,
  // ma_dither_mode_none);
  memcpy(tempInputBuffer, short_input, frame_count * sizeof(short));

  for (int i = 0; i < chunks; i++) {
    float temp[RNNOISE_FRAME_SIZE];
    for (int j = 0; j < RNNOISE_FRAME_SIZE; j++)
      temp[j] = short_input[i * RNNOISE_FRAME_SIZE + j];
    rnnoise_process_frame(st, temp, temp);
    for (int j = 0; j < RNNOISE_FRAME_SIZE; j++)
      short_input[i * RNNOISE_FRAME_SIZE + j] = clampFloatSample(temp[j]);
  }

  short remainderBuffer[RNNOISE_FRAME_SIZE];
  memset(remainderBuffer, 0, RNNOISE_FRAME_SIZE * sizeof(short));
  memcpy(remainderBuffer, &tempInputBuffer[RNNOISE_FRAME_SIZE * chunks],
         remainder * sizeof(short));

  if (remainder > 0) {
    float temp[RNNOISE_FRAME_SIZE];
    memset(temp, 0, RNNOISE_FRAME_SIZE * sizeof(float));
    for (int j = 0; j < remainder; j++)
      temp[j] = remainderBuffer[j];
    rnnoise_process_frame(st, temp, temp);
    for (int j = 0; j < remainder; j++)
      remainderBuffer[j] = clampFloatSample(temp[j]);
    memcpy(&tempInputBuffer[chunks * RNNOISE_FRAME_SIZE], remainderBuffer,
           remainder * sizeof(short));
  }

  float *convertedBuffer = new float[frame_count];
  ma_pcm_s16_to_f32(convertedBuffer, tempInputBuffer, frame_count,
                    ma_dither_mode_none);
  // std::cout << rnnoise_get_frame_size() << std::endl;
  // divideBuffer(tempInputBuffer, 32768.f, frame_count);
  ma_encoder *pEncoder = (ma_encoder *)device->pUserData;
  MA_ASSERT(pEncoder != NULL);
  if (!first) {
    ma_encoder_write_pcm_frames(pEncoder, tempInputBuffer, frame_count, NULL);

    push_stream_buffer(convertedBuffer, frame_count, 0);
  } else {
    first = false;
  }

  delete[] convertedBuffer; */

  print_mic_level(counter, float_input, frame_count);

  buffer_push(micBuffer, float_input, frame_count);

  for (auto &speaker : activeSpeakers) {
    auto &userBuffer = speaker.second;
    consume_from_buffer(float_output, &userBuffer, frame_count);
  }

  // float_output should only contain mic data in this example
  consume_from_buffer(float_output, micBuffer, frame_count);
  /*   bool isEmpty = isZeroed(float_output, frame_count);
    if(isEmpty) {
      std::cout << "Empty mic array in iteration " << counter << std::endl;
    } */
};

bool running = true;

void stopRunning(int signal) { running = false; }

int main() {
  // st = rnnoise_create(NULL);
  // tempInputBuffer = (short *)calloc(max_buffer_size, sizeof(short));
  micBuffer = new UserBuffer;
  micBuffer->id = 0;
  micBuffer->buffer_size = max_buffer_size;
  micBuffer->minimum_size_to_recover = wait_buffer_size;
  micBuffer->buffer = (float *)calloc(max_buffer_size, sizeof(float));

  ma_device_config device_config = ma_device_config_init(ma_device_type_duplex);
  device_config.playback.format = ma_format_f32;
  device_config.playback.channels = channels;
  device_config.capture.format = ma_format_f32;
  device_config.capture.channels = channels;
  device_config.sampleRate = sample_rate;
  device_config.dataCallback = data_callback_capture;

  ma_device device;
  ma_result result;

  ma_encoder_config encoderConfig;
  // ma_encoder encoder;

  /*   encoderConfig = ma_encoder_config_init(ma_encoding_format_wav,
    ma_format_f32, channels, sample_rate); device_config.pUserData = &encoder;

    if (ma_encoder_init_file("test.wav", &encoderConfig, &encoder) !=
        MA_SUCCESS) {
      printf("Failed to initialize output file.\n");
      return -1;
    } */

  result = ma_device_init(NULL, &device_config, &device);
  if (result != MA_SUCCESS) {
    printf("Failed to initialize device\n");
    return -1;
  }

  ma_device_start(&device);

  printf("Recording. CTRL+C to stop\n");
  signal(SIGINT, stopRunning);

  while (running) {
    // get_mic_data(10);
    // getchar();
  }
  // rnnoise_destroy(st);
  printf("Closing...");
  ma_device_uninit(&device);
  for (auto &buffer : activeSpeakers) {
    auto buf = buffer.second.buffer;
    if (buf != NULL) {
      free(buf);
    }
  }
  activeSpeakers.clear();
  free(micBuffer->buffer);

  // ma_encoder_uninit(&encoder);
  return 0;
}