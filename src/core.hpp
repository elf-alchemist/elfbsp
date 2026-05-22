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
#include <chrono>

#include "zlib-ng.h"

//
//  OS support
//

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

constexpr auto WINDOWS = true;
constexpr auto MACOS = false;
constexpr auto LINUX = false;

#elif defined(__APPLE__)

constexpr auto WINDOWS = false;
constexpr auto MACOS = true;
constexpr auto LINUX = false;

#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)

constexpr auto WINDOWS = false;
constexpr auto MACOS = false;
constexpr auto LINUX = true;

#endif

constexpr char PATH_SEP_CH = (WINDOWS) ? ';' : ':';
constexpr char DIR_SEP_CH = (WINDOWS) ? '/' : '\\';

//
// The packed attribute forces structures to be packed into the minimum
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

// -Elf- updated, pulled from Chocolate Doom

#if defined(__clang__) || defined(__GNUC__)

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
constexpr auto ENDIAN_BIG = (std::endian::native == std::endian::big);
constexpr auto ENDIAN_LITTLE = !ENDIAN_BIG;

template <typename T>
constexpr T byteswap(T value) noexcept
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

template <typename T>
constexpr T GetLittleEndian(T value)
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

template <typename T>
constexpr T GetBigEndian(T value)
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

template <typename T>
constexpr void RaiseValue(T &var, T value)
{
  var = std::max(var, value);
}

// sized types
using byte = uint8_t;
using fixed_t = int32_t;
using long_angle_t = uint32_t;
using short_angle_t = uint16_t;

// misc constants
constexpr long_angle_t LONG_ANGLE_45 = 0x20000000;
constexpr long_angle_t LONG_ANGLE_1 = (LONG_ANGLE_45 / 45);

constexpr uint32_t FRACBITS = 16;
constexpr fixed_t FRACUNIT = (1 << FRACBITS);
constexpr double FRACFACTOR = FRACUNIT;

constexpr size_t ZERO_INDEX = static_cast<size_t>(0);
constexpr uint16_t ZERO_INDEX_INT16 = static_cast<uint16_t>(0);
constexpr uint32_t ZERO_INDEX_INT32 = static_cast<uint32_t>(0);

constexpr size_t NO_INDEX = static_cast<size_t>(-1);
constexpr uint16_t NO_INDEX_INT16 = static_cast<uint16_t>(-1);
constexpr uint32_t NO_INDEX_INT32 = static_cast<uint32_t>(-1);

constexpr size_t WAD_LUMP_NAME = 8;

constexpr size_t MSG_BUFFER_LENGTH = 1024;

// bitflags
constexpr uint32_t BIT(const uint32_t x)
{
  return (1u << x);
}

constexpr bool HAS_BIT(const uint32_t x, const uint32_t y)
{
  return (x & y) != 0;
}

constexpr bool HAS_ALL(const uint32_t x, const uint32_t y)
{
  return (x & y) == y;
}

constexpr bool HAS_NONE(const uint32_t x, const uint32_t y)
{
  return (x & y) == 0;
}

// doom's 32bit 16.16 fixed point
constexpr fixed_t IntToFixed(const int32_t x)
{
  return x << FRACBITS;
}

constexpr int32_t FixedToInt(const fixed_t x)
{
  return x >> FRACBITS;
}

constexpr fixed_t ShortToFixed(const int16_t x)
{
  return x << FRACBITS;
}

constexpr int16_t FixedToShort(const fixed_t x)
{
  return x >> FRACBITS;
}

constexpr double FixedToFloat(const fixed_t x)
{
  return (static_cast<double>(x) / FRACFACTOR);
}

constexpr fixed_t FloatToFixed(double x)
{
  return static_cast<fixed_t>(x * FRACFACTOR);
}

constexpr int16_t FloatToShort(double x)
{
  return static_cast<int16_t>(floor(x));
}

constexpr double ShortToFloat(int16_t x)
{
  return static_cast<double>(x);
}

constexpr uint16_t IndexToShort(size_t x)
{
  return static_cast<uint16_t>(x);
}

constexpr uint32_t IndexToInt(size_t x)
{
  return static_cast<uint32_t>(x);
}

