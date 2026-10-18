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

// Native platform headers required for low-level process priority
#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/resource.h>
#endif

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
// Bounds are computed once so the loop never calls min/max.
struct SightSegment
{
  double x1;
  double y1;
  double x2;
  double y2;
  double minX;
  double maxX;
  double minY;
  double maxY;
  bool solid = false; // one-sided line; only used to prune rays

  SightSegment(double ax, double ay, double bx, double by)
      : x1(ax),
        y1(ay),
        x2(bx),
        y2(by),
        minX(std::min(ax, bx)),
        maxX(std::max(ax, bx)),
        minY(std::min(ay, by)),
        maxY(std::max(ay, by))
  {
  }
};

// Used by the SectorPoints to store isolated corner vertices
// and line midpoints, allowing the ray-caster to run without continuously
// running through large linedef arrays.
struct SightPoint
{
  double x;
  double y;
};

// Ray endpoints for one sector, built once and reused for every pair it's in.
struct SectorPoints
{
  std::vector<SightPoint> verts;      // Every unique linedef endpoint
  std::vector<SightPoint> firstVerts; // Unique first endpoints only
  std::vector<SightPoint> openMids;   // Midpoints of non-solid lines
  std::vector<SightPoint> allMids;    // Midpoints of every line
};

// Precalculated axis-aligned bounding box (AABB) information per sector.
struct SectorBBox
{
  double minX = std::numeric_limits<double>::max();
  double maxX = std::numeric_limits<double>::lowest();
  double minY = std::numeric_limits<double>::max();
  double maxY = std::numeric_limits<double>::lowest();
};

// Removes coordinate duplicates and sort points along the X-axis.
static void SortUnique(std::vector<SightPoint>& v)
{
  std::sort(v.begin(), v.end(), [](const SightPoint& a, const SightPoint& b)
  {
    return a.x < b.x || (a.x == b.x && a.y < b.y);
  });

  v.erase(std::unique(v.begin(), v.end(), [](const SightPoint& a, const SightPoint& b)
  {
    return a.x == b.x && a.y == b.y;
  }), v.end());
}

// This extracts and groups unique locations (corners, open door frames, etc.) exactly once.
// This will shift heavy allocation and duplication entirely out of the inner loops.
static SectorPoints BuildSectorPoints(const std::vector<SightSegment>& lines)
{
  SectorPoints pts;

  for (const auto& s : lines)
  {
    // Log absolute raw corner coordinates.
    pts.verts.push_back({s.x1, s.y1});
    pts.verts.push_back({s.x2, s.y2});
    pts.firstVerts.push_back({s.x1, s.y1});

    // Do structural midpoints once
    const SightPoint mid = {(s.x1 + s.x2) * 0.5, (s.y1 + s.y2) * 0.5};
    pts.allMids.push_back(mid);

    // Non-solid structural lines can be open portals or windows.
    // These are the most likely to have a clear sightline.
    if (!s.solid)
    {
      pts.openMids.push_back(mid);
    }
  }

  // Strip out layout redundancies.
  SortUnique(pts.verts);
  SortUnique(pts.firstVerts);
  SortUnique(pts.openMids);
  SortUnique(pts.allMids);

  // The ray loops only ask whether any ray is clear, so the order of the points never
  // changes the result. Ordering them by angle around the sector's center puts neighbouring
  // rays in similar directions, so the same wall usually stops several rays in a row.
  if (!pts.verts.empty())
  {
    double cx = 0.0;
    double cy = 0.0;
    for (const auto& v : pts.verts)
    {
      cx += v.x;
      cy += v.y;
    }
    cx /= static_cast<double>(pts.verts.size());
    cy /= static_cast<double>(pts.verts.size());

    auto byAngle = [cx, cy](const SightPoint& a, const SightPoint& b)
    {
      return std::atan2(a.y - cy, a.x - cx) < std::atan2(b.y - cy, b.x - cx);
    };

    std::sort(pts.verts.begin(), pts.verts.end(), byAngle);
    std::sort(pts.firstVerts.begin(), pts.firstVerts.end(), byAngle);
    std::sort(pts.openMids.begin(), pts.openMids.end(), byAngle);
    std::sort(pts.allMids.begin(), pts.allMids.end(), byAngle);
  }

  return pts;
}

