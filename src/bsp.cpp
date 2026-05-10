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

#include <algorithm>
#include <string_view>
#include <type_traits>

#include "core.hpp"
#include "local.hpp"

// Upper-most bit is used for distinguishing sub-sectors, i.e tree leaves
constexpr uint16_t NF_SUBSECTOR_VANILLA = BIT(15);
constexpr uint32_t NF_SUBSECTOR = BIT(31);

//
// Utility
//

struct Compare_seg_pred
{
  inline bool operator()(const seg_t *A, const seg_t *B) const
  {
    return A->index < B->index;
  }
};

void SortSegs(level_t &level)
{
  // sort segs into ascending index
  std::sort(level.segs.begin(), level.segs.end(), Compare_seg_pred());

  // remove unwanted segs
  while (level.segs.size() > 0 && level.segs.back()->index == SEG_IS_GARBAGE)
  {
    UtilFree(level.segs.back());
    level.segs.pop_back();
  }
}

static inline int16_t VanillaSegDist(const seg_t *seg)
{
  double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  // use the "true" starting coord (as stored in the wad)
  double sx = round(seg->start->x);
  double sy = round(seg->start->y);

  return FloatToShort(floor(hypot(sx - lx, sy - ly) + 0.5));
}

static inline short_angle_t VanillaSegAngle(const seg_t *seg)
{
  // compute the "true" delta
  double dx = round(seg->end->x) - round(seg->start->x);
  double dy = round(seg->end->y) - round(seg->start->y);

  short_angle_t result = 0;

  switch (seg->linedef->angle)
  {
  case FX_RotateRelativeDegrees:
    result = ComputeAngle_BAM(dx, dy);
    result += DegreesToShortBAM(seg->linedef->tag);
    break;
  case FX_RotateAbsoluteDegrees:
    result = DegreesToShortBAM(seg->linedef->tag);
    break;
  case FX_RotateRelativeBAM:
    result = ComputeAngle_BAM(dx, dy);
    result += static_cast<short_angle_t>(seg->linedef->tag);
    break;
  case FX_RotateAbsoluteBAM:
    result = static_cast<short_angle_t>(seg->linedef->tag);
    break;
  case FX_DoNotRotate:
    result = ComputeAngle_BAM(dx, dy);
    break;
  }

  return result;
}

//------------------------------------------------------------------------
//  Adler-32 CHECKSUM Code
//------------------------------------------------------------------------

void Adler32_AddBlock(uint32_t *crc, const uint8_t *data, size_t length)
{
  uint32_t s1 = (*crc) & 0xFFFF;
  uint32_t s2 = ((*crc) >> 16) & 0xFFFF;

  while (length >= 1)
  {
    s1 = (s1 + *data) % 65521;
    s2 = (s2 + s1) % 65521;
    data++;
    length--;
  }

  *crc = (s2 << 16) | s1;
}

//
// TimeString
//
constexpr const char time_str[] = "%04d-%02d-%02d %02d:%02d:%02d.%04d";
constexpr size_t time_str_size = sizeof(time_str);

void TimeString(char buf[time_str_size])
{
#ifdef WIN32
  SYSTEMTIME sys_time;
  GetSystemTime(&sys_time);
  M_snprintf(buf, time_str_size, time_str,
             sys_time.wYear,               // This
             sys_time.wMonth,              // is
             sys_time.wDay,                // surely
             sys_time.wHour,               // too
             sys_time.wMinute,             // many
             sys_time.wSecond,             // props,
             sys_time.wMilliseconds * 10); // right?
#else                                      // LINUX or MACOSX
  time_t epoch_time;
  if (time(&epoch_time) == NO_TIME) return;

  tm *calend_time = localtime(&epoch_time);
  if (!calend_time) return;

  M_snprintf(buf, time_str_size, time_str,
             calend_time->tm_year + 1900, // This
             calend_time->tm_mon + 1,     // is
             calend_time->tm_mday,        // surely
             calend_time->tm_hour,        // too
             calend_time->tm_min,         // many
             calend_time->tm_sec,         // props,
             0);                          // right?
#endif
}

//
// Write out the full set of vertices
//

