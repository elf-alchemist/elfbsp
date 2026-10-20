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
#include "info.hpp"
#include "local.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

bool CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax) return false;

      x1 = x1 + ((x2 - x1) * (ymax - y1) / (y2 - y1));
      y1 = ymax;

      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin) return false;

      x1 = x1 + ((x2 - x1) * (ymin - y1) / (y2 - y1));
      y1 = ymin;

      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax) return false;

      y1 = y1 + ((y2 - y1) * (xmax - x1) / (x2 - x1));
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin) return false;

      y1 = y1 + ((y2 - y1) * (xmin - x1) / (x2 - x1));
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

/* ----- reading routines ------------------------------ */

static void ValidateLinedef(Level &level, Linedef *line)
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
      PrintLine(Log::NORMAL, "[{}] Linedef #{} is self-referencing", __func__, line->index);
    }
  }

  if (config.effects                                   // Effects enabled ...
      && line->left                                    // ... side exists ...
      && line->left->tex_lower[0] != '-'               // ... lower texture isn't empty ...
      && StringCmp(line->left->tex_lower, "BSPNOSEG")) // ... lower texture is BSPNOSEG
  {
    line->effects |= FX_DoNotRenderBack;
  }

  if (config.effects                                    // Effects enabled ...
      && line->right                                    // ... side exists ...
      && line->right->tex_lower[0] != '-'               // ... lower texture isn't empty ...
      && StringCmp(line->right->tex_lower, "BSPNOSEG")) // ... lower texture is BSPNOSEG
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
    PrintLine(Log::WARN, "[{}] Linedef #{} is VERY long, it may cause problems", __func__, line->index);
    config.total_warnings++;
  }
}

static void GetVertices_Integral(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapVertexIntegral>(level.lump, "VERTEXES");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Vertex *vert = level.NewVertex();
    vert->x = ShortToFloat(GetLittleEndian(raw.x));
    vert->y = ShortToFloat(GetLittleEndian(raw.y));
  }

  level.num_old_vert = level.vertices.size();
}

static void GetVertices_Fractional(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapVertexFractional>(level.lump, "VERTEXES");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Vertex *vert = level.NewVertex();

    vert->x = static_cast<double>(GetLittleEndian(raw.x) / FRACFACTOR);
    vert->y = static_cast<double>(GetLittleEndian(raw.y) / FRACFACTOR);
  }

  level.num_old_vert = level.vertices.size();
}

static void GetSectors_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSectorDoom>(level.lump, "SECTORS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &_ : lump)
  {
    Sector *sector = level.NewSector();
    sector->effects = FX_Sector_None;
  }
}

static void GetThings_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingDoom>(level.lump, "THINGS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Mobj *thing = level.NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

static void GetSidedefs_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSidedefDoom>(level.lump, "SIDEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Sidedef *side = level.NewSidedef();

    side->offset_x = GetLittleEndian(raw.x_offset);
    side->offset_y = GetLittleEndian(raw.y_offset);
    side->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), side->index);

    memcpy(side->tex_upper, raw.upper_tex, 8);
    memcpy(side->tex_lower, raw.lower_tex, 8);
    memcpy(side->tex_middle, raw.mid_tex, 8);
    CStringToUpper(side->tex_upper);
    CStringToUpper(side->tex_lower);
    CStringToUpper(side->tex_middle);
  }
}

static void GetLinedefs_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefDoom>(level.lump, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Linedef *line = level.NewLinedef();

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), line->index);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), line->index);
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));
    line->special = GetLittleEndian(raw.special);
    line->id = GetLittleEndian(raw.tag);

    line->start->is_used = true;
    line->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_TWOSIDED))
    {
      line->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    // Line tags ( 900 <= x <=999 ) are considered "precious" and will, therefore, have a much higher seg split cost
    switch (line->id)
    {
    case 900 ... 996:
      line->effects |= FX_DoNotSplitSeg;
      break;
    case Tag_NoReject:
      line->effects |= FX_NoReject | FX_DoNotSplitSeg;
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

static void GetThings_Hexen(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingHexen>(level.lump, "THINGS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Mobj *thing = level.NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

static void GetLinedefs_Hexen(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefHexen>(level.lump, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Linedef *line = level.NewLinedef();

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), line->index);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), line->index);
    line->special = raw.special;
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
      if (raw.args[0]) line->effects |= FX_NoBlockmap;
      if (raw.args[1]) line->effects |= FX_DoNotSplitSeg;
      if (raw.args[2]) line->effects |= FX_DoNotRenderBack;
      if (raw.args[3]) line->effects |= FX_DoNotRenderFront;
      if (raw.args[4]) line->effects |= FX_NoReject;
      break;
    default:
      break;
    }
  }
}