// binary angular measurement, BAM!
constexpr long_angle_t DegreesToLongBAM(const double x)
{
  return static_cast<long_angle_t>(LONG_ANGLE_1 * x);
}

constexpr short_angle_t DegreesToShortBAM(const double x)
{
  return static_cast<short_angle_t>(static_cast<long_angle_t>(LONG_ANGLE_1 * x) >> FRACBITS);
}

//------------------------------------------------------------------------
// STRING STUFF
//------------------------------------------------------------------------

using log_level_t = enum
{
  LOG_NORMAL,
  LOG_DEBUG,
  LOG_WARN,
  LOG_ERROR,
};

using debug_t = enum : uint32_t
{
  DEBUG_NONE = 0,
  DEBUG_BLOCKMAP = BIT(0),
  DEBUG_REJECT = BIT(1),
  DEBUG_LOAD = BIT(2),
  DEBUG_BSP = BIT(3),
  DEBUG_WALLTIPS = BIT(4),
  DEBUG_POLYOBJ = BIT(5),
  DEBUG_OVERLAPS = BIT(6),
  DEBUG_PICKNODE = BIT(7),
  DEBUG_SPLIT = BIT(8),
  DEBUG_CUTLIST = BIT(9),
  DEBUG_BUILDER = BIT(10),
  DEBUG_SORTER = BIT(11),
  DEBUG_SUBSEC = BIT(12),
  DEBUG_WAD = BIT(13),
};

// Safe, portable vsnprintf().
inline int32_t PRINTF_ATTR(2, 0) M_vsnprintf(char *buf, const char *s, va_list args)
{
  // Windows (and other OSes?) has a vsnprintf() that doesn't always
  // append a trailing \0. So we must do it, and write into a buffer
  // that is one byte shorter; otherwise this function is unsafe.
  int32_t result = vsnprintf(buf, MSG_BUFFER_LENGTH, s, args);

  // If truncated, change the final char in the buffer to a \0.
  // A negative result indicates a truncated buffer on Windows.
  if (result < 0 || result >= static_cast<int32_t>(MSG_BUFFER_LENGTH))
  {
    buf[MSG_BUFFER_LENGTH - 1] = '\0';
    result = static_cast<int32_t>(MSG_BUFFER_LENGTH) - 1;
  }

  return result;
}

//
//  show a message
//
inline void PRINTF_ATTR(2, 3) PrintLine(const log_level_t level, const char *fmt, ...)
{
  FILE *const stream = (level == LOG_NORMAL) ? stdout : stderr;
  char buffer[MSG_BUFFER_LENGTH];

  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  M_vsnprintf(buffer, fmt, arg_ptr);
  va_end(arg_ptr);

  buffer[MSG_BUFFER_LENGTH - 1] = '\0';

  fprintf(stream, "%s\n", buffer);
  fflush(stream);

  if (level == LOG_ERROR)
  {
    exit(3);
  }
}

//
// Assertion macros
//
#define SYS_ASSERT(cond) \
  (cond) ? (void)0 : PrintLine(LOG_ERROR, "ERROR: Assertion failed! In function %s (%s:%d)", __func__, __FILE__, __LINE__);

//
// Version info
//

#define VERSION_INFO                            \
  PROJECT_NAME " version " PROJECT_VERSION "\n" \
               "Branch: " GIT_BRANCH "\n"       \
               "Timestamp: " PROJECT_DATE "\n"  \
               "Revision: " GIT_REV_COUNT "\n"  \
               "Commit: " GIT_SHORT_HASH "\n"

//------------------------------------------------------------------------
// MEMORY ALLOCATION
//------------------------------------------------------------------------

//
// Allocate memory with error checking.  Zeros the memory.
//
template <typename T>
constexpr T *UtilCalloc(const size_t size)
{
  T *ret = static_cast<T *>(calloc(1, size));

  if (!ret)
  {
    PrintLine(LOG_ERROR, "ERROR: Out of memory (cannot allocate %zu bytes)", size);
  }

  return ret;
}

