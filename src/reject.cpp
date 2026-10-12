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

//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------

//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init(Level &level)
{
  level.reject_size = (level.sectors.size() * level.sectors.size() + 7) / 8;

  level.reject_matrix = new uint8_t[level.reject_size];
  memset(level.reject_matrix, 0, level.reject_size);

  for (auto sec : level.sectors)
  {
    sec->rej_group = sec->index;
    sec->rej_next = sec->rej_prev = sec;
  }
}

static void Reject_Free(Level &level)
{
  delete[] level.reject_matrix;
  level.reject_matrix = nullptr;
}

//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static void Reject_GroupSectors(Level &level)
{
  for (auto line : level.linedefs)
  {
    // must be valid two-sided line
    if (!line->right || !line->left)
    {
      continue;
    }

    Sector *sec1 = line->right->sector;
    Sector *sec2 = line->left->sector;
    Sector *tmp;

    if (!sec1                                         // invalid
        || !sec2                                      // invalid
        || sec1 == sec2                               // same
        || HAS_BIT(sec1->effects, FX_Sector_NoReject) // blind in sector
        || HAS_BIT(sec2->effects, FX_Sector_NoReject) // blind in sector
        || HAS_BIT(line->effects, FX_NoReject)        // blocked by line
        || sec1->rej_group == sec2->rej_group         // already in the same group
    )
    {
      continue;
    }

    // swap sectors so that the smallest group is added to the biggest
    // group.  This is based on the assumption that sector numbers in
    // wads will generally increase over the set of linedefs, and so
    // (by swapping) we'll tend to add small groups into larger groups,
    // thereby minimising the updates to 'rej_group' fields when merging.
    if (sec1->rej_group > sec2->rej_group)
    {
      tmp = sec1;
      sec1 = sec2;
      sec2 = tmp;
    }

    // update the group numbers in the second group
    sec2->rej_group = sec1->rej_group;

    for (tmp = sec2->rej_next; tmp != sec2; tmp = tmp->rej_next)
    {
      tmp->rej_group = sec1->rej_group;
    }

    // merge 'em baby...
    sec1->rej_next->rej_prev = sec2;
    sec2->rej_next->rej_prev = sec1;

    tmp = sec1->rej_next;
    sec1->rej_next = sec2->rej_next;
    sec2->rej_next = tmp;
  }
}

// KnightMB Modification
// Helper function to check line segment intersection
static bool LinesIntersect(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy)
{
  float den = (bx - ax) * (dy - cy) - (by - ay) * (cx - dx);
  if (den == 0.0f) return false; // Parallel lines

  float u = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / den;
  float v = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / den;

  return (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
}

// KnightMB Modification
// Basic geometry raycast to check if Sector A can see Sector B
static bool CheckSectorVisibility(Level &level, size_t sector_A_idx, size_t sector_B_idx)
{
  const Sector *secA = level.sectors[sector_A_idx];
  const Sector *secB = level.sectors[sector_B_idx];

  float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;

  int countA = 0, countB = 0;
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const Linedef *line = level.linedefs[i];
    if (line->right && line->right->sector == secA)
    {
      ax += static_cast<float>(line->start->x + line->end->x) / 2.0f;
      ay += static_cast<float>(line->start->y + line->end->y) / 2.0f;
      countA++;
    }
    else if (line->left && line->left->sector == secA)
    {
      ax += static_cast<float>(line->start->x + line->end->x) / 2.0f;
      ay += static_cast<float>(line->start->y + line->end->y) / 2.0f;
      countA++;
    }

    if (line->right && line->right->sector == secB)
    {
      bx += static_cast<float>(line->start->x + line->end->x) / 2.0f;
      by += static_cast<float>(line->start->y + line->end->y) / 2.0f;
      countB++;
    }
    else if (line->left && line->left->sector == secB)
    {
      bx += static_cast<float>(line->start->x + line->end->x) / 2.0f;
      by += static_cast<float>(line->start->y + line->end->y) / 2.0f;
      countB++;
    }
  }

  if (countA > 0)
  {
    ax /= static_cast<float>(countA);
    ay /= static_cast<float>(countA);
  }
  if (countB > 0)
  {
    bx /= static_cast<float>(countB);
    by /= static_cast<float>(countB);
  }

  // Create a bounding box for the ray segment itself
  float ray_xmin = (ax < bx) ? ax : bx;
  float ray_xmax = (ax > bx) ? ax : bx;
  float ray_ymin = (ay < by) ? ay : by;
  float ray_ymax = (ay > by) ? ay : by;

  // Check if this ray intersects any sight-blocking, 1-sided linedefs
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const Linedef *line = level.linedefs[i];

    // If it's a 1-sided wall, it completely blocks vision
    if (!line->left)
    {
      float cx = static_cast<float>(line->start->x);
      float cy = static_cast<float>(line->start->y);
      float dx = static_cast<float>(line->end->x);
      float dy = static_cast<float>(line->end->y);

      // Bounding Box Intersection Filter
      float line_xmin = (cx < dx) ? cx : dx;
      float line_xmax = (cx > dx) ? cx : dx;
      float line_ymin = (cy < dy) ? cy : dy;
      float line_ymax = (cy > dy) ? cy : dy;

      // If the line's bounding box doesn't even touch the ray's bounding box,
      // it is impossible for them to intersect. Skip the expensive math entirely!
      if (line_xmax < ray_xmin || line_xmin > ray_xmax || line_ymax < ray_ymin || line_ymin > ray_ymax)
      {
        continue;
      }

      if (LinesIntersect(ax, ay, bx, by, cx, cy, dx, dy))
      {
        return false; // Vision is BLOCKED
      }
    }
  }

  return true; // No solid wall crossed the ray path; assume visible
}

