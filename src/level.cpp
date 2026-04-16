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

#include "core.hpp"
#include "local.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

Wad_file *cur_wad;

int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax)
      {
        return false;
      }

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymax - y1) / static_cast<double>(y2 - y1));
      y1 = ymax;

      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin)
      {
        return false;
      }

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymin - y1) / static_cast<double>(y2 - y1));
      y1 = ymin;

      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax)
      {
        return false;
      }

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmax - x1) / static_cast<double>(x2 - x1));
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin)
      {
        return false;
      }

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmin - x1) / static_cast<double>(x2 - x1));
      x1 = xmin;

      count = 2;
      continue;
    }

    count--;

    if (count == 0)
    {
      break;
    }

    // swap end points
    tmp = x1;
    x1 = x2;
    x2 = tmp;
    tmp = y1;
    y1 = y2;
    y2 = tmp;
  }

  // linedef touches block
  return true;
}

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------

/* ----- allocation routines ---------------------------- */

vertex_t *NewVertex(level_t &level)
{
  vertex_t *V = UtilCalloc<vertex_t>(sizeof(vertex_t));
  V->index = level.vertices.size();
  level.vertices.push_back(V);
  return V;
}

linedef_t *NewLinedef(level_t &level)
{
  linedef_t *L = UtilCalloc<linedef_t>(sizeof(linedef_t));
  L->index = level.linedefs.size();
  level.linedefs.push_back(L);
  return L;
}

sidedef_t *NewSidedef(level_t &level)
{
  sidedef_t *S = UtilCalloc<sidedef_t>(sizeof(sidedef_t));
  S->index = level.sidedefs.size();
  level.sidedefs.push_back(S);
  return S;
}

sector_t *NewSector(level_t &level)
{
  sector_t *S = UtilCalloc<sector_t>(sizeof(sector_t));
  S->index = level.sectors.size();
  level.sectors.push_back(S);
  return S;
}

thing_t *NewThing(level_t &level)
{
  thing_t *T = UtilCalloc<thing_t>(sizeof(thing_t));
  T->index = level.things.size();
  level.things.push_back(T);
  return T;
}

seg_t *NewSeg(level_t &level)
{
  seg_t *S = UtilCalloc<seg_t>(sizeof(seg_t));
  level.segs.push_back(S);
  return S;
}

subsec_t *NewSubsec(level_t &level)
{
  subsec_t *S = UtilCalloc<subsec_t>(sizeof(subsec_t));
  level.subsecs.push_back(S);
  return S;
}

node_t *NewNode(level_t &level)
{
  node_t *N = UtilCalloc<node_t>(sizeof(node_t));
  level.nodes.push_back(N);
  return N;
}

walltip_t *NewWallTip(level_t &level)
{
  walltip_t *WT = UtilCalloc<walltip_t>(sizeof(walltip_t));
  level.walltips.push_back(WT);
  return WT;
}

intersection_t *NewIntersection(level_t &level)
{
  intersection_t *cut = new intersection_t;
  level.intercuts.push_back(cut);
  return cut;
}

/* ----- free routines ---------------------------- */

void FreeVertices(level_t &level)
{
  for (size_t i = 0; i < level.vertices.size(); i++)
  {
    UtilFree(level.vertices[i]);
  }

  level.vertices.clear();
}

void FreeLinedefs(level_t &level)
{
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    UtilFree(level.linedefs[i]);
  }

  level.linedefs.clear();
}

void FreeSidedefs(level_t &level)
{
  for (size_t i = 0; i < level.sidedefs.size(); i++)
  {
    UtilFree(level.sidedefs[i]);
  }

  level.sidedefs.clear();
}

void FreeSectors(level_t &level)
{
  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    UtilFree(level.sectors[i]);
  }

  level.sectors.clear();
}

void FreeThings(level_t &level)
{
  for (size_t i = 0; i < level.things.size(); i++)
  {
    UtilFree(level.things[i]);
  }

  level.things.clear();
}