//
// Reallocate memory with error checking.
//
template <typename T>
constexpr T *UtilRealloc(T *old, const size_t size)
{
  T *ret = static_cast<T *>(realloc(old, size));

  if (!ret)
  {
    PrintLine(LOG_ERROR, "ERROR: Out of memory (cannot reallocate %zu bytes)", size);
  }

  return ret;
}

//
// Free the memory with error checking.
//
template <typename T>
constexpr void UtilFree(T *data)
{
  if (data == nullptr)
  {
    PrintLine(LOG_ERROR, "ERROR: Trying to free a nullptr");
  }

  free(data);
}

//------------------------------------------------------------------------
// STRINGS
//------------------------------------------------------------------------

//
// a case-insensitive compare
//
inline int32_t StringCaseCmp(const char *s1, const char *s2)
{
  SYS_ASSERT(s1 && s2);
  return strcasecmp(s1, s2);
}

//
// a case-insensitive compare
//
inline int32_t StringCaseCmpMax(const char *s1, const char *s2, size_t len)
{
  SYS_ASSERT(s1 && s2 && len);
  return strncasecmp(s1, s2, len);
}

//------------------------------------------------------------------------
//  FILENAMES
//------------------------------------------------------------------------

inline bool HasExtension(const char *filename)
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
// FindExtension
//
// Return offset of the '.', or NO_INDEX when no extension was found.
//
inline size_t FindExtension(std::string_view filename)
{
  const size_t len = strlen(filename.data());
  if (len == 0)
  {
    return NO_INDEX;
  }

  for (size_t pos = len; pos-- > 0;)
  {
    const char ch = filename[pos];

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
inline double ComputeAngle(double dx, double dy)
{
  const double angle = atan2(dy, dx) * 180.0 / M_PI;
  return angle + (angle < 0.0) * 360.0;
}

inline short_angle_t ComputeAngle_BAM(double dx, double dy)
{
  double angle = atan2(dy, dx) * 180.0 / M_PI;
  angle = angle + (angle < 0.0) * 360.0;
  return static_cast<short_angle_t>(floor(angle * FRACFACTOR / 360.0 + 0.5));
}

//------------------------------------------------------------------------
// LEVEL STRUCTURES
//------------------------------------------------------------------------

using map_format_t = enum map_format_e
{
  MapFormat_INVALID = 0,
  MapFormat_Doom,
  MapFormat_Hexen,
  MapFormat_Doom64,
  MapFormat_UDMF,
};

static std::string_view format_name[] = {"ERROR", "Doom Format", "Hexen Format", "Doom64 Format", "UDMF"};

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

  D64_LEAFS = LL_BLOCKMAP + 1, // PSX/N64 hardware rendering
  D64_LIGHTS,                  // Colored sectors
  D64_MACROS,                  // BLAM
  D64_SCRIPTS,                 // BLAM

  LL_TEXTMAP = LL_LABEL + 1, // UDMF geometry
  LL_ENDMAP,                 // UDMF end marker

  GL_LABEL = 0,
  GL_VERT,
  GL_SEGS,
  GL_SSECT,
  GL_NODES,
  GL_PVS,

  LM_LABEL = 0,
  LM_CELLS,
  LM_SUN,
  LM_SURFS,
  LM_TXCRD,
  LM_LMAPS,
};

static constexpr uint32_t MAX_LUMPS_IN_A_LEVEL = 21;

using raw_vertex_short_t = struct raw_vertex_short_s
{
  int16_t x;
  int16_t y;
} PACKEDATTR;

using raw_vertex_fixed_t = struct raw_vertex_fixed_s
{
  fixed_t x;
  fixed_t y;
} PACKEDATTR;

using raw_linedef_doom_t = struct raw_linedef_doom_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type (0 for none, 97 for teleporter, etc)
  uint16_t tag;     // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR;

using raw_sidedef_doom_t = struct raw_sidedef_doom_s
{
  int16_t x_offset;  // X offset for texture
  int16_t y_offset;  // Y offset for texture
  char upper_tex[8]; // texture name for the part above
  char lower_tex[8]; // texture name for the part below
  char mid_tex[8];   // texture name for the regular part
  uint16_t sector;   // adjacent sector
} PACKEDATTR;

using raw_sector_doom_t = struct raw_sector_doom_s
{
  int16_t floorh;    // floor height
  int16_t ceilh;     // ceiling height
  char floor_tex[8]; // floor texture
  char ceil_tex[8];  // ceiling texture
  uint16_t light;    // light level (0-255)
  uint16_t type;     // special type (0 = normal, 9 = secret, ...)
  uint16_t tag;      // sector activated by a linedef with same tag
} PACKEDATTR;

using raw_thing_doom_t = struct raw_thing_doom_s
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR;

// -JL- Hexen definition
using raw_linedef_hexen_t = struct raw_linedef_hexen_s
{
  uint16_t start;  // from this vertex...
  uint16_t end;    // ... to this vertex
  uint16_t flags;  // linedef flags (impassible, etc)
  uint8_t special; // special type
  uint8_t args[5]; // special arguments
  uint16_t right;  // right sidedef
  uint16_t left;   // left sidedef
} PACKEDATTR;

using raw_thing_hexen_t = struct raw_thing_hexen_s
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

// -Elf- Doom64 definitions
// Some are shared with PSX Doom's and PSX Final Doom's formats
// but we don't support those

using raw_thing_doom64_t = struct raw_thing_doom64_s
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t z;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR;

using raw_linedef_doom64_t = struct raw_linedef_doom64_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint32_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type
  uint16_t tag;     // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR;

using raw_sidedef_doom64_t = struct raw_sidedef_doom64_s
{
  int16_t x_offset;   // X offset for texture
  int16_t y_offset;   // Y offset for texture
  uint16_t upper_tex; // texture index for the part above
  uint16_t lower_tex; // texture index for the part below
  uint16_t mid_tex;   // texture index for the regular part
  uint16_t sector;    // adjacent sector
} PACKEDATTR;

using raw_sector_doom64_t = struct raw_sector_doom64_s
{
  int16_t floorh;     // floor height
  int16_t ceilh;      // ceiling height
  uint16_t floor_tex; // floor texture
  uint16_t ceil_tex;  // ceiling texture
  uint16_t colors[5]; // hardware rendering colors, LIGHTS
  uint16_t special;   // special type
  uint16_t tag;       // tag number
  uint16_t flags;     // reverb, damage, water, etc
};

//------------------------------------------------------------------------
// BSP TREE STRUCTURES
//------------------------------------------------------------------------

using bsp_format_t = enum bsp_format_e : uint8_t
{
  BSP_DoomBSP,
  BSP_DeePBSPV4,
  BSP_XNOD,
  BSP_XGLN,
  BSP_XGL2,
  BSP_XGL3,

  BSP_MIN = BSP_DoomBSP,
  BSP_MAX = BSP_XGL3,
};

using bmap_format_t = enum bmap_format_e : uint8_t
{
  BMAP_DoomBSP,
  BMAP_XBM1,

  BMAP_MIN = BMAP_DoomBSP,
  BMAP_MAX = BMAP_XBM1,
};

//
// Binary format upper bounds.
// Some of these, namely the BSP tree indexes, are addressed by using later formats, such as DeePBSPV4, etc
//
// * No known ports seem to reserve 0xFFFF for vertices
// * The linedef index 0xFFFF is reserved for minisegs in XGLN/XGL2/XGL3 nodes
// * The sidedef index 0xFFFF is reserved to mean "no side" in DOOM map format
// * No known ports seem to reserve 0xFFFF for sectors
//
// * Nodes and subsectors share the same index slot, distinguished only by the high bit
// * No known additional limits exist for segments
//

constexpr size_t LIMIT_VERT = UINT16_MAX;
constexpr size_t LIMIT_LINE = UINT16_MAX - 1;
constexpr size_t LIMIT_SIDE = UINT16_MAX - 1;
constexpr size_t LIMIT_SECTOR = UINT16_MAX;

constexpr size_t LIMIT_NODE = INT16_MAX;
constexpr size_t LIMIT_SUBSEC = INT16_MAX;
constexpr size_t LIMIT_SEG = UINT16_MAX;

constexpr size_t LIMIT_BMAP_INDEX = UINT16_MAX;

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
  uint16_t x_blocks, y_blocks;
} PACKEDATTR;

