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
  int16_t dist;     // distance from starting point
} PACKEDATTR;

using raw_leaf_vanilla_t = struct raw_leaf_vanilla_s
{
  uint16_t vertex;
  uint16_t seg;
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
  int16_t dist;     // distance from starting point
} PACKEDATTR;

using raw_leaf_deepbspv4_t = struct raw_leaf_deepbspv4_s
{
  uint32_t vertex;
  uint32_t seg;
} PACKEDATTR;

//
// ZDoom BSP
// * compared to vanilla, some types were raise to 32bit
// * each version (XNOD->XGLN->XGL2->XGL3) builds on top of the previous
//
using raw_vertex_xnod_t = struct raw_vertex_xnod_s
{
  int32_t x;
  int32_t y;
} PACKEDATTR;

using raw_node_xnod_t = struct raw_node_xnod_s
{
  int16_t x, y;         // starting point
  int16_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

using raw_subsec_xnod_t = struct raw_subsec_xnod_s
{
  uint32_t segnum;
  // NOTE : no "first" value, segs must be contiguous and appear
  //        in an order dictated by the subsector list, e.g. all
  //        segs of the second subsector must appear directly after
  //        all segs of the first subsector.
} PACKEDATTR;

using raw_seg_xnod_t = struct raw_seg_xnod_s
{
  uint32_t start;   // from this vertex...
  uint32_t end;     // ... to this vertex
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

using raw_seg_xgln_t = struct raw_seg_xgln_s
{
  uint32_t vertex;  // from this vertex ... to the next
  uint32_t partner; // partner seg, unused by most ports outside of U/G/ZDoom
  uint16_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

using raw_seg_xgl2_t = struct raw_seg_xgl2_s
{
  uint32_t vertex;  // from this vertex ... to the next
  uint32_t partner; // partner seg, unused by most ports outside of U/G/ZDoom
  uint32_t linedef; // linedef that this seg goes along, or NO_INDEX
  uint8_t side;     // 0 if on right of linedef, 1 if on left
} PACKEDATTR;

using raw_node_xgl3_t = struct raw_node_xgl3_s
{
  int32_t x, y;         // starting point
  int32_t dx, dy;       // offset to ending point
  raw_bbox_t b1, b2;    // bounding rectangles
  uint32_t right, left; // children: Node or SSector (if high bit is set)
} PACKEDATTR;

static_size(raw_node_vanilla_t, 28);
static_size(raw_subsec_vanilla_t, 4);
static_size(raw_seg_vanilla_t, 12);
static_size(raw_node_deepbspv4_t, 32);
static_size(raw_subsec_deepbspv4_t, 6);
static_size(raw_seg_deepbspv4_t, 16);
static_size(raw_vertex_xnod_t, 8);
static_size(raw_node_xnod_t, 32);
static_size(raw_subsec_xnod_t, 4);
static_size(raw_seg_xnod_t, 11);
static_size(raw_seg_xgln_t, 11);
static_size(raw_seg_xgl2_t, 13);
static_size(raw_node_xgl3_t, 40);

// put all the segs in each subsector into clockwise order, and renumber
// the seg indices.
//
// [ This cannot be done DURING BuildNodes() since splitting a seg with
//   a partner will insert another seg into that partner's list, usually
//   in the wrong place order-wise. ]
void ClockwiseBspTree(level_t &level);

// traverse the BSP tree and do whatever is necessary to convert the
// node information from GL standard to normal standard (for example,
// removing minisegs).
void NormaliseBspTree(level_t &level);

// Mark new vertices as old for writing into the VERTEXES lump
// Needed for RoundOffBspTree and saving to Doom 64 map formats directly
void RoundOffVertices(level_t &level);

// traverse the BSP tree, doing whatever is necessary to round
// vertices to integer coordinates (for example, removing segs whose
// rounded coordinates degenerate to the same point).
void RoundOffBspTree(level_t &level);

// both BLOCKAMP and REJECT exist as single lumps on all supported map formats
void InitBlockmap(level_t &level);
void PutBlockmap(level_t &level);
void PutReject(level_t &level);

// the BSP tree lumps differ notably on each map format -- Doom/Hexen/Doom64
// have NODES, SSECTORS & SEGS, but UDMF is generally only ZNODES, and there's
// some format overlap between them
void SaveDoom_DoomBSP(level_t &level, node_t *root_node);
void SaveDoom_DeePBSPV4(level_t &level, node_t *root_node);
void SaveDoom_XNOD(level_t &level, node_t *root_node);
void SaveDoom_XGLN(level_t &level, node_t *root_node);
void SaveDoom_XGL2(level_t &level, node_t *root_node);
void SaveDoom_XGL3(level_t &level, node_t *root_node);

void SaveDoom64_DoomBSP(level_t &level, node_t *root_node);
void SaveDoom64_DeePBSPV4(level_t &level, node_t *root_node);

void SaveTextmap_ZNODES(level_t &level, node_t *root_node);
