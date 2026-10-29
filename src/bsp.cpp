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

#include "bsp.hpp"
#include "core.hpp"
#include "local.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "zlib-ng.h"

// Upper-most bit is used for distinguishing sub-sectors, i.e tree leaves
constexpr uint16_t NF_SUBSECTOR_VANILLA = BIT(15);
constexpr uint32_t NF_SUBSECTOR = BIT(31);

//
// Utility
//

void SortSegs(Level &level)
{
  // sort segs into ascending index
  auto PredicateCompare_SegmentIndex = [](const Segment *Segment1, const Segment *Segment2) -> auto
  { return Segment1->index < Segment2->index; };

  std::sort(level.segs.begin(), level.segs.end(), PredicateCompare_SegmentIndex);

  // remove unwanted segs
  while (level.segs.size() > 0 && level.segs.back()->index == SEG_IS_GARBAGE)
  {
    UtilFree(level.segs.back());
    level.segs.pop_back();
  }
}

static inline int16_t VanillaSegDist(const Segment *seg)
{
  double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  // use the "true" starting coord (as stored in the wad)
  double sx = round(seg->start->x);
  double sy = round(seg->start->y);

  return FloatToShort(floor(hypot(sx - lx, sy - ly) + 0.5));
}

static inline short_angle_t VanillaSegAngle(const Segment *seg)
{
  // compute the "true" delta
  double dx = round(seg->end->x) - round(seg->start->x);
  double dy = round(seg->end->y) - round(seg->start->y);

  short_angle_t result = 0;

  switch (seg->linedef->angle)
  {
  case FX_RotateRelativeDegrees:
    result = ComputeAngle_BAM(dx, dy);
    result += DegreesToShortBAM(seg->linedef->id);
    break;
  case FX_RotateAbsoluteDegrees:
    result = DegreesToShortBAM(seg->linedef->id);
    break;
  case FX_RotateRelativeBAM:
    result = ComputeAngle_BAM(dx, dy);
    result += static_cast<short_angle_t>(seg->linedef->id);
    break;
  case FX_RotateAbsoluteBAM:
    result = static_cast<short_angle_t>(seg->linedef->id);
    break;
  case FX_DoNotRotate:
    result = ComputeAngle_BAM(dx, dy);
    break;
  }

  return result;
}

// Must be in simple ISO8601 format
// i.e. 1970-01-01 00:00:00.000
static std::string TimeString(void)
{
  using namespace std::chrono;
  auto now = system_clock::now();
  return std::format("{:%F %T}", floor<milliseconds>(now));
}

//
// Write out the full set of vertices
//

// TODO: add glbsp
void PutVertices(WadIO &io, Level &level, bool fractional, bool include_new_verts)
{
  const size_t vert_count = include_new_verts ? level.vertices.size() : level.num_old_vert;
  io.StartWritingLump("VERTEXES");

  for (size_t i = 0; i < vert_count; i++)
  {
    const Vertex *vert = level.vertices[i];
    if (fractional)
    {
      MapVertexFractional raw;
      raw.x = GetLittleEndian(FloatToFixed(vert->x));
      raw.y = GetLittleEndian(FloatToFixed(vert->y));
      io.AddToLump(&raw, sizeof(raw));
    }
    else
    {
      MapVertexIntegral raw;
      raw.x = GetLittleEndian(FloatToShort(floor(vert->x)));
      raw.y = GetLittleEndian(FloatToShort(floor(vert->y)));
      io.AddToLump(&raw, sizeof(raw));
    }
  }
}

//
// Vanilla format
//