using raw_blockmap_xbm1_header_t = struct raw_blockmap_xbm1_header_s
{
  fixed_t x_origin, y_origin;
  uint32_t x_blocks, y_blocks;
} PACKEDATTR;

// Fail way earlier
#define static_size(x, y) static_assert(sizeof(x) == y, "Size mismatch for '" #x "'. Should be " #y ".")

static_size(raw_vertex_short_t, 4);
static_size(raw_vertex_fixed_t, 8);

static_assert(sizeof(raw_linedef_doom_t) == 14, "Size mismatch for 'raw_linedef_doom_t'. Should be 14.");
static_assert(sizeof(raw_sidedef_doom_t) == 30, "Size mismatch for 'raw_sidedef_doom_t'. Should be 30.");
static_assert(sizeof(raw_sector_doom_t) == 26, "Size mismatch for 'raw_sector_doom_t'. Should be 26.");
static_assert(sizeof(raw_thing_doom_t) == 10, "Size mismatch for 'raw_thing_doom_t'. Should be 10.");
static_assert(sizeof(raw_linedef_hexen_t) == 16, "Size mismatch for 'raw_linedef_hexen_t'. Should be 16.");
static_assert(sizeof(raw_thing_hexen_t) == 20, "Size mismatch for 'raw_thing_hexen_t'. Should be 20.");
static_assert(sizeof(raw_thing_doom64_t) == 12, "Size mismatch for 'raw_thing_doom64_t'. Should be 12.");
static_assert(sizeof(raw_linedef_doom64_t) == 16, "Size mismatch for 'raw_linedef_doom64_t'. Should be 16.");
static_assert(sizeof(raw_sidedef_doom64_t) == 12, "Size mismatch for 'raw_sidedef_doom64_t'. Should be 12.");
static_assert(sizeof(raw_sector_doom64_t) == 24, "Size mismatch for 'raw_sector_doom64_t'. Should be 24.");
static_assert(sizeof(raw_bbox_t) == 8, "Size mismatch for 'raw_bbox_t'. Should be 8.");
static_assert(sizeof(raw_blockmap_header_t) == 8, "Size mismatch for 'raw_blockmap_header_t'. Should be 8.");
static_assert(sizeof(raw_blockmap_xbm1_header_t) == 16, "Size mismatch for 'raw_blockmap_xbm1_header_t'. Should be 16.");

