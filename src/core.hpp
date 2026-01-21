//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2001-2018 Andrew Apted, et al
//  Copyright 1997-2003 André Majorel et al
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
 *  Standard headers
 */

#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <bit>

//
//  OS support
//

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #if !defined(WIN32)
    #define WIN32
  #endif

static constexpr bool WINDOWS = true;
static constexpr bool MACOS = false;
static constexpr bool LINUX = false;

#elif defined(__APPLE__)

static constexpr bool WINDOWS = false;
static constexpr bool MACOS = true;
static constexpr bool LINUX = false;

#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

static constexpr bool WINDOWS = false;
static constexpr bool MACOS = false;
static constexpr bool LINUX = true;

#endif

static constexpr char PATH_SEP_CH = (WINDOWS) ? ';' : ':';
static constexpr char DIR_SEP_CH = (WINDOWS) ? '/' : '\\';

static constexpr bool DEBUG_BLOCKMAP = false;
static constexpr bool DEBUG_REJECT = false;
static constexpr bool DEBUG_LOAD = false;
static constexpr bool DEBUG_BSP = false;
static constexpr bool DEBUG_WALLTIPS = false;
static constexpr bool DEBUG_POLYOBJ = false;
static constexpr bool DEBUG_OVERLAPS = false;
static constexpr bool DEBUG_PICKNODE = false;
static constexpr bool DEBUG_SPLIT = false;
static constexpr bool DEBUG_CUTLIST = false;
static constexpr bool DEBUG_BUILDER = false;
static constexpr bool DEBUG_SORTER = false;
static constexpr bool DEBUG_SUBSEC = false;
static constexpr bool DEBUG_WAD = false;

//
// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

// -Elf- updated, pulled from Chocolate Doom

#if defined(__GNUC__)

  #if defined(_WIN32) && !defined(__clang__)
    #define PACKEDATTR __attribute__((packed, gcc_struct))
  #else
    #define PACKEDATTR __attribute__((packed))
  #endif

  #define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
  #define PRINTF_ARG_ATTR(x)      __attribute__((format_arg(x)))
  #define NORETURN                __attribute__((noreturn))

#else
  #define PACKEDATTR
  #define PRINTF_ATTR(fmt, first)
  #define PRINTF_ARG_ATTR(x)
  #define NORETURN
#endif

// endianness
static constexpr bool ENDIAN_BIG = (std::endian::native == std::endian::big);
static constexpr bool ENDIAN_LITTLE = !ENDIAN_BIG;

template <typename T> static inline constexpr T byteswap(T value) noexcept
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

// sized types
using byte = uint8_t;
using args_t = uint8_t[5];
using fixed_t = int32_t;
using long_angle_t = uint32_t;
using short_angle_t = uint16_t;
using lump_t = char[8];

// misc constants
static constexpr long_angle_t LONG_ANGLE_45 = 0x20000000;
static constexpr long_angle_t LONG_ANGLE_1 = (LONG_ANGLE_45 / 45);

static constexpr uint32_t FRACBITS = 16;
static constexpr fixed_t FRACUNIT = (1 << FRACBITS);
static constexpr double FRACFACTOR = static_cast<double>(FRACUNIT);

static constexpr size_t NO_INDEX = static_cast<size_t>(-1);
static constexpr uint16_t NO_INDEX_INT16 = static_cast<uint16_t>(-1);
static constexpr uint32_t NO_INDEX_INT32 = static_cast<uint32_t>(-1);

static constexpr size_t MSG_BUFFER_LENGTH = 1024;
static constexpr size_t WAD_LUMP_NAME = 8;

// bitflags
static inline constexpr uint32_t BIT(uint32_t x)
{
  return (1u << x);
}

static inline constexpr bool HAS_BIT(uint32_t x, uint32_t y)
{
  return (x & y) != 0;
}

// doom's 32bit 16.16 fixed point
static inline constexpr fixed_t IntToFixed(int32_t x)
{
  return x << FRACBITS;
}

static inline constexpr int32_t FixedToInt(fixed_t x)
{
  return x >> FRACBITS;
}

