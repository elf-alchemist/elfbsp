#include <algorithm>

#include "core.hpp"
#include "local.hpp"

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

void SortSegs(void)
{
  // sort segs into ascending index
  std::sort(lev_segs.begin(), lev_segs.end(), Compare_seg_pred());

  // remove unwanted segs
  while (lev_segs.size() > 0 && lev_segs.back()->index == SEG_IS_GARBAGE)
  {
    UtilFree(lev_segs.back());
    lev_segs.pop_back();
  }
}

static inline uint16_t VanillaSegDist(const seg_t *seg)
{
  double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  // use the "true" starting coord (as stored in the wad)
  double sx = round(seg->start->x);
  double sy = round(seg->start->y);

  return static_cast<uint16_t>(floor(hypot(sx - lx, sy - ly) + 0.5));
}

static inline short_angle_t VanillaSegAngle(const seg_t *seg)
{
  // compute the "true" delta
  double dx = round(seg->end->x) - round(seg->start->x);
  double dy = round(seg->end->y) - round(seg->start->y);

  double angle = ComputeAngle(dx, dy);

  if (angle < 0)
  {
    angle += 360.0;
  }

  short_angle_t result = short_angle_t(floor(angle * 65536.0 / 360.0 + 0.5));

  // TODO: make sure this works better -- Hexen, UDMF
  // -Elf- ZokumBSP
  // 1080 => Additive degrees stored in tag
  // 1081 => Set to degrees stored in tag
  // 1082 => Additive BAM stored in tag
  // 1083 => Set to BAM stored in tag
  if (seg->linedef->special == Special_RotateDegrees)
  {
    result += DegreesToShortBAM(static_cast<uint16_t>(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateDegreesHard)
  {
    result = DegreesToShortBAM(static_cast<uint16_t>(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateAngleT)
  {
    result += static_cast<short_angle_t>(seg->linedef->tag);
  }
  else if (seg->linedef->special == Special_RotateAngleTHard)
  {
    result = static_cast<short_angle_t>(seg->linedef->tag);
  }

  return result;
}

static void PutVertices_Vanilla(void)
{
  // this size is worst-case scenario
  size_t size = lev_vertices.size() * sizeof(raw_vertex_t);
  Lump_c *lump = CreateLevelLump("VERTEXES", size);

  size_t count = 0;
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    raw_vertex_t raw;

    const vertex_t *vert = lev_vertices[i];

    if (vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(static_cast<int16_t>(floor(vert->x)));
    raw.y = GetLittleEndian(static_cast<int16_t>(floor(vert->y)));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  lump->Finish();

  if (count != num_old_vert)
  {
    PrintLine(LOG_ERROR, "PutVertices miscounted (%zu != %zu)", count, num_old_vert);
  }

  if (count > 65534)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of vertices has overflowed.");
    lev_overflows = true;
  }
}

static inline uint16_t VertexIndex16Bit(const vertex_t *v)
{
  if (v->is_new)
  {
    return static_cast<uint16_t>(v->index | 0x8000U);
  }

  return static_cast<uint16_t>(v->index);
}

static inline uint32_t VertexIndex_XNOD(const vertex_t *v)
{
  if (v->is_new)
  {
    return static_cast<uint32_t>(num_old_vert + v->index);
  }

  return static_cast<uint32_t>(v->index);
}

//
// Vanilla format
//

static void PutSegs_Vanilla(void)
{
  // this size is worst-case scenario
  size_t size = lev_segs.size() * sizeof(raw_seg_vanilla_t);

  Lump_c *lump = CreateLevelLump("SEGS", size);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    raw_seg_vanilla_t raw;

    const seg_t *seg = lev_segs[i];

    raw.start = GetLittleEndian(VertexIndex16Bit(seg->start));
    raw.end = GetLittleEndian(VertexIndex16Bit(seg->end));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    // -Elf- ZokumBSP
    if ((seg->linedef->dont_render_back && seg->side) || (seg->linedef->dont_render_front && !seg->side))
    {
      raw = {};
    }

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  Vert %04X->%04X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();

  if (lev_segs.size() > 65534)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of segs has overflowed.");
    lev_overflows = true;
  }
}

static void PutSubsecs_Vanilla(void)
{
  size_t size = lev_subsecs.size() * sizeof(raw_subsec_vanilla_t);

  Lump_c *lump = CreateLevelLump("SSECTORS", size);

  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    raw_subsec_vanilla_t raw;

    const subsec_t *sub = lev_subsecs[i];

    raw.first = GetLittleEndian(static_cast<uint16_t>(sub->seg_list->index));
    raw.num = GetLittleEndian(static_cast<uint16_t>(sub->seg_count));

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  First %04X  Num %04X", __func__, sub->index, GetLittleEndian(raw.first),
                GetLittleEndian(raw.num));
    }
  }

  if (lev_subsecs.size() > 32767)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of subsectors has overflowed.");
    lev_overflows = true;
  }

  lump->Finish();
}