//
// LineDef attributes.
//

using lineflag_doom_t = enum lineflag_doom_e : uint16_t
{
  MLF_BLOCKING = BIT(0),      // Solid, is an obstacle
  MLF_BLOCKMONSTERS = BIT(1), // Blocks monsters only
  MLF_TWOSIDED = BIT(2),      // Backside will not be present at all if not two sided
  MLF_UPPERUNPEGGED = BIT(3), // Upper texture unpegged
  MLF_LOWERUNPEGGED = BIT(4), // Lower texture unpegged
  MLF_SECRET = BIT(5),        // In AutoMap: don't map as two sided: IT'S A SECRET!
  MLF_SOUNDBLOCK = BIT(6),    // Sound rendering: don't let sound cross two of these
  MLF_DONTDRAW = BIT(7),      // Don't draw on the automap at all
  MLF_MAPPED = BIT(8),        // Set as if already seen, thus drawn in automap

  // Boom lineage
  MLF_BOOM_PASSUSE = BIT(9),       // Allow multiple lines to be pushed simultaneously.
  MLF_BOOM_3DMIDTEX = BIT(10),     // Solid middle texture
  MLF_BOOM_RESERVED = BIT(11),     // comp_reservedlineflag
  MLF_BOOM_BLOCKGROUND = BIT(12),  // Block Grounded Monster
  MLF_BOOM_BLOCKPLAYERS = BIT(13), // Block Players Only
};