static inline constexpr double FixedToFloat(fixed_t x)
{
  return (static_cast<double>(x) / FRACFACTOR);
}

static inline constexpr fixed_t FloatToFixed(double x)
{
  return static_cast<fixed_t>(x * FRACFACTOR);
}

// binary angular measurement, BAM!
static inline constexpr long_angle_t DegreesToLongBAM(uint16_t x)
{
  return static_cast<long_angle_t>(LONG_ANGLE_1 * x);
}

static inline constexpr short_angle_t DegreesToShortBAM(uint16_t x)
{
  return static_cast<short_angle_t>((LONG_ANGLE_1 * x) >> FRACBITS);
}

//------------------------------------------------------------------------
// STRING STUFF
//------------------------------------------------------------------------

// this is > 0 when ShowMap() is used and the current line
// has not been terminated with a new-line ('\n') character.
static inline size_t hanging_pos = 0;

static inline void StopHanging()
{
  if (hanging_pos > 0)
  {
    hanging_pos = 0;

    printf("\n");
    fflush(stdout);
  }
}

// Safe, portable vsnprintf().
static inline int32_t PRINTF_ATTR(3, 0) M_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
{
  if (buf_len < 1)
  {
    return 0;
  }

  // Windows (and other OSes?) has a vsnprintf() that doesn't always
  // append a trailing \0. So we must do it, and write into a buffer
  // that is one byte shorter; otherwise this function is unsafe.
  int32_t result = vsnprintf(buf, buf_len, s, args);

  // If truncated, change the final char in the buffer to a \0.
  // A negative resultindicates a truncated buffer on Windows.
  if (result < 0 || result >= static_cast<int32_t>(buf_len))
  {
    buf[buf_len - 1] = '\0';
    result = static_cast<int32_t>(buf_len) - 1;
  }

  return result;
}

// Safe, portable snprintf().
static inline int32_t PRINTF_ATTR(3, 4) M_snprintf(char *buf, size_t buf_len, const char *s, ...)
{
  va_list args;
  int result;
  va_start(args, s);
  result = M_vsnprintf(buf, buf_len, s, args);
  va_end(args);
  return result;
}

static inline void PRINTF_ATTR(1, 2) Debug(const char *fmt, ...)
{
  char buffer[MSG_BUFFER_LENGTH];

  va_list args;

  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  fprintf(stderr, "%s", buffer);
}

//
//  show a message
//
static inline void PRINTF_ATTR(1, 2) Print(const char *fmt, ...)
{
  char buffer[MSG_BUFFER_LENGTH];

  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  vsnprintf(buffer, MSG_BUFFER_LENGTH - 1, fmt, arg_ptr);
  va_end(arg_ptr);

  buffer[MSG_BUFFER_LENGTH - 1] = 0;

  StopHanging();
  printf("%s", buffer);
  fflush(stdout);
}

//
//  show an error message and terminate the program
//
static inline void PRINTF_ATTR(1, 2) FatalError(const char *fmt, ...)
{
  char buffer[MSG_BUFFER_LENGTH];

  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  vsnprintf(buffer, MSG_BUFFER_LENGTH - 1, fmt, arg_ptr);
  va_end(arg_ptr);

  buffer[MSG_BUFFER_LENGTH - 1] = 0;

  StopHanging();
  fprintf(stderr, "\nFATAL ERROR: %s", buffer);
  exit(3);
}

// Assertion macros

static inline constexpr void SYS_ASSERT(bool cond)
{
  return (cond) ? (void)0 : FatalError("Assertion failed\nIn function %s (%s:%d)\n", __func__, __FILE__, __LINE__);
}

//------------------------------------------------------------------------
// MEMORY ALLOCATION
//------------------------------------------------------------------------

//
// Allocate memory with error checking.  Zeros the memory.
//
template <typename T> static inline constexpr T *UtilCalloc(size_t size)
{
  T *ret = static_cast<T *>(calloc(1, size));

  if (!ret)
  {
    FatalError("Out of memory (cannot allocate %zu bytes)", size);
  }

  return ret;
}

