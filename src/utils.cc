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

void consume_from_buffer(float *output, UserBuffer *user_buffer,
                         int frame_count) {
    uint32_t playable_size = user_buffer->buf_end - user_buffer->buf_start;

    if (user_buffer->is_exhausted &&
        playable_size < user_buffer->minimum_size_to_recover) {
        user_buffer->exhaust_count++;
    } else {

        user_buffer->is_exhausted = false;

        if (playable_size < frame_count) {
            add_to_buffer(output, &user_buffer->buffer[user_buffer->buf_start],
                          playable_size);

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
    uint32_t playable_size = user_buffer->buf_end - user_buffer->buf_start;

    while (playable_size < frame_count) {
        user_buffer->is_exhausted = true;
        user_buffer->exhaust_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    user_buffer->is_exhausted = false;
    auto start = &user_buffer->buffer[user_buffer->buf_start];
    add_to_buffer(output, start, frame_count);

    user_buffer->buf_start += frame_count;
}

int buffer_push(UserBuffer *userBuffer, float *src, int length) {
    // ignore if no buffer remains
    if (userBuffer->buf_end - userBuffer->buf_start + length >
        userBuffer->buffer_size) {
        userBuffer->full_count++;
        return -1;
    }

    // move the waiting buffer to the head of the buffer, if needed
    if (userBuffer->buf_end + length > userBuffer->buffer_size) {
        // messages.emplace_back("Moved waiting buffer");
        memmove(userBuffer->buffer,
                &(userBuffer->buffer[userBuffer->buf_start]),
                (userBuffer->buf_end - userBuffer->buf_start) * sizeof(float));
        userBuffer->buf_end -= userBuffer->buf_start;
        userBuffer->buf_start = 0;
    }

    memcpy(&userBuffer->buffer[userBuffer->buf_end], src,
           length * sizeof(float));
    userBuffer->buf_end += length;

    return 0;
}

Stopwatch::Stopwatch() { start(); }

void Stopwatch::start() { start_time = std::chrono::steady_clock::now(); }

double Stopwatch::elapsed() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::micro> elapsed = now - start_time;
    return elapsed.count();
}