void FreeSegs(level_t &level)
{
  for (size_t i = 0; i < level.segs.size(); i++)
  {
    UtilFree(level.segs[i]);
  }

  level.segs.clear();
}

void FreeSubsecs(level_t &level)
{
  for (size_t i = 0; i < level.subsecs.size(); i++)
  {
    UtilFree(level.subsecs[i]);
  }

  level.subsecs.clear();
}

void FreeNodes(level_t &level)
{
  for (size_t i = 0; i < level.nodes.size(); i++)
  {
    UtilFree(level.nodes[i]);
  }

  level.nodes.clear();
}

void FreeWallTips(level_t &level)
{
  for (size_t i = 0; i < level.walltips.size(); i++)
  {
    UtilFree(level.walltips[i]);
  }

  level.walltips.clear();
}

void FreeIntersections(level_t &level)
{
  for (size_t i = 0; i < level.intercuts.size(); i++)
  {
    delete level.intercuts[i];
  }

  level.intercuts.clear();
}

/* ----- reading routines ------------------------------ */

void ValidateLinedef(level_t &level, linedef_t *line)
{
  if (line->right || line->left)
  {
    level.num_real_lines++;
  }

  if (line->left && line->right && line->left->sector == line->right->sector)
  {
    line->effects |= FX_SelfReferential;
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Linedef #%zu is self-referencing", line->index);
    }
  }

  if (config.effects                                            // Effects enabled ...
      && line->left                                             // ... side exists ...
      && line->left->tex_lower[0] != '-'                        // ... lower texture isn't empty ...
      && StringCaseCmp(line->left->tex_lower, "BSPNOSEG") == 0) // ... lower texture is BSPNOSEG
  {
    line->effects |= FX_DoNotRenderBack;
  }

  if (config.effects                                             // Effects enabled ...
      && line->right                                             // ... side exists ...
      && line->right->tex_lower[0] != '-'                        // ... lower texture isn't empty ...
      && StringCaseCmp(line->right->tex_lower, "BSPNOSEG") == 0) // ... lower texture is BSPNOSEG
  {
    line->effects |= FX_DoNotRenderFront;
  }

  double deltax = line->start->x - line->end->x;
  double deltay = line->start->y - line->end->y;

  // check for extremely short lines
  if ((fabs(deltax) < DIST_EPSILON) && (fabs(deltay) < DIST_EPSILON))
  {
    line->effects |= FX_ZeroLength;
  }

  // check for extremely long lines
  if (hypot(deltax, deltay) >= 32000)
  {
    PrintLine(LOG_NORMAL, "WARNING: Linedef #%zu is VERY long, it may cause problems", line->index);
    config.total_warnings++;
  }
}

static void GetVertices_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("VERTEXES");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_vertex_t);
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to vertices.");
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_vertex_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading vertices.");
    }

    vertex_t *vert = NewVertex(level);

    vert->x = ShortToFloat(GetLittleEndian(raw.x));
    vert->y = ShortToFloat(GetLittleEndian(raw.y));
  }

  level.num_old_vert = level.vertices.size();
}

static void GetSectors_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SECTORS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sector_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to sectors.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sector_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading sectors.");
    }

    sector_t *sector = NewSector(level);

    sector->height_floor = ShortToFloat(GetLittleEndian(raw.floorh));
    sector->height_ceiling = ShortToFloat(GetLittleEndian(raw.ceilh));
  }
}

static void GetThings_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading things.");
    }

    thing_t *thing = NewThing(level);

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<doomednum_t>(GetLittleEndian(raw.type));
  }
}

static void GetSidedefs_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SIDEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sidedef_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to sidedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sidedef_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading sidedefs.");
    }

    sidedef_t *side = NewSidedef(level);

    side->offset_x = GetLittleEndian(raw.x_offset);
    side->offset_y = GetLittleEndian(raw.y_offset);
    memcpy(side->tex_upper, raw.upper_tex, 8);
    memcpy(side->tex_lower, raw.lower_tex, 8);
    memcpy(side->tex_middle, raw.mid_tex, 8);
    side->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), i);
  }
}

