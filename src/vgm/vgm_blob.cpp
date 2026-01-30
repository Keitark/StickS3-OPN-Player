#include "vgm_blob.hpp"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <string.h>

// M5GFX が内部でリンクしている miniz(tinfl) を使うための宣言
extern "C" {
  // raw deflate を out バッファへ展開する（minizのtinfl API）
  int tinfl_decompress_mem_to_mem(void* out_buf, size_t out_len,
                                  const void* src_buf, size_t src_len,
                                  int flags);
}


VGMBlob::~VGMBlob(){ clear(); }

void VGMBlob::clear() {
  if (data_) {
    ps_free_(data_);
    data_ = nullptr;
    size_ = 0;
  }
  name_.clear();
}

void* VGMBlob::ps_alloc_(size_t n) {
#if defined(ESP32)
  // PSRAM優先
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = malloc(n);
  return p;
#else
  return malloc(n);
#endif
}
void VGMBlob::ps_free_(void* p) {
#if defined(ESP32)
  free(p);
#else
  free(p);
#endif
}

static bool ends_with(const char* s, const char* suf){
  size_t a=strlen(s), b=strlen(suf);
  if (a<b) return false;
  return strcasecmp(s + (a-b), suf) == 0;
}

bool VGMBlob::load_from_file(const char* path) {
  clear();
  name_ = path;

  if (ends_with(path, ".vgz")) return load_vgz_(path);
  return load_vgm_(path);
}
bool VGMBlob::load_vgz_(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;

  size_t gz_size = (size_t)f.size();
  if (gz_size < 18) return false; // gzip最小

  // gzip全体を読む（まずは簡単に全読み）
  uint8_t* gz = (uint8_t*)ps_alloc_(gz_size);
  if (!gz) return false;
  if (f.read(gz, gz_size) != (int)gz_size) {
    ps_free_(gz);
    return false;
  }

  // gzip header parse
  const uint8_t* p = gz;
  const uint8_t* end = gz + gz_size;

  if (p[0] != 0x1F || p[1] != 0x8B || p[2] != 0x08) { // deflate
    ps_free_(gz);
    return false;
  }
  uint8_t flg = p[3];
  p += 10; // base header (ID1 ID2 CM FLG MTIME[4] XFL OS)

  // FEXTRA
  if (flg & 0x04) {
    if (p + 2 > end) { ps_free_(gz); return false; }
    uint16_t xlen = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    p += 2;
    if (p + xlen > end) { ps_free_(gz); return false; }
    p += xlen;
  }
  // FNAME (0-terminated)
  if (flg & 0x08) {
    while (p < end && *p) p++;
    if (p >= end) { ps_free_(gz); return false; }
    p++; // skip null
  }
  // FCOMMENT
  if (flg & 0x10) {
    while (p < end && *p) p++;
    if (p >= end) { ps_free_(gz); return false; }
    p++;
  }
  // FHCRC
  if (flg & 0x02) {
    if (p + 2 > end) { ps_free_(gz); return false; }
    p += 2;
  }

  // trailer: last 8 bytes = CRC32(4) + ISIZE(4)
  if (end < gz + 8) { ps_free_(gz); return false; }
  const uint8_t* trailer = end - 8;
  if (p >= trailer) { ps_free_(gz); return false; }

  uint32_t isize = (uint32_t)trailer[4] | ((uint32_t)trailer[5] << 8) |
                   ((uint32_t)trailer[6] << 16) | ((uint32_t)trailer[7] << 24);

  // deflate payload
  const uint8_t* def = p;
  size_t def_len = (size_t)(trailer - p);

  // output allocate (ISIZE ぶん)
  uint8_t* out = (uint8_t*)ps_alloc_(isize);
  if (!out) { ps_free_(gz); return false; }

  // flags=0: raw deflate として展開（gzipはzlibヘッダ無し）
  int dec = tinfl_decompress_mem_to_mem(out, (size_t)isize, def, def_len, 0);

  ps_free_(gz);

  if (dec <= 0) {  // 失敗
    ps_free_(out);
    return false;
  }

  data_ = out;
  size_ = (size_t)dec;  // 通常 isize と一致するはず
  parse_gd3_();
  return true;
}