using lineflag_hexen_t = enum lineflag_hexen_e : uint16_t
{
  // Inherited from vanilla Doom
  MLF_HEXEN_BLOCKING = MLF_BLOCKING,
  MLF_HEXEN_BLOCKMONSTERS = MLF_BLOCKMONSTERS,
  MLF_HEXEN_TWOSIDED = MLF_TWOSIDED,
  MLF_HEXEN_UPPERUNPEGGED = MLF_UPPERUNPEGGED,
  MLF_HEXEN_LOWERUNPEGGED = MLF_LOWERUNPEGGED,
  MLF_HEXEN_SECRET = MLF_SECRET,
  MLF_HEXEN_SOUNDBLOCK = MLF_SOUNDBLOCK,
  MLF_HEXEN_DONTDRAW = MLF_DONTDRAW,
  MLF_HEXEN_MAPPED = MLF_MAPPED,

  // Hexen-original
  MLF_HEXEN_REPEATABLE = BIT(9),
  MLF_HEXEN_ACTIVATION = BIT(10) | BIT(11) | BIT(12),

  // Source ports, mostly ZDoom
  MLF_HEXEN_MONCANACTIVATE = BIT(13),
  MLF_HEXEN_BLOCKPLAYERS = BIT(14),
  MLF_HEXEN_BLOCKEVERYTHING = BIT(15),
};

// See https://www.doom64.com/maps/linedefs.html
using lineflag_doom64_t = enum lineflag_doom64_e : uint32_t
{
  MLF_DOOM64_BLOCKING = MLF_BLOCKING,
  MLF_DOOM64_BLOCKMONSTERS = MLF_BLOCKMONSTERS,
  MLF_DOOM64_TWOSIDED = MLF_TWOSIDED,
  MLF_DOOM64_UPPERUNPEGGED = MLF_UPPERUNPEGGED,
  MLF_DOOM64_LOWERUNPEGGED = MLF_LOWERUNPEGGED,
  MLF_DOOM64_AUTOMAP_ONESIDED = BIT(5),
  MLF_DOOM64_BLOCKSOUND = BIT(6),
  MLF_DOOM64_AUTOMAP_HIDE = BIT(7),
  MLF_DOOM64_AUTOMAP_SHOW = BIT(8),
  MLF_DOOM64_RENDER_MIDTEX = BIT(9),
  MLF_DOOM64_NO_OCCLUSION = BIT(10),
  MLF_DOOM64_BLOCKPROJ = BIT(11),
  MLF_DOOM64_DEAD_TRIGGER = BIT(12),
  MLF_DOOM64_DECAL_UPPER = BIT(13),
  MLF_DOOM64_DECAL_LOWER = BIT(14),
  MLF_DOOM64_SWITCH_UPPER = BIT(15),
  MLF_DOOM64_SWITCH_LOWER = BIT(16),
  MLF_DOOM64_SCROLL_LEFT = BIT(17),
  MLF_DOOM64_SCROLL_RIGHT = BIT(18),
  MLF_DOOM64_SCROLL_UP = BIT(19),
  MLF_DOOM64_SCROLL_DOWN = BIT(20),
  MLF_DOOM64_PEG_COLOR_UPPER = BIT(21),
  MLF_DOOM64_PEG_COLOR_LOWER = BIT(22),
  MLF_DOOM64_UPPER_WALL_COLOR_BLEND = BIT(23),
  MLF_DOOM64_TRIGGER_FRONT = BIT(24),
  MLF_DOOM64_AUTOMAP_HIDE_SPECIAL = BIT(25),
  MLF_DOOM64_FLIP_UPPER_PEGGED_COLOR = BIT(26),
  MLF_DOOM64_BLOCKPLAYER = BIT(27),  // Doom 64 EX+ & EX+ Enhanced
  MLF_DOOM64_NO_BLOCKMAP = BIT(28),  // BSP64 Enhanced
  MLF_DOOM64_COMB_SIDEDEF = BIT(29), // BSP64 Enhanced
  MLF_DOOM64_MIRROR_HORI = BIT(30),
  MLF_DOOM64_MIRROR_VERT = BIT(31),
};

