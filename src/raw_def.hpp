//------------------------------------------------------------------------
//
//  ELFBSP  Copyright (C) 2025       Guilherme Miranda
//          Copyright (C) 2007-2018  Andrew Apted
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

#ifndef __ELFBSP_RAW_DEF_H__
#define __ELFBSP_RAW_DEF_H__

#include "system.hpp"
#include <cstdint>

/* ----- The wad structures ---------------------- */

#define WAD_TEX_NAME  8
#define WAD_FLAT_NAME 8

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
  LL_BEHAVIOR   // Hexen scripting stuff
} lump_order_e;

/* ----- The level structures ---------------------- */

typedef struct raw_vertex_s
{
  int16_t x, y;
} PACKEDATTR raw_vertex_t;

typedef struct raw_zdoom_vertex_s
{
  int32_t x, y;
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
  int16_t x_offset; // X offset for texture
  int16_t y_offset; // Y offset for texture
  char upper_tex[8]; // texture name for the part above
  char lower_tex[8]; // texture name for the part below
  char mid_tex[8];   // texture name for the regular part
  uint16_t sector; // adjacent sector
} PACKEDATTR raw_sidedef_t;

typedef struct raw_sector_s
{
  int16_t floorh; // floor height
  int16_t ceilh;  // ceiling height
  char floor_tex[8]; // floor texture
  char ceil_tex[8];  // ceiling texture
  uint16_t light; // light level (0-255)
  uint16_t type;  // special type (0 = normal, 9 = secret, ...)
  int16_t tag;    // sector activated by a linedef with same tag
} PACKEDATTR raw_sector_t;

typedef struct raw_thing_s
{
  int16_t x, y;     // position of thing
  int16_t angle;    // angle thing faces (degrees)
  uint16_t type;    // type of thing
  uint16_t options; // when appears, deaf, etc..
} PACKEDATTR raw_thing_t;

// -JL- Hexen thing definition
typedef struct raw_hexen_thing_s
{
  int16_t tid;      // tag id (for scripts/specials)
  int16_t x, y;     // position
  int16_t height;   // start height above floor
  int16_t angle;    // angle thing faces
  uint16_t type;    // type of thing
  uint16_t options; // when appears, deaf, dormant, etc..
  uint8_t special; // special type
  uint8_t args[5]; // special arguments
} PACKEDATTR raw_hexen_thing_t;

/* ----- The BSP tree structures ----------------------- */

constexpr const char *DEEP_MAGIC = "xNd4\0\0\0\0";
constexpr const char *XNOD_MAGIC = "XNOD";
constexpr const char *ZNOD_MAGIC = "ZNOD";
constexpr const char *XGLN_MAGIC = "XGLN";
constexpr const char *ZGLN_MAGIC = "ZGLN";
constexpr const char *XGL2_MAGIC = "XGL2";
constexpr const char *ZGL2_MAGIC = "ZGL2";
constexpr const char *XGL3_MAGIC = "XGL3";
constexpr const char *ZGL3_MAGIC = "ZGL3";

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
  int16_t maxy, miny;
  int16_t minx, maxx;
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
  // bounding box size
  int16_t width;
  int16_t height;
  // pixels to the left of origin
  int16_t leftoffset;
  // pixels below the origin
  int16_t topoffset;
  uint32_t columnofs[1]; // only [width] used
} PACKEDATTR patch_t;

//
// LineDef attributes.
//

typedef enum : uint16_t
{
  MLF_Blocking = 0x0001,      // Solid, is an obstacle
  MLF_BlockMonsters = 0x0002, // Blocks monsters only
  MLF_TwoSided = 0x0004,      // Backside will not be present at all if not two sided

  // If a texture is pegged, the texture will have
  // the end exposed to air held constant at the
  // top or bottom of the texture (stairs or pulled
  // down things) and will move with a height change
  // of one of the neighbor sectors.
  //
  // Unpegged textures allways have the first row of
  // the texture at the top pixel of the line for both
  // top and bottom textures (use next to windows).

  MLF_UpperUnpegged = 0x0008, // Upper texture unpegged
  MLF_LowerUnpegged = 0x0010, // Lower texture unpegged
  MLF_Secret = 0x0020,        // In AutoMap: don't map as two sided: IT'S A SECRET!
  MLF_SoundBlock = 0x0040,    // Sound rendering: don't let sound cross two of these
  MLF_DontDraw = 0x0080,      // Don't draw on the automap at all
  MLF_Mapped = 0x0100,        // Set as if already seen, thus drawn in automap
  MLF_Boom_PassThru = 0x0200, // -AJA- this one is from Boom. Allows multiple lines to be pushed simultaneously.
  MLF_3DMidTex = 0x0400,      // [EA] Solid middle texture
  MLF_Reserved = 0x0800,      // [EA] MBF21's comp_reservedlineflag
  MLF_BlockGrounded = 0x1000, // [EA] MBF21's Block Grounded Monster
  MLF_BlockPlayers = 0x2000,  // [EA] MBF21's Block Players Only
} compatible_lineflag_e;