// Uniform grid of solid blockers in CSR form (one flat array instead of a vector per cell).
struct BlockerGrid
{
  double originX = 0.0;
  double originY = 0.0;
  double cellSize = 512.0;
  ptrdiff_t cols = 0;
  ptrdiff_t rows = 0;
  std::vector<uint32_t> cellStart; // cols * rows + 1 offsets into 'items'
  std::vector<uint32_t> items;     // blocker indices, grouped by cell
};

// Per-thread space for the ray walk. A blocker that spans several cells
// would otherwise be tested once per cell; the stamp makes it once per ray.
struct RayWalkSpace
{
  // It's ring cache time!
  static constexpr int RING_SIZE = 8;

  std::vector<uint32_t> seen; // stamp of the last ray that tested each blocker
  uint32_t stamp = 0;

  // The blockers that most recently stopped a ray, newest first. Consecutive rays
  // tend to be stopped by the same few walls, so testing these before the grid walk
  // resolves most blocked rays without walking the grid at all.
  uint32_t ring[RING_SIZE] = {};
  int32_t ringCount = 0;

  explicit RayWalkSpace(size_t blockerCount)
      : seen(blockerCount, 0)
  {
  }

  // Start a new ray. Return to compare against 'seen'
  uint32_t Next()
  {
    if (++stamp == 0) // Wrapped around? Clear and restart.
    {
      std::fill(seen.begin(), seen.end(), 0);
      stamp = 1;
    }
    return stamp;
  }

  // Move the ring entry at 'i' to the front.
  void Promote(int32_t i)
  {
    const uint32_t id = ring[i];
    for (int32_t j = i; j > 0; --j)
    {
      ring[j] = ring[j - 1];
    }
    ring[0] = id;
  }

  // Put a new blocker at the front, dropping the oldest one when the ring is full.
  void Push(uint32_t id)
  {
    // The power of the Rings!!
    const int32_t n = (ringCount < RING_SIZE) ? ringCount + 1 : RING_SIZE;
    for (int32_t j = n - 1; j > 0; --j)
    {
      ring[j] = ring[j - 1];
    }
    ring[0] = id;
    ringCount = n;
  }
};

// Maps a coordinate to a grid cell index, clamp to [0, limit]
static inline size_t GetCellIndex(double v, double origin, size_t limit, double cellSize)
{
  const double c = std::floor((v - origin) / cellSize);
  return static_cast<size_t>(std::clamp(c, 0.0, static_cast<double>(limit)));
}

// Each blocker is registered in every cell its
// padded bounding box covers, same coverage as the old vector-of-vectors grid.
static BlockerGrid BuildBlockerGrid(const std::vector<SightSegment>& blockers,
                                    double originX, double originY,
                                    size_t cols, size_t rows,
                                    double cellSize, double padding)
{
  BlockerGrid g;
  g.originX = originX;
  g.originY = originY;
  g.cellSize = cellSize;
  g.cols = static_cast<ptrdiff_t>(cols);
  g.rows = static_cast<ptrdiff_t>(rows);
  g.cellStart.assign(cols * rows + 1, 0);

  // Calls (cellIndex) for every cell the blocker's padded bbox covers.
  auto forEachCell = [&](const SightSegment& s, auto&& fn)
  {
    const size_t x0 = GetCellIndex(s.minX - padding, originX, cols - 1, cellSize);
    const size_t x1 = GetCellIndex(s.maxX + padding, originX, cols - 1, cellSize);
    const size_t y0 = GetCellIndex(s.minY - padding, originY, rows - 1, cellSize);
    const size_t y1 = GetCellIndex(s.maxY + padding, originY, rows - 1, cellSize);

    for (size_t y = y0; y <= y1; ++y)
    {
      for (size_t x = x0; x <= x1; ++x)
      {
        fn(y * cols + x);
      }
    }
  };

  // Count entries per cell.
  for (const auto& b : blockers)
  {
    forEachCell(b, [&](size_t cell) { g.cellStart[cell + 1]++; });
  }

  // Prefix sum turns the counts into start offsets.
  for (size_t i = 0; i < cols * rows; ++i)
  {
    g.cellStart[i + 1] += g.cellStart[i];
  }

  // Fill the flat item array.
  g.items.resize(g.cellStart.back());
  std::vector<uint32_t> fillPos(g.cellStart.begin(), g.cellStart.end() - 1);

  for (size_t i = 0; i < blockers.size(); ++i)
  {
    forEachCell(blockers[i], [&](size_t cell)
    {
      g.items[fillPos[cell]++] = static_cast<uint32_t>(i);
    });
  }

  return g;
}