static void GetLinedefs_Doom(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_doom_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_doom_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading linedefs.");
    }

    linedef_t *line = NewLinedef(level);

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), i);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), i);
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));
    line->special = GetLittleEndian(raw.special);
    line->args[0] = GetLittleEndian(raw.tag);

    line->start->is_used = true;
    line->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_TWOSIDED))
    {
      line->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    // Line tags ( 900 <= x <=999 ) are considered "precious" and will, therefore, have a much higher seg split cost
    switch (line->args[0])
    {
    case 900 ... 997:
      line->effects |= FX_DoNotSplitSeg;
      break;
    case Tag_DoNotRender:
      line->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack | FX_DoNotSplitSeg;
      break;
    case Tag_NoBlockmap:
      line->effects |= FX_NoBlockmap | FX_DoNotSplitSeg;
      break;
    }

    switch (line->special)
    {
    case Special_RotateRelativeDegrees:
      line->angle = FX_RotateRelativeDegrees;
      break;
    case Special_RotateAbsoluteDegrees:
      line->angle = FX_RotateAbsoluteDegrees;
      break;
    case Special_RotateRelativeBAM:
      line->angle = FX_RotateRelativeBAM;
      break;
    case Special_RotateAbsoluteBAM:
      line->angle = FX_RotateAbsoluteBAM;
      break;
    case Special_DoNotRenderSegmentBack:
      line->effects |= FX_DoNotRenderBack;
      break;
    case Special_DoNotRenderSegmentFront:
      line->effects |= FX_DoNotRenderFront;
      break;
    case Special_DoNotRenderSegmentBoth:
      line->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack;
      break;
    case Special_DoNotSplitSeg:
      line->effects |= FX_DoNotSplitSeg;
      break;
    default:
      break;
    }
  }
}

static void GetThings_Hexen(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_hexen_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_hexen_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading things.");
    }

    thing_t *thing = NewThing(level);

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<doomednum_t>(GetLittleEndian(raw.type));
  }
}

static void GetLinedefs_Hexen(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_hexen_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_hexen_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading linedefs.");
    }

    linedef_t *line = NewLinedef(level);

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), i);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), i);
    line->special = raw.special;
    line->args[0] = raw.args[0];
    line->args[1] = raw.args[1];
    line->args[2] = raw.args[2];
    line->args[3] = raw.args[3];
    line->args[4] = raw.args[4];
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    line->start->is_used = true;
    line->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_HEXEN_TWOSIDED))
    {
      line->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    switch (line->special)
    {
    case BSP_SpecialEffects:
      if (line->args[0]) line->effects |= FX_NoBlockmap;
      if (line->args[1]) line->effects |= FX_DoNotSplitSeg;
      if (line->args[2]) line->effects |= FX_DoNotRenderBack;
      if (line->args[3]) line->effects |= FX_DoNotRenderFront;
      break;
    default:
      break;
    }
  }
}

static void GetVertices_Doom64(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("VERTEXES");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_vertex_doom64_t);
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to 32bit vertices.");
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_vertex_doom64_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading 32bit vertices.");
    }

    vertex_t *vert = NewVertex(level);

    vert->x = static_cast<double>(GetLittleEndian(raw.x) / FRACFACTOR);
    vert->y = static_cast<double>(GetLittleEndian(raw.y) / FRACFACTOR);
  }

  level.num_old_vert = level.vertices.size();
}

static void GetSectors_Doom64(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SECTORS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sector_doom64_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to Doom64 sectors.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sector_doom64_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading Doom64 sectors.");
    }

    sector_t *sector = NewSector(level);

    sector->height_floor = static_cast<double>(GetLittleEndian(raw.floorh));
    sector->height_ceiling = static_cast<double>(GetLittleEndian(raw.ceilh));
  }
}

