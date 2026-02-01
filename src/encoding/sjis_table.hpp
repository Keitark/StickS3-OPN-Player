#pragma once
#include <cstdint>
#include <cstddef>

struct SjisMap {
  uint16_t sjis;
  uint16_t unicode;
};

extern const SjisMap kSjisMap[];
extern const size_t kSjisMapSize;
