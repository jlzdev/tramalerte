#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

typedef bool boolean;
#define PROGMEM
#define F(x) (x)
class __FlashStringHelper;

inline unsigned long millis() { return 0; }
inline void yield() {}
inline void delay(unsigned long) {}
inline float radians(float deg) { return deg * 0.017453292519943295f; }
inline float degrees(float rad) { return rad * 57.29577951308232f; }

class String {
 public:
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& c) : s(c) {}
  const char* c_str() const { return s.c_str(); }
  unsigned length() const { return s.size(); }
  char operator[](unsigned i) const { return s[i]; }

 private:
  std::string s;
};

#include "Print.h"
#include "pgmspace.h"
