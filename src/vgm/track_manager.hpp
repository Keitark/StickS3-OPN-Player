#pragma once
#include <vector>
#include <string>

class TrackManager {
public:
  bool scan();                 // LittleFS rootから .vgm/.vgz/.mdx を列挙
  bool empty() const { return tracks_.empty(); }

  const std::string& current() const { return tracks_[idx_]; }
  int index() const { return idx_; }
  int count() const { return (int)tracks_.size(); }

  void next();
  void prev();

private:
  std::vector<std::string> tracks_;
  int idx_ = 0;
};
