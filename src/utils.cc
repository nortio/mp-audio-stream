#include "utils.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ratio>
#include <thread>

void add_to_buffer(float *dest, float *src, int frame_count) {
    for (int i = 0; i < frame_count; i++) {
        // dest[i] represents a float, so setting bytes directly with
        // frame_count*sizeof(float) as limit for i is not correct.
        dest[i] += src[i];
    }
}

const int bar_size = 40;
char bar[bar_size + 1];

void print_mic_level(uint64_t counter, float *input, uint32_t frame_count,
                     bool speaking) {
    float level = calculate_mic_level(input, frame_count);
    int bar_i = floor(level * bar_size);
    bar[bar_size] = '\0';
    memset(bar, ' ', bar_size * sizeof(char));
    for (int i = 0; i < bar_i; i++) {
        bar[i] = 'x';
    }
    std::printf("[%s%s\x1b[0m]\tlev:%f\t\t%lu\tspeaking:%d\r",
                speaking ? "\x1b[32m" : "\x1b[0m", bar, level, counter,
                speaking);

    std::flush(std::cout);
}

void print_info(const std::unordered_map<int, Buffer> &map,
                unsigned long long data_callback_counter,
                const char device_name[256]) {
    std::printf("\033[H\033[2J");
    std::printf("Callbacks: %llu\tDevice name: %s\n", data_callback_counter,
                device_name);
    for (const auto &[id, buffer] : map) {
        auto size = buffer.get_buf_end() - buffer.get_buf_start();
        auto isFull = "\x1b[32m";
        if (size == buffer.get_buffer_size()) {
            isFull = "\x1b[31m";
        } else if (size > (buffer.get_buffer_size() / 2)) {
            isFull = "\x1b[33m";
        }
        std::printf("[%d]\t filled: %s%d\x1b[0m\texhaust_count: "
                    "%d\tlast_playable_size: %d\tfull_count: %d\n",
                    id, isFull, size, buffer.get_exhaustion_count(), size,
                    buffer.get_full_count());
    }
}

Stopwatch::Stopwatch() { start(); }

void Stopwatch::start() { start_time = std::chrono::steady_clock::now(); }

double Stopwatch::elapsed() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> elapsed = now - start_time;
    return elapsed.count();
}