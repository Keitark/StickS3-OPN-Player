#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

class VGMBlob {
public:
  ~VGMBlob();

  bool load_from_file(const char* path);   // .vgm / .vgz どちらもOK
  void clear();

  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }
  const std::string& name() const { return name_; }

  const std::string& gd3_track_name_en() const { return gd3_track_en_; }
  const std::string& gd3_track_name_jp() const { return gd3_track_jp_; }
  const std::string& gd3_game_name_en()  const { return gd3_game_en_; }
  const std::string& gd3_author_en()     const { return gd3_author_en_; }

private:
  uint8_t* data_ = nullptr;
  size_t size_ = 0;
  std::string name_;

  std::string gd3_track_en_, gd3_track_jp_, gd3_game_en_, gd3_author_en_;

  bool load_vgm_(const char* path);
  bool load_vgz_(const char* path);

  void* ps_alloc_(size_t n);
  void  ps_free_(void* p);

  void parse_gd3_();   // ★追加
};
