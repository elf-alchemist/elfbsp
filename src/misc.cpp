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

//------------------------------------------------------------------------
// ANALYZE : Analyzing level structures
//------------------------------------------------------------------------

/* ----- analysis routines ----------------------------- */

bool Overlaps(const Vertex *vertex, const Vertex *other)
{
  double dx = fabs(other->x - vertex->x);
  double dy = fabs(other->y - vertex->y);
  return (dx < DIST_EPSILON) && (dy < DIST_EPSILON);
}

void DetectOverlappingVertices(Level &level)
{
  if (level.vertices.size() < 2)
  {
    return;
  }

  // copy the vertex pointers
  auto array = std::vector<Vertex *>(level.vertices);

  // sort the vertices by increasing X coordinate.
  // hence any overlapping vertices will be near each other.
  auto PredicateCompare_VertexCoordinateX = [](const Vertex *Vertex1, const Vertex *Vertex2) -> auto
  { return Vertex1->x < Vertex2->x; };

  std::sort(array.begin(), array.end(), PredicateCompare_VertexCoordinateX);

  // now mark them off
  for (size_t i = 0; i < level.vertices.size() - 1; i++)
  {
    Vertex *A = array[i];

    for (size_t k = i + 1; k < level.vertices.size(); k++)
    {
      Vertex *B = array[k];

      if (B->x > A->x + DIST_EPSILON)
      {
        break;
      }

      if (Overlaps(A, B))
      {
        // found an overlap !
        B->overlap = A->overlap ? A->overlap : A;

        if (HAS_BIT(config.debug, DEBUG_OVERLAPS))
        {
          PrintLine(Log::DEBUG, "[{}] Overlap: #{} + #{}", __func__, array[i]->index, array[i + 1]->index);
        }
      }
    }
  }

  // update the in-memory linedefs.
  // DOES NOT affect the on-disk linedefs.
  // this is mainly to help the miniseg creation code.
  for (auto L : level.linedefs)
  {
    while (L->start->overlap)
    {
      L->start = L->start->overlap;
    }

    while (L->end->overlap)
    {
      L->end = L->end->overlap;
    }
  }
}

void ClearNewVertices(Level &level)
{
  for (size_t i = level.vertices.size() - 1; i != NO_INDEX; i--)
  {
    Vertex *V = level.vertices[i];

    if (!V->is_new)
    {
      break;
    }

    UtilFree(V);

    level.vertices.pop_back();
  }

  level.num_old_vert = level.vertices.size();
}

void PruneVerticesAtEnd(Level &level)
{
  // always prune vertices at end of lump, otherwise all the
  // unused vertices from seg splits would keep accumulating.
  size_t old_num = level.vertices.size();

  // scan all vertices.
  // only remove from the end, so stop when hit a used one.
  for (size_t i = level.vertices.size() - 1; i != NO_INDEX; i--)
  {
    Vertex *V = level.vertices[i];

    if (V->is_used)
    {
      break;
    }

    UtilFree(V);

    level.vertices.pop_back();
  }

  size_t unused = old_num - level.vertices.size();

  if (unused > 0 && config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Pruned {} unused vertices at end", __func__, unused);
  }

  level.num_old_vert = level.vertices.size();
}

void DetectOverlappingLines(Level &level)
{
  // copy
  auto array = std::vector<Linedef *>(level.linedefs);

  // Algorithm:
  //   Sort all lines by minimum X coordinate.
  //   Overlapping lines will then be near each other in this set.
  //   NOTE: does not detect partially overlapping lines.
  auto PredicateCompare_LinedefMinX = [](const Linedef *Linedef1, const Linedef *Linedef2) -> auto
  { return Linedef1->MinX() < Linedef2->MinX(); };

  std::sort(array.begin(), array.end(), PredicateCompare_LinedefMinX);

  size_t count = 0;

  for (size_t i = 0; i < level.linedefs.size() - 1; i++)
  {
    Linedef *A = array[i];

    for (size_t k = i + 1; k < level.linedefs.size(); k++)
    {
      Linedef *B = array[k];

      if (B->MinX() > A->MinX() + DIST_EPSILON)
      {
        break;
      }

      // due to DetectOverlappingVertices(), we can compare the vertex
      // pointers
      bool over1 = (A->start == B->start) && (A->end == B->end);
      bool over2 = (A->start == B->end) && (A->end == B->start);

      if (over1 || over2)
      {
        // found an overlap !

        // keep the lowest numbered one
        if (A->index < B->index)
        {
          A->overlap = B->overlap ? B->overlap : B;
        }
        else
        {
          B->overlap = A->overlap ? A->overlap : A;
        }

        count++;
      }
    }
  }

  if (config.verbose && count > 0)
  {
    PrintLine(Log::NORMAL, "[{}] Detected {} overlapped linedefs", __func__, count);
  }
}

/* ----- vertex routines ------------------------------- */