static void PutOneNode_Vanilla(node_t *node, size_t &node_cur_index, Lump_c *lump)
{
  if (node->r.node)
  {
    PutOneNode_Vanilla(node->r.node, node_cur_index, lump);
  }

  if (node->l.node)
  {
    PutOneNode_Vanilla(node->l.node, node_cur_index, lump);
  }

  node->index = node_cur_index++;

  raw_node_vanilla_t raw;

  // note that x/y/dx/dy are always integral in non-UDMF maps
  raw.x = GetLittleEndian(static_cast<int16_t>(floor(node->x)));
  raw.y = GetLittleEndian(static_cast<int16_t>(floor(node->y)));
  raw.dx = GetLittleEndian(static_cast<int16_t>(floor(node->dx)));
  raw.dy = GetLittleEndian(static_cast<int16_t>(floor(node->dy)));

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %04X  Right %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Vanilla(node_t *root)
{
  // this can be bigger than the actual size, but never smaller
  size_t max_size = (lev_nodes.size() + 1) * sizeof(raw_node_vanilla_t);
  size_t node_cur_index = 0;
  Lump_c *lump = CreateLevelLump("NODES", max_size);

  if (root != nullptr)
  {
    PutOneNode_Vanilla(root, node_cur_index, lump);
  }

  lump->Finish();

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }

  if (node_cur_index > 32767)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of nodes has overflowed.");
    lev_overflows = true;
  }
}

//
// DeepBSPV4 format
//

static void PutSegs_DeepBSPV4(void)
{
  // this size is worst-case scenario
  size_t size = lev_segs.size() * sizeof(raw_seg_deepbspv4_t);
  Lump_c *lump = CreateLevelLump("SEGS", size);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    raw_seg_deepbspv4_t raw;

    const seg_t *seg = lev_segs[i];

    raw.start = GetLittleEndian(static_cast<uint32_t>(seg->start->index));
    raw.end = GetLittleEndian(static_cast<uint32_t>(seg->end->index));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  Vert %08X->%08X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();
}

static void PutSubsecs_DeepBSPV4(void)
{
  size_t size = lev_subsecs.size() * sizeof(raw_subsec_deepbspv4_t);

  Lump_c *lump = CreateLevelLump("SSECTORS", size);

  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    raw_subsec_deepbspv4_t raw;

    const subsec_t *sub = lev_subsecs[i];

    raw.first = GetLittleEndian(static_cast<uint32_t>(sub->seg_list->index));
    raw.num = GetLittleEndian(static_cast<uint16_t>(sub->seg_count));

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  First %08X  Num %04X", __func__, sub->index, GetLittleEndian(raw.first),
                GetLittleEndian(raw.num));
    }
  }

  lump->Finish();
}

static void PutOneNode_DeepBSPV4(node_t *node, size_t node_cur_index, Lump_c *lump)
{
  if (node->r.node)
  {
    PutOneNode_DeepBSPV4(node->r.node, node_cur_index, lump);
  }

  if (node->l.node)
  {
    PutOneNode_DeepBSPV4(node->l.node, node_cur_index, lump);
  }

  node->index = node_cur_index++;

  raw_node_deepbspv4_t raw;

  // note that x/y/dx/dy are always integral in non-UDMF maps
  raw.x = GetLittleEndian(static_cast<int16_t>(floor(node->x)));
  raw.y = GetLittleEndian(static_cast<int16_t>(floor(node->y)));
  raw.dx = GetLittleEndian(static_cast<int16_t>(floor(node->dx)));
  raw.dy = GetLittleEndian(static_cast<int16_t>(floor(node->dy)));

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_DeepBSPV4(node_t *root)
{
  // this can be bigger than the actual size, but never smaller
  // 8 bytes for BSP_MAGIC_DEEPBSPV4 header
  size_t max_size = 8 + (lev_nodes.size() + 1) * sizeof(raw_node_deepbspv4_t);
  size_t node_cur_index = 0;

  Lump_c *lump = CreateLevelLump("NODES", max_size);
  lump->Write(BSP_MAGIC_DEEPBSPV4, 4);

  if (root != nullptr)
  {
    PutOneNode_DeepBSPV4(root, node_cur_index, lump);
  }

  lump->Finish();

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }
}