// Classify which side of a linedef split plane a vertex falls on.
inline double SegmentSide(double ax, double ay, double bx, double by, double cx, double cy)
{
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

// Is point (cx,cy) on segment (ax,ay)-(bx,by), within DIST_EPSILON?
static inline bool IsPointOnSegment(double ax, double ay, double bx, double by, double cx, double cy)
{
  // Broadphase bounding box check using DIST_EPSILON as the safety margin threshold
  if (cx < std::min(ax, bx) - DIST_EPSILON || cx > std::max(ax, bx) + DIST_EPSILON) return false;
  if (cy < std::min(ay, by) - DIST_EPSILON || cy > std::max(ay, by) + DIST_EPSILON) return false;

  // Collinear test checking if this falls within our distance epsilon
  return std::abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax)) < DIST_EPSILON;
}

// 2D intersection check between two line segments.
inline bool TrueSegmentsIntersect(const SightSegment& s1, const SightSegment& s2)
{
  double d1 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1);
  double d2 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2);
  double d3 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1);
  double d4 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2);

  if (((d1 > 1e-3 && d2 < -1e-3) || (d1 < -1e-3 && d2 > 1e-3)) &&
      ((d3 > 1e-3 && d4 < -1e-3) || (d3 < -1e-3 && d4 > 1e-3)))
  {
    return true;
  }

  // Each below can only succeed when its' cross product, which is
  // exactly d1 to d4 again, is smaller than DIST_EPSILON.
  // If none of them are, all four calls will fail and can be skipped.
  if (std::abs(d1) >= DIST_EPSILON && std::abs(d2) >= DIST_EPSILON &&
      std::abs(d3) >= DIST_EPSILON && std::abs(d4) >= DIST_EPSILON)
  {
    return false;
  }

  if (IsPointOnSegment(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1)) return true;
  if (IsPointOnSegment(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2)) return true;
  if (IsPointOnSegment(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1)) return true;
  if (IsPointOnSegment(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2)) return true;

  return false;
}