void AddWallTip(Level &level, Vertex *vertex, double dx, double dy, bool open_left, bool open_right)
{
  SYS_ASSERT(vertex->overlap == nullptr);

  WallTip *tip = level.NewWallTip();
  WallTip *after;

  tip->angle = ComputeAngle(dx, dy);
  tip->open_left = open_left;
  tip->open_right = open_right;

  // find the correct place (order is increasing angle)
  for (after = vertex->tip_set; after && after->next; after = after->next)
  {
  }

  while (after && tip->angle + ANG_EPSILON < after->angle)
  {
    after = after->prev;
  }

  // link it in
  tip->next = after ? after->next : vertex->tip_set;
  tip->prev = after;

  if (after)
  {
    if (after->next)
    {
      after->next->prev = tip;
    }

    after->next = tip;
  }
  else
  {
    if (vertex->tip_set != nullptr)
    {
      vertex->tip_set->prev = tip;
    }

    vertex->tip_set = tip;
  }
}

void CalculateWallTips(Level &level)
{
  for (auto L : level.linedefs)
  {
    if (L->overlap || HAS_BIT(L->effects, FX_ZeroLength))
    {
      continue;
    }

    double x1 = L->start->x;
    double y1 = L->start->y;
    double x2 = L->end->x;
    double y2 = L->end->y;

    bool left = (L->left != nullptr) && (L->left->sector != nullptr);
    bool right = (L->right != nullptr) && (L->right->sector != nullptr);

    // note that start->overlap and end->overlap should be nullptr
    // due to logic in DetectOverlappingVertices.

    AddWallTip(level, L->start, x2 - x1, y2 - y1, left, right);
    AddWallTip(level, L->end, x1 - x2, y1 - y2, right, left);
  }

  if (HAS_BIT(config.debug, DEBUG_WALLTIPS))
  {
    for (auto V : level.vertices)
    {
      PrintLine(Log::DEBUG, "[{}] WallTips for vertex {}:", __func__, V->index);

      for (WallTip *tip = V->tip_set; tip; tip = tip->next)
      {
        PrintLine(Log::DEBUG, "[{}] Angle={:1.1f} left={} right={}", __func__, tip->angle, tip->open_left ? 1 : 0,
                  tip->open_right ? 1 : 0);
      }
    }
  }
}

Vertex *NewVertexFromSplitSeg(Level &level, Segment *seg, double x, double y)
{
  Vertex *vert = level.NewVertex();

  vert->x = x;
  vert->y = y;

  vert->is_new = true;
  vert->is_used = true;
  level.num_new_vert++;

  // compute wall-tip info
  if (seg->linedef == nullptr)
  {
    AddWallTip(level, vert, seg->pdx, seg->pdy, true, true);
    AddWallTip(level, vert, -seg->pdx, -seg->pdy, true, true);
  }
  else
  {
    const Sidedef *front = seg->side ? seg->linedef->left : seg->linedef->right;
    const Sidedef *back = seg->side ? seg->linedef->right : seg->linedef->left;

    bool left = (back != nullptr) && (back->sector != nullptr);
    bool right = (front != nullptr) && (front->sector != nullptr);

    AddWallTip(level, vert, seg->pdx, seg->pdy, left, right);
    AddWallTip(level, vert, -seg->pdx, -seg->pdy, right, left);
  }

  return vert;
}

Vertex *NewVertexDegenerate(Level &level, Vertex *start, Vertex *end)
{
  // this is only called when rounding off the BSP tree and
  // all the segs are degenerate (zero length), hence we need
  // to create at least one seg which won't be zero length.
  double dx = end->x - start->x;
  double dy = end->y - start->y;

  double dlen = hypot(dx, dy);

  Vertex *vert = level.NewVertex();

  vert->is_new = false;
  vert->is_used = true;

  vert->index = level.num_old_vert;
  level.num_old_vert++;

  // compute new coordinates
  vert->x = start->x;
  vert->y = start->x;

  if (dlen == 0)
  {
    throw RuntimeException("NewVertexDegenerate: bad delta!");
  }

  dx /= dlen;
  dy /= dlen;

  while (FloatToShort(vert->x) == FloatToShort(start->x) && FloatToShort(vert->y) == FloatToShort(start->y))
  {
    vert->x += dx;
    vert->y += dy;
  }

  return vert;
}

bool CheckOpen(const Vertex *vertex, double dx, double dy)
{
  const WallTip *tip;

  double angle = ComputeAngle(dx, dy);

  // first check whether there's a wall-tip that lies in the exact
  // direction of the given direction (which is relative to the
  // vertex).

  for (tip = vertex->tip_set; tip; tip = tip->next)
  {
    if (fabs(tip->angle - angle) < ANG_EPSILON || fabs(tip->angle - angle) > (360.0 - ANG_EPSILON))
    {
      // found one, hence closed
      return false;
    }
  }

  // OK, now just find the first wall-tip whose angle is greater than
  // the angle we're interested in.  Therefore we'll be on the RIGHT
  // side of that wall-tip.

  for (tip = vertex->tip_set; tip; tip = tip->next)
  {
    if (angle + ANG_EPSILON < tip->angle)
    {
      // found it
      return tip->open_right;
    }

    if (!tip->next)
    {
      // no more tips, thus we must be on the LEFT side of the tip
      // with the largest angle.

      return tip->open_left;
    }
  }

  // usually won't get here
  return true;
}
