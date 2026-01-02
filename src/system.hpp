//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2018 Andrew Apted
//  Copyright 1994-1998 Colin Reed
//  Copyright 1997-1998 Lee Killough
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
//------------------------------------------------------------------------------

#pragma once

/*
 *  Windows support
 */

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #if !defined(WIN32)
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

#if !defined(WIN32)
  #include <unistd.h>
#endif

// sized types
typedef uint8_t byte;

// misc constants
#define MSG_BUF_LEN 1024

// basic macros
#undef M_PI
#define M_PI 3.14159265358979323846

#undef I_ROUND
#define I_ROUND(x) ((int)round(x))

//
// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

#if defined(__GNUC__) || defined(__clang__)
  #define PACKEDATTR __attribute__((packed))
#else
  #define PACKEDATTR
#endif

// endianness
#if defined(__GNUC__) || defined(__clang__)
  #define __Swap16 __builtin_bswap16
  #define __Swap32 __builtin_bswap32
  #define __Swap64 __builtin_bswap64
#endif

// the Makefile or build system must define BIG_ENDIAN_CPU
// WISH: some preprocessor checks to detect a big-endian cpu.
#if defined(BIG_ENDIAN_CPU)
  #define LE_U16(x) __Swap16(x)
  #define LE_U32(x) __Swap32(x)
  #define LE_U64(x) __Swap64(x)
  #define BE_U16(x) ((uint16_t)(x))
  #define BE_U32(x) ((uint32_t)(x))
  #define BE_U64(x) ((uint64_t)(x))
#else
  #define LE_U16(x) ((uint16_t)(x))
  #define LE_U32(x) ((uint32_t)(x))
  #define LE_U64(x) ((uint64_t)(x))
  #define BE_U16(x) __Swap16(x)
  #define BE_U32(x) __Swap32(x)
  #define BE_U64(x) __Swap64(x)
#endif

// signed versions of the above
#define LE_S16(x) ((int16_t)LE_U16((uint16_t)(x)))
#define LE_S32(x) ((int32_t)LE_U32((uint32_t)(x)))
#define LE_S64(x) ((int64_t)LE_U64((uint64_t)(x)))

#define BE_S16(x) ((int16_t)BE_U16((uint16_t)(x)))
#define BE_S32(x) ((int32_t)BE_U32((uint32_t)(x)))
#define BE_S64(x) ((int64_t)BE_U64((uint64_t)(x)))

constexpr size_t NO_INDEX = (size_t)(-1);