// Is ray blocked by any solid walls?
// Walks only the grid cells the ray passes through DDA,
// so each ray is tested against nearby blockers instead of a gathered list.
static bool IsRayBlocked(const SightSegment& ray,
                         const std::vector<SightSegment>& blockers,
                         const BlockerGrid& grid,
                         RayWalkSpace& scratch,
                         double maxRayLengthSq)
{
  const double dx = ray.x2 - ray.x1;
  const double dy = ray.y2 - ray.y1;

  // Instantly skip rays that exceed the dynamically calculated max sector span.
  // Using squared distances avoids slow calculations in our heaviest inner loop.
  if ((dx * dx) + (dy * dy) > maxRayLengthSq) return true;

  // The same checks apply to every candidate blocker, whether it comes from
  // the recent-blocker ring or from the grid walk below.
  auto blocks = [&ray](const SightSegment& blocker) -> bool
  {
    // AABB check, skip instantly if the blocker's bounding box does not overlap the ray's span.
    if (blocker.maxX < ray.minX || blocker.minX > ray.maxX ||
        blocker.maxY < ray.minY || blocker.minY > ray.maxY)
    {
      return false;
    }

    // Ignore intersection tests if the sight ray is touching the exact corner vertex.
    if ((std::abs(ray.x1 - blocker.x1) < DIST_EPSILON && std::abs(ray.y1 - blocker.y1) < DIST_EPSILON) ||
        (std::abs(ray.x1 - blocker.x2) < DIST_EPSILON && std::abs(ray.y1 - blocker.y2) < DIST_EPSILON))
    {
      return false;
    }

    return TrueSegmentsIntersect(ray, blocker);
  };

  // Try the walls that stopped the previous rays first. This is only a reordering of
  // the same tests, so a ray is blocked here only if the full walk would also block it.
  for (int32_t i = 0; i < scratch.ringCount; ++i)
  {
    if (blocks(blockers[scratch.ring[i]]))
    {
      scratch.Promote(i);
      return true;
    }
  }

  // Ray start position in grid cell units.
  const double inv = 1.0 / grid.cellSize;
  const double fx = (ray.x1 - grid.originX) * inv;
  const double fy = (ray.y1 - grid.originY) * inv;

  auto clampCell = [](double v, ptrdiff_t limit) -> ptrdiff_t
  {
    return static_cast<ptrdiff_t>(std::clamp(std::floor(v), 0.0, static_cast<double>(limit)));
  };

  // Cell holding the ray start, and cell holding the ray end.
  ptrdiff_t cx = clampCell(fx, grid.cols - 1);
  ptrdiff_t cy = clampCell(fy, grid.rows - 1);
  const ptrdiff_t endCx = clampCell((ray.x2 - grid.originX) * inv, grid.cols - 1);
  const ptrdiff_t endCy = clampCell((ray.y2 - grid.originY) * inv, grid.rows - 1);

  // Direction of travel on each axis (-1, 0 or +1).
  const ptrdiff_t stepX = (dx > 0) - (dx < 0);
  const ptrdiff_t stepY = (dy > 0) - (dy < 0);

  // Ray parameter at which the ray crosses the next cell border on each axis,
  // and how much t advances per whole cell. An axis with no movement never advances.
  constexpr double kInf = std::numeric_limits<double>::infinity();
  double tMaxX = kInf;
  double tDeltaX = kInf;
  double tMaxY = kInf;
  double tDeltaY = kInf;

  if (stepX != 0)
  {
    const double border = static_cast<double>(stepX > 0 ? cx + 1 : cx);
    tMaxX = (border - fx) * grid.cellSize / dx;
    tDeltaX = grid.cellSize / std::abs(dx);
  }
  if (stepY != 0)
  {
    const double border = static_cast<double>(stepY > 0 ? cy + 1 : cy);
    tMaxY = (border - fy) * grid.cellSize / dy;
    tDeltaY = grid.cellSize / std::abs(dy);
  }

  // Walk exactly this many steps. Counting steps will stop even if
  // rounding picks a neighbouring cell at a corner.
  ptrdiff_t remaining = std::abs(endCx - cx) + std::abs(endCy - cy);
  const uint32_t stamp = scratch.Next();

  for (;;)
  {
    const size_t cell = static_cast<size_t>(cy * grid.cols + cx);

    for (uint32_t k = grid.cellStart[cell]; k < grid.cellStart[cell + 1]; ++k)
    {
      // A blocker spanning several cells is only tested once per ray.
      const uint32_t idx = grid.items[k];
      if (scratch.seen[idx] == stamp) continue;
      scratch.seen[idx] = stamp;

      if (blocks(blockers[idx]))
      {
        // Remember the wall that stopped this ray, the next ray will likely hit it too.
        scratch.Push(idx);
        return true;
      }
    }

    // Reached the end cell and nothing blocked the ray.
    if (remaining-- == 0) break;

    // Step into the neighbouring cell whose border the ray reaches first.
    // Once a row or column matches the end cell, only the other axis can move.
    bool stepInX;
    if (cx == endCx)      stepInX = false;
    else if (cy == endCy) stepInX = true;
    else                  stepInX = tMaxX < tMaxY;

    if (stepInX)
    {
      cx += stepX;
      tMaxX += tDeltaX;
    }
    else
    {
      cy += stepY;
      tMaxY += tDeltaY;
    }
  }

  return false;
}

