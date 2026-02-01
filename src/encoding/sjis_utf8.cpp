#include "sjis_utf8.hpp"
#include "sjis_table.hpp"
#include <cstdint>

static uint16_t sjis_to_unicode(uint16_t sjis) {
  size_t lo = 0;
  size_t hi = kSjisMapSize;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    uint16_t key = kSjisMap[mid].sjis;
    if (key == sjis) return kSjisMap[mid].unicode;
    if (key < sjis) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return 0;
}

static void append_utf8(std::string& out, uint16_t code) {
  if (code < 0x80) {
    out.push_back((char)code);
  } else if (code < 0x800) {
    out.push_back((char)(0xC0 | (code >> 6)));
    out.push_back((char)(0x80 | (code & 0x3F)));
  } else {
    out.push_back((char)(0xE0 | (code >> 12)));
    out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (code & 0x3F)));
  }
}

std::string sjis_to_utf8(const char* sjis) {
  if (!sjis) return {};
  size_t len = 0;
  while (sjis[len] != '\0') ++len;
  return sjis_to_utf8(sjis, len);
}

std::string sjis_to_utf8(const char* sjis, size_t len) {
  std::string out;
  if (!sjis || len == 0) return out;
  out.reserve(len * 2);

  for (size_t i = 0; i < len; ++i) {
    uint8_t c = (uint8_t)sjis[i];
    if (c == 0) break;

    if (c <= 0x7F) {
      append_utf8(out, c);
      continue;
    }

    if (c >= 0xA1 && c <= 0xDF) {
      // Halfwidth katakana (JIS X 0201)
      uint16_t code = (uint16_t)(0xFF61 + (c - 0xA1));
      append_utf8(out, code);
      continue;
    }

    // Two-byte sequence
    if (i + 1 >= len) {
      out.push_back('?');
      break;
    }
    uint8_t c2 = (uint8_t)sjis[i + 1];
    uint16_t sjis_code = (uint16_t)((c << 8) | c2);
    uint16_t uni = sjis_to_unicode(sjis_code);
    if (uni) {
      append_utf8(out, uni);
    } else {
      out.push_back('?');
    }
    ++i;
  }

  return out;
}