static void PutVertices_Doom(level_t &level)
{
  // this size is worst-case scenario
  size_t size = level.vertices.size() * sizeof(raw_vertex_t);
  Lump_c *lump = CreateLevelLump(level, "VERTEXES", size);

  size_t count = 0;
  for (size_t i = 0; i < level.vertices.size(); i++)
  {
    raw_vertex_t raw;

    const vertex_t *vert = level.vertices[i];

    // see: RoundOffVertices()
    if (vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(FloatToShort(floor(vert->x)));
    raw.y = GetLittleEndian(FloatToShort(floor(vert->y)));

    lump->Write(&raw, sizeof(raw_vertex_t));

    count++;
  }

  lump->Finish();

  if (count != level.num_old_vert)
  {
    PrintLine(LOG_ERROR, "ERROR: PutVertices miscounted (%zu != %zu)", count, level.num_old_vert);
  }
}

static void PutVertices_Doom64(level_t &level)
{
  // this size is worst-case scenario
  size_t size = level.vertices.size() * sizeof(raw_vertex_doom64_t);
  Lump_c *lump = CreateLevelLump(level, "VERTEXES", size);

  for (size_t i = 0; i < level.vertices.size(); i++)
  {
    raw_vertex_doom64_t raw;

    const vertex_t *vert = level.vertices[i];

    // do not ignore vertex_t::is_new
    // it is required for leafs

    raw.x = GetLittleEndian(FloatToFixed(vert->x));
    raw.y = GetLittleEndian(FloatToFixed(vert->y));

    lump->Write(&raw, sizeof(raw_vertex_doom64_t));
  }

  lump->Finish();
}

//
// Vanilla format
//

static void PutSegs_Vanilla(level_t &level)
{
  // this size is worst-case scenario
  size_t size = level.segs.size() * sizeof(raw_seg_vanilla_t);

  Lump_c *lump = CreateLevelLump(level, "SEGS", size);

  for (size_t i = 0; i < level.segs.size(); i++)
  {
    raw_seg_vanilla_t raw;

    const seg_t *seg = level.segs[i];

    raw.start = GetLittleEndian(IndexToShort(seg->start->index));
    raw.end = GetLittleEndian(IndexToShort(seg->end->index));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    lump->Write(&raw, sizeof(raw_seg_vanilla_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  Vert %04X->%04X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();
}

static void PutSubsecs_Vanilla(level_t &level)
{
  size_t size = level.subsecs.size() * sizeof(raw_subsec_vanilla_t);

  Lump_c *lump = CreateLevelLump(level, "SSECTORS", size);

  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    raw_subsec_vanilla_t raw;

    const subsec_t *sub = level.subsecs[i];

    raw.first = GetLittleEndian(IndexToShort(sub->seg_list->index));
    raw.num = GetLittleEndian(IndexToShort(sub->seg_count));

    lump->Write(&raw, sizeof(raw_subsec_vanilla_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  First %04X  Num %04X", __func__, sub->index, GetLittleEndian(raw.first),
                GetLittleEndian(raw.num));
    }
  }

  lump->Finish();
}

static void PutOneNode_Vanilla(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_Vanilla(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Vanilla(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  raw_node_vanilla_t raw;

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
    PrintLine(LOG_ERROR, "ERROR: Bad right child in node %zu", node->index);
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
    PrintLine(LOG_ERROR, "ERROR: Bad left child in node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw_node_vanilla_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %04X  Right %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Vanilla(level_t &level, node_t *root_node)
{
  // this can be bigger than the actual size, but never smaller
  size_t max_size = (level.nodes.size() + 1) * sizeof(raw_node_vanilla_t);
  size_t node_cur_index = 0;
  Lump_c *lump = CreateLevelLump(level, "NODES", max_size);

  if (root_node != nullptr)
  {
    PutOneNode_Vanilla(lump, root_node, node_cur_index);
  }

  lump->Finish();

  if (node_cur_index != level.nodes.size())
  {
    PrintLine(LOG_ERROR, "ERROR: PutNodes miscounted (%zu != %zu)", node_cur_index, level.nodes.size());
  }
}

static void PutLeafs_Vanilla(level_t &level)
{
  Lump_c *lump = CreateLevelLump(level, "LEAFS");
  uint16_t actual_seg_index = 0;

  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    subsec_t *subsec = level.subsecs[i];
    seg_t *seg = subsec->seg_list;
    size_t seg_count = subsec->seg_count;

    lump->Write(&seg_count, sizeof(uint16_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] Subsector[%zu] leaf references: %zu", __func__, i, seg_count);
    }

    if (seg_count < 3)
    {
      PrintLine(LOG_ERROR, "[%s] Subsector[%zu] has fewer than 3 leaf references", __func__, i);
    }

    for (size_t j = 0; j < seg_count; j++)
    {
      // Do not remove minisegs from tree before
      // we are able to write all this
      raw_leaf_vanilla_t raw;
      vertex_t *vert = seg->start;

      raw.vertex = GetLittleEndian(IndexToShort(vert->index));
      if (seg->linedef)
      // if (seg->linedef && !vert->is_new)
      {
        raw.seg = GetLittleEndian(actual_seg_index);
        actual_seg_index++;
      }
      else
      {
        raw.seg = NO_INDEX_INT16;
      }

      lump->Write(&raw, sizeof(raw));
      seg = seg->next;

      if (HAS_BIT(config.debug, DEBUG_BSP))
      {
        PrintLine(LOG_DEBUG, "[%s] Segment = %hu Vertex = %hu", __func__, raw.seg, raw.vertex);
      }
    }
  }
}

//
// DeePBSPV4 format
//

static void PutSegs_DeePBSPV4(level_t &level)
{
  // this size is worst-case scenario
  size_t size = level.segs.size() * sizeof(raw_seg_deepbspv4_t);
  Lump_c *lump = CreateLevelLump(level, "SEGS", size);

  for (size_t i = 0; i < level.segs.size(); i++)
  {
    raw_seg_deepbspv4_t raw;

    const seg_t *seg = level.segs[i];

    raw.start = GetLittleEndian(IndexToInt(seg->start->index));
    raw.end = GetLittleEndian(IndexToInt(seg->end->index));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    lump->Write(&raw, sizeof(raw_seg_deepbspv4_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  Vert %08X->%08X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();
}

static void PutSubsecs_DeePBSPV4(level_t &level)
{
  size_t size = level.subsecs.size() * sizeof(raw_subsec_deepbspv4_t);

  Lump_c *lump = CreateLevelLump(level, "SSECTORS", size);

  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    raw_subsec_deepbspv4_t raw;

    const subsec_t *sub = level.subsecs[i];

    raw.first = GetLittleEndian(IndexToInt(sub->seg_list->index));
    raw.num = GetLittleEndian(IndexToShort(sub->seg_count));

    lump->Write(&raw, sizeof(raw_subsec_deepbspv4_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  First %08X  Num %04X", __func__, sub->index, GetLittleEndian(raw.first),
                GetLittleEndian(raw.num));
    }
  }

  lump->Finish();
}

static void PutOneNode_DeePBSPV4(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  if (node->r.node)
  {
    PutOneNode_DeePBSPV4(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_DeePBSPV4(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  raw_node_deepbspv4_t raw;

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
    PrintLine(LOG_ERROR, "ERROR: Bad right child in node %zu", node->index);
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
    PrintLine(LOG_ERROR, "ERROR: Bad left child in node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw_node_deepbspv4_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_DeePBSPV4(level_t &level, node_t *root_node)
{
  // this can be bigger than the actual size, but never smaller
  // 8 bytes for BSP_MAGIC_DEEPBSPV4 header
  // size_t max_size = 8 + (level.nodes.size() + 1) * sizeof(raw_node_deepbspv4_t);
  size_t node_cur_index = 0;

  Lump_c *lump = CreateLevelLump(level, "NODES");
  lump->Write("xNd4\0\0\0\0", 8);

  if (root_node != nullptr)
  {
    PutOneNode_DeePBSPV4(lump, root_node, node_cur_index);
  }

  lump->Finish();

  if (node_cur_index != level.nodes.size())
  {
    PrintLine(LOG_ERROR, "ERROR: PutNodes miscounted (%zu != %zu)", node_cur_index, level.nodes.size());
  }
}

static void PutLeafs_DeePBSPV4(level_t &level)
{
  Lump_c *lump = CreateLevelLump(level, "LEAFS");
  uint32_t actual_seg_index = 0;

  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    subsec_t *subsec = level.subsecs[i];
    seg_t *seg = subsec->seg_list;
    size_t seg_count = subsec->seg_count;

    lump->Write(&seg_count, sizeof(uint32_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] Subsector[%zu] leaf references: %zu", __func__, i, seg_count);
    }

    if (seg_count < 3)
    {
      PrintLine(LOG_ERROR, "[%s] Subsector[%zu] has fewer than 3 leaf references", __func__, i);
    }

    for (size_t j = 0; j < seg_count; j++)
    {
      // Do not remove minisegs from tree before
      // we are able to write all this
      raw_leaf_deepbspv4_t raw;
      vertex_t *vert = seg->start;

      raw.vertex = GetLittleEndian(IndexToShort(vert->index));
      if (seg->linedef)
      // if (seg->linedef && !vert->is_new)
      {
        raw.seg = GetLittleEndian(actual_seg_index);
        actual_seg_index++;
      }
      else
      {
        raw.seg = NO_INDEX_INT32;
      }

      lump->Write(&raw, sizeof(raw));
      seg = seg->next;

      if (HAS_BIT(config.debug, DEBUG_BSP))
      {
        PrintLine(LOG_DEBUG, "[%s] Segment = %u Vertex = %u", __func__, raw.seg, raw.vertex);
      }
    }
  }
}

//
// ZDBSP formats -- XNOD, ZNOD, XGLN, ZGLN, XGL2, ZGL2, XGL3, ZGL3
//

static inline uint32_t VertexIndex_XNOD(level_t &level, const vertex_t *v)
{
  if (v->is_new)
  {
    return IndexToInt(level.num_old_vert + v->index);
  }

  return IndexToInt(v->index);
}

static void PutVertices_Xnod(level_t &level, Lump_c *lump)
{
  size_t orgverts = GetLittleEndian(level.num_old_vert);
  size_t newverts = GetLittleEndian(level.num_new_vert);

  lump->WriteZ(&orgverts, 4);
  lump->WriteZ(&newverts, 4);

  size_t count = 0;
  for (size_t i = 0; i < level.vertices.size(); i++)
  {
    raw_vertex_xnod_t raw;

    const vertex_t *vert = level.vertices[i];

    if (!vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(FloatToFixed(vert->x));
    raw.y = GetLittleEndian(FloatToFixed(vert->y));

    lump->WriteZ(&raw, sizeof(raw_vertex_xnod_t));

    count++;
  }

  if (count != level.num_new_vert)
  {
    PrintLine(LOG_ERROR, "ERROR: PutZVertices miscounted (%zu != %zu)", count, level.num_new_vert);
  }
}

static void PutSubsecs_Xnod(level_t &level, Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(level.subsecs.size());
  lump->WriteZ(&raw_num, 4);

  size_t cur_seg_index = 0;
  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    const subsec_t *sub = level.subsecs[i];

    raw_num = GetLittleEndian(sub->seg_count);
    lump->WriteZ(&raw_num, 4);

    // sanity check the seg index values
    size_t count = 0;
    for (const seg_t *seg = sub->seg_list; seg; seg = seg->next, cur_seg_index++)
    {
      if (cur_seg_index != seg->index)
      {
        PrintLine(LOG_ERROR, "ERROR: PutZSubsecs: seg index mismatch in sub %zu (%zu != %zu)", i, cur_seg_index, seg->index);
      }

      count++;
    }

    if (count != sub->seg_count)
    {
      PrintLine(LOG_ERROR, "ERROR: PutZSubsecs: miscounted segs in sub %zu (%zu != %zu)", i, count, sub->seg_count);
    }
  }

  if (cur_seg_index != level.segs.size())
  {
    PrintLine(LOG_ERROR, "ERROR: PutZSubsecs miscounted segs (%zu != %zu)", cur_seg_index, level.segs.size());
  }
}

static void PutSegs_Xnod(level_t &level, Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(level.segs.size());
  lump->WriteZ(&raw_num, 4);

  for (size_t i = 0; i < level.segs.size(); i++)
  {
    const seg_t *seg = level.segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "ERROR: PutZSegs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_seg_xnod_t raw = {};

    raw.start = GetLittleEndian(VertexIndex_XNOD(level, seg->start));
    raw.end = GetLittleEndian(VertexIndex_XNOD(level, seg->end));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef->index));
    raw.side = seg->side;
    lump->WriteZ(&raw, sizeof(raw_seg_xnod_t));
  }
}

static void PutOneNode_Xnod(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  raw_node_xnod_t raw;

  if (node->r.node)
  {
    PutOneNode_Xnod(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Xnod(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

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
    PrintLine(LOG_ERROR, "ERROR: Bad right child in XNOD node %zu", node->index);
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
    PrintLine(LOG_ERROR, "ERROR: Bad left child in XNOD node %zu", node->index);
  }

  lump->WriteZ(&raw, sizeof(raw_node_xnod_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%f,%f) -> (%f,%f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Xnod(level_t &level, Lump_c *lump, node_t *root)
{
  size_t node_cur_index = 0;
  size_t raw_num = GetLittleEndian(level.nodes.size());
  lump->WriteZ(&raw_num, 4);

  if (root)
  {
    PutOneNode_Xnod(lump, root, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    PrintLine(LOG_ERROR, "ERROR: PutNodes_XNOD miscounted (%zu != %zu)", node_cur_index, level.nodes.size());
  }
}

static size_t CalcXnodNodesSize(level_t &level)
{
  // compute size of the ZDBSP format nodes.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  size_t size = 32; // header + a bit extra

  size += 8 + level.vertices.size() * sizeof(raw_vertex_xnod_t);
  size += 4 + level.subsecs.size() * sizeof(raw_subsec_xnod_t);
  size += 4 + level.segs.size() * sizeof(raw_seg_xgl2_t);
  size += 4 + level.nodes.size() * sizeof(raw_node_xgl3_t);

  return size;
}

static void PutSegs_Xgln(level_t &level, Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(level.segs.size());
  lump->WriteZ(&raw_num, 4);

  for (size_t i = 0; i < level.segs.size(); i++)
  {
    const seg_t *seg = level.segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "ERROR: PutXGL3Segs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_seg_xgln_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(level, seg->start));
    raw.partner = GetLittleEndian(IndexToInt(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(IndexToShort(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->WriteZ(&raw, sizeof(raw_seg_xgln_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] SEG[%zu] v1=%d partner=%d line=%d side=%d", __func__, i, raw.vertex, raw.partner, raw.linedef,
                raw.side);
    }
  }
}

static void PutSegs_Xgl2(level_t &level, Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(level.segs.size());
  lump->WriteZ(&raw_num, 4);

  for (size_t i = 0; i < level.segs.size(); i++)
  {
    const seg_t *seg = level.segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "ERROR: PutXGL3Segs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_seg_xgl2_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(level, seg->start));
    raw.partner = GetLittleEndian(IndexToInt(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(IndexToInt(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->WriteZ(&raw, sizeof(raw_seg_xgl2_t));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] SEG[%zu] v1=%d partner=%d line=%d side=%d", __func__, i, raw.vertex, raw.partner, raw.linedef,
                raw.side);
    }
  }
}

static void PutOneNode_Xgl3(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  raw_node_xgl3_t raw;

  if (node->r.node)
  {
    PutOneNode_Xgl3(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Xgl3(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

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
    PrintLine(LOG_ERROR, "ERROR: Bad right child in XGL3 node %zu", node->index);
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
    PrintLine(LOG_ERROR, "ERROR: Bad left child in XGL3 node %zu", node->index);
  }

  lump->WriteZ(&raw, sizeof(raw_node_xgl3_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%f,%f) -> (%f,%f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Xgl3(level_t &level, Lump_c *lump, node_t *root)
{
  size_t node_cur_index = 0;
  size_t raw_num = GetLittleEndian(level.nodes.size());
  lump->WriteZ(&raw_num, 4);

  if (root)
  {
    PutOneNode_Xgl3(lump, root, node_cur_index);
  }

  if (node_cur_index != level.nodes.size())
  {
    PrintLine(LOG_ERROR, "ERROR: PutNodes_XNOD miscounted (%zu != %zu)", node_cur_index, level.nodes.size());
  }
}

//
// glBSP formats
//

constexpr auto GL_VERT_15 = BIT(15);
constexpr auto GL_VERT_30 = BIT(30);
constexpr auto GL_VERT_31 = BIT(31);

template <typename IndexType, IndexType B>
static inline IndexType VertexIndex_GL(const vertex_t *v)
{
  static_assert(std::is_same<IndexType, uint16_t>() || std::is_same<IndexType, uint32_t>(), "Invalid GL vertex index type");
  static_assert(B == GL_VERT_15 || B == GL_VERT_30 || B == GL_VERT_31, "Invalid GL vertex index bit");
  auto index = static_cast<IndexType>(v->index);
  index |= (v->is_new) ? B : 0;
  return index;
}

static uint32_t CalcChecksumGLBSP(level_t &level)
{
  uint32_t crc = 1;

  Lump_c *lump = level.FindLevelLump("VERTEXES");
  if (lump && lump->Length() > 0)
  {
    byte *data = new byte[lump->Length()];
    if (!lump->Seek(0) || !lump->Read(data, lump->Length()))
      PrintLine(LOG_ERROR, "ERROR: Trouble reading vertices (for checksum).");
    Adler32_AddBlock(&crc, data, lump->Length());
    delete[] data;
  }

  lump = level.FindLevelLump("LINEDEFS");
  if (lump && lump->Length() > 0)
  {
    byte *data = new byte[lump->Length()];
    if (!lump->Seek(0) || !lump->Read(data, lump->Length()))
      PrintLine(LOG_ERROR, "ERROR: Trouble reading linedefs (for checksum).");
    Adler32_AddBlock(&crc, data, lump->Length());
    delete[] data;
  }

  return crc;
}

void UpdateGLMarker(level_t &level, Lump_c *marker)
{
  // this is very conservative, around 4 times the actual size
  const int max_size = 512;
  // we *must* compute the checksum BEFORE (re)creating the lump
  // [ otherwise we write data into the wrong part of the file ]
  uint32_t crc = CalcChecksumGLBSP(level);
  cur_wad->RecreateLump(marker, max_size);
  if (level.long_name) marker->PrintText("LEVEL=%s\n", level.GetLevelName());
  marker->PrintText("BUILDER=%s\n", PROJECT_STRING);
  marker->PrintText("CHECKSUM=0x%08x\n", crc);
  marker->Finish();
}

// This family is... messy
#define GLBSP_ASSERT(e, t, h, err) \
  static_assert(format == e && std::is_same_v<RawType, t> && std::string_view(magic_header) == h, err)

template <glbsp_format_t format, typename RawType, const char *magic_header>
void PutVertices_GLBSP(level_t &level)
{
  GLBSP_ASSERT(GLBSP_V1, raw_vertex_glv1_t, "", "[PutVertices_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GLBSP_V2, raw_vertex_glv2_t, "gNd2", "[PutVertices_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GLBSP_V3, raw_vertex_glv2_t, "gNd2", "[PutVertices_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GLBSP_V4, raw_vertex_glv2_t, "gNd4", "[PutVertices_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GLBSP_V5, raw_vertex_glv2_t, "gNd5", "[PutVertices_GLBSP]: V5 format is malformed!");

}

template <glbsp_format_t format, typename RawType, const char *magic_header>
void PutNodes_GLBSP(level_t &level, node_t *root_node)
{
  GLBSP_ASSERT(GLBSP_V1, raw_node_glv1_t, "", "[PutNodes_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GLBSP_V2, raw_node_glv1_t, "", "[PutNodes_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GLBSP_V3, raw_node_glv1_t, "", "[PutNodes_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GLBSP_V4, raw_node_glv4_t, "", "[PutNodes_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GLBSP_V5, raw_node_glv4_t, "", "[PutNodes_GLBSP]: V5 format is malformed!");
}

template <glbsp_format_t format, typename RawType, const char *magic_header>
void PutSubsectors_GLBSP(level_t &level)
{
  GLBSP_ASSERT(GLBSP_V1, raw_subsec_glv1_t, "", "[PutSubsectors_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GLBSP_V2, raw_subsec_glv1_t, "", "[PutSubsectors_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GLBSP_V3, raw_subsec_glv3_t, "gNd3", "[PutSubsectors_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GLBSP_V4, raw_subsec_glv4_t, "", "[PutSubsectors_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GLBSP_V5, raw_subsec_glv3_t, "", "[PutSubsectors_GLBSP]: V5 format is malformed!");
}

template <glbsp_format_t format, typename RawType, const char *magic_header>
void PutSegs_GLBSP(level_t &level)
{
  GLBSP_ASSERT(GLBSP_V1, raw_seg_glv1_t, "", "[PutSegs_GLBSP]: V1 format is malformed!");
  GLBSP_ASSERT(GLBSP_V2, raw_seg_glv1_t, "", "[PutSegs_GLBSP]: V2 format is malformed!");
  GLBSP_ASSERT(GLBSP_V3, raw_seg_glv3_t, "gNd3", "[PutSegs_GLBSP]: V3 format is malformed!");
  GLBSP_ASSERT(GLBSP_V4, raw_seg_glv4_t, "", "[PutSegs_GLBSP]: V4 format is malformed!");
  GLBSP_ASSERT(GLBSP_V5, raw_seg_glv3_t, "", "[PutSegs_GLBSP]: V5 format is malformed!");
}

#undef GLBSP_ASSERT

//
// Lump writing procedures
//

void SaveDoom_DoomBSP(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  // remove all the minisegs from subsectors
  NormaliseBspTree(level);
  // reduce vertex precision for classic DOOM nodes.
  // some segs can become "degenerate" after this, and these
  // are removed from subsectors.
  RoundOffBspTree(level);
  SortSegs(level);
  PutVertices_Doom(level);
  PutSegs_Vanilla(level);
  PutSubsecs_Vanilla(level);
  PutNodes_Vanilla(level, root_node);
}

void SaveDoom_DeePBSPV4(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  // remove all the minisegs from subsectors
  NormaliseBspTree(level);
  // reduce vertex precision for classic DOOM nodes.
  // some segs can become "degenerate" after this, and these
  // are removed from subsectors.
  RoundOffBspTree(level);
  SortSegs(level);
  PutVertices_Doom(level);
  PutSegs_DeePBSPV4(level);
  PutSubsecs_DeePBSPV4(level);
  PutNodes_DeePBSPV4(level, root_node);
}

void SaveDoom_XNOD(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  CreateLevelLump(level, "SEGS")->Finish();
  CreateLevelLump(level, "SSECTORS")->Finish();
  // remove all the minisegs from subsectors
  NormaliseBspTree(level);
  SortSegs(level);

  Lump_c *lump = CreateLevelLump(level, "NODES", CalcXnodNodesSize(level));

  if (level.zdbsp_compress) lump->Begin_Zlib();

  lump->Write(level.zdbsp_compress ? "ZNOD" : "XNOD", 4);
  PutVertices_Xnod(level, lump);
  PutSubsecs_Xnod(level, lump);
  PutSegs_Xnod(level, lump);
  PutNodes_Xnod(level, lump, root_node);

  if (level.zdbsp_compress) lump->Finish_Zlib();

  lump->Finish();
  lump = nullptr;
}

void SaveDoom_XGLN(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  // leave SEGS empty
  CreateLevelLump(level, "SEGS")->Finish();

  SortSegs(level);

  Lump_c *lump = CreateLevelLump(level, "SSECTORS", CalcXnodNodesSize(level));

  if (level.zdbsp_compress) lump->Begin_Zlib();

  lump->Write(level.zdbsp_compress ? "ZGLN" : "XGLN", 4);
  PutVertices_Xnod(level, lump);
  PutSubsecs_Xnod(level, lump);
  PutSegs_Xgln(level, lump);
  PutNodes_Xnod(level, lump, root_node);

  if (level.zdbsp_compress) lump->Finish_Zlib();

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump(level, "NODES")->Finish();
}

void SaveDoom_XGL2(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  // leave SEGS empty
  CreateLevelLump(level, "SEGS")->Finish();

  SortSegs(level);

  Lump_c *lump = CreateLevelLump(level, "SSECTORS", CalcXnodNodesSize(level));

  if (level.zdbsp_compress) lump->Begin_Zlib();

  lump->Write(level.zdbsp_compress ? "ZGL2" : "XGL2", 4);
  PutVertices_Xnod(level, lump);
  PutSubsecs_Xnod(level, lump);
  PutSegs_Xgl2(level, lump);
  PutNodes_Xnod(level, lump, root_node);

  if (level.zdbsp_compress) lump->Finish_Zlib();

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump(level, "NODES")->Finish();
}

void SaveDoom_XGL3(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  // leave SEGS empty
  CreateLevelLump(level, "SEGS")->Finish();

  SortSegs(level);

  Lump_c *lump = CreateLevelLump(level, "SSECTORS", CalcXnodNodesSize(level));

  if (level.zdbsp_compress) lump->Begin_Zlib();

  lump->Write(level.zdbsp_compress ? "ZGL3" : "XGL3", 4);
  PutVertices_Xnod(level, lump);
  PutSubsecs_Xnod(level, lump);
  PutSegs_Xgl2(level, lump);
  PutNodes_Xgl3(level, lump, root_node);

  if (level.zdbsp_compress) lump->Finish_Zlib();

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump(level, "NODES")->Finish();
}

//
// Doom64 has some differences from Doom-format or Hexen-format
// This could also be shared with PSX Doom and PSX Final Doom, but we don't support those
//

void SaveDoom64_DoomBSP(level_t &level, node_t *root_node)
{
  // Needed for LEAFS
  RoundOffVertices(level);
  PutVertices_Doom64(level);
  // We need minisegs just for leafs
  PutLeafs_Vanilla(level);
  // remove all the minisegs from subsectors
  NormaliseBspTree(level);
  SortSegs(level);
  PutSegs_Vanilla(level);
  PutSubsecs_Vanilla(level);
  PutNodes_Vanilla(level, root_node);
}

void SaveDoom64_DeePBSPV4(level_t &level, node_t *root_node)
{
  // Needed for LEAFS
  RoundOffVertices(level);
  PutVertices_Doom64(level);
  // We need minisegs just for leafs
  PutLeafs_DeePBSPV4(level);
  // remove all the minisegs from subsectors
  NormaliseBspTree(level);
  SortSegs(level);
  PutSegs_DeePBSPV4(level);
  PutSubsecs_DeePBSPV4(level);
  PutNodes_DeePBSPV4(level, root_node);
}

void SaveDoom_GLV1(level_t &level, node_t *root_node)
{
}

void SaveDoom_GLV2(level_t &level, node_t *root_node)
{
}

void SaveDoom_GLV3(level_t &level, node_t *root_node)
{
}

void SaveDoom_GLV4(level_t &level, node_t *root_node)
{
}

void SaveDoom_GLV5(level_t &level, node_t *root_node)
{
}

//
// Unlike the the Doom and Hexen map formats, UDMF has a tight requirement for fractional coordinates.
// Always use the latest high-precision BSP format we support.
//
void SaveTextmap_ZNODES(level_t &level, node_t *root_node)
{
  auto mark = Benchmarker(__func__);
  SortSegs(level);

  Lump_c *lump = CreateLevelLump(level, "ZNODES", CalcXnodNodesSize(level));

  lump->Begin_Zlib();
  lump->Write("ZGL3", 4);
  PutVertices_Xnod(level, lump);
  PutSubsecs_Xnod(level, lump);
  PutSegs_Xgl2(level, lump);
  PutNodes_Xgl3(level, lump, root_node);
  lump->Finish_Zlib();

  lump->Finish();
  lump = nullptr;
}