//
// Reallocate memory with error checking.
//
template <typename T> static inline constexpr T *UtilRealloc(T *old, size_t size)
{
  T *ret = static_cast<T *>(realloc(old, size));

  if (!ret)
  {
    FatalError("Out of memory (cannot reallocate %zu bytes)", size);
  }

  return ret;
}

//
// Free the memory with error checking.
//
template <typename T> static inline constexpr void UtilFree(T *data)
{
  if (data == nullptr)
  {
    FatalError("Trying to free a nullptr");
  }

  free(data);
}

//------------------------------------------------------------------------
// FILE MANAGEMENT
//------------------------------------------------------------------------

static inline bool FileExists(const char *filename)
{
  FILE *fp = fopen(filename, "rb");

  if (fp)
  {
    fclose(fp);
    return true;
  }

  return false;
}

static inline bool FileCopy(const char *src_name, const char *dest_name)
{
  char buffer[MSG_BUFFER_LENGTH];

  FILE *src = fopen(src_name, "rb");
  if (!src)
  {
    return false;
  }

  FILE *dest = fopen(dest_name, "wb");
  if (!dest)
  {
    fclose(src);
    return false;
  }

  while (true)
  {
    size_t rlen = fread(buffer, 1, sizeof(buffer), src);
    if (rlen == 0)
    {
      break;
    }

    size_t wlen = fwrite(buffer, 1, rlen, dest);
    if (wlen != rlen)
    {
      break;
    }
  }

  bool was_OK = !ferror(src) && !ferror(dest);

  fclose(dest);
  fclose(src);

  return was_OK;
}

//------------------------------------------------------------------------
// STRINGS
//------------------------------------------------------------------------

//
// a case-insensitive compare
//
static inline int32_t StringCaseCmp(const char *s1, const char *s2)
{
  SYS_ASSERT(s1 && s2);
  return strcasecmp(s1, s2);
}

//
// a case-insensitive compare
//
static inline int32_t StringCaseCmpMax(const char *s1, const char *s2, size_t len)
{
  SYS_ASSERT(s1 && s2 && len);
  return strncasecmp(s1, s2, len);
}

//------------------------------------------------------------------------
//  FILENAMES
//------------------------------------------------------------------------

static inline bool HasExtension(const char *filename)
{
  const size_t len = strlen(filename);
  if (len == 0)
  {
    return false;
  }

  // Trailing dot → no extension
  if (filename[len - 1] == '.')
  {
    return false;
  }

  for (size_t i = len; i-- > 0;)
  {
    const char ch = filename[i];

    if (ch == '.')
    {
      return true;
    }

    if (ch == DIR_SEP_CH)
    {
      break;
    }

    if constexpr (WINDOWS)
    {
      if (ch == ':')
      {
        break;
      }
    }
  }

  return false;
}

//
// MatchExtension
//
// When ext is nullptr, checks if the file has no extension.
//
static inline bool MatchExtension(const char *filename, const char *ext)
{
  if (!ext || !*ext)
  {
    return !HasExtension(filename);
  }

  size_t A = strlen(filename);
  size_t B = strlen(ext);

  while (A-- > 0 && B-- > 0)
  {
    if (toupper(static_cast<unsigned char>(filename[A])) != toupper(static_cast<unsigned char>(ext[B])))
    {
      return false;
    }
  }

  return (B == NO_INDEX) && filename[A] == '.';
}

//
// FindExtension
//
// Return offset of the '.', or NO_INDEX when no extension was found.
//
static inline size_t FindExtension(const char *filename)
{
  const size_t len = strlen(filename);
  if (len == 0)
  {
    return NO_INDEX;
  }

  for (size_t pos = len; pos-- > 0;)
  {
    char ch = filename[pos];

    if (ch == '.')
    {
      return pos;
    }

    if (ch == DIR_SEP_CH)
    {
      break;
    }

    if constexpr (WINDOWS)
    {
      if (ch == ':')
      {
        break;
      }
    }
  }

  return NO_INDEX;
}

//------------------------------------------------------------------------
// MATH STUFF
//------------------------------------------------------------------------

