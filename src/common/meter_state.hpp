#pragma once
#include <array>

struct MeterState {
  std::array<float, 16> val{};
  std::array<float, 16> peak{};
  std::array<float, 16> hold{};
  int count = 0;
};
