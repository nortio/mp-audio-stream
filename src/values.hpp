#pragma once
#include <chrono>

using namespace std::literals;

class Duration {
public:
  constexpr static const std::chrono::nanoseconds ns20 = 20000000ns;
  constexpr static const std::chrono::nanoseconds ns40 = 40000000ns;
  constexpr static const std::chrono::nanoseconds ns50 = 50000000ns;
};