bool VGMBlob::load_vgm_(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t sz = (size_t)f.size();
  uint8_t* buf = (uint8_t*)ps_alloc_(sz);
  if (!buf) return false;

  if (f.read(buf, sz) != (int)sz) {
    ps_free_(buf);
    return false;
  }
  data_ = buf;
  size_ = sz;
  parse_gd3_();
  return true;
}
static inline uint32_t u32le_at(const uint8_t* d, size_t sz, uint32_t off) {
  if (!d || off + 4 > sz) return 0;
  return (uint32_t)d[off] | ((uint32_t)d[off+1] << 8) | ((uint32_t)d[off+2] << 16) | ((uint32_t)d[off+3] << 24);
}

// UTF-16LE null-terminated を読み、UTF-8 std::stringに変換（BMP範囲）
static std::string read_utf16le_z_to_utf8(const uint8_t* d, size_t sz, uint32_t& pos) {
  std::string out;
  while (pos + 1 < sz) {
    uint16_t u = (uint16_t)d[pos] | ((uint16_t)d[pos+1] << 8);
    pos += 2;
    if (u == 0) break;

    // UTF-8 encode (BMP)
    if (u < 0x80) {
      out.push_back((char)u);
    } else if (u < 0x800) {
      out.push_back((char)(0xC0 | (u >> 6)));
      out.push_back((char)(0x80 | (u & 0x3F)));
    } else {
      out.push_back((char)(0xE0 | (u >> 12)));
      out.push_back((char)(0x80 | ((u >> 6) & 0x3F)));
      out.push_back((char)(0x80 | (u & 0x3F)));
    }
  }
  return out;
}

void VGMBlob::parse_gd3_() {
  gd3_track_en_.clear();
  gd3_track_jp_.clear();
  gd3_game_en_.clear();
  gd3_author_en_.clear();

  if (!data_ || size_ < 0x40) return;

  // GD3 offset: header 0x14 is relative to 0x14 (0 if none)
  uint32_t rel = u32le_at(data_, size_, 0x14);
  if (rel == 0) return;

  uint32_t gd3 = 0x14 + rel;
  if (gd3 + 12 > size_) return;

  // signature "Gd3 "
  if (!(data_[gd3+0]=='G' && data_[gd3+1]=='d' && data_[gd3+2]=='3' && data_[gd3+3]==' ')) return;

  // uint32_t ver = u32le_at(data_, size_, gd3+4); (使わなくてもOK)
  uint32_t len = u32le_at(data_, size_, gd3+8);
  uint32_t pos = gd3 + 12;
  uint32_t end = pos + len;
  if (end > size_) end = (uint32_t)size_;

  // GD3 string order:
  // Track(en), Track(jp), Game(en), Game(jp), System(en), System(jp),
  // Author(en), Author(jp), ReleaseDate, Creator, Notes
  gd3_track_en_  = read_utf16le_z_to_utf8(data_, end, pos);
  gd3_track_jp_  = read_utf16le_z_to_utf8(data_, end, pos);
  gd3_game_en_   = read_utf16le_z_to_utf8(data_, end, pos);
  (void)read_utf16le_z_to_utf8(data_, end, pos); // game jp
  (void)read_utf16le_z_to_utf8(data_, end, pos); // system en
  (void)read_utf16le_z_to_utf8(data_, end, pos); // system jp
  gd3_author_en_ = read_utf16le_z_to_utf8(data_, end, pos);
  (void)read_utf16le_z_to_utf8(data_, end, pos); // author jp
  (void)read_utf16le_z_to_utf8(data_, end, pos); // release
  (void)read_utf16le_z_to_utf8(data_, end, pos); // creator
  (void)read_utf16le_z_to_utf8(data_, end, pos); // notes
}
