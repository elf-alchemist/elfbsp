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
#include <limits>
#include <vector>

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

constexpr size_t OFF_BY_ONE = 1;

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

enum class Log
{
  NORMAL,
  DEBUG,
  WARN,
};

enum Debug : uint32_t
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
template <typename... Args>
void PrintLine(const Log level, std::format_string<Args...> fmt, Args &&...args)
{
  FILE *const stream = (level == Log::NORMAL) ? stdout : stderr;
  std::string str = std::format(fmt, std::forward<Args>(args)...);

  fprintf(stream, "%s\n", str.c_str());
  fflush(stream);
}

//
// Awh, dangit.
//
template <typename... Args>
std::runtime_error RuntimeException(std::format_string<Args...> fmt, Args &&...args)
{
  std::string str = "ERROR: " + std::format(fmt, std::forward<Args>(args)...);
  return std::runtime_error(str);
}

//
// Assertion macros
//
#define SYS_ASSERT(cond) \
  (cond) ? (void)0 : throw RuntimeException("Assertion failed! In function {} ({})", __func__, __FILE__, __LINE__);

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

  if (!ret) throw std::bad_alloc();

  return ret;
}

//
// Reallocate memory with error checking.
//
template <typename T>
constexpr T *UtilRealloc(T *old, const size_t size)
{
  T *ret = static_cast<T *>(realloc(old, size));

  if (!ret) throw std::bad_alloc();

  return ret;
}

//
// Free the memory with error checking.
//
template <typename T>
constexpr void UtilFree(T *data)
{
  if (data == nullptr) throw RuntimeException("Trying to free a nullptr");

  free(data);
}

//
// Zero init a chunk of memory
//
template <typename T, size_t TSize = sizeof(T)>
constexpr void UtilZero(T *data)
{
  memset(data, 0, TSize);
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
// a case-sensitive compare
//
inline int32_t StringCmp(const char *s1, const char *s2)
{
  SYS_ASSERT(s1 && s2);
  return strcmp(s1, s2) == 0;
}

inline void StringToLower(std::string &str)
{
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
}

inline void StringToUpper(std::string &str)
{
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::toupper(c); });
}

inline char CToUpper(const char c)
{
  return (c >= 'a' && c <= 'z') ? (c + 'A' - 'a') : c;
}

inline void CStringToUpper(char *str)
{
  while (*str)
  {
    *str = CToUpper(*str);
    ++str;
  }
}

