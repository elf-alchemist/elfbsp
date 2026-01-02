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
 *  OS support
 */

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #if !defined(WIN32)
    #define WIN32
  #endif

constexpr bool WINDOWS = true;
constexpr bool MACOS = false;
constexpr bool LINUX = false;

#elif defined(__APPLE__)

constexpr bool WINDOWS = false;
constexpr bool MACOS = true;
constexpr bool LINUX = false;

#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

constexpr bool WINDOWS = false;
constexpr bool MACOS = false;
constexpr bool LINUX = true;

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
typedef uint8_t args_t[5];

// misc constants
static constexpr size_t MSG_BUF_LEN = 1024;
static constexpr size_t NO_INDEX = static_cast<size_t>(-1);
static constexpr uint16_t NO_INDEX_SHORT = static_cast<uint16_t>(-1);

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
static constexpr bool ENDIAN_BIG = (std::endian::native == std::endian::big);
static constexpr bool ENDIAN_LITTLE = !ENDIAN_BIG;

template <typename T> inline constexpr T byteswap(T value) noexcept
{
  static_assert(std::is_integral_v<T>, "byteswap: integral required");
  static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "byteswap: only 16/32/64-bit supported");

  if constexpr (sizeof(T) == 2)
  {
    return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
  }
  else if constexpr (sizeof(T) == 4)
  {
    return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
  }
  else
  {
    return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(value)));
  }
}

template <typename T> static inline constexpr T GetLittleEndian(T value)
{
  static_assert(std::is_integral_v<T>, "integral required");
  if constexpr (ENDIAN_BIG)
  {
    return byteswap(value);
  }
  else
  {
    return value;
  }
}

template <typename T> static inline constexpr T GetBigEndian(T value)
{
  if constexpr (ENDIAN_LITTLE)
  {
    return byteswap(value);
  }
  else
  {
    return value;
  }
}