//
// Compute angle of line from (0,0) to (dx,dy).
// Result is degrees, where 0 is east and 90 is north, and so on.
//
static inline double ComputeAngle(double dx, double dy)
{
  if (dx == 0)
  {
    return (dy > 0) ? 90.0 : 270.0;
  }

  const double angle = atan2(dy, dx) * 180.0 / M_PI;

  return (angle < 0) ? angle + 360.0 : angle;
}

//------------------------------------------------------------------------
// WAD STRUCTURES
//------------------------------------------------------------------------

// wad header
using raw_wad_header_t = struct raw_wad_header_s
{
  char ident[4];
  uint32_t num_entries;
  uint32_t dir_start;
} PACKEDATTR;

// directory entry
using raw_wad_entry_t = struct raw_wad_entry_s
{
  uint32_t pos;
  uint32_t size;
  char name[8];
} PACKEDATTR;

//------------------------------------------------------------------------
// LEVEL STRUCTURES
//------------------------------------------------------------------------

using map_format_t = enum map_format_e
{
  MAPF_INVALID = 0,

  MAPF_Doom,
  MAPF_Hexen,
  MAPF_UDMF
};

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
using lump_order_t = enum lump_order_e
{
  LL_LABEL = 0, // A separator name, ExMx or MAPxx
  LL_THINGS,    // Monsters, items..
  LL_LINEDEFS,  // LineDefs, from editing
  LL_SIDEDEFS,  // SideDefs, from editing
  LL_VERTEXES,  // Vertices, edited and BSP splits generated
  LL_SEGS,      // LineSegs, from LineDefs split by BSP
  LL_SSECTORS,  // SubSectors, list of LineSegs
  LL_NODES,     // BSP nodes
  LL_SECTORS,   // Sectors, from editing
  LL_REJECT,    // LUT, sector-sector visibility
  LL_BLOCKMAP,  // LUT, motion clipping, walls/grid element
  LL_BEHAVIOR,  // ACS bytecode
  LL_SCRIPTS,   // ACS source code
};

using raw_vertex_t = struct raw_vertex_s
{
  int16_t x;
  int16_t y;
} PACKEDATTR;

using raw_linedef_t = struct raw_linedef_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type (0 for none, 97 for teleporter, etc)
  uint16_t tag;     // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR;

using raw_hexen_linedef_t = struct raw_hexen_linedef_s
{
  uint16_t start;  // from this vertex...
  uint16_t end;    // ... to this vertex
  uint16_t flags;  // linedef flags (impassible, etc)
  uint8_t special; // special type
  args_t args;     // special arguments
  uint16_t right;  // right sidedef
  uint16_t left;   // left sidedef
} PACKEDATTR;

using raw_sidedef_t = struct raw_sidedef_s
{
  int16_t x_offset;  // X offset for texture
  int16_t y_offset;  // Y offset for texture
  char upper_tex[8]; // texture name for the part above
  char lower_tex[8]; // texture name for the part below
  char mid_tex[8];   // texture name for the regular part
  uint16_t sector;   // adjacent sector
} PACKEDATTR;

using raw_sector_t = struct raw_sector_s
{
  int16_t floorh;    // floor height
  int16_t ceilh;     // ceiling height
  char floor_tex[8]; // floor texture
  char ceil_tex[8];  // ceiling texture
  uint16_t light;    // light level (0-255)
  uint16_t type;     // special type (0 = normal, 9 = secret, ...)
  uint16_t tag;      // sector activated by a linedef with same tag
} PACKEDATTR;

using raw_thing_t = struct raw_thing_s
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR;

// -JL- Hexen thing definition
using raw_hexen_thing_t = struct raw_hexen_thing_s
{
  int16_t tid;      // tag id (for scripts/specials)
  int16_t x;        // x position
  int16_t y;        // y position
  int16_t height;   // start height above floor
  int16_t angle;    // angle thing faces
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, dormant, etc..
  uint8_t special;  // special type
  uint8_t args[5];  // special arguments
} PACKEDATTR;

//------------------------------------------------------------------------
// BSP TREE STRUCTURES
//------------------------------------------------------------------------

//
// We do not write ZIP-compressed ZDoom nodes
//
using bsp_type_t = enum bsp_type_e : uint8_t
{
  BSP_VANILLA,
  BSP_DEEPBSPV4,
  BSP_XNOD,
  BSP_XGLN,
  BSP_XGL2,
  BSP_XGL3,
};

