//------------------------------------------------------------------------------
//
//  ELFBSP
//
//------------------------------------------------------------------------------
//
//  Copyright 2025-2026 Guilherme Miranda
//  Copyright 2000-2018 Andrew Apted, et al
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

#include "core.hpp"
#include "local.hpp"

//
// DoomBSP
//
struct MapNodeDoomBSP
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  MapBoundBox b1, b2;   // bounding rectangles
  uint16_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

struct MapSubsectorDoomBSP
{
  uint16_t num;   // number of Segs in this Sub-Sector
  uint16_t first; // first Seg
} PACKEDATTR;

struct MapSegmentDoomBSP
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t angle;   // angle (0 = east, 16384 = north, ...)
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  int16_t dist;     // distance from starting point
} PACKEDATTR;

struct MapLeafRefDoomBSP
{
  uint16_t vertex;
  uint16_t seg;
} PACKEDATTR;

//
// DeepSea DeePBSP
// * compared to vanilla, some types were raise to 32bit
//
struct MapNodeDeePBSPV4
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  MapBoundBox b1, b2;   // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

struct MapSubsectorDeePBSPV4
{
  uint16_t num;   // number of Segs in this Sub-Sector
  uint32_t first; // first Seg
} PACKEDATTR;

struct MapSegmentDeePBSPV4
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t angle;   // angle (0 = east, 16384 = north, ...)
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  int16_t dist;     // distance from starting point
} PACKEDATTR;

struct MapLeafRefDeePBSPV4
{
  uint32_t vertex;
  uint32_t seg;
} PACKEDATTR;

//
// ZDoom BSP
// * compared to vanilla, some types were raise to 32bit
// * each version (XNOD->XGLN->XGL2->XGL3) builds on top of the previous
//
struct MapVertexXNOD
{
  fixed_t x;
  fixed_t y;
} PACKEDATTR;

struct MapNodeXNOD
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  MapBoundBox b1, b2;   // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

struct MapSubsectorXNOD
{
  uint32_t segnum;
  // NOTE : no "first" value, segs must be contiguous and appear
  //        in an order dictated by the subsector list, e.g. all
  //        segs of the second subsector must appear directly after
  //        all segs of the first subsector.
} PACKEDATTR;

struct MapSegmentXNOD
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

struct MapSegmentXGLN
{
  uint32_t vertex;  // from this vertex ... to the next
  uint32_t partner; // partner seg, unused by most ports outside of U/G/ZDoom
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

struct MapSegmentXGL2
{
  uint32_t vertex;  // from this vertex ... to the next
  uint32_t partner; // partner seg, unused by most ports outside of U/G/ZDoom
  uint32_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

struct MapNodeXGL3
{
  fixed_t x, y;         // starting point
  fixed_t dx, dy;       // offset to ending point
  MapBoundBox b1, b2;   // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

//
// glBSP
// * legacy format, hardly supported outside of old GL ports
// * compared to vanilla, some types were raise to 32bit
// * convex minisegs are always required
// * each version (V1->V2->V3->V4->V5) builds on top of the previous
//

constexpr uint16_t GL_V1_VERT = BIT(15);
constexpr uint32_t GL_V3_VERT = BIT(30);
constexpr uint32_t GL_V5_VERT = BIT(31);

struct MapSubsectorGlv3
{
  uint32_t num;   // number of Segs in this Sub-Sector
  uint32_t first; // first Seg
} PACKEDATTR;

struct MapSegmentGlv1
{
  uint16_t start;   // from this vertex...
  uint16_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  uint16_t partner; // partner segment, islands!
} PACKEDATTR;

struct MapSegmentGlv3
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
  uint32_t partner; // partner segment, islands!
} PACKEDATTR;

struct MapSegmentGlv4
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along
  uint16_t flip;    // true if not the same direction as linedef
} PACKEDATTR;

static_size(MapNodeDoomBSP, 28);
static_size(MapSubsectorDoomBSP, 4);
static_size(MapSegmentDoomBSP, 12);

static_size(MapNodeDeePBSPV4, 32);
static_size(MapSubsectorDeePBSPV4, 6);
static_size(MapSegmentDeePBSPV4, 16);

static_size(MapVertexXNOD, 8);
static_size(MapNodeXNOD, 32);
static_size(MapSubsectorXNOD, 4);
static_size(MapSegmentXNOD, 11);
static_size(MapSegmentXGLN, 11);
static_size(MapSegmentXGL2, 13);
static_size(MapNodeXGL3, 40);

// MapVertexGlv1 identical to MapVertexIntegral
// MapVertexGlv2 identical to MapVertexFractional (+ header)
// MapNodeGlv1 identical to MapNodeDoomBSP
// MapNodeGlv4 identical to MapNodeDeePBSPV4
// MapSubsectorGlv1 identical to MapSubsectorDoomBSP
// MapSubsectorGlv4 identical to MapSubsectorDeePBSPV4
static_size(MapSubsectorGlv3, 8); // unique
static_size(MapSegmentGlv1, 10);  // unique
static_size(MapSegmentGlv3, 16);  // unique
static_size(MapSegmentGlv4, 12);  // unique

struct ExtractedGlBspLumps
{
  std::vector<MapNodeDoomBSP> nodes0;
  std::vector<MapNodeDeePBSPV4> nodes1;

  std::vector<MapSubsectorDoomBSP> subsectors0;
  std::vector<MapSubsectorGlv3> subsectors1;
  std::vector<MapSubsectorDeePBSPV4> subsectors2;

  std::vector<MapSegmentGlv1> segs0;
  std::vector<MapSegmentGlv3> segs1;
  std::vector<MapSegmentGlv4> segs2;

  // used in Adler32 checksum
  std::vector<byte> verts;
  std::vector<byte> linedefs0;
};

// put all the segs in each subsector into clockwise order, and renumber
// the seg indices.
//
// [ This cannot be done DURING BuildNodes() since splitting a seg with
//   a partner will insert another seg into that partner's list, usually
//   in the wrong place order-wise. ]
void ClockwiseBspTree(Level &level);

// traverse the BSP tree and do whatever is necessary to convert the
// node information from GL standard to normal standard (for example,
// removing minisegs).
void NormaliseBspTree(Level &level);

// traverse the BSP tree, doing whatever is necessary to round
// vertices to integer coordinates (for example, removing segs whose
// rounded coordinates degenerate to the same point).
void RoundOffBspTree(Level &level);

// both BLOCKAMP and REJECT exist as single lumps on all supported map formats
void InitBlockmap(Level &level);
void PutBlockmap(WadIO &io, Level &level);
void PutReject(WadIO &io, Level &level);

// the BSP tree lumps differ notably on each map format -- Doom/Hexen/Doom64
// have NODES, SSECTORS & SEGS, but UDMF is generally only ZNODES, and there's
// some format overlap between them
void SortSegs(Level &level);

void PutVertices(WadIO &io, Level &level, bool fractional, bool include_new_verts);

void PutSegs_DoomBSP(WadIO &io, Level &level);
void PutSubsecs_DoomBSP(WadIO &io, Level &level);
void PutNodes_DoomBSP(WadIO &io, Level &level);
void PutLeafs_DoomBSP(WadIO &io, Level &level);

void PutSegs_DeePBSPV4(WadIO &io, Level &level);
void PutSubsecs_DeePBSPV4(WadIO &io, Level &level);
void PutNodes_DeePBSPV4(WadIO &io, Level &level);
void PutLeafs_DeePBSPV4(WadIO &io, Level &level);

void PutTree_ZDBSP(WadIO &io, Level &level, std::string lumpname, BspFormat bsp_format, bool compress);