static void GetSectors_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSectorDoom64>(level.lump, "SECTORS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Sector *sector = level.NewSector();

    if (!config.effects) continue;

    if (GetLittleEndian(raw.special) == SS_Doom64_NoReject)
    {
      sector->effects = FX_Sector_NoReject;
    }
  }
}

static void GetSidedefs_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSidedefDoom64>(level.lump, "SIDEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Sidedef *side = level.NewSidedef();

    side->offset_x = GetLittleEndian(raw.x_offset);
    side->offset_y = GetLittleEndian(raw.y_offset);
    // We don't care about texture indexes here
    side->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), side->index);
  }
}

static void GetLinedefs_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefDoom64>(level.lump, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Linedef *line = level.NewLinedef();

    line->start = level.SafeLookupVertex(GetLittleEndian(raw.start), line->index);
    line->end = level.SafeLookupVertex(GetLittleEndian(raw.end), line->index);
    line->special = GetLittleEndian(raw.special);
    line->id = GetLittleEndian(raw.tag);
    line->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    line->start->is_used = true;
    line->end->is_used = true;

    auto flags = GetLittleEndian(raw.flags);

    if (HAS_BIT(flags, MLF_DOOM64_TWOSIDED))
    {
      line->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, line);

    if (!config.effects) continue;

    if (HAS_BIT(flags, MLF_DOOM64_NO_BLOCKMAP))
    {
      line->effects |= FX_NoBlockmap;
    }
    if (HAS_ALL(flags, MLF_DOOM64_AUTOMAP_SHOW | MLF_DOOM64_AUTOMAP_HIDE))
    {
      line->effects |= FX_NoBlockmap;
    }
  }
}

static void GetThings_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingDoom64>(level.lump, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    Mobj *thing = level.NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

void GetVertices_UDMF(Level &level)
{
  level.num_old_vert = level.udmf_vertex.size();
  level.vertices.reserve(level.num_old_vert);

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_vertex.size());
  }

  for (auto &u_vertex : level.udmf_vertex)
  {
    auto v = level.NewVertex();
    v->x = u_vertex.x;
    v->y = u_vertex.y;
  }
}

void GetSectors_UDMF(Level &level)
{
  level.sectors.reserve(level.udmf_sector.size());

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_sector.size());
  }

  for (auto &_ : level.udmf_sector)
  {
    auto s = level.NewSector();
    s->effects = FX_Sector_None;
  }
}

void GetSidedefs_UDMF(Level &level)
{
  level.sidedefs.reserve(level.udmf_sidedef.size());

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_sidedef.size());
  }

  for (auto &u_sidedef : level.udmf_sidedef)
  {
    auto side = level.NewSidedef();

    side->offset_x = u_sidedef.offsetx;
    side->offset_y = u_sidedef.offsety;
    memcpy(side->tex_upper, u_sidedef.texturetop, 8);
    memcpy(side->tex_lower, u_sidedef.texturebottom, 8);
    memcpy(side->tex_middle, u_sidedef.texturemiddle, 8);
    side->sector = level.SafeLookupSector(u_sidedef.sector_id, side->index);

    CStringToUpper(side->tex_upper);
    CStringToUpper(side->tex_lower);
    CStringToUpper(side->tex_middle);
  }
}


