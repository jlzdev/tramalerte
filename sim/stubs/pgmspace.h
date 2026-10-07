#pragma once

#include <cstdint>

#ifndef PROGMEM
#define PROGMEM
#endif
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#define pgm_read_word(addr) (*(const unsigned short*)(addr))
#define pgm_read_dword(addr) (*(const unsigned long*)(addr))
#define pgm_read_pointer(addr) (*(void* const*)(addr))
