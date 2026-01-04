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
typedef uint8_t byte;
typedef uint8_t args_t[5];
typedef int32_t fixed32_t;
typedef uint32_t long_angle_t;
typedef uint16_t short_angle_t;
typedef _Float64 float64_t; // No C++23 yet, so a hack it is

// misc constants
static constexpr uint32_t ANG45 = 0x20000000;
static constexpr uint32_t FRACBITS = 16;
static constexpr fixed32_t FRACUNIT = (1 << FRACBITS);
static constexpr float64_t FRACFACTOR = static_cast<float64_t>(FRACUNIT);

static constexpr size_t NO_INDEX = static_cast<size_t>(-1);
static constexpr uint16_t NO_INDEX_INT16 = static_cast<uint16_t>(-1);
static constexpr uint32_t NO_INDEX_INT32 = static_cast<uint32_t>(-1);

static constexpr size_t MSG_BUFFER_LENGTH = 1024;

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
static inline constexpr fixed32_t IntToFixed(int32_t x)
{
  return x << FRACBITS;
}

static inline constexpr int32_t FixedToInt(fixed32_t x)
{
  return x >> FRACBITS;
}

static inline constexpr float64_t FixedToFloat(fixed32_t x)
{
  return (float64_t(x) / FRACFACTOR);
}

static inline constexpr fixed32_t FloatToFixed(float64_t x)
{
  return fixed32_t(x * FRACFACTOR);
}

// binary angular measurement, BAM!
template <typename T> static inline constexpr long_angle_t DegreesToLongBAM(T x)
{
  return ANG45 * (x / 45);
}

