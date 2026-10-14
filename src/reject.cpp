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
#include <vector>
#include <algorithm>
#include <cmath>

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
    // Must be valid two-sided line
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

// Hold processed line segment 2D geometry checks.
struct SightSegment
{
    float x1, y1, x2, y2;
};

// Precalculated axis-aligned bounding box (AABB) information per sector.
struct SectorBBox
{
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
};

// Classify which side of a linedef split plane a vertex falls on.
inline float SegmentSide(float ax, float ay, float bx, float by, float cx, float cy)
{
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

// 2D intersection check between two line segments.
// Guards against vertex collinear edge alignment failures.
inline bool TrueSegmentsIntersect(const SightSegment& s1, const SightSegment& s2)
{
  // Check if endpoints of segment 1 lie on opposite sides of segment 2
  float d1 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1);
  float d2 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2);
  // Check if endpoints of segment 2 lie on opposite sides of segment 1
  float d3 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1);
  float d4 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2);
  // Using 1e-3f instead of 0.0f. Tighter tolerances caused micro-gaps
  // where rays slipped through overlapping linedef vertices in E1M1.
  if (((d1 > 1e-3f && d2 < -1e-3f) || (d1 < -1e-3f && d2 > 1e-3f)) &&
  ((d3 > 1e-3f && d4 < -1e-3f) || (d3 < -1e-3f && d4 > 1e-3f)))
  {
    return true;
  }

  // Boundary intersection guard
  auto isPointOnSeg = [](float ax, float ay, float bx, float by, float cx, float cy) -> bool
  {
    if (cx < std::min(ax, bx) - 0.1f || cx > std::max(ax, bx) + 0.1f) return false;
    if (cy < std::min(ay, by) - 0.1f || cy > std::max(ay, by) + 0.1f) return false;

    // Collinear test
    return std::abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax)) < 0.1f;
  };


  // Check for boundary vertex snaps (prevents rays slipping through wall-to-wall joins damn it!)
  if (isPointOnSeg(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1)) return true;
  if (isPointOnSeg(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2)) return true;
  if (isPointOnSeg(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1)) return true;
  if (isPointOnSeg(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2)) return true;

  return false;
}

// Is ray blocked by any solid walls.
// Uses fast AABB to reject and minimize redudant test.
inline bool IsRayBlocked(const SightSegment& ray, const std::vector<const SightSegment*>& localBlockers, float maxRayLengthSq)
{
  const float dx = ray.x2 - ray.x1;
  const float dy = ray.y2 - ray.y1;
  const float currentDistSq = (dx * dx) + (dy * dy);

  // Instantly skip rays that exceed the dynamically calculated max sector span.
  // Using squared distances avoids slow calculations in our heaviest inner loop.
  if (currentDistSq > maxRayLengthSq) return true;

  const float rMinX = std::min(ray.x1, ray.x2), rMaxX = std::max(ray.x1, ray.x2);
  const float rMinY = std::min(ray.y1, ray.y2), rMaxY = std::max(ray.y1, ray.y2);

  for (const auto* blockerPtr : localBlockers)
  {
    const auto& blocker = *blockerPtr;
    // AABB Check, Skip instantly if the blocker's bounding box does not overlap the ray's span.
    if (std::max(blocker.x1, blocker.x2) < rMinX || std::min(blocker.x1, blocker.x2) > rMaxX ||
        std::max(blocker.y1, blocker.y2) < rMinY || std::min(blocker.y1, blocker.y2) > rMaxY)
    {
      continue;
    }
    // Ignore intersection tests if the sight ray is touching the exact corner
    // vertex of a blocker segment.
    if ((std::abs(ray.x1 - blocker.x1) < 0.1f && std::abs(ray.y1 - blocker.y1) < 0.1f) ||
        (std::abs(ray.x1 - blocker.x2) < 0.1f && std::abs(ray.y1 - blocker.y2) < 0.1f))
    {
      continue;
    }
    if (TrueSegmentsIntersect(ray, blocker)) return true;
  }
  return false;
}

// Tests between lines of Sector A and lines of Sector B.
// Returns true if a valid line of sight connects the two places.
static bool DoVisibilityTest(const std::vector<SightSegment>& linesA, const std::vector<SightSegment>& linesB, const std::vector<const SightSegment*>& localizedBlockers, float maxRayLengthSq)
{
  for (const auto& segA : linesA)
  {
    for (const auto& segB : linesB)
    {
      // Corner check, cast rays directly between absolute structural vertices.
      if (!IsRayBlocked({segA.x1, segA.y1, segB.x1, segB.y1}, localizedBlockers, maxRayLengthSq)) return true;
      if (!IsRayBlocked({segA.x1, segA.y1, segB.x2, segB.y2}, localizedBlockers, maxRayLengthSq)) return true;
      if (!IsRayBlocked({segA.x2, segA.y2, segB.x1, segB.y1}, localizedBlockers, maxRayLengthSq)) return true;
      if (!IsRayBlocked({segA.x2, segA.y2, segB.x2, segB.y2}, localizedBlockers, maxRayLengthSq)) return true;
      // Captures narrow visual paths (windows, stairs, holes, etc.) that corner checks miss.
      float midAx = (segA.x1 + segA.x2) * 0.5f; float midAy = (segA.y1 + segA.y2) * 0.5f;
      float midBx = (segB.x1 + segB.x2) * 0.5f; float midBy = (segB.y1 + segB.y2) * 0.5f;

      if (!IsRayBlocked({midAx, midAy, midBx, midBy}, localizedBlockers, maxRayLengthSq)) return true;
      if (!IsRayBlocked({midAx, midAy, segB.x1, segB.y1}, localizedBlockers, maxRayLengthSq)) return true;
      if (!IsRayBlocked({segA.x1, segA.y1, midBx, midBy}, localizedBlockers, maxRayLengthSq)) return true;
    }
  }
  return false;
}