// Tests between the points of Sector A and the points of Sector B.
// Returns true if a valid line of sight connects the two places.
static bool DoVisibilityTest(const SectorPoints& A,
                             const SectorPoints& B,
                             const std::vector<SightSegment>& blockers,
                             const BlockerGrid& grid,
                             RayWalkSpace& scratch,
                             double maxRayLengthSq)
{
  auto clear = [&](const SightPoint& from, const SightPoint& to) -> bool
  {
    return !IsRayBlocked({from.x, from.y, to.x, to.y}, blockers, grid, scratch, maxRayLengthSq);
  };

  // Portal midpoint to portal midpoint first is most likely to succeed,
  // so visible pairs usually exit early.
  for (const auto& a : A.openMids)
  {
    for (const auto& b : B.openMids)
    {
      if (clear(a, b)) return true;
    }
  }

  // Portal midpoint to the first endpoint of each line in B.
  for (const auto& a : A.openMids)
  {
    for (const auto& b : B.firstVerts)
    {
      if (clear(a, b)) return true;
    }
  }

  // First endpoint of each line in A to any midpoint in B. Solid midpoints stay
  // in this list on purpose because a ray from a shared corner along a solid wall is
  // not blocked by that wall because IsRayBlocked skips blockers touching the ray start.
  for (const auto& a : A.firstVerts)
  {
    for (const auto& b : B.allMids)
    {
      if (clear(a, b)) return true;
    }
  }

  // Corner to corner.
  for (const auto& a : A.verts)
  {
    for (const auto& b : B.verts)
    {
      if (clear(a, b)) return true;
    }
  }

  return false;
}

// Mark a sector pair as blind in both directions.
static inline void SetPairBits(Level &level, size_t view, size_t target, size_t num_secs)
{
  const size_t p1 = view * num_secs + target;
  const size_t p2 = target * num_secs + view;
  level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
  level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
}

// Mark a sector pair as blind in both directions using thread-safe atomic guards.
static inline void SetPairBitsAtomic(Level &level, size_t view, size_t target, size_t num_secs)
{
  const size_t p1 = view * num_secs + target;
  const size_t p2 = target * num_secs + view;
  const uint8_t bit1 = static_cast<uint8_t>(1u << (p1 & 7));
  const uint8_t bit2 = static_cast<uint8_t>(1u << (p2 & 7));
  #ifdef _OPENMP
    #pragma omp atomic
  #endif
  level.reject_matrix[p1 >> 3] |= bit1;
  #ifdef _OPENMP
    #pragma omp atomic
  #endif
  level.reject_matrix[p2 >> 3] |= bit2;
}

// Restore process priority back to default.
static inline void RestorePriority()
{
  #if defined(_WIN32)
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
  #else
    setpriority(PRIO_PROCESS, 0, 0);
  #endif
}