void GetLinedefs_UDMF(Level &level)
{
  level.linedefs.reserve(level.udmf_linedef.size());

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_linedef.size());
  }

  for (auto &u_linedef : level.udmf_linedef)
  {
    auto l = level.NewLinedef();
    l->start = level.SafeLookupVertex(u_linedef.v1_id, l->index);
    l->end = level.SafeLookupVertex(u_linedef.v2_id, l->index);
    l->right = level.SafeLookupSidedef(u_linedef.sidefront);
    l->left = level.SafeLookupSidedef(u_linedef.sideback);
    l->special = u_linedef.special;
    l->id = u_linedef.id;

    l->start->is_used = true;
    l->end->is_used = true;

    if (HAS_BIT(u_linedef.flags, ULF_TwoSided))
    {
      l->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, l);
  }
}

void GetThings_UDMF(Level &level)
{
  level.things.reserve(level.udmf_thing.size());

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_thing.size());
  }

  for (auto &u_thing : level.udmf_thing)
  {
    auto t = level.NewThing();
    t->x = u_thing.x;
    t->y = u_thing.y;
    t->type = static_cast<DoomEdNum>(u_thing.type);
  }
}

/* ----- writing routines ------------------------------ */

//
// Check Limits
//

static void CheckBinaryFormatLimits(Level &level)
{
  if (level.num_old_vert > LIMIT_VERT)
  {
    PrintLine(Log::WARN, "[{}] Map overflow. Has too many vertices.", __func__);
    level.overflows = true;
  }

  if (level.sectors.size() > LIMIT_SECTOR)
  {
    PrintLine(Log::WARN, "[{}] Map overflow. Has too many sectors.", __func__);
    level.overflows = true;
  }

  if (level.sidedefs.size() > LIMIT_SIDE)
  {
    PrintLine(Log::WARN, "[{}] Map overflow. Has too many sidedefs.", __func__);
    level.overflows = true;
  }

  if (level.linedefs.size() > LIMIT_LINE)
  {
    PrintLine(Log::WARN, "[{}] Map overflow. Has too many linedefs.", __func__);
    level.overflows = true;
  }
}

static BspFormat CheckFormatBSP(BuildInfo &ctx, Level &level)
{
  BspFormat Levelype = ctx.bsp_format;

  if (level.map_format == MapFormat::Doom64)
  {
    // Clamp Doom64 maps to only vanilla and DeePBSPV4 formats.
    // With support for fractional coordinates, it doesn't
    // make sense for it to need to support XNOD, etc.
    Levelype = BspFormat::DoomBSP;
  }

  if (Levelype == BspFormat::DoomBSP &&       // always allow for a valid map to be produced
      (level.vertices.size() > LIMIT_VERT     // even if it may not run on some older source ports
       || level.nodes.size() > LIMIT_NODE     // or the vanilla EXE
       || level.subsecs.size() > LIMIT_SUBSEC //
       || level.segs.size() > LIMIT_SEG))     //
  {
    PrintLine(Log::WARN, "[{}] BSP overflow. Forcing DeePBSPV4 node format.", __func__);
    config.total_warnings++;
    Levelype = BspFormat::DeePBSPV4;
  }

  return level.bsp_format = Levelype;
}

/* ----- whole-level routines --------------------------- */

