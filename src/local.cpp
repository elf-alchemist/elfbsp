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

Vertex *Level::SafeLookupVertex(size_t num, size_t num_line) const
{
  if (num >= vertices.size())
  {
    throw RuntimeException("Illegal map-vertex number {}, on line {}, maximum is {}", num, num_line, vertices.size());
  }
  return vertices[num];
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

  return sectors[num];
}

Sidedef *Level::SafeLookupSidedef(int32_t num) const
{
  // silently ignore illegal sidedef numbers
  auto num_as_index = static_cast<size_t>(num);

  if (num < 0 || num >= NO_INDEX_INT16 || num_as_index >= sidedefs.size())
  {
    return nullptr;
  }

  return sidedefs[num_as_index];
}

/* ----- allocation routines ---------------------------- */

Vertex *Level::NewVertex(void)
{
  Vertex *V = UtilCalloc<Vertex>(sizeof(Vertex));
  V->index = this->vertices.size();
  this->vertices.push_back(V);
  return V;
}

Linedef *Level::NewLinedef(void)
{
  Linedef *L = UtilCalloc<Linedef>(sizeof(Linedef));
  L->index = this->linedefs.size();
  this->linedefs.push_back(L);
  return L;
}

Sidedef *Level::NewSidedef(void)
{
  Sidedef *S = UtilCalloc<Sidedef>(sizeof(Sidedef));
  S->index = this->sidedefs.size();
  this->sidedefs.push_back(S);
  return S;
}

Sector *Level::NewSector(void)
{
  Sector *S = UtilCalloc<Sector>(sizeof(Sector));
  S->index = this->sectors.size();
  this->sectors.push_back(S);
  return S;
}

Mobj *Level::NewThing(void)
{
  Mobj *T = UtilCalloc<Mobj>(sizeof(Mobj));
  T->index = this->things.size();
  this->things.push_back(T);
  return T;
}

Segment *Level::NewSeg(void)
{
  Segment *S = UtilCalloc<Segment>(sizeof(Segment));
  this->segs.push_back(S);
  return S;
}

Subsector *Level::NewSubsec(void)
{
  Subsector *S = UtilCalloc<Subsector>(sizeof(Subsector));
  this->subsecs.push_back(S);
  return S;
}

Node *Level::NewNode(void)
{
  Node *N = UtilCalloc<Node>(sizeof(Node));
  this->nodes.push_back(N);
  return N;
}

WallTip *Level::NewWallTip(void)
{
  WallTip *WT = UtilCalloc<WallTip>(sizeof(WallTip));
  this->walltips.push_back(WT);
  return WT;
}

Intersection *Level::NewIntersection(void)
{
  Intersection *cut = new Intersection;
  this->intercuts.push_back(cut);
  return cut;
}

/* ----- free routines ---------------------------- */

void Level::FreeVertices(void)
{
  for (auto V : this->vertices)
  {
    UtilFree(V);
  }

  this->vertices.clear();
}

void Level::FreeLinedefs(void)
{
  for (auto L : this->linedefs)
  {
    UtilFree(L);
  }

  this->linedefs.clear();
}

void Level::FreeSidedefs(void)
{
  for (auto S : this->sidedefs)
  {
    UtilFree(S);
  }

  this->sidedefs.clear();
}

void Level::FreeSectors(void)
{
  for (auto S : this->sectors)
  {
    UtilFree(S);
  }

  this->sectors.clear();
}

void Level::FreeThings(void)
{
  for (auto T : this->things)
  {
    UtilFree(T);
  }

  this->things.clear();
}

void Level::FreeSegs(void)
{
  for (auto S : this->segs)
  {
    UtilFree(S);
  }

  this->segs.clear();
}

void Level::FreeSubsecs(void)
{
  for (auto S : this->subsecs)
  {
    UtilFree(S);
  }

  this->subsecs.clear();
}

void Level::FreeNodes(void)
{
  for (auto N : this->nodes)
  {
    UtilFree(N);
  }

  this->nodes.clear();
}

void Level::FreeWallTips(void)
{
  for (auto W : this->walltips)
  {
    UtilFree(W);
  }

  this->walltips.clear();
}

void Level::FreeIntersections(void)
{
  for (auto I : this->intercuts)
  {
    delete I;
  }

  this->intercuts.clear();
}
