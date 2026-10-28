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

    if (!line->special)
    {
      line->effects |= FX_NoBlockmap;
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
    line->effects |= FX_ZeroLength | FX_NoBlockmap;
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
  auto lump = io.ReadLevelLump<MapVertexIntegral>(level.lumps.label, "VERTEXES");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto v = level.NewVertex();
    v->x = ShortToFloat(GetLittleEndian(raw.x));
    v->y = ShortToFloat(GetLittleEndian(raw.y));
  }

  level.num_old_vert = level.vertices.size();
}

static void GetVertices_Fractional(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapVertexFractional>(level.lumps.label, "VERTEXES");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto v = level.NewVertex();

    v->x = static_cast<double>(GetLittleEndian(raw.x) / FRACFACTOR);
    v->y = static_cast<double>(GetLittleEndian(raw.y) / FRACFACTOR);
  }

  level.num_old_vert = level.vertices.size();
}

static void GetSectors_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSectorDoom>(level.lumps.label, "SECTORS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &_ : lump)
  {
    auto s = level.NewSector();
    s->effects = FX_Sector_None;
  }
}

static void GetThings_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingDoom>(level.lumps.label, "THINGS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto t = level.NewThing();

    t->x = GetLittleEndian(raw.x);
    t->y = GetLittleEndian(raw.y);
    t->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

static void GetSidedefs_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSidedefDoom>(level.lumps.label, "SIDEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto s = level.NewSidedef();

    s->offset_x = GetLittleEndian(raw.x_offset);
    s->offset_y = GetLittleEndian(raw.y_offset);
    s->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), s->index);

    memcpy(s->tex_upper, raw.upper_tex, 8);
    memcpy(s->tex_lower, raw.lower_tex, 8);
    memcpy(s->tex_middle, raw.mid_tex, 8);
    CStringToUpper(s->tex_upper);
    CStringToUpper(s->tex_lower);
    CStringToUpper(s->tex_middle);
  }
}

static void GetLinedefs_Doom(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefDoom>(level.lumps.label, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto l = level.NewLinedef();

    l->start = level.SafeLookupVertex(GetLittleEndian(raw.start), l->index);
    l->end = level.SafeLookupVertex(GetLittleEndian(raw.end), l->index);
    l->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    l->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));
    l->special = GetLittleEndian(raw.special);
    l->id = GetLittleEndian(raw.tag);

    l->start->is_used = true;
    l->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_TWOSIDED))
    {
      l->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, l);

    if (!config.effects) continue;

    // Line tags ( 900 <= x <=999 ) are considered "precious" and will, therefore, have a much higher seg split cost
    switch (l->id)
    {
    case 900 ... 996:
      l->effects |= FX_DoNotSplitSeg;
      break;
    case Tag_NoReject:
      l->effects |= FX_NoReject | FX_DoNotSplitSeg;
      break;
    case Tag_DoNotRender:
      l->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack | FX_DoNotSplitSeg;
      break;
    case Tag_NoBlockmap:
      l->effects |= FX_DoNotSplitSeg | FX_NoBlockmap;
      break;
    }

    switch (l->special)
    {
    case Special_RotateRelativeDegrees:
      l->angle = FX_RotateRelativeDegrees;
      break;
    case Special_RotateAbsoluteDegrees:
      l->angle = FX_RotateAbsoluteDegrees;
      break;
    case Special_RotateRelativeBAM:
      l->angle = FX_RotateRelativeBAM;
      break;
    case Special_RotateAbsoluteBAM:
      l->angle = FX_RotateAbsoluteBAM;
      break;
    case Special_DoNotRenderSegmentBack:
      l->effects |= FX_DoNotRenderBack;
      break;
    case Special_DoNotRenderSegmentFront:
      l->effects |= FX_DoNotRenderFront;
      break;
    case Special_DoNotRenderSegmentBoth:
      l->effects |= FX_DoNotRenderFront | FX_DoNotRenderBack;
      break;
    case Special_DoNotSplitSeg:
      l->effects |= FX_DoNotSplitSeg;
      break;
    default:
      break;
    }
  }
}

