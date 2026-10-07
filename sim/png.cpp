#include "png.h"

#include <cstdio>
#include <vector>

static uint32_t crc32(const uint8_t* d, size_t n, uint32_t crc = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return crc;
}

static void u32(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(x >> 24);
  v.push_back(x >> 16);
  v.push_back(x >> 8);
  v.push_back(x);
}

static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  u32(out, data.size());
  std::vector<uint8_t> td(type, type + 4);
  td.insert(td.end(), data.begin(), data.end());
  out.insert(out.end(), td.begin(), td.end());
  u32(out, crc32(td.data(), td.size()) ^ 0xFFFFFFFFu);
}

bool ecrirePngMonochrome(const std::string& chemin, const uint8_t* bits, int largeur, int hauteur) {
  const int octetsParLigne = (largeur + 7) / 8;
  std::vector<uint8_t> brut;
  for (int y = 0; y < hauteur; y++) {
    brut.push_back(0);
    for (int x = 0; x < largeur; x++) {
      bool noir = !(bits[y * octetsParLigne + x / 8] & (0x80 >> (x & 7)));
      brut.push_back(noir ? 0x10 : 0xF4);
    }
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  size_t pos = 0;
  uint32_t a = 1, b = 0;
  for (uint8_t c : brut) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  while (pos < brut.size()) {
    size_t n = std::min<size_t>(65535, brut.size() - pos);
    bool dernier = pos + n == brut.size();
    z.push_back(dernier ? 1 : 0);
    z.push_back(n & 0xFF);
    z.push_back(n >> 8);
    z.push_back(~n & 0xFF);
    z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), brut.begin() + pos, brut.begin() + pos + n);
    pos += n;
  }
  u32(z, (b << 16) | a);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<uint8_t> ihdr;
  u32(ihdr, largeur);
  u32(ihdr, hauteur);
  ihdr.insert(ihdr.end(), {8, 0, 0, 0, 0});
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});

  FILE* f = fopen(chemin.c_str(), "wb");
  if (!f) return false;
  fwrite(png.data(), 1, png.size(), f);
  fclose(f);
  return true;
}