void PutSegs_DoomBSP(WadIO &io, Level &level)
{
  io.StartWritingLump("SEGS");

  for (auto seg : level.segs)
  {
    MapSegmentDoomBSP raw;

    raw.start = GetLittleEndian(IndexToShort(seg->start->index));
    raw.end = GetLittleEndian(IndexToShort(seg->end->index));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    io.AddToLump(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG,
                "[{}] {}  Vert {:04X}->{:04X}  Line {:04X} {}  Angle {:04X}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

void PutSubsecs_DoomBSP(WadIO &io, Level &level)
{
  io.StartWritingLump("SSECTORS");

  for (auto sub : level.subsecs)
  {
    MapSubsectorDoomBSP raw;

    raw.first = GetLittleEndian(IndexToShort(sub->seg_list->index));
    raw.num = GetLittleEndian(IndexToShort(sub->seg_count));

    io.AddToLump(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] {}  First {:04X}  Num {:04X} Mid ({:1.1f},{:1.1f})", __func__, sub->index,
                GetLittleEndian(raw.first), GetLittleEndian(raw.num), sub->mid_x, sub->mid_y);
    }
  }
}

static void PutOneNode_Vanilla(WadIO &io, Node *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_Vanilla(io, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Vanilla(io, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  MapNodeDoomBSP raw;

  raw.x = GetLittleEndian(FloatToShort(node->x));
  raw.y = GetLittleEndian(FloatToShort(node->y));
  raw.dx = GetLittleEndian(FloatToShort(node->dx));
  raw.dy = GetLittleEndian(FloatToShort(node->dy));

  raw.b1.minx = GetLittleEndian(node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian(node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian(node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian(node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian(node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian(node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian(node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian(node->l.bounds.maxy);

  if (node->r.node)
  {
    raw.right = GetLittleEndian(IndexToShort(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(IndexToShort(node->r.subsec->index | NF_SUBSECTOR_VANILLA));
  }
  else
  {
    throw RuntimeException("Bad right child in node {}", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(IndexToShort(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(IndexToShort(node->l.subsec->index | NF_SUBSECTOR_VANILLA));
  }
  else
  {
    throw RuntimeException("Bad left child in node {}", node->index);
  }

  io.AddToLump(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(Log::DEBUG, "[{}] {}  Left {:04X}  Right {:04X}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

void PutNodes_DoomBSP(WadIO &io, Level &level)
{
  size_t node_cur_index = 0;
  io.StartWritingLump("NODES");

  if (level.root_node != nullptr)
  {
    PutOneNode_Vanilla(io, level.root_node, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    throw RuntimeException("[{}] miscounted ({} != {})", __func__, node_cur_index, level.nodes.size());
  }
}

void PutLeafs_DoomBSP(WadIO &io, Level &level)
{
  io.StartWritingLump("LEAFS");
  uint16_t actual_seg_index = 0;

  for (auto subsec : level.subsecs)
  {
    Segment *seg = subsec->seg_list;
    uint16_t seg_count = IndexToShort(subsec->seg_count);

    io.AddToLump(&seg_count, sizeof(seg_count));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] Subsector[{}] leaf references: {}", __func__, subsec->index, seg_count);
    }

    for (size_t j = 0; j < seg_count; j++)
    {
      // Do not remove minisegs from tree before
      // we are able to write all this
      MapLeafRefDoomBSP raw;
      Vertex *vert = seg->start;

      raw.vertex = GetLittleEndian(IndexToShort(vert->index));
      if (seg->linedef)
      {
        raw.seg = GetLittleEndian(actual_seg_index);
        actual_seg_index++;
      }
      else
      {
        raw.seg = NO_INDEX_INT16;
      }

      io.AddToLump(&raw, sizeof(raw));

      seg = seg->next;

      if (HAS_BIT(config.debug, DEBUG_BSP))
      {
        PrintLine(Log::DEBUG, "[{}] Segment = {} Vertex = {}", __func__, raw.seg, raw.vertex);
      }
    }
  }
}

//
// DeePBSPV4 format
//

void PutSegs_DeePBSPV4(WadIO &io, Level &level)
{
  io.StartWritingLump("SEGS");

  for (auto seg : level.segs)
  {
    MapSegmentDeePBSPV4 raw;

    raw.start = GetLittleEndian(IndexToInt(seg->start->index));
    raw.end = GetLittleEndian(IndexToInt(seg->end->index));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    io.AddToLump(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG,
                "[{}] {}  Vert {:08X}->{:08X}  Line {:04X} {}  Angle {:04X}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

void PutSubsecs_DeePBSPV4(WadIO &io, Level &level)
{
  io.StartWritingLump("SSECTORS");

  for (auto sub : level.subsecs)
  {
    MapSubsectorDeePBSPV4 raw;

    raw.first = GetLittleEndian(IndexToInt(sub->seg_list->index));
    raw.num = GetLittleEndian(IndexToShort(sub->seg_count));

    io.AddToLump(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] {}  First {:08X}  Num {:04X} Mid ({:1.1f},{:1.1f})", __func__, sub->index,
                GetLittleEndian(raw.first), GetLittleEndian(raw.num), sub->mid_x, sub->mid_y);
    }
  }
}

static void PutOneNode_DeePBSPV4(WadIO &io, Node *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_DeePBSPV4(io, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_DeePBSPV4(io, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  MapNodeDeePBSPV4 raw;

  raw.x = GetLittleEndian(FloatToShort(node->x));
  raw.y = GetLittleEndian(FloatToShort(node->y));
  raw.dx = GetLittleEndian(FloatToShort(node->dx));
  raw.dy = GetLittleEndian(FloatToShort(node->dy));

  raw.b1.minx = GetLittleEndian(node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian(node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian(node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian(node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian(node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian(node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian(node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian(node->l.bounds.maxy);

  if (node->r.node)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad right child in node {}", __func__, node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad left child in node {}", __func__, node->index);
  }

  io.AddToLumpZ(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(Log::DEBUG, "[{}] {}  Left {:08X}  Right {:08X}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

void PutNodes_DeePBSPV4(WadIO &io, Level &level)
{
  size_t node_cur_index = 0;
  io.StartWritingLump("NODES");
  io.AddToLump("xNd4\0\0\0\0", 8);

  if (level.root_node != nullptr)
  {
    PutOneNode_DeePBSPV4(io, level.root_node, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    throw RuntimeException("[{}] miscounted ({} != {})", __func__, node_cur_index, level.nodes.size());
  }
}

void PutLeafs_DeePBSPV4(WadIO &io, Level &level)
{
  io.StartWritingLump("LEAFS");
  uint32_t actual_seg_index = 0;

  for (auto subsec : level.subsecs)
  {
    Segment *seg = subsec->seg_list;
    uint32_t seg_count = IndexToInt(subsec->seg_count);

    io.AddToLump(&seg_count, sizeof(seg_count));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] Subsector[{}] leaf references: {}", __func__, subsec->index, seg_count);
    }

    for (size_t j = 0; j < seg_count; j++)
    {
      // Do not remove minisegs from tree before
      // we are able to write all this
      MapLeafRefDeePBSPV4 raw;
      Vertex *vert = seg->start;

      raw.vertex = GetLittleEndian(IndexToInt(vert->index));
      if (seg->linedef)
      {
        raw.seg = GetLittleEndian(actual_seg_index);
        actual_seg_index++;
      }
      else
      {
        raw.seg = NO_INDEX_INT32;
      }

      io.AddToLump(&raw, sizeof(raw));

      seg = seg->next;

      if (HAS_BIT(config.debug, DEBUG_BSP))
      {
        PrintLine(Log::DEBUG, "[{}] Segment = {} Vertex = {}", __func__, raw.seg, raw.vertex);
      }
    }
  }
}

//
// ZDBSP formats -- XNOD, ZNOD, XGLN, ZGLN, XGL2, ZGL2, XGL3, ZGL3
//

static void PutVertices_XNOD(WadIO &io, Level &level)
{
  uint32_t orgverts = GetLittleEndian(IndexToInt(level.num_old_vert));
  uint32_t newverts = GetLittleEndian(IndexToInt(level.num_new_vert));

  io.AddToLumpZ(&orgverts, sizeof(orgverts));
  io.AddToLumpZ(&newverts, sizeof(newverts));

  size_t count = 0;
  for (auto vert : level.vertices)
  {
    MapVertexXNOD raw;

    if (!vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(FloatToFixed(vert->x));
    raw.y = GetLittleEndian(FloatToFixed(vert->y));

    io.AddToLumpZ(&raw, sizeof(raw));

    count++;
  }

  if (count != level.num_new_vert)
  {
    throw RuntimeException("[{}] miscounted ({} != {})", __func__, count, level.num_new_vert);
  }
}

static void PutSubsecs_XNOD(WadIO &io, Level &level)
{
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.subsecs.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  size_t cur_seg_index = 0;
  for (auto sub : level.subsecs)
  {
    MapSubsectorXNOD raw;
    raw.segnum = GetLittleEndian(IndexToInt(sub->seg_count));
    io.AddToLumpZ(&raw, sizeof(raw));

    // sanity check the seg index values
    size_t count = 0;
    for (const Segment *seg = sub->seg_list; seg; seg = seg->next, cur_seg_index++)
    {
      if (cur_seg_index != seg->index)
      {
        throw RuntimeException("[{}] seg index mismatch in sub {} ({} != {})", __func__, sub->index, cur_seg_index, seg->index);
      }

      count++;
    }

    if (count != sub->seg_count)
    {
      throw RuntimeException("[{}] miscounted segs in sub {} ({} != {})", __func__, sub->index, count, sub->seg_count);
    }
  }

  if (cur_seg_index != level.segs.size())
  {
    throw RuntimeException("[{}] miscounted segs ({} != {})", __func__, cur_seg_index, level.segs.size());
  }
}

static void PutSegs_XNOD(WadIO &io, Level &level)
{
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.segs.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  for (auto seg : level.segs)
  {
    MapSegmentXNOD raw;

    raw.start = GetLittleEndian(IndexToInt(seg->start->index));
    raw.end = GetLittleEndian(IndexToInt(seg->end->index));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.side = seg->side;

    io.AddToLumpZ(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] {}  Vert {:04X}->{:04X}  Line {:04X} {}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                raw.side ? "L" : "R", seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

static void PutOneNode_XNOD(WadIO &io, Node *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_XNOD(io, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_XNOD(io, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  MapNodeXNOD raw;

  raw.x = GetLittleEndian(FloatToShort(node->x));
  raw.y = GetLittleEndian(FloatToShort(node->y));
  raw.dx = GetLittleEndian(FloatToShort(node->dx));
  raw.dy = GetLittleEndian(FloatToShort(node->dy));

  raw.b1.minx = GetLittleEndian(node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian(node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian(node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian(node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian(node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian(node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian(node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian(node->l.bounds.maxy);

  if (node->r.node)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad right child in ZDBSP node {}", __func__, node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad left child in ZDBSP node {}", __func__, node->index);
  }

  io.AddToLumpZ(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(Log::DEBUG, "[{}] {}  Left {:08X}  Right {:08X}  ({},{}) -> ({},{})", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_XNOD(WadIO &io, Level &level)
{
  size_t node_cur_index = 0;
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.nodes.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  if (level.root_node)
  {
    PutOneNode_XNOD(io, level.root_node, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    throw RuntimeException("[{}] miscounted ({} != {})", __func__, node_cur_index, level.nodes.size());
  }
}

static void PutSegs_XGLN(WadIO &io, Level &level)
{
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.segs.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  for (auto seg : level.segs)
  {
    MapSegmentXGLN raw;

    raw.vertex = GetLittleEndian(IndexToInt(seg->start->index));
    raw.partner = GetLittleEndian(IndexToInt(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    io.AddToLumpZ(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] {}  Vert {:04X}  Line {:04X} {}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__,
                seg->index, GetLittleEndian(raw.vertex), GetLittleEndian(raw.linedef), raw.side ? "L" : "R", seg->start->x,
                seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

static void PutSegs_XGL2(WadIO &io, Level &level)
{
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.segs.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  for (auto seg : level.segs)
  {
    MapSegmentXGL2 raw;

    raw.vertex = GetLittleEndian(IndexToInt(seg->start->index));
    raw.partner = GetLittleEndian(IndexToInt(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(IndexToInt(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    io.AddToLumpZ(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(Log::DEBUG, "[{}] {}  Vert {:04X}  Line {:04X} {}  ({:1.1f},{:1.1f}) -> ({:1.1f},{:1.1f})", __func__,
                seg->index, GetLittleEndian(raw.vertex), GetLittleEndian(raw.linedef), raw.side ? "L" : "R", seg->start->x,
                seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

static void PutOneNode_XGL3(WadIO &io, Node *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_XGL3(io, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_XGL3(io, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  MapNodeXGL3 raw;

  raw.x = GetLittleEndian(FloatToFixed(node->x));
  raw.y = GetLittleEndian(FloatToFixed(node->y));
  raw.dx = GetLittleEndian(FloatToFixed(node->dx));
  raw.dy = GetLittleEndian(FloatToFixed(node->dy));

  raw.b1.minx = GetLittleEndian(node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian(node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian(node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian(node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian(node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian(node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian(node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian(node->l.bounds.maxy);

  if (node->r.node)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(IndexToInt(node->r.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad right child in ZDBSP node {}", __func__, node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(IndexToInt(node->l.subsec->index | NF_SUBSECTOR));
  }
  else
  {
    throw RuntimeException("[{}] Bad left child in ZDBSP node {}", __func__, node->index);
  }

  io.AddToLumpZ(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(Log::DEBUG, "[{}] {}  Left {:08X}  Right {:08X}  ({},{}) -> ({},{})", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_XGL3(WadIO &io, Level &level)
{
  size_t node_cur_index = 0;
  uint32_t raw_num = GetLittleEndian(IndexToInt(level.nodes.size()));
  io.AddToLumpZ(&raw_num, sizeof(raw_num));

  if (level.root_node)
  {
    PutOneNode_XGL3(io, level.root_node, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    throw RuntimeException("[{}] miscounted ({} != {})", __func__, node_cur_index, level.nodes.size());
  }
}

//
// glBSP formats
//

constexpr auto GL_VERT_15 = BIT(15);
constexpr auto GL_VERT_30 = BIT(30);
constexpr auto GL_VERT_31 = BIT(31);

template <typename IndexType, IndexType B>
static inline IndexType VertexIndex_GL(Level &level, const Vertex *v)
{
  static_assert(std::is_same<IndexType, uint16_t>() || std::is_same<IndexType, uint32_t>(), "Invalid GL vertex index type");
  static_assert(B == GL_VERT_15 || B == GL_VERT_30 || B == GL_VERT_31, "Invalid GL vertex index bit");
  auto index = static_cast<IndexType>(v->index);
  return !v->is_new ? index : ((index - level.num_old_vert) | B);
}

static uint32_t CalcChecksumGLBSP(WadIO &io, Level &level)
{
  uint32_t crc = 1;

  auto vertexes = io.ReadLevelLump<byte>(level.lumps.label, "VERTEXES");
  if (vertexes.size() > 0)
  {
    crc = zng_adler32(crc, vertexes.data(), IndexToInt(vertexes.size()));
  }

  auto linedefs = io.ReadLevelLump<byte>(level.lumps.label, "LINEDEFS");
  if (linedefs.size() > 0)
  {
    crc = zng_adler32(crc, linedefs.data(), IndexToInt(linedefs.size()));
  }

  return crc;
}

void WriteGLMarker(WadIO &io, Level &level)
{
  std::string marker;
  uint32_t crc = CalcChecksumGLBSP(io, level);
  auto name = io.LumpName(level.lumps.label);
  auto gl_name = io.GlBspHeaderName(level.lumps.label);

  io.StartWritingLump(gl_name);
  if (gl_name == "GL_LEVEL")
  {
    marker += std::format("LEVEL={}\n", name);
  }
  marker += std::format("BUILDER={}\n", PROJECT_STRING);
  marker += std::format("CHECKSUM=0x{:08x}\n", crc);
  marker += std::format("TIME={}\n", TimeString());

  io.AddToLump(marker.c_str(), marker.size());
}

// This family is... messy
#define GLBSP_ASSERT(e, t, h, err) \
  if constexpr (format == e) static_assert(std::is_same_v<RawType, t> && std::string_view(magic_header) == h, err)

template <GlBspFormat format, typename RawType, const char *magic_header>
void PutNodes_GLBSP(Level &level)
{
  GLBSP_ASSERT(GlBspFormat::V1, MapNodeDoomBSP, "", "[PutNodes_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V2, MapNodeDoomBSP, "", "[PutNodes_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V3, MapNodeDoomBSP, "", "[PutNodes_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V4, MapNodeDeePBSPV4, "", "[PutNodes_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V5, MapNodeDeePBSPV4, "", "[PutNodes_GLBSP]: V5 format is malformed!");
}

template <GlBspFormat format, typename RawType, const char *magic_header>
void PutSubsectors_GLBSP(Level &level)
{
  GLBSP_ASSERT(GlBspFormat::V1, MapSubsectorDoomBSP, "", "[PutSubsectors_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V2, MapSubsectorDoomBSP, "", "[PutSubsectors_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V3, MapSubsectorGlv3, "gNd3", "[PutSubsectors_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V4, MapSubsectorDeePBSPV4, "", "[PutSubsectors_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V5, MapSubsectorGlv3, "", "[PutSubsectors_GLBSP]: V5 format is malformed!");
}

template <GlBspFormat format, typename RawType, const char *magic_header>
void PutSegs_GLBSP(Level &level)
{
  GLBSP_ASSERT(GlBspFormat::V1, MapSegmentGlv1, "", "[PutSegs_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V2, MapSegmentGlv1, "", "[PutSegs_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V3, MapSegmentGlv3, "gNd3", "[PutSegs_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V4, MapSegmentGlv4, "", "[PutSegs_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GlBspFormat::V5, MapSegmentGlv3, "", "[PutSegs_GLBSP]: V5 format is malformed!");
}

#undef GLBSP_ASSERT

//
// The family of ZDBSP BSP tree formats use a single lump no matter what
//
void PutTree_ZDBSP(WadIO &io, Level &level, std::string lumpname, BspFormat bsp_format, bool compress)
{
  if (bsp_format == BspFormat::XNOD) NormaliseBspTree(level);

  SortSegs(level);

  io.StartWritingLump(lumpname.c_str());

  if (bsp_format == BspFormat::XNOD) io.AddToLump(compress ? "ZNOD" : "XNOD", 4);
  if (bsp_format == BspFormat::XGLN) io.AddToLump(compress ? "ZGLN" : "XGLN", 4);
  if (bsp_format == BspFormat::XGL2) io.AddToLump(compress ? "ZGL2" : "XGL2", 4);
  if (bsp_format == BspFormat::XGL3) io.AddToLump(compress ? "ZGL3" : "XGL3", 4);

  if (compress) io.Begin_Zlib();

  PutVertices_XNOD(io, level);
  PutSubsecs_XNOD(io, level);

  if (bsp_format == BspFormat::XNOD) PutSegs_XNOD(io, level);
  if (bsp_format == BspFormat::XGLN) PutSegs_XGLN(io, level);
  if (bsp_format == BspFormat::XGL2 || bsp_format == BspFormat::XGL3) PutSegs_XGL2(io, level);

  if (bsp_format == BspFormat::XNOD || bsp_format == BspFormat::XGLN || bsp_format == BspFormat::XGL2) PutNodes_XNOD(io, level);
  if (bsp_format == BspFormat::XGL3) PutNodes_XGL3(io, level);

  if (compress) io.Finish_Zlib();
}