static void GetSidedefs_Doom64(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("SIDEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sidedef_doom64_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to Doom64 sidedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sidedef_doom64_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading Doom64 sidedefs.");
    }

    sidedef_t *side = NewSidedef(level);

    side->offset_x = GetLittleEndian(raw.x_offset);
    side->offset_y = GetLittleEndian(raw.y_offset);
    // We don't care about texture indexes here
    side->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), i);
  }
}

static void GetLinedefs_Doom64(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_doom64_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to Doom64 linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_doom64_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading Doom64 linedefs.");
    }

    linedef_t *line = NewLinedef(level);

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), i);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), i);
    line->special = GetLittleEndian(raw.special);
    line->args[0] = GetLittleEndian(raw.tag);
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    line->start->is_used = true;
    line->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_TWOSIDED))
    {
      line->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, line);
  }
}

static void GetThings_Doom64(level_t &level)
{
  size_t count = 0;

  Lump_c *lump = level.FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_doom64_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to Doom64 things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_doom64_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading Doom64 things.");
    }

    thing_t *thing = NewThing(level);

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<doomednum_t>(GetLittleEndian(raw.type));
  }
}

/* ----- UDMF reading routines ------------------------- */

static constexpr uint32_t UDMF_THING = 1;
static constexpr uint32_t UDMF_VERTEX = 2;
static constexpr uint32_t UDMF_SECTOR = 3;
static constexpr uint32_t UDMF_SIDEDEF = 4;
static constexpr uint32_t UDMF_LINEDEF = 5;

static void ParseThingField(thing_t *thing, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    thing->x = LEX_Double(value);
  }

  if (key == "y")
  {
    thing->y = LEX_Double(value);
  }

  if (key == "type")
  {
    thing->type = static_cast<doomednum_t>(LEX_Int16(value));
  }
}

static void ParseVertexField(vertex_t *vertex, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    vertex->x = LEX_Double(value);
  }

  if (key == "y")
  {
    vertex->y = LEX_Double(value);
  }
}

static void ParseSectorField(sector_t *sector, const std::string &key, token_kind_e kind, const std::string &value)
{
  // nothing actually needed
}

static void ParseSidedefField(level_t &level, sidedef_t *side, const std::string &key, token_kind_e kind,
                              const std::string &value)
{
  if (key == "sector")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sectors.size())
    {
      PrintLine(LOG_ERROR, "ERROR: illegal sector number #%zu", static_cast<size_t>(num));
    }

    side->sector = level.sectors[num];
  }
}

static void ParseLinedefField(level_t &level, linedef_t *line, const std::string &key, token_kind_e kind,
                              const std::string &value)
{
  if (key == "v1")
  {
    line->start = level.SafeLookupVertex(LEX_Index(value), static_cast<size_t>(line - level.linedefs[0]));
  }

  if (key == "v2")
  {
    line->end = level.SafeLookupVertex(LEX_Index(value), static_cast<size_t>(line - level.linedefs[0]));
  }

  if (key == "special")
  {
    line->special = LEX_Int(value);
  }

  if (key == "twosided")
  {
    if (LEX_Boolean(value))
    {
      line->effects |= FX_TwoSided;
    };
  }

  if (key == "sidefront")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sidedefs.size())
    {
      line->right = nullptr;
    }
    else
    {
      line->right = level.sidedefs[num];
    }
  }

  if (key == "sideback")
  {
    size_t num = LEX_Index(value);

    if (num >= level.sidedefs.size())
    {
      line->left = nullptr;
    }
    else
    {
      line->left = level.sidedefs[num];
    }
  }
}