using line_spac_hexen_t = enum line_spac_hexen_e
{
  SPAC_Cross = 0,   // when line is crossed (W1 / WR)
  SPAC_Use = 1,     // when line is used    (S1 / SR)
  SPAC_Monster = 2, // when monster walks over line
  SPAC_Impact = 3,  // when bullet/projectile hits line (G1 / GR)
  SPAC_Push = 4,    // when line is bumped (player is stopped)
  SPAC_PCross = 5,  // when projectile crosses the line
};

using line_special_doom_t = enum line_special_doom_e : int32_t
{
  Special_ScrollLeft = 48,
  Special_ScrollRight = 85,

  Special_RemoteScrollLeft = 1048,
  Special_RemoteScrollRight,

  Special_ChangeStartVertex = 1078,
  Special_ChangeEndVertex,

  Special_RotateRelativeDegrees,
  Special_RotateAbsoluteDegrees,
  Special_RotateRelativeBAM,
  Special_RotateAbsoluteBAM,

  Special_DoNotRenderSegmentBack,
  Special_DoNotRenderSegmentFront,
  Special_DoNotRenderSegmentBoth,

  Special_DoNotSplitSeg,
  Special_ChangeSegLine,
};

using line_tags_doom_t = enum line_tags_doom_e : uint16_t
{
  Tag_NoReject = 997,
  Tag_DoNotRender = 998,
  Tag_NoBlockmap = 999,
};

using line_special_hexen_t = enum line_special_hexen_e : int32_t
{
  // We need to be aware of parameterized actions 1 and 5
  Polyobj_StartLine = 1,
  Polyobj_RotateLeft,
  Polyobj_RotateRight,
  Polyobj_Move,
  Polyobj_ExplicitLine,
  Polyobj_MoveTimes8,
  Polyobj_DoorSwing,
  Polyobj_DoorSlide,

  Polyobj_OR_MoveToSpot = 59,

  PolyObj_MoveToSpot = 86,
  PolyObj_Stop,
  PolyObj_MoveTo,
  PolyObj_OR_MoveTo,
  PolyObj_OR_RotateLeft,
  PolyObj_OR_RotateRight,
  PolyObj_OR_Move,
  PolyObj_OR_MoveTimes8,

  Polyobj_StopSound = 283,

  // Actual special effects
  BSP_SpecialEffects = 108,

  Unused141 = 141,
  Unused142,
  Unused143,
  Unused144,
  Unused145,
  Unused146,
  Unused147,
  Unused148,
  Unused149,
  Unused150,
  Unused151,
  Unused152,
  Unused153,

  Unused155 = 155,

  Unused161 = 161,
  Unused162,
  Unused163,
  Unused164,
  Unused165,
  Unused166,
  Unused167,
};

// The power of node building manipulation!
using line_effects_t = enum line_effects_e : uint32_t
{
  FX_Nothing = 0,
  FX_TwoSided = BIT(0),
  FX_ZeroLength = BIT(1),
  FX_SelfReferential = BIT(2),

  FX_NoReject = BIT(27),
  FX_DoNotRenderFront = BIT(28),
  FX_DoNotRenderBack = BIT(29),
  FX_DoNotSplitSeg = BIT(30),
  FX_NoBlockmap = BIT(31),
};

// Segment rotation
using seg_rotation_t = enum seg_rotation_e : uint8_t
{
  FX_DoNotRotate = 0,
  FX_RotateRelativeDegrees,
  FX_RotateAbsoluteDegrees,
  FX_RotateRelativeBAM,
  FX_RotateAbsoluteBAM,
};

//
// Sector attributes.
//

using sector_special_doom64_t = enum sector_special_doom64_e : uint16_t
{
  SS_Doom64_NoReject = 666,
};

using sector_effects_t = enum sector_effects_e : uint8_t
{
  FX_Sector_None = 0,
  FX_Sector_NoReject = BIT(0),
};

//
// Thing attributes.
//