static void GetThings_Hexen(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingHexen>(level.lumps.label, "THINGS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto t = level.NewThing();

    t->x = GetLittleEndian(raw.x);
    t->y = GetLittleEndian(raw.y);
    t->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

static void GetLinedefs_Hexen(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefHexen>(level.lumps.label, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto l = level.NewLinedef();

    l->start = level.SafeLookupVertex(GetLittleEndian(raw.start), l->index);
    l->end = level.SafeLookupVertex(GetLittleEndian(raw.end), l->index);
    l->special = raw.special;
    l->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    l->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    l->start->is_used = true;
    l->end->is_used = true;

    if (HAS_BIT(GetLittleEndian(raw.flags), MLF_HEXEN_TWOSIDED))
    {
      l->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, l);

    if (!config.effects) continue;

    switch (l->special)
    {
    case BSP_SpecialEffects:
      if (raw.args[0]) l->effects |= FX_NoBlockmap;
      if (raw.args[1]) l->effects |= FX_DoNotSplitSeg;
      if (raw.args[2]) l->effects |= FX_DoNotRenderBack;
      if (raw.args[3]) l->effects |= FX_DoNotRenderFront;
      if (raw.args[4]) l->effects |= FX_NoReject;
      break;
    default:
      break;
    }
  }
}

static void GetSectors_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSectorDoom64>(level.lumps.label, "SECTORS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto s = level.NewSector();

    if (!config.effects) continue;

    if (GetLittleEndian(raw.special) == SS_Doom64_NoReject)
    {
      s->effects = FX_Sector_NoReject;
    }
  }
}

static void GetSidedefs_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapSidedefDoom64>(level.lumps.label, "SIDEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto s = level.NewSidedef();

    s->offset_x = GetLittleEndian(raw.x_offset);
    s->offset_y = GetLittleEndian(raw.y_offset);
    // We don't care about texture indexes here
    s->sector = level.SafeLookupSector(GetLittleEndian(raw.sector), s->index);
  }
}

static void GetLinedefs_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapLinedefDoom64>(level.lumps.label, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto l = level.NewLinedef();

    l->start = level.SafeLookupVertex(GetLittleEndian(raw.start), l->index);
    l->end = level.SafeLookupVertex(GetLittleEndian(raw.end), l->index);
    l->special = GetLittleEndian(raw.special);
    l->id = GetLittleEndian(raw.tag);
    l->right = level.SafeLookupSidedef(GetLittleEndian(raw.right));
    l->left = level.SafeLookupSidedef(GetLittleEndian(raw.left));

    l->start->is_used = true;
    l->end->is_used = true;

    auto flags = GetLittleEndian(raw.flags);

    if (HAS_BIT(flags, MLF_DOOM64_TWOSIDED))
    {
      l->effects |= FX_TwoSided;
    }

    ValidateLinedef(level, l);

    if (!config.effects) continue;

    if (HAS_BIT(flags, MLF_DOOM64_NO_BLOCKMAP))
    {
      l->effects |= FX_NoBlockmap;
    }
    if (HAS_ALL(flags, MLF_DOOM64_AUTOMAP_SHOW | MLF_DOOM64_AUTOMAP_HIDE))
    {
      l->effects |= FX_NoBlockmap;
    }
  }
}

static void GetThings_Doom64(WadIO &io, Level &level)
{
  auto lump = io.ReadLevelLump<MapThingDoom64>(level.lumps.label, "LINEDEFS");

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, lump.size());
  }

  for (auto &raw : lump)
  {
    auto t = level.NewThing();

    t->x = GetLittleEndian(raw.x);
    t->y = GetLittleEndian(raw.y);
    t->type = static_cast<DoomEdNum>(GetLittleEndian(raw.type));
  }
}

static void GetVertices_UDMF(Level &level)
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

static void GetSectors_UDMF(Level &level)
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

static void GetSidedefs_UDMF(Level &level)
{
  level.sidedefs.reserve(level.udmf_sidedef.size());

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(Log::DEBUG, "[{}] num = {}", __func__, level.udmf_sidedef.size());
  }

  for (auto &u_sidedef : level.udmf_sidedef)
  {
    auto s = level.NewSidedef();

    s->offset_x = u_sidedef.offsetx;
    s->offset_y = u_sidedef.offsety;
    memcpy(s->tex_upper, u_sidedef.texturetop, 8);
    memcpy(s->tex_lower, u_sidedef.texturebottom, 8);
    memcpy(s->tex_middle, u_sidedef.texturemiddle, 8);
    s->sector = level.SafeLookupSector(u_sidedef.sector_id, s->index);

    CStringToUpper(s->tex_upper);
    CStringToUpper(s->tex_lower);
    CStringToUpper(s->tex_middle);
  }
}

static void GetLinedefs_UDMF(Level &level)
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

static void GetThings_UDMF(Level &level)
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
  auto LEV = io.LumpName(level.lumps.label);

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

