#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class MDXBlob {
public:
  ~MDXBlob();

  bool load_from_file(const char* path);
  void clear();

  uint8_t* data() const { return data_; }
  size_t size() const { return size_; }
  const std::string& name() const { return name_; }

private:
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  std::string name_;

  void* ps_alloc_(size_t n);
  void ps_free_(void* p);
};