template <typename T> static inline constexpr short_angle_t DegreesToShortBAM(T x)
{
  return (ANG45 * (x / 45)) >> FRACBITS;
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
  if (result < 0 || result >= (int32_t)buf_len)
  {
    buf[buf_len - 1] = '\0';
    result = (int32_t)buf_len - 1;
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
  static char buffer[MSG_BUFFER_LENGTH];

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
  static char buffer[MSG_BUFFER_LENGTH];

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
  static char buffer[MSG_BUFFER_LENGTH];

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

template <typename T> static inline constexpr void SYS_ASSERT(T cond)
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
  T *ret = (T *)calloc(1, size);

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
  T *ret = (T *)realloc(old, size);

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
  size_t A = strlen(filename);

  if (A > 1 && filename[A] == '.')
  {
    return false;
  }

  while (A--)
  {
    if (filename[A] == '.')
    {
      return true;
    }

    if (filename[A] == DIR_SEP_CH)
    {
      break;
    }

    if constexpr (WINDOWS)
    {
      if (filename[A] == DIR_SEP_CH || filename[A] == ':')
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
  if (!ext)
  {
    return !HasExtension(filename);
  }

  size_t A = strlen(filename);
  size_t B = strlen(ext);

  while (A-- && B--)
  {
    if (toupper(filename[A]) != toupper(ext[B]))
    {
      return false;
    }
  }

  return filename[A] == '.';
}

//
// FindExtension
//
// Return offset of the '.', or NO_INDEX when no extension was found.
//
static inline size_t FindExtension(const char *filename)
{
  if (filename[0] == 0)
  {
    return NO_INDEX;
  }

  size_t pos = strlen(filename) - 1;

  for (; filename[pos] != '.'; pos--)
  {
    char ch = filename[pos];

    if (ch == DIR_SEP_CH)
    {
      break;
    }

    if constexpr (WINDOWS)
    {
      if (ch == DIR_SEP_CH || ch == ':')
      {
        break;
      }
    }
  }

  if (filename[pos] != '.')
  {
    return NO_INDEX;
  }

  return pos;
}

//------------------------------------------------------------------------
// MATH STUFF
//------------------------------------------------------------------------

//
// rounds the value _up_ to the nearest power of two.
//
static inline int RoundPOW2(int x)
{
  if (x <= 2)
  {
    return x;
  }

  x--;

  for (int tmp = x >> 1; tmp; tmp >>= 1)
  {
    x |= tmp;
  }

  return x + 1;
}

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

static constexpr size_t WAD_LUMP_NAME = 8;

// wad header
typedef struct raw_wad_header_s
{
  char ident[4];
  uint32_t num_entries;
  uint32_t dir_start;
} PACKEDATTR raw_wad_header_t;

// directory entry
typedef struct raw_wad_entry_s
{
  uint32_t pos;
  uint32_t size;
  char name[8];
} PACKEDATTR raw_wad_entry_t;

//------------------------------------------------------------------------
// LEVEL STRUCTURES
//------------------------------------------------------------------------

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
typedef enum
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
} lump_order_e;

typedef struct raw_vertex_s
{
  int16_t x;
  int16_t y;
} PACKEDATTR raw_vertex_t;

typedef struct raw_zdoom_vertex_s
{
  int32_t x;
  int32_t y;
} PACKEDATTR raw_zdoom_vertex_t;

typedef struct raw_linedef_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t flags;   // linedef flags (impassible, etc)
  uint16_t special; // special type (0 for none, 97 for teleporter, etc)
  int16_t tag;      // this linedef activates the sector with same tag
  uint16_t right;   // right sidedef
  uint16_t left;    // left sidedef (only if this line adjoins 2 sectors)
} PACKEDATTR raw_linedef_t;

typedef struct raw_hexen_linedef_s
{
  uint16_t start;  // from this vertex...
  uint16_t end;    // ... to this vertex
  uint16_t flags;  // linedef flags (impassible, etc)
  uint8_t special; // special type
  uint8_t args[5]; // special arguments
  uint16_t right;  // right sidedef
  uint16_t left;   // left sidedef
} PACKEDATTR raw_hexen_linedef_t;

typedef struct raw_sidedef_s
{
  int16_t x_offset;  // X offset for texture
  int16_t y_offset;  // Y offset for texture
  char upper_tex[8]; // texture name for the part above
  char lower_tex[8]; // texture name for the part below
  char mid_tex[8];   // texture name for the regular part
  uint16_t sector;   // adjacent sector
} PACKEDATTR raw_sidedef_t;

typedef struct raw_sector_s
{
  int16_t floorh;    // floor height
  int16_t ceilh;     // ceiling height
  char floor_tex[8]; // floor texture
  char ceil_tex[8];  // ceiling texture
  uint16_t light;    // light level (0-255)
  uint16_t type;     // special type (0 = normal, 9 = secret, ...)
  int16_t tag;       // sector activated by a linedef with same tag
} PACKEDATTR raw_sector_t;

typedef struct raw_thing_s
{
  int16_t x;        // x position of thing
  int16_t y;        // y position of thing
  int16_t angle;    // angle thing faces (degrees)
  uint16_t type;    // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR raw_thing_t;

// -JL- Hexen thing definition
typedef struct raw_hexen_thing_s
{
  int16_t tid;      // tag id (for scripts/specials)
  int16_t x;        // x position
  int16_t y;        // y position
  int16_t height;   // start height above floor
  int16_t angle;    // angle thing faces
  uint16_t type;    // type of thing
  uint16_t options; // when appears, deaf, dormant, etc..
  uint8_t special;  // special type
  uint8_t args[5];  // special arguments
} PACKEDATTR raw_hexen_thing_t;

//------------------------------------------------------------------------
// BSP TREE STRUCTURES
//------------------------------------------------------------------------

static constexpr const char *DEEP_MAGIC = "xNd4\0\0\0\0";
static constexpr const char *XNOD_MAGIC = "XNOD";
static constexpr const char *ZNOD_MAGIC = "ZNOD";
static constexpr const char *XGLN_MAGIC = "XGLN";
static constexpr const char *ZGLN_MAGIC = "ZGLN";
static constexpr const char *XGL2_MAGIC = "XGL2";
static constexpr const char *ZGL2_MAGIC = "ZGL2";
static constexpr const char *XGL3_MAGIC = "XGL3";
static constexpr const char *ZGL3_MAGIC = "ZGL3";

typedef struct raw_seg_s
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t angle;   // angle (0 = east, 16384 = north, ...)
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  uint16_t dist;    // distance from starting point
} PACKEDATTR raw_seg_t;

typedef struct raw_zdoom_seg_s
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR raw_zdoom_seg_t;

typedef struct raw_xgl2_seg_s
{
  uint32_t vertex;  // from this vertex...
  uint32_t partner; // ... to this vertex
  uint32_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR raw_xgl2_seg_t;

typedef struct raw_bbox_s
{
  int16_t maxy;
  int16_t miny;
  int16_t minx;
  int16_t maxx;
} PACKEDATTR raw_bbox_t;

typedef struct raw_node_s
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint16_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR raw_node_t;

typedef struct raw_subsec_s
{
  uint16_t num;   // number of Segs in this Sub-Sector
  uint16_t first; // first Seg
} PACKEDATTR raw_subsec_t;

typedef struct raw_zdoom_subsec_s
{
  uint32_t segnum;
  // NOTE : no "first" value, segs must be contiguous and appear
  //        in an order dictated by the subsector list, e.g. all
  //        segs of the second subsector must appear directly after
  //        all segs of the first subsector.
} PACKEDATTR raw_zdoom_subsec_t;

typedef struct raw_zdoom_node_s
{
  // this structure used by ZDoom nodes too
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR raw_zdoom_node_t;

typedef struct raw_blockmap_header_s
{
  int16_t x_origin, y_origin;
  int16_t x_blocks, y_blocks;
} PACKEDATTR raw_blockmap_header_t;

/* ----- Graphical structures ---------------------- */

typedef struct
{
  int16_t x_origin;
  int16_t y_origin;
  uint16_t pname;    // index into PNAMES
  uint16_t stepdir;  // NOT USED
  uint16_t colormap; // NOT USED
} PACKEDATTR raw_patchdef_t;

typedef struct
{
  int16_t x_origin;
  int16_t y_origin;
  uint16_t pname; // index into PNAMES
} PACKEDATTR raw_strife_patchdef_t;

// Texture definition.
//
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
//
typedef struct
{
  char name[8];
  uint32_t masked; // NOT USED
  uint16_t width;
  uint16_t height;
  uint16_t column_dir[2]; // NOT USED
  uint16_t patch_count;
  raw_patchdef_t patches[1];
} PACKEDATTR raw_texture_t;

typedef struct
{
  char name[8];
  uint32_t masked; // NOT USED
  uint16_t width;
  uint16_t height;
  uint16_t patch_count;
  raw_strife_patchdef_t patches[1];
} PACKEDATTR raw_strife_texture_t;

// Patches.
//
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
//
typedef struct patch_s
{
  int16_t width;         // bounding box size
  int16_t height;        //
  int16_t leftoffset;    // pixels to the left of origin
  int16_t topoffset;     // pixels below the origin
  uint32_t columnofs[1]; // only [width] used
} PACKEDATTR patch_t;

//
// LineDef attributes.
//

typedef enum : uint16_t
{
  MLF_Blocking = BIT(0),      // Solid, is an obstacle
  MLF_BlockMonsters = BIT(1), // Blocks monsters only
  MLF_TwoSided = BIT(2),      // Backside will not be present at all if not two sided

  // If a texture is pegged, the texture will have
  // the end exposed to air held constant at the
  // top or bottom of the texture (stairs or pulled
  // down things) and will move with a height change
  // of one of the neighbor sectors.
  //
  // Unpegged textures allways have the first row of
  // the texture at the top pixel of the line for both
  // top and bottom textures (use next to windows).

  MLF_UpperUnpegged = BIT(3), // Upper texture unpegged
  MLF_LowerUnpegged = BIT(4), // Lower texture unpegged
  MLF_Secret = BIT(5),        // In AutoMap: don't map as two sided: IT'S A SECRET!
  MLF_SoundBlock = BIT(6),    // Sound rendering: don't let sound cross two of these
  MLF_DontDraw = BIT(7),      // Don't draw on the automap at all
  MLF_Mapped = BIT(8),        // Set as if already seen, thus drawn in automap
  MLF_PassUse = BIT(9),       // Allow multiple lines to be pushed simultaneously.
  MLF_3DMidTex = BIT(10),     // Solid middle texture
  MLF_Reserved = BIT(11),     // comp_reservedlineflag
  MLF_BlockGround = BIT(12),  // Block Grounded Monster
  MLF_BlockPlayers = BIT(13), // Block Players Only
} compatible_lineflag_e;

// first few flags are same as DOOM above
typedef enum : uint16_t
{
  MLF_Hexen_Repeatable = BIT(9),
  MLF_Hexen_Activation = BIT(10) | BIT(11) | BIT(12),
} hexen_lineflag_e;

// these are supported by ZDoom (and derived ports)
typedef enum : uint16_t
{
  MLF_ZDoom_MonCanActivate = BIT(13),
  MLF_ZDoom_BlockPlayers = BIT(14),
  MLF_ZDoom_BlockEverything = BIT(15),
} zdoom_lineflag_e;

static constexpr uint32_t BOOM_GENLINE_FIRST = 0x2f80;
static constexpr uint32_t BOOM_GENLINE_LAST = 0x7fff;

static inline constexpr bool IsGeneralizedSpecial(uint32_t special)
{
  return special >= BOOM_GENLINE_FIRST && special <= BOOM_GENLINE_LAST;
}

typedef enum
{
  SPAC_Cross = 0,   // when line is crossed (W1 / WR)
  SPAC_Use = 1,     // when line is used    (S1 / SR)
  SPAC_Monster = 2, // when monster walks over line
  SPAC_Impact = 3,  // when bullet/projectile hits line (G1 / GR)
  SPAC_Push = 4,    // when line is bumped (player is stopped)
  SPAC_PCross = 5,  // when projectile crosses the line
} hexen_activation_e;

// The power of node building manipulation!
typedef enum bsp_specials_e : uint32_t
{
  Special_VanillaScroll = 48,

  Special_DoNotRender = 998, // in ZokumBSP, originally a tag :/ not gonna deal with that :v
  Special_NoBlockmap = 999,  //

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
} bsp_specials_t;

//
// Sector attributes.
//

typedef enum : uint16_t
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
} compatible_sectorflag_e;

static constexpr uint32_t SF_BoomFlags = SF_DamageMask | SF_Secret | SF_Friction | SF_Wind;
static constexpr uint32_t SF_MBF21Flags = SF_DamageMask | SF_Secret | SF_Friction | SF_Wind | SF_AltDeathMode | SF_MonsterDeath;

//
// Thing attributes.
//

typedef enum : uint16_t
{
  MTF_Easy = BIT(0),
  MTF_Medium = BIT(1),
  MTF_Hard = BIT(2),
  MTF_Ambush = BIT(3),

  MTF_Not_SP = BIT(4),
  MTF_Not_DM = BIT(5),
  MTF_Not_COOP = BIT(6),
  MTF_Friend = BIT(7),
} thing_option_e;

static constexpr uint32_t MTF_EXFLOOR_MASK = 0x3C00;
static constexpr uint32_t MTF_EXFLOOR_SHIFT = 10;

typedef enum : uint16_t
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
} hexen_option_e;

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