//
// ZDoom format -- XNOD
//

static void PutVertices_Xnod(Lump_c *lump)
{
  size_t orgverts = GetLittleEndian(num_old_vert);
  size_t newverts = GetLittleEndian(num_new_vert);

  lump->Write(&orgverts, 4);
  lump->Write(&newverts, 4);

  size_t count = 0;
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    raw_xnod_vertex_t raw;

    const vertex_t *vert = lev_vertices[i];

    if (!vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(static_cast<int32_t>(floor(vert->x * 65536.0)));
    raw.y = GetLittleEndian(static_cast<int32_t>(floor(vert->y * 65536.0)));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  if (count != num_new_vert)
  {
    PrintLine(LOG_ERROR, "PutZVertices miscounted (%zu != %zu)", count, num_new_vert);
  }
}

static void PutSubsecs_Xnod(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_subsecs.size());
  lump->Write(&raw_num, 4);

  size_t cur_seg_index = 0;
  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    const subsec_t *sub = lev_subsecs[i];

    raw_num = GetLittleEndian(sub->seg_count);
    lump->Write(&raw_num, 4);

    // sanity check the seg index values
    size_t count = 0;
    for (const seg_t *seg = sub->seg_list; seg; seg = seg->next, cur_seg_index++)
    {
      if (cur_seg_index != seg->index)
      {
        PrintLine(LOG_ERROR, "PutZSubsecs: seg index mismatch in sub %zu (%zu != %zu)", i, cur_seg_index, seg->index);
      }

      count++;
    }

    if (count != sub->seg_count)
    {
      PrintLine(LOG_ERROR, "PutZSubsecs: miscounted segs in sub %zu (%zu != %zu)", i, count, sub->seg_count);
    }
  }

  if (cur_seg_index != lev_segs.size())
  {
    PrintLine(LOG_ERROR, "PutZSubsecs miscounted segs (%zu != %zu)", cur_seg_index, lev_segs.size());
  }
}

static void PutSegs_Xnod(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "PutZSegs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_xnod_seg_t raw = {};

    raw.start = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.end = GetLittleEndian(VertexIndex_XNOD(seg->end));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef->index));
    raw.side = seg->side;
    lump->Write(&raw, sizeof(raw));
  }
}

static void PutOneNode_Xnod(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  raw_xnod_node_t raw;

  if (node->r.node)
  {
    PutOneNode_Xnod(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Xnod(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  raw.x = GetLittleEndian(static_cast<int16_t>(floor(node->x)));
  raw.y = GetLittleEndian(static_cast<int16_t>(floor(node->y)));
  raw.dx = GetLittleEndian(static_cast<int16_t>(floor(node->dx)));
  raw.dy = GetLittleEndian(static_cast<int16_t>(floor(node->dy)));

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in ZDoom node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in ZDoom node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw_xnod_node_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%f,%f) -> (%f,%f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Xnod(Lump_c *lump, node_t *root)
{
  size_t node_cur_index = 0;
  size_t raw_num = GetLittleEndian(lev_nodes.size());
  lump->Write(&raw_num, 4);

  if (root)
  {
    PutOneNode_Xnod(lump, root, node_cur_index);
  }

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutZNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }
}

static size_t CalcXnodNodesSize(void)
{
  // compute size of the ZDoom format nodes.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  size_t size = 32; // header + a bit extra

  size += 8 + lev_vertices.size() * sizeof(raw_xnod_vertex_t);
  size += 4 + lev_subsecs.size() * sizeof(raw_xnod_subsec_t);
  size += 4 + lev_segs.size() * sizeof(raw_xnod_seg_t);
  size += 4 + lev_nodes.size() * sizeof(raw_xnod_node_t);

  return size;
}

//
// ZDoom format -- XGLN, XGL2, XGL3
//

static void PutSegs_Xgln(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "PutXGL3Segs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_xgln_seg_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.partner = GetLittleEndian(static_cast<uint32_t>(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] SEG[%zu] v1=%d partner=%d line=%d side=%d", __func__, i, raw.vertex, raw.partner, raw.linedef,
                raw.side);
    }
  }
}