// Main process now, handles everything!
static void Reject_ProcessSectors(Level &level)
{
  const size_t num_secs = level.sectors.size();
  if (num_secs == 0) return;

  // Snapshot of the group numbers. Read-only from here on, so the parallel
  // loop can compare groups without touching the shared reject matrix.
  std::vector<size_t> groupOf(num_secs);
  for (size_t i = 0; i < num_secs; ++i)
  {
    groupOf[i] = level.sectors[i]->rej_group;
  }

  // Fills the reject matrix for all mismatched void island groups in bulk.
  // Disconnected islands completely enveloped by void space share no visual vectors, duh!
  for (size_t view = 0; view < num_secs; view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      if (groupOf[view] != groupOf[target])
      {
        SetPairBits(level, view, target, num_secs);
      }
    }
  }

  // If the user enables the fast compile mode (-f or --fast), skip the heavy CPU stuff!
  if (config.fast)
  {
    return; // Exit out early!
  }

  // Lowers the operating system priority for this process.
  // This prevents the PC from becoming choppy or unresponsive during long compile times.
  #if defined(_WIN32)
    // On Windows, set priority to below normal
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
  #else
    // On Unix/Linux/macOS systems, set priority nice value to 10
    setpriority(PRIO_PROCESS, 0, 10);
  #endif

  // Declare empty first
  std::vector<std::vector<SightSegment>> sectorLines;
  std::vector<SectorBBox> sectorBoxes;
  std::vector<SightSegment> solidBlockers;

  // Allocate the size right after
  sectorLines.resize(num_secs);
  sectorBoxes.resize(num_secs);

  // Pre-allocate bit-matrix to track touching sectors instantly.
  size_t adj_matrix_size = (num_secs * num_secs + 7) / 8;
  std::vector<uint8_t> adjacencyMatrix;
  adjacencyMatrix.assign(adj_matrix_size, 0);

  // Tracking limits for the entire map canvas initialized using maximum bounds
  double mapMinX = std::numeric_limits<double>::max();
  double mapMaxX = std::numeric_limits<double>::lowest();
  double mapMinY = std::numeric_limits<double>::max();
  double mapMaxY = std::numeric_limits<double>::lowest();

  // Parse geometry and build basic sector tracking dimensions
  for (const auto* line : level.linedefs)
  {
    if (!line || !line->start || !line->end) continue;

    SightSegment seg(
        static_cast<double>(line->start->x),
        static_cast<double>(line->start->y),
        static_cast<double>(line->end->x),
        static_cast<double>(line->end->y)
    );

    mapMinX = std::min({mapMinX, seg.x1, seg.x2});
    mapMaxX = std::max({mapMaxX, seg.x1, seg.x2});
    mapMinY = std::min({mapMinY, seg.y1, seg.y2});
    mapMaxY = std::max({mapMaxY, seg.y1, seg.y2});

    const Sector* sRight = line->right ? line->right->sector : nullptr;
    const Sector* sLeft  = line->left  ? line->left->sector  : nullptr;
    const bool isSolid   = !sRight || !sLeft;

    // The flag is only used to prune rays that are always blocked.
    // Require length > 1 so a degenerate line never gets pruned.
    const double ldx = seg.x2 - seg.x1;
    const double ldy = seg.y2 - seg.y1;
    seg.solid = isSolid && (ldx * ldx + ldy * ldy) > 1.0;

    if (isSolid)
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

    // Map the line straight to its parent sectors and track their bounding boxes.
    if (sRight)
    {
      const size_t rIdx = sRight->index;
      auto& box = sectorBoxes[rIdx];
      sectorLines[rIdx].push_back(seg);
      box.minX = std::min({box.minX, seg.x1, seg.x2});
      box.maxX = std::max({box.maxX, seg.x1, seg.x2});
      box.minY = std::min({box.minY, seg.y1, seg.y2});
      box.maxY = std::max({box.maxY, seg.y1, seg.y2});
    }
    if (sLeft && sLeft != sRight)
    {
      const size_t lIdx = sLeft->index;
      auto& box = sectorBoxes[lIdx];
      sectorLines[lIdx].push_back(seg);
      box.minX = std::min({box.minX, seg.x1, seg.x2});
      box.maxX = std::max({box.maxX, seg.x1, seg.x2});
      box.minY = std::min({box.minY, seg.y1, seg.y2});
      box.maxY = std::max({box.maxY, seg.y1, seg.y2});
    }
  }

  // No usable geometry at all?
  if (mapMinX > mapMaxX || mapMinY > mapMaxY)
  {
    RestorePriority();
    return;
  }

  // Build the per-sector ray endpoint lists once, reused for every pair.
  std::vector<SectorPoints> sectorPoints(num_secs);
  for (size_t i = 0; i < num_secs; ++i)
  {
    sectorPoints[i] = BuildSectorPoints(sectorLines[i]);
  }

  // Calculate the max sector span across the map to scale ray bounds correctly.
  // This value caps our visibility ray checks on wide open fields without manual configuration.
  double maxSectorSpanSq = 4096.0 * 4096.0;

  for (size_t i = 0; i < num_secs; ++i)
  {
    const auto& box = sectorBoxes[i];
    if (box.maxX > box.minX && box.maxY > box.minY)
    {
      double sWidth = box.maxX - box.minX;
      double sHeight = box.maxY - box.minY;
      double currentSpanSq = ((sWidth * sWidth) + (sHeight * sHeight)) * 4.0;
      if (currentSpanSq > maxSectorSpanSq) maxSectorSpanSq = currentSpanSq;
    }
  }

  // Build the spatial grid as a flat CSR array.
  // Dense maps pack many walls into a few cells, so small cells keep each ray's candidate list short.
  constexpr double CELL_SIZE = 128.0;

  // Yet Another Accuracy Fix! Grid Edge Leaks :(
  // Using a 1.0 map-unit padding ensures that wall lines landing directly on grid border seams
  // are added to preventing visibility rays from leaking through gaps.
  constexpr double PADDING = 1.0;

  const size_t gridCols = static_cast<size_t>(std::ceil((mapMaxX - mapMinX) / CELL_SIZE)) + 1;
  const size_t gridRows = static_cast<size_t>(std::ceil((mapMaxY - mapMinY) / CELL_SIZE)) + 1;
  const BlockerGrid grid = BuildBlockerGrid(solidBlockers, mapMinX, mapMinY,
                                            gridCols, gridRows, CELL_SIZE, PADDING);

  // Maps only the sectors that physically possess line geometry.
  // This helps to stop wasting allocation overhead on dead void space control sectors.
  std::vector<size_t> activeSectors;
  activeSectors.reserve(num_secs);
  for (size_t i = 0; i < num_secs; ++i)
  {
    if (!sectorLines[i].empty())
    {
      activeSectors.push_back(i);
    }
  }

  // Signed loop counter
  // OpenMP 2.0 compilers (MSVC) require one.
  const ptrdiff_t activeCount = static_cast<ptrdiff_t>(activeSectors.size());

  // Parallelizes the loop across available CPU cores.
  // Dynamic scheduling lets each idle core pick up the next sector immediately,
  // because some sectors take far longer than others.
  #ifdef _OPENMP
    #pragma omp parallel
  #endif
  {
    // Just one thread-private
    RayWalkSpace scratch(solidBlockers.size());

    #ifdef _OPENMP
      #pragma omp for schedule(dynamic)
    #endif
    for (ptrdiff_t i = 0; i < activeCount; i++)
    {
      // Walk from the last active sector down.
      // Row 'a' tests 'a' targets, so the biggest rows go first
      // and no long row is left as a straggler.
      const size_t a = static_cast<size_t>(activeCount - 1 - i);

      // Map our active iterator index back to the real sector index
      const size_t view = activeSectors[a];
      const auto& boxA = sectorBoxes[view];

      for (size_t b = 0; b < a; b++)
      {
        // Map our active target iterator index back to the real sector index
        const size_t target = activeSectors[b];

        // Sectors in different groups were already marked blind in bulk.
        if (groupOf[view] != groupOf[target])
        {
          continue;
        }

        // Wow Yet Again Another Accuracy Fix! :(
        // If these sectors touch directly, they are unconditionally visible!
        // Skipping curved alternative geometric checks prevents tunnel/cave blindness bugs.
        const size_t adjIdx = view * num_secs + target;
        if ((adjacencyMatrix[adjIdx >> 3] & (1 << (adjIdx & 7))) != 0)
        {
          continue;
        }

        const auto& boxB = sectorBoxes[target];

        // Every ray endpoint lies inside its sector's bbox, so no ray can be shorter
        // than the gap between the boxes. IsRayBlocked rejects anything longer than
        // maxSectorSpanSq anyway, so such a pair is blind without casting a ray.
        const double gapX = std::max({0.0, boxB.minX - boxA.maxX, boxA.minX - boxB.maxX});
        const double gapY = std::max({0.0, boxB.minY - boxA.maxY, boxA.minY - boxB.maxY});
        if ((gapX * gapX) + (gapY * gapY) > maxSectorSpanSq)
        {
          SetPairBitsAtomic(level, view, target, num_secs);
          continue;
        }

        // Each ray now walks the grid on its own inside IsRayBlocked via DDA steps.
        if (!DoVisibilityTest(sectorPoints[view], sectorPoints[target],
                              solidBlockers, grid, scratch, maxSectorSpanSq))
        {
          SetPairBitsAtomic(level, view, target, num_secs);
        }
      }
    }
  }

  // Restore process priority back to normal once the heavy CPU work finishes
  RestorePriority();
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