using doomednum_t = enum doomednum_e : int16_t
{
  // -JL- ZDoom polyobj thing types
  ZDoom_PolyObj_Anchor = 9300,
  ZDoom_PolyObj_Spawn = 9301,
  ZDoom_PolyObj_SpawnCrush = 9302,
  ZDoom_PolyObj_SpawnHurt = 9303,

  // -JL- Hexen polyobj thing types
  Hexen_PolyObj_Anchor = 3000,
  Hexen_PolyObj_Spawn = 3001,
  Hexen_PolyObj_SpawnCrush = 3002,
  Hexen_PolyObj_SpawnHurt = 3003, // does any port actually handle this?
};

//
// Node Build Information Structure
//

constexpr double SPLIT_COST_MIN = 1.0;
constexpr double SPLIT_COST_DEFAULT = 11.0;
constexpr double SPLIT_COST_MAX = 32.0;

using buildinfo_t = struct buildinfo_s;

extern buildinfo_t config;

struct buildinfo_s
{
  struct
  {
    doomednum_t anchor = ZDoom_PolyObj_Anchor;
    doomednum_t spawn = ZDoom_PolyObj_Spawn;
    doomednum_t spawn_crush = ZDoom_PolyObj_SpawnCrush;
    doomednum_t spawn_hurt = ZDoom_PolyObj_SpawnHurt;
  } polyobj;

  double split_cost = SPLIT_COST_DEFAULT;
  size_t total_warnings = 0;
  uint32_t debug = DEBUG_NONE;

  bsp_format_t bsp_format = bsp_format_t::BSP_XNOD;
  bmap_format_t bmap_format = bmap_format_t::BMAP_DoomBSP;
  bool fast = false;     // use a faster method to pick nodes
  bool analysis = false; // write out CSV for data analysis and visualization
  bool verbose = false;  // this affects how some messages are shown
  bool effects = true;   // disable special effects
  bool compress = false; // compress lumps using zlib
};

struct AnalysisData
{
  size_t vertex = 0;  // Original set of vertices
  size_t lines = 0;   // Original set of linedefs
  size_t sides = 0;   // Original set of sidedefs
  size_t sectors = 0; // Original set of sectors

  size_t bsp_vertex = 0; // BSP-generated set of vertices
  size_t nodes = 0;      // Each node of BSP tree, bounding box for culling
  size_t subsecs = 0;    // Leaves of the tree, points to a set of segs
  size_t segs = 0;       // Segments, actually used for rendering
  size_t splits = 0;     // Difference between segments and sidedefs

  size_t left_depth = 0;      // Maximum depth on the left side of the tree
  size_t right_depth = 0;     // Maximum depth on the right side of the tree
  double average_depth = 0.0; // Arithmetic mean depth
  size_t optimal_depth = 0;   // Optimal depth for a tree of N leaves
  double tree_balance = 0.0;  // Balance factor

  double worst_case_ratio = 0.0; // Best possible tree
  double tree_quality = 0.0;     // Tree's actual figure of merit
};

constexpr const char PRINT_HELP[] = "\n"
                                    "Usage: elfbsp [options...] FILE...\n"
                                    "\n"
                                    "Available options are:\n"
                                    "    -v --verbose       Verbose output, show all warnings\n"
                                    "    -f --fast          Faster partition selection\n"
                                    "    -m --map   XXXX    Control which map(s) are built\n"
                                    "    -c --cost  ##      Cost assigned to seg splits (1-32)\n"
                                    "\n"
                                    "Short options may be mixed, for example: -fbv\n"
                                    "Long options must always begin with a double hyphen\n"
                                    "\n"
                                    "Map names should be full, like E1M3 or MAP24, but a list\n"
                                    "and/or ranges can be specified: MAP01,MAP04-MAP07,MAP12\n";

//
// Benchmark
//

struct Benchmarker
{
  using clock = std::chrono::steady_clock;
  clock::time_point start;
  std::string_view name;
  bool enabled;

  Benchmarker(std::string_view _name, bool _enabled = true)
  {
    if (!_enabled) return;
    enabled = _enabled;
    name = _name;
    start = clock::now();
  };

  ~Benchmarker(void)
  {
    if (!enabled) return;
    auto end = clock::now();
    auto time = std::chrono::duration<double, std::milli>(end - start);
    PrintLine(LOG_NORMAL, "[Benchmarker] '%s' runtime: %.2f ms", name.data(), time.count());
  };
};