static void PutSegs_Xgl2(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "PutXGL3Segs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_xgl2_seg_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.partner = GetLittleEndian(static_cast<uint32_t>(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(static_cast<uint32_t>(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] SEG[%zu] v1=%d partner=%d line=%d side=%d", __func__, i, raw.vertex, raw.partner, raw.linedef,
                raw.side);
    }
  }
}

static void PutOneNode_Xgl3(Lump_c *lump, node_t *node, size_t &node_cur_index)
{
  raw_xgl3_node_t raw;

  if (node->r.node)
  {
    PutOneNode_Xgl3(lump, node->r.node, node_cur_index);
  }

  if (node->l.node)
  {
    PutOneNode_Xgl3(lump, node->l.node, node_cur_index);
  }

  node->index = node_cur_index++;

  raw.x = GetLittleEndian(static_cast<int32_t>(floor(node->x) * 65536.0));
  raw.y = GetLittleEndian(static_cast<int32_t>(floor(node->y) * 65536.0));
  raw.dx = GetLittleEndian(static_cast<int32_t>(floor(node->dx * 65536.0)));
  raw.dy = GetLittleEndian(static_cast<int32_t>(floor(node->dy * 65536.0)));

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in ZDoom node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in ZDoom node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw_xgl3_node_t));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%f,%f) -> (%f,%f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Xgl3(Lump_c *lump, node_t *root)
{
  size_t node_cur_index = 0;
  size_t raw_num = GetLittleEndian(lev_nodes.size());
  lump->Write(&raw_num, 4);

  if (root)
  {
    PutOneNode_Xgl3(lump, root, node_cur_index);
  }

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutZNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }
}

//
// Lump writing procedures
//

void SaveFormat_Vanilla(node_t *root_node)
{
  // remove all the mini-segs from subsectors
  NormaliseBspTree();
  // reduce vertex precision for classic DOOM nodes.
  // some segs can become "degenerate" after this, and these
  // are removed from subsectors.
  RoundOffBspTree();
  SortSegs();
  PutVertices_Vanilla();
  PutSegs_Vanilla();
  PutSubsecs_Vanilla();
  PutNodes_Vanilla(root_node);
}

void SaveFormat_DeepBSPV4(node_t *root_node)
{
  // remove all the mini-segs from subsectors
  NormaliseBspTree();
  // reduce vertex precision for classic DOOM nodes.
  // some segs can become "degenerate" after this, and these
  // are removed from subsectors.
  RoundOffBspTree();
  SortSegs();
  PutVertices_Vanilla();
  PutSegs_DeepBSPV4();
  PutSubsecs_DeepBSPV4();
  PutNodes_DeepBSPV4(root_node);
}

void SaveFormat_Xnod(node_t *root_node)
{
  CreateLevelLump("SEGS")->Finish();
  CreateLevelLump("SSECTORS")->Finish();
  // remove all the mini-segs from subsectors
  NormaliseBspTree();
  SortSegs();

  Lump_c *lump = CreateLevelLump("NODES", CalcXnodNodesSize());
  lump->Write(BSP_MAGIC_XNOD, 4);
  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xnod(lump);
  PutNodes_Xnod(lump, root_node);

  lump->Finish();
  lump = nullptr;
}

void SaveFormat_Xgln(node_t *root_node)
{
  // leave SEGS empty
  CreateLevelLump("SEGS")->Finish();

  SortSegs();

  // WISH : compute a max_size
  Lump_c *lump = CreateLevelLump("SSECTORS");
  lump->Write(BSP_MAGIC_XGLN, 4);
  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xgln(lump);
  PutNodes_Xnod(lump, root_node);

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump("NODES")->Finish();
}

void SaveFormat_Xgl2(node_t *root_node)
{
  // leave SEGS empty
  CreateLevelLump("SEGS")->Finish();

  SortSegs();

  // WISH : compute a max_size
  Lump_c *lump = CreateLevelLump("SSECTORS");
  lump->Write(BSP_MAGIC_XGL2, 4);
  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xgl2(lump);
  PutNodes_Xnod(lump, root_node);

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump("NODES")->Finish();
}

void SaveFormat_Xgl3(node_t *root_node)
{
  // leave SEGS empty
  CreateLevelLump("SEGS")->Finish();

  SortSegs();

  // WISH : compute a max_size
  Lump_c *lump = CreateLevelLump("SSECTORS");
  lump->Write(BSP_MAGIC_XGL3, 4);
  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xgl2(lump);
  PutNodes_Xgl3(lump, root_node);

  lump->Finish();
  lump = nullptr;

  // leave NODES empty
  CreateLevelLump("NODES")->Finish();
}
