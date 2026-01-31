#include "track_manager.hpp"
#include <LittleFS.h>

static bool has_ext(const char* n, const char* ext){
  String s(n);
  s.toLowerCase();
  String e(ext);
  e.toLowerCase();
  return s.endsWith(e);
}

bool TrackManager::scan() {
  tracks_.clear();
  idx_ = 0;

  File root = LittleFS.open("/");
  if (!root) return false;

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      const char* name = f.name();
      if (has_ext(name, ".vgm") || has_ext(name, ".vgz") || has_ext(name, ".mdx")) {
        std::string p = name;
        if (!p.empty() && p[0] != '/') p = "/" + p;
        tracks_.push_back(p);
      }
    }
    f = root.openNextFile();
  }

  return true;
}

void TrackManager::next() {
  if (tracks_.empty()) return;
  idx_ = (idx_ + 1) % (int)tracks_.size();
}
void TrackManager::prev() {
  if (tracks_.empty()) return;
  idx_ = (idx_ - 1 + (int)tracks_.size()) % (int)tracks_.size();
}