static void ParseUDMF_Block(level_t &level, lexer_c &lex, int cur_type)
{
  vertex_t *vertex = nullptr;
  thing_t *thing = nullptr;
  sector_t *sector = nullptr;
  sidedef_t *side = nullptr;
  linedef_t *line = nullptr;

  switch (cur_type)
  {
  case UDMF_VERTEX:
    vertex = NewVertex(level);
    break;
  case UDMF_THING:
    thing = NewThing(level);
    break;
  case UDMF_SECTOR:
    sector = NewSector(level);
    break;
  case UDMF_SIDEDEF:
    side = NewSidedef(level);
    break;
  case UDMF_LINEDEF:
    line = NewLinedef(level);
    break;
  default:
    break;
  }

  for (;;)
  {
    if (lex.Match("}"))
    {
      break;
    }

    std::string key;
    std::string value;

    token_kind_e tok = lex.Next(key);

    if (tok == TOK_EOF)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: unclosed block");
    }

    if (tok != TOK_Ident)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing key");
    }

    if (!lex.Match("="))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing '='");
    }

    tok = lex.Next(value);

    if (tok == TOK_EOF || tok == TOK_ERROR || value == "}")
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing value");
    }

    if (!lex.Match(";"))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing ';'");
    }

    switch (cur_type)
    {
    case UDMF_VERTEX:
      ParseVertexField(vertex, key, tok, value);
      break;
    case UDMF_THING:
      ParseThingField(thing, key, tok, value);
      break;
    case UDMF_SECTOR:
      ParseSectorField(sector, key, tok, value);
      break;
    case UDMF_SIDEDEF:
      ParseSidedefField(level, side, key, tok, value);
      break;
    case UDMF_LINEDEF:
      ParseLinedefField(level, line, key, tok, value);
      break;

    default: /* just skip it */
      break;
    }
  }

  // validate stuff

  if (line != nullptr)
  {
    if (line->start == nullptr || line->end == nullptr)
    {
      PrintLine(LOG_ERROR, "ERROR: Linedef #%zu is missing a vertex!", line->index);
    }

    ValidateLinedef(level, line);
  }
}

static void ParseUDMF_Pass(level_t &level, const std::string &data, int pass)
{
  // pass = 1 : vertices, sectors, things
  // pass = 2 : sidedefs
  // pass = 3 : linedefs

  lexer_c lex(data);

  for (;;)
  {
    std::string section;
    token_kind_e tok = lex.Next(section);

    if (tok == TOK_EOF)
    {
      return;
    }

    if (tok != TOK_Ident)
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump.");
      return;
    }

    // ignore top-level assignments
    if (lex.Match("="))
    {
      lex.Next(section);
      if (!lex.Match(";"))
      {
        PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing ';'");
      }
      continue;
    }

    if (!lex.Match("{"))
    {
      PrintLine(LOG_ERROR, "ERROR: Malformed TEXTMAP lump: missing '{'");
    }

    int cur_type = 0;

    if (section == "thing")
    {
      if (pass == 1)
      {
        cur_type = UDMF_THING;
      }
    }
    else if (section == "vertex")
    {
      if (pass == 1)
      {
        cur_type = UDMF_VERTEX;
      }
    }
    else if (section == "sector")
    {
      if (pass == 1)
      {
        cur_type = UDMF_SECTOR;
      }
    }
    else if (section == "sidedef")
    {
      if (pass == 2)
      {
        cur_type = UDMF_SIDEDEF;
      }
    }
    else if (section == "linedef")
    {
      if (pass == 3)
      {
        cur_type = UDMF_LINEDEF;
      }
    }

    // process the block
    ParseUDMF_Block(level, lex, cur_type);
  }
}

