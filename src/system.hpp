//------------------------------------------------------------------------
//
//  ELFBSP  Copyright (C) 2025       Guilherme Miranda
//          Copyright (C) 2001-2018  Andrew Apted
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __ELFBSP_SYSTEM_H__
#define __ELFBSP_SYSTEM_H__


/*
 *  Windows support
 */

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
  #ifndef WIN32
  #define WIN32
  #endif
#endif


/*
 *  Standard headers
 */

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <climits>
#include <cmath>
#include <cstring>

#include "config.h"

#ifndef WIN32
#include <unistd.h>
#endif

// allow using std::min, std::max with MSVC
#undef min
#undef max


// sized types

typedef uint8_t byte;


// misc constants

#define MSG_BUF_LEN  1024


// basic macros

#undef  NULL
#define NULL  nullptr

#undef  M_PI
#define M_PI  3.14159265358979323846

#undef  I_ROUND
#define I_ROUND(x)  ((int) round(x))

#define MONTH0 (  __DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? '0' : '0') \
				: __DATE__ [2] == 'b' ? 2 \
				: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? '0' : '0') \
				: __DATE__ [2] == 'y' ? '0' \
				: __DATE__ [2] == 'l' ? '0' \
				: __DATE__ [2] == 'g' ? '0' \
				: __DATE__ [2] == 'p' ? '0' \
				: __DATE__ [2] == 't' ? '1' \
				: __DATE__ [2] == 'v' ? '1' \
				: '1')

#define MONTH1 (  __DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? '1' : '6') \
				: __DATE__ [2] == 'b' ? 2 \
				: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? '3' : '4') \
				: __DATE__ [2] == 'y' ? '5' \
				: __DATE__ [2] == 'l' ? '7' \
				: __DATE__ [2] == 'g' ? '8' \
				: __DATE__ [2] == 'p' ? '9' \
				: __DATE__ [2] == 't' ? '0' \
				: __DATE__ [2] == 'v' ? '1' \
				: '2')

#define ISO_8601_DATE (const char[]){ __DATE__[7], \
									__DATE__[8],   \
									__DATE__[9],   \
									__DATE__[10],  \
									'-',           \
									MONTH0,        \
									MONTH1,        \
									'-',           \
									(__DATE__[4] == ' ' ? '0' : __DATE__[4]), \
									__DATE__[5],   \
									'\0' }


//
// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

#ifdef __GNUC__
#define PACKEDATTR __attribute__((packed))
#else
#define PACKEDATTR
#endif


// endianness

#if defined(__GNUC__) || defined(__clang__)
#define __Swap16  __builtin_bswap16
#define __Swap32  __builtin_bswap32
#define __Swap64  __builtin_bswap64

#elif defined(_MSC_VER)
#define __Swap16  _byteswap_ushort
#define __Swap32  _byteswap_ulong
#define __Swap64  _byteswap_uint64

#else
static inline uint16_t __Swap16(uint16_t n) {
	uint16_t a;
	a  = (n & 0xFF) << 8;
	a |= (n >> 8) & 0xFF;
	return a;
}
static inline uint32_t __Swap32(uint32_t n) {
	uint32_t a;
	a  = (n & 0xFFU)   << 24;
	a |= (n & 0xFF00U) << 8;
	a |= (n >>  8) & 0xFF00U;
	a |= (n >> 24) & 0xFFU;
	return a;
}
static inline uint64_t __Swap64(uint64_t n) {
	uint64_t a;
	a  = (n & 0xFFULL)       << 56;
	a |= (n & 0xFF00ULL)     << 40;
	a |= (n & 0xFF0000ULL)   << 24;
	a |= (n & 0xFF000000ULL) << 8;
	a |= (n >>  8) & 0xFF000000ULL;
	a |= (n >> 24) & 0xFF0000ULL;
	a |= (n >> 40) & 0xFF00ULL;
	a |= (n >> 56) & 0xFFULL;
	return a;
}
#endif

// the Makefile or build system must define BIG_ENDIAN_CPU
// WISH: some preprocessor checks to detect a big-endian cpu.

#ifdef BIG_ENDIAN_CPU
#define LE_U16(x)  __Swap16(x)
#define LE_U32(x)  __Swap32(x)
#define LE_U64(x)  __Swap64(x)
#define BE_U16(x)  ((uint16_t)(x))
#define BE_U32(x)  ((uint32_t)(x))
#define BE_U64(x)  ((uint64_t)(x))
#else
#define LE_U16(x)  ((uint16_t)(x))
#define LE_U32(x)  ((uint32_t)(x))
#define LE_U64(x)  ((uint64_t)(x))
#define BE_U16(x)  __Swap16(x)
#define BE_U32(x)  __Swap32(x)
#define BE_U64(x)  __Swap64(x)
#endif

// signed versions of the above
#define LE_S16(x)  ((int16_t) LE_U16((uint16_t) (x)))
#define LE_S32(x)  ((int32_t) LE_U32((uint32_t) (x)))
#define LE_S64(x)  ((int64_t) LE_U64((uint64_t) (x)))

#define BE_S16(x)  ((int16_t) BE_U16((uint16_t) (x)))
#define BE_S32(x)  ((int32_t) BE_U32((uint32_t) (x)))
#define BE_S64(x)  ((int64_t) BE_U64((uint64_t) (x)))


#endif  /* __ELFBSP_SYSTEM_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