// Obviously, vanilla did not include any magic headers
static constexpr const char *BSP_MAGIC_DEEPBSPV4 = "xNd4\0\0\0\0";
static constexpr const char *BSP_MAGIC_XNOD = "XNOD";
static constexpr const char *BSP_MAGIC_XGLN = "XGLN";
static constexpr const char *BSP_MAGIC_XGL2 = "XGL2";
static constexpr const char *BSP_MAGIC_XGL3 = "XGL3";

// Upper-most bit is used for distinguishing tree children as either nodes or sub-sectors
// All known non-vanilla formats are know to use 32bit indexes
static constexpr size_t LIMIT_VANILLA_NODE = INT16_MAX;
static constexpr size_t LIMIT_VANILLA_SUBSEC = INT16_MAX;
static constexpr size_t LIMIT_VANILLA_SEG = UINT16_MAX;

//
// Vanilla blockmap
//
using raw_bbox_t = struct raw_bbox_s
{
  int16_t maxy;
  int16_t miny;
  int16_t minx;
  int16_t maxx;
} PACKEDATTR;

using raw_blockmap_header_t = struct raw_blockmap_header_s
{
  int16_t x_origin, y_origin;
  int16_t x_blocks, y_blocks;
} PACKEDATTR;

//
// Vanilla BSP
//
using raw_node_vanilla_t = struct raw_node_vanilla_s
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint16_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

using raw_subsec_vanilla_t = struct raw_subsec_vanilla_s
{
  uint16_t num;   // number of Segs in this Sub-Sector
  uint16_t first; // first Seg
} PACKEDATTR;

using raw_seg_vanilla_t = struct raw_seg_vanilla_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t angle;   // angle (0 = east, 16384 = north, ...)
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  uint16_t dist;    // distance from starting point
} PACKEDATTR;

//
// DeepSea BSP
// * compared to vanilla, some types were raise to 32bit
//
using raw_node_deepbspv4_t = struct raw_node_deepbspv4_s
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

using raw_subsec_deepbspv4_t = struct raw_subsec_deepbspv4_s
{
  uint16_t num;   // number of Segs in this Sub-Sector
  uint32_t first; // first Seg
} PACKEDATTR;

using raw_seg_deepbspv4_t = struct raw_seg_deepbspv4_s
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t angle;   // angle (0 = east, 16384 = north, ...)
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  uint16_t dist;    // distance from starting point
} PACKEDATTR;

//
// ZDoom BSP
// * compared to vanilla, some types were raise to 32bit
// * each version (XNOD->XGLN->XGL2->XGL3) builds on top of the previous
//
using raw_xnod_vertex_t = struct raw_xnod_vertex_s
{
  int32_t x;
  int32_t y;
} PACKEDATTR;

using raw_xnod_node_t = struct raw_xnod_node_s
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

using raw_xnod_subsec_t = struct raw_xnod_subsec_s
{
  uint32_t segnum;
  // NOTE : no "first" value, segs must be contiguous and appear
  //        in an order dictated by the subsector list, e.g. all
  //        segs of the second subsector must appear directly after
  //        all segs of the first subsector.
} PACKEDATTR;

using raw_xnod_seg_t = struct raw_xnod_seg_s
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

// XGLN segs use the same type definition as XNOD segs, with slightly
// different semantics for mini-segs