void ParseUDMF(level_t &level)
{
  Lump_c *lump = level.FindLevelLump("TEXTMAP");

  if (lump == nullptr || !lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "ERROR: Failure finding TEXTMAP lump.");
  }

  size_t remain = lump->Length();

  // load the lump into this string
  std::string data;

  while (remain > 0)
  {
    char buffer[4096];

    size_t want = std::min(remain, sizeof(buffer));

    if (!lump->Read(buffer, want))
    {
      PrintLine(LOG_ERROR, "ERROR: Failure reading TEXTMAP lump.");
    }

    data.append(buffer, want);

    remain -= want;
  }

  // now parse it...

  // the UDMF spec does not require objects to be in a dependency order.
  // for example: sidedefs may occur *after* the linedefs which refer to
  // them.  hence we perform multiple passes over the TEXTMAP data.
  ParseUDMF_Pass(level, data, 1);
  ParseUDMF_Pass(level, data, 2);
  ParseUDMF_Pass(level, data, 3);

  level.num_old_vert = level.vertices.size();
}

/* ----- writing routines ------------------------------ */

//
// Check Limits
//

static void CheckBinaryFormatLimits(level_t &level)
{
  if (level.num_old_vert > LIMIT_VERT)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many vertices.");
    level.overflows = true;
  }

  if (level.sectors.size() > LIMIT_SECTOR)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sectors.");
    level.overflows = true;
  }

  if (level.sidedefs.size() > LIMIT_SIDE)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sidedefs.");
    level.overflows = true;
  }

  if (level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many linedefs.");
    level.overflows = true;
  }
}

bsp_format_t CheckFormatBSP(buildinfo_t &ctx, level_t &level)
{
  bsp_format_t level_type = ctx.bsp_format;

  if (level.format == MapFormat_Doom64)
  {
    // Clamp Doom64 maps to only vanilla and DeePBSPV4 formats.
    // With support for fractional coordinates, it doesn't
    // make sense for it to need to support XNOD, etc.
    level_type = BSP_DoomBSP;
  }

  if (level_type == BSP_DoomBSP &&            // always allow for a valid map to be produced
      (level.vertices.size() > LIMIT_VERT     // even if it may not run on some older source ports
       || level.nodes.size() > LIMIT_NODE     // or the vanilla EXE
       || level.subsecs.size() > LIMIT_SUBSEC //
       || level.segs.size() > LIMIT_SEG))     //
  {
    PrintLine(LOG_NORMAL, "WARNING: BSP overflow. Forcing DeePBSPV4 node format.");
    config.total_warnings++;
    level_type = BSP_DeePBSPV4;
  }

  return level_type;
}

/* ----- whole-level routines --------------------------- */