static void HandleBinaryMapBSP(WadIO &io, Level &level, BspFormat bsp_format)
{
  // Option to skip
  if (level.skip_bsp)
  {
    io.LevelCopyLump(level.lumps.label, "VERTEXES");
    io.LevelCopyLump(level.lumps.label, "SEGS");
    io.LevelCopyLump(level.lumps.label, "SSECTORS");
    io.LevelCopyLump(level.lumps.label, "NODES");
    return;
  }

  // Use fractional VERTEXES lump in some map formats
  bool do_fractional = level.map_format == MapFormat::Doom64;

  // Only include BSP-generated vertices in non-ZDBSP lump format
  bool do_new_vert = bsp_format == BspFormat::DoomBSP || bsp_format == BspFormat::DeePBSPV4;

  // Use Zlib-compressed version of ZDBSP lump formats
  bool zdbsp_compress = config.compress;

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
}

static void HandleBinaryMapLeafs(WadIO &io, Level &level, BspFormat bsp_format)
{
  // Part of Doom 64
  if (level.map_format != MapFormat::Doom64) return;

  // Option to skip
  if (level.skip_bsp)
  {
    io.LevelCopyLump(level.lumps.label, "LEAFS");
    return;
  }

  // Generate LEAFS lump
  if (bsp_format == BspFormat::DoomBSP)
  {
    PutLeafs_DoomBSP(io, level);
  }
  else if (bsp_format == BspFormat::DeePBSPV4)
  {
    PutLeafs_DeePBSPV4(io, level);
  }
}

static BuildResult SaveLevelBinary(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);

  // Normal binary format limits
  CheckBinaryFormatLimits(level);

  // On BSP overflow, bump to DeePBSPV4 from DoomBSP
  BspFormat bsp_format = CheckFormatBSP(config, level);

  // Copy marker wholesale
  size_t lump = level.lumps.label;
  io.CopyLump(lump);

  // Correct order is paramount
  io.LevelCopyLump(lump, "THINGS");
  io.LevelCopyLump(lump, "LINEDEFS");
  io.LevelCopyLump(lump, "SIDEDEFS");

  HandleBinaryMapBSP(io, level, bsp_format);

  // Why is SECTORS right here? T_T
  io.LevelCopyLump(lump, "SECTORS");

  // Bruh
  PutReject(io, level);
  PutBlockmap(io, level);

  HandleBinaryMapLeafs(io, level, bsp_format);

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

static BuildResult SaveLevelTextMap(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);

  // Copy marker wholesale
  size_t lump = level.lumps.label;
  io.CopyLump(lump);

  // duh.
  io.LevelCopyLump(lump, "TEXTMAP");

  if (!level.skip_bsp)
  {
    // Always use ZGL3 for UDMF maps
    PutTree_ZDBSP(io, level, "ZNODES", BspFormat::XGL3, true);
  }
  else
  {
    io.LevelCopyLump(lump, "ZNODES");
  }

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

static void SkipBuild(WadIO &io, Level &level)
{
  auto l = level.lumps;

  bool has_rej = l.reject != NO_INDEX;
  bool has_bmap = l.blockmap != NO_INDEX;
  bool has_bsp = (l.znodes != NO_INDEX) || (l.nodes != NO_INDEX && l.ssectors != NO_INDEX && l.segs != NO_INDEX);

  if (config.skip_bsp && has_bsp)
  {
    level.skip_bsp = true;
    PrintLine(Log::NORMAL, "[{}] Skip building BSP lump for {}.", __func__, io.LumpName(l.label));
  }

  if (config.skip_bmap && has_bmap)
  {
    level.skip_bmap = true;
    PrintLine(Log::NORMAL, "[{}] Skip building BLOCKMAP lump for {}.", __func__, io.LumpName(l.label));
  }

  if (config.skip_rej && has_rej)
  {
    level.skip_rej = true;
    PrintLine(Log::NORMAL, "[{}] Skip building REJECT lump for {}.", __func__, io.LumpName(l.label));
  }
}

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
  // Get default
  level.po = config.po;

  LoadLevel(io, level);

  SkipBuild(io, level);

  if (level.num_real_lines > 0 && !level.skip_bsp)
  {
    if (config.analysis)
    {
      PrintLine(Log::NORMAL, "[{}] Starting analysis loop for {}", __func__, io.LumpName(level.lumps.label));
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
              level.subsecs.size(), level.segs.size(), level.num_new_vert);
  }

  if (config.verbose && level.root_node != nullptr)
  {
    PrintLine(Log::NORMAL, "[{}] Heights of subtrees: {} / {}", __func__, ComputeBspHeight(level.root_node->l.node),
              ComputeBspHeight(level.root_node->r.node));
  }

  if (!level.skip_bsp)
  {
     ClockwiseBspTree(level);
  }

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
