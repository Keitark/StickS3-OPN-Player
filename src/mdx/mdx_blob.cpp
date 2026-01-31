#include "mdx_blob.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

MDXBlob::~MDXBlob() { clear(); }

void MDXBlob::clear() {
  if (data_) {
    ps_free_(data_);
    data_ = nullptr;
    size_ = 0;
  }
  name_.clear();
}

void* MDXBlob::ps_alloc_(size_t n) {
#if defined(ESP32)
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(n);
  return p;
#else
  return malloc(n);
#endif
}

void MDXBlob::ps_free_(void* p) {
#if defined(ESP32)
  free(p);
#else
  free(p);
#endif
}

bool MDXBlob::load_from_file(const char* path) {
  clear();
  name_ = path;

  File f = LittleFS.open(path, "r");
  if (!f) return false;

  size_t sz = (size_t)f.size();
  if (sz == 0) return false;

  uint8_t* buf = (uint8_t*)ps_alloc_(sz);
  if (!buf) return false;

  if (f.read(buf, sz) != (int)sz) {
    ps_free_(buf);
    return false;
  }

  data_ = buf;
  size_ = sz;
  return true;
}