void LoadLevel(level_t &level)
{
  auto mark = Benchmarker(__func__);
  Lump_c *LEV = cur_wad->GetLump(level.level_header_lump_index);

  level.overflows = false;

  PrintLine(LOG_NORMAL, "[%s] Reading %s", __func__, LEV->Name());

  level.num_new_vert = 0;
  level.num_real_lines = 0;

  switch (level.format)
  {
  case MapFormat_Doom:
    GetVertices_Doom(level);
    GetSectors_Doom(level);
    GetSidedefs_Doom(level);
    GetLinedefs_Doom(level);
    GetThings_Doom(level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat_Hexen:
    GetVertices_Doom(level);
    GetSectors_Doom(level);
    GetSidedefs_Doom(level);
    GetLinedefs_Hexen(level);
    GetThings_Hexen(level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat_Doom64:
    GetVertices_Doom64(level);
    GetSectors_Doom64(level);
    GetSidedefs_Doom64(level);
    GetLinedefs_Doom64(level);
    GetThings_Doom64(level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat_UDMF:
    ParseUDMF(level);
    break;
  case MapFormat_INVALID:
    PrintLine(LOG_ERROR, "[%s] Unknown level format on level %s", __func__, LEV->Name());
    break;
  }

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Loaded %zu vertices, %zu sectors, %zu sides, %zu lines, %zu things", level.vertices.size(),
              level.sectors.size(), level.sidedefs.size(), level.linedefs.size(), level.things.size());
  }

  DetectOverlappingVertices(level);
  DetectOverlappingLines(level);

  CalculateWallTips(level);

  // -JL- Find sectors containing polyobjs
  switch (level.format)
  {
  case MapFormat_Hexen:
  case MapFormat_UDMF:
    DetectPolyobjSectors(config, level);
    break;
  default:
    break;
  }
}

void FreeLevel(level_t &level)
{
  FreeVertices(level);
  FreeSidedefs(level);
  FreeLinedefs(level);
  FreeSectors(level);
  FreeThings(level);
  FreeSegs(level);
  FreeSubsecs(level);
  FreeNodes(level);
  FreeWallTips(level);
  FreeIntersections(level);
}

static void AddMissingLump(level_t &level, const char *name, const char *after)
{
  if (cur_wad->LevelLookupLump(level.level_num, name) != NO_INDEX)
  {
    return;
  }

  size_t exist = cur_wad->LevelLookupLump(level.level_num, after);

  // if this happens, the level structure is very broken
  if (exist == NO_INDEX)
  {
    PrintLine(LOG_NORMAL, "WARNING: Missing %s lump -- level structure is broken", after);
    config.total_warnings++;
    exist = cur_wad->LevelLastLump(level.level_num);
  }

  cur_wad->InsertPoint(exist + 1);
  cur_wad->AddLump(name)->Finish();
}

build_result_e SaveLevelBinaryFormat(level_t &level, node_t *root_node)
{
  // Note: root_node may be nullptr

  cur_wad->BeginWrite();

  // ensure all necessary level lumps are present
  AddMissingLump(level, "SEGS", "VERTEXES");
  AddMissingLump(level, "SSECTORS", "SEGS");
  AddMissingLump(level, "NODES", "SSECTORS");
  AddMissingLump(level, "SECTORS", "NODES");
  AddMissingLump(level, "REJECT", "SECTORS");
  AddMissingLump(level, "BLOCKMAP", "REJECT");

  // No need to add BEHAVIOR, LIGHTS or MACROS
  if (level.format == MapFormat_Doom64)
  {
    AddMissingLump(level, "LEAFS", "BLOCKMAP");
  }

  // check for overflows...
  CheckBinaryFormatLimits(level);

  bsp_format_t level_type = CheckFormatBSP(config, level);

  if (level.format == MapFormat_Doom64)
  {
    switch (level_type)
    {
    case BSP_DeePBSPV4:
      SaveDoom64_DeePBSPV4(level, root_node);
      break;
    case BSP_DoomBSP:
      SaveDoom64_DoomBSP(level, root_node);
      break;
    default:
      PrintLine(LOG_ERROR, "ERROR: Tried to write unsupported BSP format #%d on Doom64 map format", level_type);
      break;
    }
  }
  else // MapFormat_Doom or MapFormat_Hexen
  {
    switch (level_type)
    {
    case BSP_XGL3:
      SaveDoom_XGL3(level, root_node);
      break;
    case BSP_XGL2:
      SaveDoom_XGL2(level, root_node);
      break;
    case BSP_XGLN:
      SaveDoom_XGLN(level, root_node);
      break;
    case BSP_XNOD:
      SaveDoom_XNOD(level, root_node);
      break;
    case BSP_DeePBSPV4:
      SaveDoom_DeePBSPV4(level, root_node);
      break;
    case BSP_DoomBSP:
      SaveDoom_DoomBSP(level, root_node);
      break;
    }
  }

  PutBlockmap(level);
  PutReject(level);

  cur_wad->EndWrite();

  if (level.overflows)
  {
    // no message here
    // [ in verbose mode, each overflow already printed a message ]
    // [ in normal mode, we don't want any messages at all ]
    return BUILD_LumpOverflow;
  }

  return BUILD_OK;
}

build_result_e SaveLevelTextMap(level_t &level, node_t *root_node)
{
  cur_wad->BeginWrite();

  Lump_c *lump = CreateLevelLump(level, "ZNODES");
  AddMissingLump(level, "REJECT", "ZNODES");
  AddMissingLump(level, "BLOCKMAP", "REJECT");

  if (level.num_real_lines == 0)
  {
    lump->Finish();
  }
  else
  {
    SaveTextmap_ZNODES(level, root_node);
  }

  PutBlockmap(level);
  PutReject(level);

  cur_wad->EndWrite();

  return BUILD_OK;
}

/* ---------------------------------------------------------------- */

Lump_c *CreateLevelLump(level_t &level, const char *name, size_t max_size)
{
  // look for existing one
  Lump_c *lump = level.FindLevelLump(name);

  if (lump)
  {
    cur_wad->RecreateLump(lump, max_size);
  }
  else
  {
    size_t last_idx = cur_wad->LevelLastLump(level.level_num);

    // in UDMF maps, insert before the ENDMAP lump, otherwise insert
    // after the last known lump of the level.
    if (level.format != MapFormat_UDMF)
    {
      last_idx += 1;
    }

    cur_wad->InsertPoint(last_idx);

    lump = cur_wad->AddLump(name, max_size);
  }

  return lump;
}

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

void OpenWad(const char *filename)
{
  cur_wad = Wad_file::Open(filename, 'a');
  if (cur_wad == nullptr)
  {
    PrintLine(LOG_ERROR, "ERROR: Cannot open file: %s", filename);
  }

  if (cur_wad->IsReadOnly())
  {
    delete cur_wad;
    cur_wad = nullptr;
    PrintLine(LOG_ERROR, "ERROR: file is read only: %s", filename);
  }
}

void CloseWad(void)
{
  if (cur_wad != nullptr)
  {
    // this closes the file
    delete cur_wad;
    cur_wad = nullptr;
  }
}

size_t LevelsInWad(void)
{
  if (cur_wad == nullptr)
  {
    return 0;
  }

  return cur_wad->LevelCount();
}

size_t ComputeBspHeight(const node_t *node)
{
  if (node == nullptr)
  {
    return 0;
  }

  size_t right = ComputeBspHeight(node->r.node);
  size_t left = ComputeBspHeight(node->l.node);

  return std::max(left, right) + 1;
}

/* ----- build nodes for a single level ----- */

build_result_e BuildLevel(level_t &level, const char *filename)
{
  node_t *root_node = nullptr;
  subsec_t *root_sub = nullptr;

  LoadLevel(level);

  InitBlockmap(level);

  if (level.num_real_lines > 0)
  {
    if (config.analysis)
    {
      PrintLine(LOG_NORMAL, "[%s] Starting analysis loop for %s", __func__, level.GetLevelName());
      GenerateAnalysis(level, filename);
    }

    bbox_t dummy;
    // recursively create nodes
    seg_t *seg_list = CreateSegs(level);
    // recursive function T-T
    auto mark = Benchmarker("BuildNodes");
    BuildNodes(level, seg_list, 0, &dummy, &root_node, &root_sub, config.split_cost, config.fast, false);
  }

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Built %zu NODES, %zu SSECTORS, %zu SEGS, %zu VERTEXES", level.nodes.size(), level.subsecs.size(),
              level.segs.size(), level.num_old_vert + level.num_new_vert);
  }

  if (config.verbose && root_node != nullptr)
  {
    PrintLine(LOG_NORMAL, "Heights of subtrees: %zu / %zu", ComputeBspHeight(root_node->l.node),
              ComputeBspHeight(root_node->r.node));
  }

  ClockwiseBspTree(level);

  build_result_t ret = BUILD_OK;
  switch (level.format)
  {
  case MapFormat_Doom:
  case MapFormat_Hexen:
  case MapFormat_Doom64:
    ret = SaveLevelBinaryFormat(level, root_node);
    break;
  case MapFormat_UDMF:
    ret = SaveLevelTextMap(level, root_node);
    break;
  default:
    break;
  }

  FreeLevel(level);

  if (config.analysis)
  {
    WriteAnalysis(filename);
  }

  return ret;
}
