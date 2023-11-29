#pragma once
#include <chrono>

using namespace std::literals;

class Duration {
  public:
    constexpr static const std::chrono::nanoseconds ms20 = 20000000ns;
    constexpr static const std::chrono::nanoseconds ms40 = 40000000ns;
    constexpr static const std::chrono::nanoseconds ms50 = 50000000ns;
    constexpr static const std::chrono::nanoseconds ms500 = 500000000ns;
};