using raw_xgln_seg_t = struct raw_xgln_seg_s
{
  uint32_t vertex;  // from this vertex...
  uint32_t partner; // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

using raw_xgl2_seg_t = struct raw_xgl2_seg_s
{
  uint32_t vertex;  // from this vertex...
  uint32_t partner; // ... to this vertex
  uint32_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

using raw_xgl3_node_t = struct raw_xgl3_node_s
{
  int32_t x, y;         // starting point
  int32_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

/* ----- Graphical structures ---------------------- */

using raw_patchdef_t = struct raw_patchdef_s
{
  int16_t x_origin;
  int16_t y_origin;
  uint16_t pname;    // index into PNAMES
  uint16_t stepdir;  // NOT USED
  uint16_t colormap; // NOT USED
} PACKEDATTR;

using raw_strife_patchdef_t = struct raw_strife_patchdef_s
{
  int16_t x_origin;
  int16_t y_origin;
  uint16_t pname; // index into PNAMES
} PACKEDATTR;

// Texture definition.
//
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
//
using raw_texture_t = struct raw_texture_s
{
  char name[8];
  uint32_t masked; // NOT USED
  uint16_t width;
  uint16_t height;
  uint16_t column_dir[2]; // NOT USED
  uint16_t patch_count;
  raw_patchdef_t patches[1];
} PACKEDATTR;

using raw_strife_texture_t = struct raw_strife_texture_s
{
  char name[8];
  uint32_t masked; // NOT USED
  uint16_t width;
  uint16_t height;
  uint16_t patch_count;
  raw_strife_patchdef_t patches[1];
} PACKEDATTR;

// Patches.
//
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
//
using patch_t = struct patch_s
{
  int16_t width;         // bounding box size
  int16_t height;        //
  int16_t leftoffset;    // pixels to the left of origin
  int16_t topoffset;     // pixels below the origin
  uint32_t columnofs[1]; // only [width] used
} PACKEDATTR;

//
// LineDef attributes.
//

using compatible_lineflag_t = enum compatible_lineflag_e : uint16_t
{
  MLF_BLOCKING = BIT(0),      // Solid, is an obstacle
  MLF_BLOCKMONSTERS = BIT(1), // Blocks monsters only
  MLF_TWOSIDED = BIT(2),      // Backside will not be present at all if not two sided

  // If a texture is pegged, the texture will have
  // the end exposed to air held constant at the
  // top or bottom of the texture (stairs or pulled
  // down things) and will move with a height change
  // of one of the neighbor sectors.
  //
  // Unpegged textures allways have the first row of
  // the texture at the top pixel of the line for both
  // top and bottom textures (use next to windows).

  MLF_UPPERUNPEGGED = BIT(3), // Upper texture unpegged
  MLF_LOWERUNPEGGED = BIT(4), // Lower texture unpegged
  MLF_SECRET = BIT(5),        // In AutoMap: don't map as two sided: IT'S A SECRET!
  MLF_SOUNDBLOCK = BIT(6),    // Sound rendering: don't let sound cross two of these
  MLF_DONTDRAW = BIT(7),      // Don't draw on the automap at all
  MLF_MAPPED = BIT(8),        // Set as if already seen, thus drawn in automap
  MLF_PASSUSE = BIT(9),       // Allow multiple lines to be pushed simultaneously.
  MLF_3DMIDTEX = BIT(10),     // Solid middle texture
  MLF_RESERVED = BIT(11),     // comp_reservedlineflag
  MLF_BLOCKGROUND = BIT(12),  // Block Grounded Monster
  MLF_BLOCKPLAYERS = BIT(13), // Block Players Only
};

// first few flags are same as DOOM above
using hexen_lineflag_e = enum : uint16_t
{
  MLF_HEXEN_REPEATABLE = BIT(9),
  MLF_HEXEN_ACTIVATION = BIT(10) | BIT(11) | BIT(12),
};

// these are supported by ZDoom (and derived ports)
using zdoom_lineflag_t = enum zdoom_lineflag_e : uint16_t
{
  MLF_ZDOOM_MONCANACTIVATE = BIT(13),
  MLF_ZDOOM_BLOCKPLAYERS = BIT(14),
  MLF_ZDOOM_BLOCKEVERYTHING = BIT(15),
};

static constexpr uint32_t BOOM_GENLINE_FIRST = 0x2f80;
static constexpr uint32_t BOOM_GENLINE_LAST = 0x7fff;

static inline constexpr bool IsGeneralizedSpecial(uint32_t special)
{
  return special >= BOOM_GENLINE_FIRST && special <= BOOM_GENLINE_LAST;
}

using hexen_activation_t = enum hexen_activation_e
{
  SPAC_Cross = 0,   // when line is crossed (W1 / WR)
  SPAC_Use = 1,     // when line is used    (S1 / SR)
  SPAC_Monster = 2, // when monster walks over line
  SPAC_Impact = 3,  // when bullet/projectile hits line (G1 / GR)
  SPAC_Push = 4,    // when line is bumped (player is stopped)
  SPAC_PCross = 5,  // when projectile crosses the line
};

// The power of node building manipulation!
using bsp_specials_t = enum bsp_specials_e : uint32_t
{
  Special_VanillaScroll = 48,

  Special_RemoteScroll = 1048, // potentialy lossy? -- i.e alters user-provided lumps?

  Special_ChangeStartVertex = 1078,
  Special_ChangeEndVertex,

  Special_RotateDegrees,     // only vanilla segs encode angle
  Special_RotateDegreesHard, //
  Special_RotateAngleT,      //
  Special_RotateAngleTHard,  //

  Special_DoNotRenderBackSeg,  // not supported on SSECTORS' XGL nodes
  Special_DoNotRenderFrontSeg, //
  Special_DoNotRenderAnySeg,   //

  Special_Unknown1, // related to splitting?
  Special_Unknown2, // line tag value becomes seg's associated line index? why?
};

using bsp_tags_t = enum bsp_tags_e
{
  Tag_DoNotRender = 998,
  Tag_NoBlockmap = 999,
};

//
// Sector attributes.
//

using compatible_sectorflag_t = enum compatible_sectorflag_e : uint16_t
{
  SF_TypeMask = 31,
  SF_DamageMask = BIT(5) | BIT(6),

  SF_Secret = BIT(7),
  SF_Friction = BIT(8),
  SF_Wind = BIT(9),
  SF_NoSounds = BIT(10),
  SF_QuietPlane = BIT(11),

  SF_AltDeathMode = BIT(12),
  SF_MonsterDeath = BIT(13),
};

static constexpr uint32_t SF_BoomFlags = SF_DamageMask | SF_Secret | SF_Friction | SF_Wind;
static constexpr uint32_t SF_MBF21Flags = SF_DamageMask | SF_Secret | SF_Friction | SF_Wind | SF_AltDeathMode | SF_MonsterDeath;

//
// Thing attributes.
//

using thing_option_t = enum thing_option_e : uint16_t
{
  MTF_Easy = BIT(0),
  MTF_Medium = BIT(1),
  MTF_Hard = BIT(2),
  MTF_Ambush = BIT(3),

  MTF_Not_SP = BIT(4),
  MTF_Not_DM = BIT(5),
  MTF_Not_COOP = BIT(6),
  MTF_Friend = BIT(7),
};

static constexpr uint32_t MTF_EXFLOOR_MASK = 0x3C00;
static constexpr uint32_t MTF_EXFLOOR_SHIFT = 10;

using hexen_option_t = enum hexen_option_e : uint16_t
{
  MTF_Hexen_Easy = BIT(0),
  MTF_Hexen_Medium = BIT(1),
  MTF_Hexen_Hard = BIT(2),
  MTF_Hexen_Ambush = BIT(3),

  MTF_Hexen_Dormant = BIT(4),
  MTF_Hexen_Fighter = BIT(5),
  MTF_Hexen_Cleric = BIT(6),
  MTF_Hexen_Mage = BIT(7),

  MTF_Hexen_SP = BIT(8),
  MTF_Hexen_COOP = BIT(9),
  MTF_Hexen_DM = BIT(10),
};

//
// Polyobject stuff
//
static constexpr uint32_t HEXTYPE_POLY_START = 1;
static constexpr uint32_t HEXTYPE_POLY_EXPLICIT = 5;

// -JL- Hexen polyobj thing types
static constexpr uint32_t PO_ANCHOR_TYPE = 3000;
static constexpr uint32_t PO_SPAWN_TYPE = 3001;
static constexpr uint32_t PO_SPAWNCRUSH_TYPE = 3002;

// -JL- ZDoom polyobj thing types
static constexpr uint32_t ZDOOM_PO_ANCHOR_TYPE = 9300;
static constexpr uint32_t ZDOOM_PO_SPAWN_TYPE = 9301;
static constexpr uint32_t ZDOOM_PO_SPAWNCRUSH_TYPE = 9302;
