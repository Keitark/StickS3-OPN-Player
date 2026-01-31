#pragma once
#include <array>

struct MeterState {
  std::array<float, 8> val{};
  std::array<float, 8> peak{};
  std::array<float, 8> hold{};
  int count = 0;
};