typedef enum : uint16_t
{
  // flags 0x001 .. 0x200 are same as DOOM above
  MLF_Hexen_Repeatable = 0x0200,
  MLF_Hexen_Activation = 0x1c00,
} hexen_lineflag_e;

typedef enum : uint16_t
{
  // these are supported by ZDoom (and derived ports)
  MLF_ZDoom_MonCanActivate = 0x2000,
  MLF_ZDoom_BlockPlayers = 0x4000,
  MLF_ZDoom_BlockEverything = 0x8000,
} zdoom_lineflag_e;

#define BOOM_GENLINE_FIRST 0x2f80
#define BOOM_GENLINE_LAST  0x7fff

#define is_genline(tp)     ((tp) >= BOOM_GENLINE_FIRST && (tp) <= BOOM_GENLINE_LAST)

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
typedef enum zokumbsp_specials_e : uint32_t
{
  Special_VanillaScroll = 48,

  Special_DoNotRender = 998, // in ZokumBSP, originally a tag :/
  Special_NoBlockmap = 999,  // not gonna deal with that :v

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
  Special_Unknown2, // line tag value becomes seg's associated line index?
                    // why?
} zokumbsp_specials_t;

typedef uint32_t long_angle_t;
typedef uint16_t short_angle_t;
constexpr uint32_t ANG45 = 0x20000000;

template <typename T> constexpr inline static long_angle_t DegreesToLongBAM(T x)
{
  return ANG45 * (x / 45);
}

template <typename T> constexpr inline static short_angle_t DegreesToShortBAM(T x)
{
  return (ANG45 * (x / 45)) >> 16;
}

//
// Sector attributes.
//

typedef enum : uint16_t
{
  BoomSF_TypeMask = 0x001F,
  BoomSF_DamageMask = 0x0060,

  BoomSF_Secret = 0x0080,
  BoomSF_Friction = 0x0100,
  BoomSF_Wind = 0x0200,
  BoomSF_NoSounds = 0x0400,
  BoomSF_QuietPlane = 0x0800,

  MBF21SF_AltDeathMode = 0x1000,
  MBF21SF_MonsterDeath = 0x2000,
} compatible_sectorflag_e;

  #define MSF_BoomFlags 0x0FE0

//
// Thing attributes.
//

typedef enum : uint16_t
{
  MTF_Easy = 0x0001,
  MTF_Medium = 0x0002,
  MTF_Hard = 0x0004,
  MTF_Ambush = 0x0008,

  MTF_Not_SP = 0x0010,
  MTF_Not_DM = 0x0020,
  MTF_Not_COOP = 0x0040,
  MTF_Friend = 0x0080,
} thing_option_e;

#define MTF_EXFLOOR_MASK  0x3C00
#define MTF_EXFLOOR_SHIFT 10

typedef enum : uint16_t
{
  MTF_Hexen_Easy = 0x0001,
  MTF_Hexen_Medium = 0x0002,
  MTF_Hexen_Hard = 0x0004,
  MTF_Hexen_Ambush = 0x0008,

  MTF_Hexen_Dormant = 0x0010,
  MTF_Hexen_Fighter = 0x0020,
  MTF_Hexen_Cleric = 0x0040,
  MTF_Hexen_Mage = 0x0080,

  MTF_Hexen_SP = 0x0100,
  MTF_Hexen_COOP = 0x0200,
  MTF_Hexen_DM = 0x0400,
} hexen_option_e;

//
// Polyobject stuff
//
#define HEXTYPE_POLY_START       1
#define HEXTYPE_POLY_EXPLICIT    5

// -JL- Hexen polyobj thing types
#define PO_ANCHOR_TYPE           3000
#define PO_SPAWN_TYPE            3001
#define PO_SPAWNCRUSH_TYPE       3002

// -JL- ZDoom polyobj thing types
#define ZDOOM_PO_ANCHOR_TYPE     9300
#define ZDOOM_PO_SPAWN_TYPE      9301
#define ZDOOM_PO_SPAWNCRUSH_TYPE 9302

#endif /* __ELFBSP_RAW_DEF_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