void LoadLevel(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);
  auto LEV = io.LumpName(level.lump);

  level.overflows = false;

  PrintLine(Log::NORMAL, "[{}] Reading {}", __func__, LEV);

  level.num_new_vert = 0;
  level.num_real_lines = 0;

  // Ordering is important
  switch (level.map_format)
  {
  case MapFormat::Doom:
    GetVertices_Integral(io, level);
    GetSectors_Doom(io, level);
    GetSidedefs_Doom(io, level);
    GetLinedefs_Doom(io, level);
    GetThings_Doom(io, level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat::Hexen:
    GetVertices_Integral(io, level);
    GetSectors_Doom(io, level);
    GetSidedefs_Doom(io, level);
    GetLinedefs_Hexen(io, level);
    GetThings_Hexen(io, level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat::Doom64:
    GetVertices_Fractional(io, level);
    GetSectors_Doom64(io, level);
    GetSidedefs_Doom64(io, level);
    GetLinedefs_Doom64(io, level);
    GetThings_Doom64(io, level);
    PruneVerticesAtEnd(level);
    break;
  case MapFormat::UDMF:
    ParseUDMF(io, level);
    GetVertices_UDMF(level);
    GetSectors_UDMF(level);
    GetSidedefs_UDMF(level);
    GetLinedefs_UDMF(level);
    GetThings_UDMF(level);
    break;
  case MapFormat::Invalid:
    throw RuntimeException("[{}] Unknown level format on level {}", __func__, LEV);
    break;
  }

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Loaded {} vertices, {} sectors, {} sides, {} lines, {} things", __func__,
              level.vertices.size(), level.sectors.size(), level.sidedefs.size(), level.linedefs.size(), level.things.size());
  }

  DetectOverlappingVertices(level);
  DetectOverlappingLines(level);

  CalculateWallTips(level);

  // -JL- Find sectors containing polyobjs
  switch (level.map_format)
  {
  case MapFormat::Hexen:
  case MapFormat::UDMF:
    DetectPolyobjSectors(config, level);
    break;
  default:
    break;
  }
}

//------------------------------------------------------------------------
// Writing stuff
//------------------------------------------------------------------------

BuildResult SaveLevelBinary(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);

  // Normal binary format limits
  CheckBinaryFormatLimits(level);

  // Use fractional VERTEXES lump in some map formats
  bool do_fractional = level.map_format == MapFormat::Doom64;

  // Generate LEAFS lump
  bool do_leafs = level.map_format == MapFormat::Doom64;

  // On BSP overflow, bump to DeePBSPV4 from DoomBSP
  BspFormat bsp_format = CheckFormatBSP(config, level);

  // Only include BSP-generated vertices in non-ZDBSP lump format
  bool do_new_vert = bsp_format == BspFormat::DoomBSP || bsp_format == BspFormat::DeePBSPV4;

  // Use Zlib-compressed version of ZDBSP lump formats
  bool zdbsp_compress = config.compress;

  // Copy marker wholesale
  size_t lump = level.lump;
  io.CopyLump(lump);

  // Correct order is paramount
  io.LevelCopyLump(lump, "THINGS");
  io.LevelCopyLump(lump, "LINEDEFS");
  io.LevelCopyLump(lump, "SIDEDEFS");

  // Do the four BSP lumps, before promptly doing the other normal lumps
  if (bsp_format == BspFormat::DoomBSP)
  {
    NormaliseBspTree(level);
    if (!do_fractional) RoundOffBspTree(level);
    SortSegs(level);

    PutVertices(io, level, do_fractional, do_new_vert);
    PutSegs_DoomBSP(io, level);
    PutSubsecs_DoomBSP(io, level);
    PutNodes_DoomBSP(io, level);
  }
  else if (bsp_format == BspFormat::DeePBSPV4)
  {
    NormaliseBspTree(level);
    if (!do_fractional) RoundOffBspTree(level);
    SortSegs(level);

    PutVertices(io, level, do_fractional, do_new_vert);
    PutSegs_DeePBSPV4(io, level);
    PutSubsecs_DeePBSPV4(io, level);
    PutNodes_DeePBSPV4(io, level);
  }
  else if (bsp_format == BspFormat::XNOD)
  {
    // ZDBSP formats do not write to VERTEXES
    PutVertices(io, level, do_fractional, do_new_vert);
    io.CreateEmptyLump("SEGS");
    io.CreateEmptyLump("SSECTORS");
    PutTree_ZDBSP(io, level, "NODES", bsp_format, zdbsp_compress);
  }
  else if (bsp_format >= BspFormat::XGLN && bsp_format <= BspFormat::XGL3)
  {
    // ZDBSP formats do not write to VERTEXES
    PutVertices(io, level, do_fractional, do_new_vert);
    io.CreateEmptyLump("SEGS");
    PutTree_ZDBSP(io, level, "SSECTORS", bsp_format, zdbsp_compress);
    io.CreateEmptyLump("NODES");
  }

  // Why is SECTORS right here? T_T
  io.LevelCopyLump(lump, "SECTORS");

  // Bruh
  PutReject(io, level);
  PutBlockmap(io, level);

  // The better? convex subsector option
  if (do_leafs && bsp_format == BspFormat::DoomBSP)
  {
    PutLeafs_DoomBSP(io, level);
  }
  else if (do_leafs && bsp_format == BspFormat::DeePBSPV4)
  {
    PutLeafs_DeePBSPV4(io, level);
  }

  // Level scripts and stuff
  if (level.map_format == MapFormat::Hexen)
  {
    io.LevelCopyLump(lump, "BEHAVIOR");
    io.LevelCopyLump(lump, "SCRIPTS");
  }
  else if (level.map_format == MapFormat::Doom64)
  {
    io.LevelCopyLump(lump, "LIGHTS");
    io.LevelCopyLump(lump, "MACROS");
    io.LevelCopyLump(lump, "SCRIPTS");
  }

  return level.overflows ? BuildResult::Overflow : BuildResult::Ok;
}

