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

#include "local.hpp"

void Level::SetLumps(WadIO &io, size_t label_index)
{
  this->lumps.label = label_index;
  this->map_format = io.LevelFormat(this->lumps.label);

  this->lumps.textmap = io.LevelLookupLump(this->lumps.label, "TEXTMAP");
  this->lumps.things = io.LevelLookupLump(this->lumps.label, "THINGS");
  this->lumps.linedefs = io.LevelLookupLump(this->lumps.label, "LINEDEFS");
  this->lumps.sidedefs = io.LevelLookupLump(this->lumps.label, "SIDEDEFS");
  this->lumps.vertexes = io.LevelLookupLump(this->lumps.label, "VERTEXES");
  this->lumps.sectors = io.LevelLookupLump(this->lumps.label, "SECTORS");
  this->lumps.lights = io.LevelLookupLump(this->lumps.label, "LIGHTS");
  this->lumps.znodes = io.LevelLookupLump(this->lumps.label, "ZNODES");
  this->lumps.nodes = io.LevelLookupLump(this->lumps.label, "NODES");
  this->lumps.ssectors = io.LevelLookupLump(this->lumps.label, "SSECTORS");
  this->lumps.segs = io.LevelLookupLump(this->lumps.label, "SEGS");
  this->lumps.leafs = io.LevelLookupLump(this->lumps.label, "LEAFS");
  this->lumps.reject = io.LevelLookupLump(this->lumps.label, "REJECT");
  this->lumps.blockmap = io.LevelLookupLump(this->lumps.label, "BLOCKMAP");
  this->lumps.behavior = io.LevelLookupLump(this->lumps.label, "BEHAVIOR");
  this->lumps.macros = io.LevelLookupLump(this->lumps.label, "MACROS");
  this->lumps.scripts = io.LevelLookupLump(this->lumps.label, "SCRIPTS");
}

Vertex *Level::SafeLookupVertex(size_t num, size_t num_line) const
{
  if (num >= vertices.size())
  {
    throw RuntimeException("Illegal map-vertex number {}, on line {}, maximum is {}", num, num_line, vertices.size());
  }
  return vertices[num].get();
}

Sector *Level::SafeLookupSector(size_t num, size_t num_side) const
{
  if (num >= NO_INDEX_INT16)
  {
    return nullptr;
  }

  if (num >= sectors.size())
  {
    throw RuntimeException("Illegal sector number {}, on side {}, maximum is {}", num, num_side, sectors.size() - 1);
  }

  return sectors[num].get();
}

Sidedef *Level::SafeLookupSidedef(int32_t num) const
{
  // silently ignore illegal sidedef numbers
  const auto num_as_index = static_cast<size_t>(num);

  if (num < 0 || num_as_index >= sidedefs.size())
  {
    return nullptr;
  }

  return sidedefs[num_as_index].get();
}

/* ----- allocation routines ---------------------------- */

Vertex *Level::NewVertex()
{
  auto *V = this->vertices.emplace_back().get();
  V->index = this->vertices.size() - 1;
  return V;
}

Linedef *Level::NewLinedef()
{
  auto *L = this->linedefs.emplace_back().get();
  L->index = this->linedefs.size() - 1;
  return L;
}

Sidedef *Level::NewSidedef()
{
  auto *S = this->sidedefs.emplace_back().get();
  S->index = this->sidedefs.size() - 1;
  return S;
}

Sector *Level::NewSector()
{
  auto *S = this->sectors.emplace_back().get();
  S->index = this->sectors.size() - 1;
  return S;
}

Mobj *Level::NewThing()
{
  auto *T = this->things.emplace_back().get();
  T->index = this->things.size() - 1;
  return T;
}

Segment *Level::NewSeg()
{
  return this->segs.emplace_back().get();
}

Subsector *Level::NewSubsec()
{
  return this->subsecs.emplace_back().get();
}

Node *Level::NewNode()
{
  return this->nodes.emplace_back().get();
}

WallTip *Level::NewWallTip()
{
  return this->walltips.emplace_back().get();
}

Intersection *Level::NewIntersection()
{
  return this->intercuts.emplace_back().get();
}

/* ----- free routines ---------------------------- */

void Level::FreeVertices()
{
  this->vertices.clear();
}

void Level::FreeLinedefs()
{
  this->linedefs.clear();
}

void Level::FreeSidedefs()
{
  this->sidedefs.clear();
}

void Level::FreeSectors()
{
  this->sectors.clear();
}

void Level::FreeThings()
{
  this->things.clear();
}

void Level::FreeSegs()
{
  this->segs.clear();
}

void Level::FreeSubsecs()
{
  this->subsecs.clear();
}

void Level::FreeNodes()
{
  this->nodes.clear();
}

void Level::FreeWallTips()
{
  this->walltips.clear();
}

void Level::FreeIntersections()
{
  this->intercuts.clear();
}