inline char CToLower(const char c)
{
  return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

inline void CStringToLower(char *str)
{
  while (*str)
  {
    *str = CToLower(*str);
    ++str;
  }
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

enum class MapFormat
{
  Invalid = 0,
  Doom,
  Hexen,
  Doom64,
  UDMF,
};

static std::string_view format_name[] = {"ERROR", "Doom Format", "Hexen Format", "Doom64 Format", "UDMF"};

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum lump_order_t
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

struct MapVertexIntegral
{
  int16_t x;
  int16_t y;
} PACKEDATTR;

struct MapVertexFractional
{
  fixed_t x;
  fixed_t y;
} PACKEDATTR;

struct MapLinedefDoom
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type (0 for none, 97 for teleporter, etc)
  uint16_t tag;     // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR;

struct MapSidedefDoom
{
  int16_t x_offset;  // X offset for texture
  int16_t y_offset;  // Y offset for texture
  char upper_tex[8]; // texture name for the part above
  char lower_tex[8]; // texture name for the part below
  char mid_tex[8];   // texture name for the regular part
  uint16_t sector;   // adjacent sector
} PACKEDATTR;

struct MapSectorDoom
{
  int16_t floorh;    // floor height
  int16_t ceilh;     // ceiling height
  char floor_tex[8]; // floor texture
  char ceil_tex[8];  // ceiling texture
  uint16_t light;    // light level (0-255)
  uint16_t type;     // special type (0 = normal, 9 = secret, ...)
  uint16_t tag;      // sector activated by a linedef with same tag
} PACKEDATTR;

struct MapThingDoom
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR;

// -JL- Hexen definition
struct MapLinedefHexen
{
  uint16_t start;  // from this vertex...
  uint16_t end;    // ... to this vertex
  uint16_t flags;  // linedef flags (impassible, etc)
  uint8_t special; // special type
  uint8_t args[5]; // special arguments
  uint16_t right;  // right sidedef
  uint16_t left;   // left sidedef
} PACKEDATTR;

struct MapThingHexen
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
//
// ... yet.

struct MapThingDoom64
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t z;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  int16_t type;     // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR;

struct MapLinedefDoom64
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint32_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type
  uint16_t tag;     // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR;

struct MapSidedefDoom64
{
  int16_t x_offset;   // X offset for texture
  int16_t y_offset;   // Y offset for texture
  uint16_t upper_tex; // texture index for the part above
  uint16_t lower_tex; // texture index for the part below
  uint16_t mid_tex;   // texture index for the regular part
  uint16_t sector;    // adjacent sector
} PACKEDATTR;

struct MapSectorDoom64
{
  int16_t floorh;     // floor height
  int16_t ceilh;      // ceiling height
  uint16_t floor_tex; // floor texture
  uint16_t ceil_tex;  // ceiling texture
  uint16_t colors[5]; // hardware rendering colors, LIGHTS
  uint16_t special;   // special type
  uint16_t tag;       // tag number
  uint16_t flags;     // reverb, damage, water, etc
} PACKEDATTR;

struct UVertexMinimal
{
  double x;
  double y;
};

struct UThingMinimal
{
  int32_t tid;
  int32_t type;
  double x;
  double y;
  double height;
  int32_t angle;
  int32_t special;
  int32_t args[5];
  uint32_t options;
};

struct ULinedefMinimal
{
  int32_t id;
  size_t v1_id;
  size_t v2_id;
  int32_t special;
  int32_t args[5];
  int32_t sidefront;
  int32_t sideback;
  uint32_t flags;
  uint32_t spac;
};

struct USidedefMinimal
{
  size_t sector_id;
  char texturetop[9];
  char texturemiddle[9];
  char texturebottom[9];
  int32_t offsetx;
  int32_t offsety;
};

struct USectorMinimal
{
  int32_t tag;
  int32_t heightfloor;
  int32_t heightceiling;
  char texturefloor[9];
  char textureceiling[9];
  int32_t lightlevel;
  int32_t special;
};

//------------------------------------------------------------------------
// BSP TREE STRUCTURES
//------------------------------------------------------------------------

enum class BspFormat : uint8_t
{
  DoomBSP,
  DeePBSPV4,
  XNOD,
  XGLN,
  XGL2,
  XGL3,

  MIN = DoomBSP,
  MAX = XGL3,
};

enum class BlockmapFormat : uint8_t
{
  DoomBSP,
  XBM1,

  MIN = DoomBSP,
  MAX = XBM1,
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
// * These entries need OFF_BY_ONE, since std::vector's size() method is 1 indexed;
//

template <typename T>
using Limits = std::numeric_limits<T>;

constexpr size_t LIMIT_VERT = Limits<uint16_t>::max() + OFF_BY_ONE;
constexpr size_t LIMIT_LINE = Limits<uint16_t>::max();
constexpr size_t LIMIT_SIDE = Limits<uint16_t>::max();
constexpr size_t LIMIT_SECTOR = Limits<uint16_t>::max() + OFF_BY_ONE;

constexpr size_t LIMIT_NODE = Limits<int16_t>::max() + OFF_BY_ONE;
constexpr size_t LIMIT_SUBSEC = Limits<int16_t>::max() + OFF_BY_ONE;
constexpr size_t LIMIT_SEG = Limits<uint16_t>::max() + OFF_BY_ONE;

constexpr int16_t LIMIT_BMAP_INDEX_VANILLA = Limits<int16_t>::max();
constexpr uint16_t LIMIT_BMAP_INDEX = Limits<uint16_t>::max();

//
// Vanilla blockmap
//
struct MapBoundBox
{
  int16_t maxy;
  int16_t miny;
  int16_t minx;
  int16_t maxx;
} PACKEDATTR;

struct MapBlockmapHeader
{
  int16_t x_origin, y_origin;
  uint16_t x_blocks, y_blocks;
} PACKEDATTR;

struct MapBlockmapHeaderXBM1
{
  fixed_t x_origin, y_origin;
  uint32_t x_blocks, y_blocks;
} PACKEDATTR;

// Fail way earlier
#define static_size(x, y) static_assert(sizeof(x) == y, "Size mismatch for '" #x "'. Should be " #y ".")

static_size(MapVertexIntegral, 4);
static_size(MapVertexFractional, 8);

static_size(MapLinedefDoom, 14);
static_size(MapSidedefDoom, 30);
static_size(MapSectorDoom, 26);
static_size(MapThingDoom, 10);
static_size(MapLinedefHexen, 16);
static_size(MapThingHexen, 20);
static_size(MapThingDoom64, 12);
static_size(MapLinedefDoom64, 16);
static_size(MapSidedefDoom64, 12);
static_size(MapSectorDoom64, 24);
static_size(MapBoundBox, 8);
static_size(MapBlockmapHeader, 8);
static_size(MapBlockmapHeaderXBM1, 16);

//
// LineDef attributes.
//

enum LineflagDoom : uint16_t
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

enum LineflagHexen : uint16_t
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
enum LineflagDoom64 : uint32_t
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

enum LineflagUniversal : uint32_t
{
  ULF_None = 0,
  ULF_Blocking = BIT(1),
  ULF_BlockMonsters = BIT(2),
  ULF_TwoSided = BIT(3),
  ULF_DontPegTop = BIT(4),
  ULF_DontPegBottom = BIT(5),
  ULF_Secret = BIT(6),
  ULF_BlockSound = BIT(7),
  ULF_DontDraw = BIT(8),
  ULF_Mapped = BIT(9),

  ULF_JumpOver = BIT(10),
  ULF_BlockFloaters = BIT(11),
  ULF_Translucent = BIT(12),
  ULF_Transparent = BIT(13),
};

enum LineSpacHexen
{
  SPAC_Cross = 0,   // when line is crossed (W1 / WR)
  SPAC_Use = 1,     // when line is used    (S1 / SR)
  SPAC_Monster = 2, // when monster walks over line
  SPAC_Impact = 3,  // when bullet/projectile hits line (G1 / GR)
  SPAC_Push = 4,    // when line is bumped (player is stopped)
  SPAC_PCross = 5,  // when projectile crosses the line
};

enum LineSpacUniversal : uint32_t
{
  ULS_PlayerCross = BIT(1),
  ULS_PlayerUse = BIT(2),
  ULS_MonsterCross = BIT(3),
  ULS_MonsterUse = BIT(4),
  ULS_Impact = BIT(5),
  ULS_PlayerPush = BIT(6),
  ULS_MonsterPush = BIT(7),
  ULS_MissileCross = BIT(8),
  ULS_RepeatSpecial = BIT(9),
};

enum LineSpecialDoom : int32_t
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

enum LineTagDoom : uint16_t
{
  Tag_NoReject = 997,
  Tag_DoNotRender = 998,
  Tag_NoBlockmap = 999,
};

enum LineSpecialHexen : int32_t
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
enum LineEffects : uint32_t
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
enum SegmentRotation : uint8_t
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

enum SectorSpecialDoom64 : uint16_t
{
  SS_Doom64_NoReject = 666,
};

enum SectorEffects : uint8_t
{
  FX_Sector_None = 0,
  FX_Sector_NoReject = BIT(0),
};

//
// Thing attributes.
//

enum DoomEdNum : int16_t
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

enum ThingOptionsUniversal : uint32_t
{
  UTF_Skill1 = BIT(1),
  UTF_Skill2 = BIT(2),
  UTF_Skill3 = BIT(3),
  UTF_Skill4 = BIT(4),
  UTF_Skill5 = BIT(5),
  UTF_Ambush = BIT(6),
  UTF_Single = BIT(7),
  UTF_DM = BIT(8),
  UTF_Coop = BIT(9),

  UTF_Friend = BIT(10),

  UTF_Dormant = BIT(11),
  UTF_Class1 = BIT(12),
  UTF_Class2 = BIT(13),
  UTF_Class3 = BIT(14),

  UTF_Standing = BIT(15),
  UTF_StrifeAlly = BIT(16),
  UTF_Translucent = BIT(17),
  UTF_Invisible = BIT(18),
};

//
// Node Build Information Structure
//

constexpr double SPLIT_COST_MIN = 1.0;
constexpr double SPLIT_COST_DEFAULT = 11.0;
constexpr double SPLIT_COST_MAX = 32.0;

struct PolyObject
{
  DoomEdNum anchor = ZDoom_PolyObj_Anchor;
  DoomEdNum spawn = ZDoom_PolyObj_Spawn;
  DoomEdNum spawn_crush = ZDoom_PolyObj_SpawnCrush;
  DoomEdNum spawn_hurt = ZDoom_PolyObj_SpawnHurt;
};

struct BuildInfo
{
  PolyObject po;

  double split_cost = SPLIT_COST_DEFAULT;
  size_t total_warnings = 0;
  uint32_t debug = DEBUG_NONE;

  BspFormat bsp_format = BspFormat::XNOD;
  BlockmapFormat bmap_format = BlockmapFormat::DoomBSP;
  bool fast = false;     // use a faster method to pick nodes
  bool analysis = false; // write out CSV for data analysis and visualization
  bool verbose = false;  // this affects how some messages are shown
  bool effects = true;   // disable special effects
  bool compress = false; // compress lumps using zlib

  bool skip_bsp = false;  // Just copy over the lumps, don't build anything ...
  bool skip_bmap = false; // ... if, and only if, the lymp already exists ...
  bool skip_rej = false;  // ... for if the lump does not exist, it will be built.

  bool skip_bmap_zero = false; // Skip "zero index" header on vanilla blockmap lump
};

extern BuildInfo config;

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
                                    "    -t --type  #       Select targeted node/BSP tree format\n"
                                    "                       0=Vanilla, 1=DeePBSPV4, 2=XNOD, 3=XGLN, 4=XGL2, 5=XGL3\n"
                                    "       --compress      Compress ZDBSP nodes\n"
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
    PrintLine(Log::NORMAL, "[Benchmarker] '{}' runtime: {:.2f} ms", name.data(), time.count());
  };
};

//
// FNV-1a
//

template <typename Hash_Type>
struct FNV1a
{
  static constexpr size_t Hash_size = sizeof(Hash_Type);

  static_assert(std::is_unsigned<Hash_Type>(), "FNV1a hash type must be an unsigned integral.");
  static_assert(Hash_size == 4 || Hash_size == 8, "FNV1a only supports 32-bit or 64-bit hash types.");

  static constexpr uint32_t FNV1a_Basis32 = 0x811c9dc5;
  static constexpr uint32_t FNV1a_Prime32 = 0x01000193;
  static constexpr uint64_t FNV1a_Basis64 = 0xcbf29ce484222325;
  static constexpr uint64_t FNV1a_Prime64 = 0x00000100000001b3;

  static constexpr Hash_Type Basis = (Hash_size == 4) ? FNV1a_Basis32 : FNV1a_Basis64;
  static constexpr Hash_Type Prime = (Hash_size == 4) ? FNV1a_Prime32 : FNV1a_Prime64;

  Hash_Type hash = Basis;

  Hash_Type ComputeSingleByte(byte value)
  {
    this->hash ^= value;
    this->hash *= this->Prime;
    return this->hash;
  }

  template <typename Value_Type>
  Hash_Type Compute(Value_Type val)
  {
    constexpr size_t Value_Size = sizeof(Value_Type);

    auto data = std::bit_cast<std::array<byte, Value_Size>>(val);

    for (size_t index = 0; index < Value_Size; index++)
    {
      this->ComputeSingleByte(data[index]);
    }

    return this->hash;
  }
};

//
// Simple iota, from Rum-And-Raisin
//

template <typename T>
struct Iota
{
  struct Iterator
  {
    T val;

    constexpr T operator*() noexcept
    {
      return val;
    }

    constexpr bool operator!=(const Iterator &rhs)
    {
      return val != rhs.val;
    }

    constexpr Iterator &operator++()
    {
      ++val;
      return *this;
    }
  };

  constexpr Iota(T e)
  {
    _begin = {0};
    _end = {e};
  }

  constexpr Iota(T b, T e)
  {
    _begin = {b};
    _end = {e};
  }

  constexpr Iterator begin() noexcept
  {
    return _begin;
  }

  constexpr Iterator end() noexcept
  {
    return _end;
  }

  Iterator _begin;
  Iterator _end;
};

template <typename T>
std::pair<size_t, T> LargestElement(std::vector<T> input)
{
  static_assert(std::is_integral_v<T>, "LargestElement: integral required");
  auto value = std::max_element(input.begin(), input.end());
  auto index = std::distance(input.begin(), value);
  return std::pair<size_t, T>(index, *value);
}
