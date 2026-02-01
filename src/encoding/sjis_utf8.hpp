#pragma once
#include <string>
#include <cstddef>

// Convert Shift-JIS (CP932) byte string to UTF-8.
// Invalid sequences are replaced with '?'.
std::string sjis_to_utf8(const char* sjis);
std::string sjis_to_utf8(const char* sjis, size_t len);
