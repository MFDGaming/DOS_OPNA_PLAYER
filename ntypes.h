#ifndef VGMCONV_NTYPES_H
#define VGMCONV_NTYPES_H

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#include <limits.h>

typedef signed char i8_t;
typedef unsigned char u8_t;
typedef signed short i16_t;
typedef unsigned short u16_t;
#if defined(LONG_MAX) && LONG_MAX > 2147483647L
typedef signed int i32_t;
typedef unsigned int u32_t;
#else
typedef signed long i32_t;
typedef unsigned long u32_t;
#endif
#else
#include <stdint.h>

typedef int8_t i8_t;
typedef uint8_t u8_t;
typedef int16_t i16_t;
typedef uint16_t u16_t;
typedef int32_t i32_t;
typedef uint32_t u32_t;
#endif

#endif
