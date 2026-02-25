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

bool Overlaps(const vertex_t *vertex, const vertex_t *other)
{
  double dx = fabs(other->x - vertex->x);
  double dy = fabs(other->y - vertex->y);
  return (dx < DIST_EPSILON) && (dy < DIST_EPSILON);
}

struct Compare_vertex_X_pred
{
  inline bool operator()(const vertex_t *A, const vertex_t *B) const
  {
    return A->x < B->x;
  }
};

void DetectOverlappingVertices(void)
{
  if (lev_vertices.size() < 2)
  {
    return;
  }

  // copy the vertex pointers
  std::vector<vertex_t *> array(lev_vertices);

  // sort the vertices by increasing X coordinate.
  // hence any overlapping vertices will be near each other.
  std::sort(array.begin(), array.end(), Compare_vertex_X_pred());

  // now mark them off
  for (size_t i = 0; i < lev_vertices.size() - 1; i++)
  {
    vertex_t *A = array[i];

    for (size_t k = i + 1; k < lev_vertices.size(); k++)
    {
      vertex_t *B = array[k];

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
          PrintLine(LOG_DEBUG, "[%s] Overlap: #%zu + #%zu", __func__, array[i]->index, array[i + 1]->index);
        }
      }
    }
  }

  // update the in-memory linedefs.
  // DOES NOT affect the on-disk linedefs.
  // this is mainly to help the miniseg creation code.
  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    linedef_t *L = lev_linedefs[i];

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

void PruneVerticesAtEnd(void)
{
  // always prune vertices at end of lump, otherwise all the
  // unused vertices from seg splits would keep accumulating.
  size_t old_num = lev_vertices.size();

  // scan all vertices.
  // only remove from the end, so stop when hit a used one.
  for (size_t i = lev_vertices.size() - 1; i != NO_INDEX; i--)
  {
    vertex_t *V = lev_vertices[i];

    if (V->is_used)
    {
      break;
    }

    UtilFree(V);

    lev_vertices.pop_back();
  }

  size_t unused = old_num - lev_vertices.size();

  if (unused > 0 && config.verbose)
  {
    PrintLine(LOG_NORMAL, "Pruned %zu unused vertices at end", unused);
  }

  num_old_vert = lev_vertices.size();
}

struct Compare_line_MinX_pred
{
  inline bool operator()(const linedef_t *A, const linedef_t *B) const
  {
    return A->MinX() < B->MinX();
  }
};

void DetectOverlappingLines(void)
{
  // Algorithm:
  //   Sort all lines by minimum X coordinate.
  //   Overlapping lines will then be near each other in this set.
  //   NOTE: does not detect partially overlapping lines.

  std::vector<linedef_t *> array(lev_linedefs);

  std::sort(array.begin(), array.end(), Compare_line_MinX_pred());

  size_t count = 0;

  for (size_t i = 0; i < lev_linedefs.size() - 1; i++)
  {
    linedef_t *A = array[i];

    for (size_t k = i + 1; k < lev_linedefs.size(); k++)
    {
      linedef_t *B = array[k];

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

  if (count > 0)
  {
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Detected %zu overlapped linedefs", count);
    }
  }
}

/* ----- vertex routines ------------------------------- */

void AddWallTip(vertex_t *vertex, double dx, double dy, bool open_left, bool open_right)
{
  SYS_ASSERT(vertex->overlap == nullptr);

  walltip_t *tip = NewWallTip();
  walltip_t *after;

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

void CalculateWallTips(void)
{
  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

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

    AddWallTip(L->start, x2 - x1, y2 - y1, left, right);
    AddWallTip(L->end, x1 - x2, y1 - y2, right, left);
  }

  if (HAS_BIT(config.debug, DEBUG_WALLTIPS))
  {
    for (size_t k = 0; k < lev_vertices.size(); k++)
    {
      vertex_t *V = lev_vertices[k];

      PrintLine(LOG_DEBUG, "[%s] WallTips for vertex %zu:", __func__, k);

      for (walltip_t *tip = V->tip_set; tip; tip = tip->next)
      {
        PrintLine(LOG_DEBUG, "Angle=%1.1f left=%d right=%d", tip->angle, tip->open_left ? 1 : 0, tip->open_right ? 1 : 0);
      }
    }
  }
}

vertex_t *NewVertexFromSplitSeg(seg_t *seg, double x, double y)
{
  vertex_t *vert = NewVertex();

  vert->x = x;
  vert->y = y;

  vert->is_new = true;
  vert->is_used = true;

  vert->index = num_new_vert;
  num_new_vert++;

  // compute wall-tip info
  if (seg->linedef == nullptr)
  {
    AddWallTip(vert, seg->pdx, seg->pdy, true, true);
    AddWallTip(vert, -seg->pdx, -seg->pdy, true, true);
  }
  else
  {
    const sidedef_t *front = seg->side ? seg->linedef->left : seg->linedef->right;
    const sidedef_t *back = seg->side ? seg->linedef->right : seg->linedef->left;

    bool left = (back != nullptr) && (back->sector != nullptr);
    bool right = (front != nullptr) && (front->sector != nullptr);

    AddWallTip(vert, seg->pdx, seg->pdy, left, right);
    AddWallTip(vert, -seg->pdx, -seg->pdy, right, left);
  }

  return vert;
}

vertex_t *NewVertexDegenerate(vertex_t *start, vertex_t *end)
{
  // this is only called when rounding off the BSP tree and
  // all the segs are degenerate (zero length), hence we need
  // to create at least one seg which won't be zero length.
  double dx = end->x - start->x;
  double dy = end->y - start->y;

  double dlen = hypot(dx, dy);

  vertex_t *vert = NewVertex();

  vert->is_new = false;
  vert->is_used = true;

  vert->index = num_old_vert;
  num_old_vert++;

  // compute new coordinates
  vert->x = start->x;
  vert->y = start->x;

  if (dlen == 0)
  {
    PrintLine(LOG_ERROR, "NewVertexDegenerate: bad delta!");
  }

  dx /= dlen;
  dy /= dlen;

  while (static_cast<int32_t>(floor(vert->x)) == static_cast<int32_t>(floor(start->x))
         && static_cast<int32_t>(floor(vert->y)) == static_cast<int32_t>(floor(start->y)))
  {
    vert->x += dx;
    vert->y += dy;
  }

  return vert;
}

bool CheckOpen(const vertex_t *vertex, double dx, double dy)
{
  const walltip_t *tip;

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