static void Reject_ProcessSectors(Level &level)
{
  size_t processed_count = 0; // Thread-safe tracking counter

  if (HAS_BIT(config.debug, DEBUG_REJECT))
  {
    PrintLine(Log::DEBUG, "Processing REJECT table for %zu sectors...", level.sectors.size());
  }

// Setup parallel process for multi-core systems
#pragma omp parallel for schedule(dynamic, 4)
  for (size_t view = 0; view < level.sectors.size(); view++)
  {
    // Store the incremented value atomically to safely print progress
    size_t current_progress;

#pragma omp atomic capture
    {
      processed_count++;
      current_progress = processed_count;
    }

    // Safely print progress without using a master directive restriction
    if (HAS_BIT(config.debug, DEBUG_REJECT) && (current_progress % 500 == 0 || current_progress == level.sectors.size()))
    {
      PrintLine(Log::NORMAL, "  Progress: {} / {} sectors handled...", current_progress, level.sectors.size());
    }

    for (size_t target = 0; target < view; target++)
    {
      Sector *view_sec = level.sectors[view];
      Sector *targ_sec = level.sectors[target];
      bool is_rejected = false;

      if (view_sec->rej_group != targ_sec->rej_group)
      {
        is_rejected = true;
      }
      else
      {
        if (!CheckSectorVisibility(level, view, target))
        {
          is_rejected = true;
        }
      }

      // for symmetry, do both sides at same time

      if (is_rejected)
      {
        size_t p1 = view * level.sectors.size() + target;
        size_t p2 = target * level.sectors.size() + view;

#pragma omp critical(matrix_write)
        {
          level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
          level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
        }
      }
    }
  }
}

// Note: this routine is destructive to the group numbers
static void Reject_DebugGroups(Level &level)
{
  if (HAS_NONE(config.debug, DEBUG_REJECT)) return;

  for (auto sec : level.sectors)
  {
    size_t group = sec->rej_group;
    size_t num = 0;

    if (group == NO_INDEX) continue;

    sec->rej_group = NO_INDEX;
    num++;

    for (Sector *tmp = sec->rej_next; tmp != sec; tmp = tmp->rej_next)
    {
      tmp->rej_group = NO_INDEX;
      num++;
    }

    PrintLine(Log::DEBUG, "[{}] Group {}  Sectors {}", __func__, group, num);
  }
}

//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject(WadIO &io, Level &level)
{
  auto mark = Benchmarker(__func__);
  io.StartWritingLump("REJECT");

  // just leave an empty reject lump
  if (level.sectors.size() == 0)
  {
    return;
  }

  Reject_Init(level);
  Reject_GroupSectors(level);
  Reject_ProcessSectors(level);
  Reject_DebugGroups(level);
  io.AddToLump(level.reject_matrix, level.reject_size);
  Reject_Free(level);

  if (config.verbose)
  {
    PrintLine(Log::NORMAL, "[{}] Reject size: {}", __func__, level.reject_size);
  }
}