// Main process now, handles everything!
static void Reject_ProcessSectors(Level &level)
{
  const size_t num_secs = level.sectors.size();
  if (num_secs == 0) return;

  std::vector<std::vector<SightSegment>> sectorLines(num_secs);
  std::vector<SectorBBox> sectorBoxes(num_secs);
  std::vector<SightSegment> solidBlockers;

  // Pre-allocate bit-matrix to track touching sectors instantly.
  size_t adj_matrix_size = (num_secs * num_secs + 7) / 8;
  std::vector<uint8_t> adjacencyMatrix(adj_matrix_size, 0);

  float mapMinX = 1e9f, mapMaxX = -1e9f;
  float mapMinY = 1e9f, mapMaxY = -1e9f;
  // Parse geometry and build basic sector tracking dimensions
  for (const auto* line : level.linedefs)
  {
    if (!line || !line->start || !line->end) continue;

    SightSegment seg = { static_cast<float>(line->start->x), static_cast<float>(line->start->y),
                         static_cast<float>(line->end->x),   static_cast<float>(line->end->y) };

    mapMinX = std::min({mapMinX, seg.x1, seg.x2}); mapMaxX = std::max({mapMaxX, seg.x1, seg.x2});
    mapMinY = std::min({mapMinY, seg.y1, seg.y2}); mapMaxY = std::max({mapMaxY, seg.y1, seg.y2});

    if (!line->left || !line->right)
    {
      solidBlockers.push_back(seg);
    }
    else
    {
      const auto* sLeft  = line->left->sector;
      const auto* sRight = line->right->sector;
      
      if (!sLeft || !sRight) 
      {
        solidBlockers.push_back(seg);
      }
      else if (sLeft != sRight)
      {
        // Accuracy Fix!
        // If a two-sided linedef sits between two different sectors, they share a physical edge.
        // We log this connection in our bitset matrix to bypass ray casting entirely later.
        size_t idx1 = sLeft->index * num_secs + sRight->index;
        size_t idx2 = sRight->index * num_secs + sLeft->index;
        adjacencyMatrix[idx1 >> 3] |= (1 << (idx1 & 7));
        adjacencyMatrix[idx2 >> 3] |= (1 << (idx2 & 7));
      }
    }

    // Map lines to their parent sectors and track bounding boxes
    for (size_t i = 0; i < num_secs; ++i)
    {
      const auto* sec = level.sectors[i];
      if ((line->right && line->right->sector == sec) || (line->left && line->left->sector == sec))
      {
        sectorLines[i].push_back(seg);
        auto& box = sectorBoxes[i];

        box.minX = std::min({box.minX, seg.x1, seg.x2}); box.maxX = std::max({box.maxX, seg.x1, seg.x2});
        box.minY = std::min({box.minY, seg.y1, seg.y2}); box.maxY = std::max({box.maxY, seg.y1, seg.y2});
      }
    }
  }

  // Calculate the max sector span across the map to scale ray bounds correctly.
  // This value caps our visibility ray checks on wide open fields without manual configuration.
  float maxSectorSpanSq = 4096.0f * 4096.0f; 

  for (size_t i = 0; i < num_secs; ++i)
  {
    const auto& box = sectorBoxes[i];
    if (box.maxX > box.minX && box.maxY > box.minY)
    {
      float sWidth = box.maxX - box.minX;
      float sHeight = box.maxY - box.minY;
      float currentSpanSq = ((sWidth * sWidth) + (sHeight * sHeight)) * 4.0f;
      if (currentSpanSq > maxSectorSpanSq) maxSectorSpanSq = currentSpanSq;
    }
  }

  // Build Spatial Hash Reference Map
  constexpr float CELL_SIZE = 512.0f;
  const size_t gridCols = static_cast<size_t>(std::ceil((mapMaxX - mapMinX) / CELL_SIZE)) + 1;
  const size_t gridRows = static_cast<size_t>(std::ceil((mapMaxY - mapMinY) / CELL_SIZE)) + 1;
  std::vector<std::vector<size_t>> grid(gridCols * gridRows);

  // Yet Another Accuracy Fix! Grid Edge Leaks :(
  // Using a 1.0 map-unit padding ensures that wall lines landing directly on grid border seams
  // are added to preventing visibility rays from leaking through gaps.
  constexpr float PADDING = 1.0f;

  for (size_t i = 0; i < solidBlockers.size(); ++i)
  {
    const auto& blocker = solidBlockers[i];
    float minX = std::min(blocker.x1, blocker.x2) - PADDING;
    float maxX = std::max(blocker.x1, blocker.x2) + PADDING;
    float minY = std::min(blocker.y1, blocker.y2) - PADDING;
    float maxY = std::max(blocker.y1, blocker.y2) + PADDING;

    const size_t bMinX = static_cast<size_t>(std::max(0.0f, std::min(static_cast<float>(gridCols - 1), std::floor((minX - mapMinX) / CELL_SIZE))));
    const size_t bMaxX = static_cast<size_t>(std::max(0.0f, std::min(static_cast<float>(gridCols - 1), std::floor((maxX - mapMinX) / CELL_SIZE))));
    const size_t bMinY = static_cast<size_t>(std::max(0.0f, std::min(static_cast<float>(gridRows - 1), std::floor((minY - mapMinY) / CELL_SIZE))));
    const size_t bMaxY = static_cast<size_t>(std::max(0.0f, std::min(static_cast<float>(gridRows - 1), std::floor((maxY - mapMinY) / CELL_SIZE))));

    for (size_t x = bMinX; x <= bMaxX; ++x)
    {
      for (size_t y = bMinY; y <= bMaxY; ++y)
      {
        grid[y * gridCols + x].push_back(i);
      }
    }
  }

  // Move loop structures to eliminate allocation thrashing
  std::vector<const SightSegment*> localizedBlockers;
  localizedBlockers.reserve(solidBlockers.size());
  // Replaces slow vector sorting operations with a high-speed bitset lookup tag.
  std::vector<int> blockerVisited(solidBlockers.size(), -1);
  int visitMarker = 0;
  // Symmetrical Evaluation Loops
  for (size_t view = 0; view < num_secs; view++)
  {
    if (sectorLines[view].empty()) continue;
    const auto& boxA = sectorBoxes[view];

    for (size_t target = 0; target < view; target++)
    {
      if (sectorLines[target].empty()) continue;
      // If the user enables the fast compile mode (-f or --fast), skip the heavy CPU stuff!
      if (config.fast)
      {
        const size_t p1 = view * num_secs + target, p2 = target * num_secs + view;
        level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
        level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
        continue;
      }
      // Void Pruning, skip processing if sectors belong to independent standalone sub-maps.
      if (level.sectors[view]->rej_group != level.sectors[target]->rej_group)
      {
        const size_t p1 = view * num_secs + target, p2 = target * num_secs + view;
        level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
        level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
        continue;
      }
      // Wow Yet Again Another Accuracy Fix! :(
      // If these sectors touch directly, they are unconditionally visible!
      // Skipping curved alternative geometric checks prevents tunnel/cave blindness bugs.
      size_t adjIdx = view * num_secs + target;
      if ((adjacencyMatrix[adjIdx >> 3] & (1 << (adjIdx & 7))) != 0) 
      {
        continue; 
      }

      const auto& boxB = sectorBoxes[target];
      
      // Clamp the box grid coordinates to the
      // specific segment connecting Sector A and Sector B. This prevents
      // copying giant unneeded canvas sectors across wide maps.
      const float cMinX = std::min({boxA.minX, boxA.maxX, boxB.minX, boxB.maxX});
      const float cMaxX = std::max({boxA.minX, boxA.maxX, boxB.minX, boxB.maxX});
      const float cMinY = std::min({boxA.minY, boxA.maxY, boxB.minY, boxB.maxY});
      const float cMaxY = std::max({boxA.minY, boxA.maxY, boxB.minY, boxB.maxY});

      const size_t gcMinX = static_cast<size_t>(std::max(0.0f, std::floor((cMinX - mapMinX) / CELL_SIZE)));
      const size_t gcMaxX = std::min(gridCols - 1, static_cast<size_t>(std::max(0.0f, std::floor((cMaxX - mapMinX) / CELL_SIZE))));
      const size_t gcMinY = static_cast<size_t>(std::max(0.0f, std::floor((cMinY - mapMinY) / CELL_SIZE)));
      const size_t gcMaxY = std::min(gridRows - 1, static_cast<size_t>(std::max(0.0f, std::floor((cMaxY - mapMinY) / CELL_SIZE))));

      localizedBlockers.clear();
      visitMarker++;

      for (size_t x = gcMinX; x <= gcMaxX; ++x)
      {
        for (size_t y = gcMinY; y <= gcMaxY; ++y)
        {
          for (size_t blockerIdx : grid[y * gridCols + x])
          {
            if (blockerVisited[blockerIdx] != visitMarker)
            {
              blockerVisited[blockerIdx] = visitMarker;
              localizedBlockers.push_back(&solidBlockers[blockerIdx]);
            }
          }
        }
      }
      // Execute magic raycasting and hope a cosmic ray doesn't hit the CPU from space!
      if (!DoVisibilityTest(sectorLines[view], sectorLines[target], localizedBlockers, maxSectorSpanSq))
      {
        const size_t p1 = view * num_secs + target, p2 = target * num_secs + view;
        level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
        level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
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