BuildResult SaveLevelTextMap(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);

  // Copy marker wholesale
  size_t lump = level.lump;
  io.CopyLump(lump);

  // duh.
  io.LevelCopyLump(lump, "TEXTMAP");

  // Always use ZGL3 for UDMF maps
  PutTree_ZDBSP(io, level, "ZNODES", BspFormat::XGL3, true);
  PutReject(io, level);
  PutBlockmap(io, level);

  // Copy everything else we don't touch
  io.LevelCopyLump(lump, "BEHAVIOR");
  io.LevelCopyLump(lump, "MACROS");
  io.LevelCopyLump(lump, "SCRIPTS");
  io.LevelCopyLump(lump, "DIALOGUE");
  io.LevelCopyLump(lump, "LIGHTMAP");

  // Finish
  io.CreateEmptyLump("ENDMAP");

  return BuildResult::Ok;
}

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

size_t ComputeBspHeight(const Node *node)
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

BuildResult BuildLevel(WadIO &io, Level &level, std::string_view filename)
{
  LoadLevel(io, level);

  InitBlockmap(level);

  if (level.num_real_lines > 0)
  {
    if (config.analysis)
    {
      PrintLine(Log::NORMAL, "[{}] Starting analysis loop for {}", __func__, io.LumpName(level.lump));
      GenerateAnalysis(io, level, filename);
    }

    MapBoundBox dummy;
    // recursively create nodes
    Segment *seg_list = CreateSegs(level);
    // recursive function T-T
    auto mark = Benchmarker("BuildNodes");
    BuildNodes(level, seg_list, 0, dummy, &level.root_node, &level.root_sub, config.split_cost, config.fast, false);
  }

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Built {} NODES, {} SSECTORS, {} SEGS, {} VERTEXES", __func__, level.nodes.size(),
              level.subsecs.size(), level.segs.size(), level.vertices.size());
  }

  if (config.verbose && level.root_node != nullptr)
  {
    PrintLine(Log::NORMAL, "[{}] Heights of subtrees: {} / {}", __func__, ComputeBspHeight(level.root_node->l.node),
              ComputeBspHeight(level.root_node->r.node));
  }

  ClockwiseBspTree(level);

  auto ret = BuildResult::Ok;
  switch (level.map_format)
  {
  case MapFormat::Doom:
  case MapFormat::Hexen:
  case MapFormat::Doom64:
    ret = SaveLevelBinary(io, level);
    break;
    break;
  case MapFormat::UDMF:
    ret = SaveLevelTextMap(io, level);
    break;
  default:
    break;
  }

  level.FreeVertices();
  level.FreeSidedefs();
  level.FreeLinedefs();
  level.FreeSectors();
  level.FreeThings();
  level.FreeSegs();
  level.FreeSubsecs();
  level.FreeNodes();
  level.FreeWallTips();
  level.FreeIntersections();

  if (config.analysis)
  {
    WriteAnalysis(filename);
  }

  return ret;
}
