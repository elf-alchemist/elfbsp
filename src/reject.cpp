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
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#endif

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
  UtilZero(level.reject_matrix);

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
  uint32_t touchStart = 0; // First entry in the touching-wall table
  uint32_t touchCount = 0; // Number of solid walls this point lies on
};

// Ray endpoints for one sector, built once and reused for every pair it's in.
struct SectorPoints
{
  std::vector<SightPoint> verts;      // Every unique linedef endpoint
  std::vector<SightPoint> firstVerts; // Unique first endpoints only
  std::vector<SightPoint> openMids;   // Midpoints of non-solid lines
  std::vector<SightPoint> openSplit;  // Points along non-solid lines, split into equal parts
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

// Number of parts each open line is cut into. A split of N gives N - 1 points per open line,
// so a pair of open lines gets (N - 1) * (N - 1) portal rays.
static constexpr int32_t SPLIT_SMALL_MAP = 18; // Maps within the ray budget
static constexpr int32_t SPLIT_LARGE_MAP = 4;  // Maps over the ray budget

// Yardstick for the budget check. The extra portal rays per pair of open lines when going
// from a split of 2 to a split of 4.
// It is not the real extra cost of SPLIT_SMALL_MAP, it only decides the budget line,
// so always check which maps pass before changing it.
static constexpr uint64_t BUDGET_RAYS_PER_PAIR = 8;

// Default Budget is 50,000,000, which seems to be a good balance between adding accuracy for
// small-medium, dense maps without wasting CPU time on large, open maps that don't benefit anyway.
// The result is even better accuracy for small to medium sized maps since the longer compile can be absorbed by optimizations.
// Large, open maps also gain accuracy but avoid the increased compile time by unnecessary cuts.
static constexpr uint64_t OPEN_SPLIT_RAY_BUDGET = 50000000;

// This extracts and groups unique locations (corners, open door frames, etc.) exactly once.
// This will shift heavy allocation and duplication entirely out of the inner loops.
static SectorPoints BuildSectorPoints(const std::vector<SightSegment>& lines, int32_t split)
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

      for (int32_t k = 1; k < split; ++k)
      {
        const double t = static_cast<double>(k) / static_cast<double>(split);
        pts.openSplit.push_back({s.x1 + (s.x2 - s.x1) * t, s.y1 + (s.y2 - s.y1) * t});
      }
    }
  }

  // Strip out layout redundancies.
  SortUnique(pts.verts);
  SortUnique(pts.firstVerts);
  SortUnique(pts.openMids);
  SortUnique(pts.openSplit);
  SortUnique(pts.allMids);

  // The ray loops only ask whether any ray is clear, so the order of the points never
  // changes the result. Ordering them by angle around the sector's center puts neighboring
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
    std::sort(pts.openSplit.begin(), pts.openSplit.end(), byAngle);
    std::sort(pts.allMids.begin(), pts.allMids.end(), byAngle);
  }

  return pts;
}

// Uniform grid of solid blockers in CSR form. One flat array instead of a vector per cell.
struct BlockerGrid
{
  double originX = 0.0;
  double originY = 0.0;
  double cellSize = 128.0; // BuildBlockerGrid overwrites with CELL_SIZE, safe default for an unset grid.
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

  std::vector<uint32_t> seen; // Stamp of the last ray that tested each blocker
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

  // Start a new ray and return its stamp, to compare against ‘seen’.
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

// How far from a line a point has to be before it counts as clearly on one side of it.
static constexpr double SIDE_MARGIN = 1e-3;

// The shadow shortcut uses twice that margin, so rounding can never make it accept a ray
// that TrueSegmentsIntersect would not call blocked. Keep it derived from SIDE_MARGIN.
static constexpr double SHADOW_MARGIN = SIDE_MARGIN * 2.0;

// 2D intersection check between two line segments.
inline bool TrueSegmentsIntersect(const SightSegment& s1, const SightSegment& s2)
{
  double d1 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1);
  double d2 = SegmentSide(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2);
  double d3 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1);
  double d4 = SegmentSide(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2);

  if (((d1 > SIDE_MARGIN && d2 < -SIDE_MARGIN) || (d1 < -SIDE_MARGIN && d2 > SIDE_MARGIN)) &&
      ((d3 > SIDE_MARGIN && d4 < -SIDE_MARGIN) || (d3 < -SIDE_MARGIN && d4 > SIDE_MARGIN)))
  {
    return true;
  }

  // Each check below can only succeed when its cross product, which is just d1 to d4 again,
  // is smaller than DIST_EPSILON. If none of them are, all four calls would fail, so skip them.
  const bool s1Outside = (std::abs(d1) >= DIST_EPSILON) && (std::abs(d2) >= DIST_EPSILON);
  const bool s2Outside = (std::abs(d3) >= DIST_EPSILON) && (std::abs(d4) >= DIST_EPSILON);

  if (s1Outside && s2Outside)
  {
    return false;
  }

  if (IsPointOnSegment(s2.x1, s2.y1, s2.x2, s2.y2, s1.x1, s1.y1)) return true;
  if (IsPointOnSegment(s2.x1, s2.y1, s2.x2, s2.y2, s1.x2, s1.y2)) return true;
  if (IsPointOnSegment(s1.x1, s1.y1, s1.x2, s1.y2, s2.x1, s2.y1)) return true;
  if (IsPointOnSegment(s1.x1, s1.y1, s1.x2, s1.y2, s2.x2, s2.y2)) return true;

  return false;
}

// Does this solid wall stop this ray? Shared by the grid walk, the ring and the
// end-point pre-check so all three always agree.
static inline bool SolidBlocksRay(const SightSegment& ray, const SightSegment& blocker)
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
    return SolidBlocksRay(ray, blocker);
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
  // rounding picks a neighboring cell at a corner.
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

    // Step into the neighboring cell whose border the ray reaches first.
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

// Which side of a wall do all four corners of a box fall on?
// Returns 1 when every corner is clearly on the positive side, -1 when every corner is clearly
// on the negative side, and 0 when the wall does not cleanly separate the box.
static inline int32_t BoxSideOfWall(const SightSegment& w, const SectorBBox& box)
{
  // Twice the margin TrueSegmentsIntersect uses, so rounding can never make this shortcut
  // accept a ray that the original test would not call blocked.
  constexpr double MARGIN = SHADOW_MARGIN;

  const double s0 = SegmentSide(w.x1, w.y1, w.x2, w.y2, box.minX, box.minY);
  const double s1 = SegmentSide(w.x1, w.y1, w.x2, w.y2, box.maxX, box.minY);
  const double s2 = SegmentSide(w.x1, w.y1, w.x2, w.y2, box.minX, box.maxY);
  const double s3 = SegmentSide(w.x1, w.y1, w.x2, w.y2, box.maxX, box.maxY);

  if (s0 > MARGIN && s1 > MARGIN && s2 > MARGIN && s3 > MARGIN)
  {
    return 1;
  }

  if (s0 < -MARGIN && s1 < -MARGIN && s2 < -MARGIN && s3 < -MARGIN)
  {
    return -1;
  }

  return 0;
}

// Does this one solid wall stop every ray from the point (px,py) to anywhere inside the box?
// The side-of-ray test changes in a straight line across the box, so its smallest value is
// always at one of the four corners. A wall that crosses the rays to all four corners
// therefore crosses the rays to every point inside the box.
// If the wall properly crosses the rays to all four corners, it crosses the rays to every point inside the box.
// 'boxSide' is BoxSideOfWall, passed in because it does not depend on the start point.
static inline bool WallShadowsBox(const SightSegment& w, int32_t boxSide, double px, double py, const SectorBBox& box)
{
  constexpr double MARGIN = 2e-3; // Same reasoning as in BoxSideOfWall

  if (boxSide == 0)
  {
    return false;
  }

  // A ray starting on the wall's own end point is ignored by that wall.
  if ((std::abs(px - w.x1) < DIST_EPSILON && std::abs(py - w.y1) < DIST_EPSILON) ||
      (std::abs(px - w.x2) < DIST_EPSILON && std::abs(py - w.y2) < DIST_EPSILON))
  {
    return false;
  }

  // The start point has to be clearly on the opposite side of the wall from the box.
  const double d1 = SegmentSide(w.x1, w.y1, w.x2, w.y2, px, py);
  if (boxSide > 0 ? !(d1 < -MARGIN) : !(d1 > MARGIN))
  {
    return false;
  }

  // The wall's two end points have to be on opposite sides of the ray to every corner.
  const double cornerX[4] = {box.minX, box.maxX, box.minX, box.maxX};
  const double cornerY[4] = {box.minY, box.minY, box.maxY, box.maxY};
  const double u1x = w.x1 - px;
  const double u1y = w.y1 - py;
  const double u2x = w.x2 - px;
  const double u2y = w.y2 - py;

  bool posNeg = true;
  bool negPos = true;

    for (int32_t j = 0; j < 4; ++j)
  {
    const double cx = cornerX[j] - px;
    const double cy = cornerY[j] - py;
    const double d3 = cx * u1y - cy * u1x; // SegmentSide(start, corner, wall start)
    const double d4 = cx * u2y - cy * u2x; // SegmentSide(start, corner, wall end)

    posNeg = posNeg && (d3 > MARGIN && d4 < -MARGIN);
    negPos = negPos && (d3 < -MARGIN && d4 > MARGIN);
  }

  return posNeg || negPos;
}

// Tests between the points of Sector A and the points of Sector B.
// Returns true if a valid line of sight connects the two places.
static bool DoVisibilityTest(const SectorPoints& A,
                             const SectorPoints& B,
                             const std::vector<SightSegment>& blockers,
                             const BlockerGrid& grid,
                             const std::vector<uint32_t>& touchWalls,
                             const SectorBBox& boxB,
                             RayWalkSpace& scratch,
                             double maxRayLengthSq)
{
  // Every point of B lies inside its sector's bounding box. When the wall that blocked the
  // most recent ray also blocks every ray from a start point to that whole box, none of the
  // rays from that start point can be clear, so they are skipped. A blocked ray never changes
  // the result, so this saves time. Neighboring start points are in similar directions,
  // so the same wall usually shadows several of them in a row.
  uint32_t shadowWall = NO_INDEX_INT32;
  int32_t shadowSide = 0;

  auto shadowed = [&](const SightPoint& from) -> bool
  {
    if (scratch.ringCount == 0) return false;

    const uint32_t id = scratch.ring[0];
    if (id != shadowWall)
    {
      shadowWall = id;
      shadowSide = BoxSideOfWall(blockers[id], boxB);
    }
    return WallShadowsBox(blockers[id], shadowSide, from.x, from.y, boxB);
  };

  auto clear = [&](const SightPoint& from, const SightPoint& to) -> bool
  {
    const SightSegment ray(from.x, from.y, to.x, to.y);

    // A ray that ends on a solid wall is blocked by it, unless that wall starts at the
    // ray's own corner. Checking the few walls the end point lies on is much cheaper
    // than a ring and grid walk, and gives the same result.
    for (uint32_t k = 0; k < to.touchCount; ++k)
    {
      if (SolidBlocksRay(ray, blockers[touchWalls[to.touchStart + k]]))
      {
        return false;
      }
    }

    return !IsRayBlocked(ray, blockers, grid, scratch, maxRayLengthSq);
  };

  // Portal midpoint to portal midpoint first is most likely to succeed,
  // so visible pairs usually exit early.
  // The split points (the midpoint plus the extra points along each open line) are used
  // here, so narrow sight windows between the midpoints are not missed.
  for (const auto& a : A.openSplit)
  {
    if (shadowed(a)) continue;

    for (const auto& b : B.openSplit)
    {
      if (clear(a, b)) return true;
    }
  }

  // Portal midpoint to the first endpoint of each line in B.
  for (const auto& a : A.openMids)
  {
    if (shadowed(a)) continue;

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
    if (shadowed(a)) continue;

    for (const auto& b : B.allMids)
    {
      if (clear(a, b)) return true;
    }
  }

  // Corner to corner.
  for (const auto& a : A.verts)
  {
    if (shadowed(a)) continue;

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

// Check if the geometric distance gap between two sector bounding boxes exceeds max span thresholds.
static inline bool CheckGapTooWide(const SectorBBox& boxA, const SectorBBox& boxB, double maxSectorSpanSq)
{
  const double gapX = std::max({0.0, boxB.minX - boxA.maxX, boxA.minX - boxB.maxX});
  const double gapY = std::max({0.0, boxB.minY - boxA.maxY, boxA.minY - boxB.maxY});
  return (gapX * gapX) + (gapY * gapY) > maxSectorSpanSq;
}

// Maps a ray endpoint vertex to its corresponding cell index and logs any touching solid wall data.
static inline void RecordTouchingWalls(std::vector<SightPoint>& list,
                                       const std::vector<SightSegment>& solidBlockers,
                                       const BlockerGrid& grid,
                                       std::vector<uint32_t>& touchWalls,
                                       double mapMinX,
                                       double mapMinY,
                                       size_t gridCols,
                                       size_t gridRows,
                                       double cellSize)
{
  for (auto& p : list)
  {
    const size_t cx = GetCellIndex(p.x, mapMinX, gridCols - 1, cellSize);
    const size_t cy = GetCellIndex(p.y, mapMinY, gridRows - 1, cellSize);
    const size_t cell = cy * gridCols + cx;

    p.touchStart = static_cast<uint32_t>(touchWalls.size());
    for (uint32_t k = grid.cellStart[cell]; k < grid.cellStart[cell + 1]; ++k)
    {
      const SightSegment& w = solidBlockers[grid.items[k]];
      if (IsPointOnSegment(w.x1, w.y1, w.x2, w.y2, p.x, p.y))
      {
        touchWalls.push_back(grid.items[k]);
      }
    }
    p.touchCount = static_cast<uint32_t>(touchWalls.size()) - p.touchStart;
  }
}

// A sight line can only move from one sector into the next across a two-sided line. If taking
// away a sector S splits the map into separate pieces, S is a "cut point", and every sight line
// from one piece to another has to pass through S. So when sector A cannot see S, it cannot see
// anything on the far side of S either, and those pairs are blind without casting a single ray.
// This is the graph idea from ZokumBSP.
//
// To find which cut points stand between two sectors without trying every cut point, the map is
// stored as a block and cut point tree. A block is a group of sectors that stay connected even
// when any single one of them is taken away. Each block hangs off the sector above it, and each
// sector hangs off its block. The cut points between two sectors are then exactly the sectors
// passed on the way between them through the tree. That is usually a short walk, and the memory
// used only grows with the number of sectors.
struct CutPoints
{
  std::vector<uint8_t> isCut;   // 1 when the sector is a cut point
  std::vector<size_t> list;     // Every cut point sector
  std::vector<uint32_t> parent; // Tree: parent of each node. Nodes below num_secs are sectors, the rest are blocks
  std::vector<uint32_t> depth;  // Tree: steps from the top of the tree to each node
  std::vector<uint32_t> compOf; // Which connected part of the map each sector is in
};

// Find the sectors that split the neighbor graph apart when taken away,
// and the tree that tells which of them stand between two sectors.
static CutPoints FindCutPoints(const std::vector<std::vector<uint32_t>>& neighbors, size_t num_secs)
{
  CutPoints cuts;
  cuts.isCut.assign(num_secs, 0);
  cuts.parent.assign(num_secs, NO_INDEX_INT32);
  cuts.compOf.assign(num_secs, NO_INDEX_INT32);

  std::vector<uint32_t> order(num_secs, 0);     // Order the depth first walk reached each sector in
  std::vector<uint32_t> lowest(num_secs, 0);    // Earliest reached sector this one can get back to
  std::vector<uint32_t> nextEdge(num_secs, 0);  // Next neighbor to look at, per sector
  std::vector<uint32_t> blockCount(num_secs, 0);
  std::vector<uint32_t> sectorStack;
  std::vector<uint32_t> callStack; // Uses its own stack instead of recursion, because a long corridor of sectors could be very deep.

  uint32_t clock = 0;
  uint32_t parts = 0;

  for (size_t first = 0; first < num_secs; ++first)
  {
    if (order[first] != 0) continue;

    order[first] = lowest[first] = ++clock;
    cuts.compOf[first] = parts;
    sectorStack.push_back(static_cast<uint32_t>(first));
    callStack.push_back(static_cast<uint32_t>(first));

    while (!callStack.empty())
    {
      const uint32_t v = callStack.back();

      if (static_cast<size_t>(nextEdge[v]) < neighbors[v].size())
      {
        const uint32_t w = neighbors[v][nextEdge[v]++];

        if (order[w] == 0)
        {
          order[w] = lowest[w] = ++clock;
          cuts.compOf[w] = parts;
          sectorStack.push_back(w);
          callStack.push_back(w);
        }
        else
        {
          lowest[v] = std::min(lowest[v], order[w]);
        }
        continue;
      }

      // All neighbors of v are done. Send the result to the sector it was reached from.
      callStack.pop_back();
      if (callStack.empty()) break;

      const uint32_t above = callStack.back();
      lowest[above] = std::min(lowest[above], lowest[v]);

      // v cannot get back past 'above', so v and everything stacked after it
      // form one block, joined to the rest of the map only through 'above'.
      if (lowest[v] >= order[above])
      {
        const uint32_t block = static_cast<uint32_t>(cuts.parent.size());
        cuts.parent.push_back(above);
        blockCount[above]++;

        uint32_t u;
        do
        {
          u = sectorStack.back();
          sectorStack.pop_back();
          cuts.parent[u] = block;
          blockCount[u]++;
        } while (u != v);
      }
    }

    sectorStack.clear();
    ++parts;
  }

  // A sector that sits in two or more blocks is a cut point.
  for (size_t s = 0; s < num_secs; ++s)
  {
    if (blockCount[s] >= 2)
    {
      cuts.isCut[s] = 1;
      cuts.list.push_back(s);
    }
  }

  // Work out how deep each node is, so two sectors can be walked up to where their paths meet.
  const size_t numNodes = cuts.parent.size();
  cuts.depth.assign(numNodes, NO_INDEX_INT32);

  std::vector<uint32_t> chain;
  for (size_t n = 0; n < numNodes; ++n)
  {
    if (cuts.depth[n] != NO_INDEX_INT32) continue;

    chain.clear();
    uint32_t m = static_cast<uint32_t>(n);
    while (m != NO_INDEX_INT32 && cuts.depth[m] == NO_INDEX_INT32)
    {
      chain.push_back(m);
      m = cuts.parent[m];
    }

    uint32_t d = (m == NO_INDEX_INT32) ? 0 : cuts.depth[m] + 1;
    for (size_t i = chain.size(); i-- > 0;)
    {
      cuts.depth[chain[i]] = d++;
    }
  }

  return cuts;
}

// Every pair that involves a cut point, grouped by how many sectors apart the two are.
// The pairs nearest to a cut point are tested first, so the result for a farther pair
// can be worked out from them.
static std::vector<std::vector<std::pair<uint32_t, uint32_t>>> BuildCutLevels(
    const CutPoints& cuts,
    const std::vector<std::vector<uint32_t>>& neighbors,
    size_t num_secs,
    const std::vector<size_t>& activeSectors)
{
  std::vector<uint8_t> isActive(num_secs, 0);
  for (size_t s : activeSectors)
  {
    isActive[s] = 1;
  }

  std::vector<std::vector<std::pair<uint32_t, uint32_t>>> levels;
  std::vector<int32_t> dist(num_secs);
  std::vector<uint32_t> queue;

  for (size_t s : cuts.list)
  {
    if (!isActive[s]) continue;

    std::fill(dist.begin(), dist.end(), -1);
    dist[s] = 0;
    queue.assign(1, static_cast<uint32_t>(s));
    for (size_t head = 0; head < queue.size(); ++head)
    {
      for (uint32_t v : neighbors[queue[head]])
      {
        if (dist[v] < 0)
        {
          dist[v] = dist[queue[head]] + 1;
          queue.push_back(v);
        }
      }
    }

    for (uint32_t x : queue)
    {
      if (x == s || !isActive[x]) continue;
      if (cuts.isCut[x] && x < s) continue; // A pair of two cut points is only listed once

      const size_t d = static_cast<size_t>(dist[x]);
      if (levels.size() <= d) levels.resize(d + 1);
      levels[d].push_back({static_cast<uint32_t>(std::max<size_t>(x, s)),
                           static_cast<uint32_t>(std::min<size_t>(x, s))});
    }
  }

  return levels;
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

  // The same connections as a list per sector, used to find the cut points.
  std::vector<std::vector<uint32_t>> neighbors(num_secs);

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
      if ((adjacencyMatrix[idx1 >> 3] & (1 << (idx1 & 7))) == 0)
      {
        neighbors[sLeft->index].push_back(static_cast<uint32_t>(sRight->index));
        neighbors[sRight->index].push_back(static_cast<uint32_t>(sLeft->index));
      }
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

  // Calculate the max sector span across the map to scale ray bounds correctly.
  // This value caps our visibility ray checks on wide open fields without manual configuration.
  constexpr double MIN_RAY_SPAN = 4096.0;  // Rays are never capped shorter than this, in map units
  constexpr double RAY_SPAN_FACTOR = 2.0;  // A ray may be this many times the diagonal of the largest sector

  double maxSectorSpanSq = MIN_RAY_SPAN * MIN_RAY_SPAN;

  for (size_t i = 0; i < num_secs; ++i)
  {
    const auto& box = sectorBoxes[i];
    if (box.maxX > box.minX && box.maxY > box.minY)
    {
      double sWidth = box.maxX - box.minX;
      double sHeight = box.maxY - box.minY;
      double currentSpanSq = ((sWidth * sWidth) + (sHeight * sHeight)) * (RAY_SPAN_FACTOR * RAY_SPAN_FACTOR);
      if (currentSpanSq > maxSectorSpanSq) maxSectorSpanSq = currentSpanSq;
    }
  }

  // Pick how finely open lines are sampled. The extra rays from the finer split land on
  // sector pairs that still need rays, scaled by how many open lines each sector has.
  std::vector<uint64_t> openCount(num_secs, 0);
  for (size_t i = 0; i < num_secs; ++i)
  {
    for (const auto& line : sectorLines[i])
    {
      if (!line.solid) openCount[i]++;
    }
  }

  uint64_t portalPairs = 0;
  for (size_t view = 0; view < num_secs; ++view)
  {
    if (openCount[view] == 0) continue;

    for (size_t target = 0; target < view; ++target)
    {
      if (openCount[target] == 0 || groupOf[view] != groupOf[target]) continue;

      const size_t adjIdx = view * num_secs + target;
      if ((adjacencyMatrix[adjIdx >> 3] & (1 << (adjIdx & 7))) != 0) continue;
      if (CheckGapTooWide(sectorBoxes[view], sectorBoxes[target], maxSectorSpanSq)) continue;

      portalPairs += openCount[view] * openCount[target];
    }
  }

  // Maps within the ray budget cut each open line finely (SPLIT_SMALL_MAP) for the best accuracy.
  // Larger maps use the SPLIT_LARGE_MAP, which keeps compile time manageable.
  const uint64_t extraRays = portalPairs * BUDGET_RAYS_PER_PAIR;
  const int32_t portalSplit = (extraRays <= OPEN_SPLIT_RAY_BUDGET) ? SPLIT_SMALL_MAP : SPLIT_LARGE_MAP;

  // Build the per-sector ray endpoint lists once, reused for every pair.
  std::vector<SectorPoints> sectorPoints(num_secs);
  for (size_t i = 0; i < num_secs; ++i)
  {
    sectorPoints[i] = BuildSectorPoints(sectorLines[i], portalSplit);
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

  // For every ray end point, record which solid walls it lies on. Only the grid cell
  // holding the point needs searching because walls are registered with padding.
  std::vector<uint32_t> touchWalls;
  for (auto& sp : sectorPoints)
  {
    RecordTouchingWalls(sp.verts, solidBlockers, grid, touchWalls, mapMinX, mapMinY, gridCols, gridRows, CELL_SIZE);
    RecordTouchingWalls(sp.firstVerts, solidBlockers, grid, touchWalls, mapMinX, mapMinY, gridCols, gridRows, CELL_SIZE);
    RecordTouchingWalls(sp.allMids, solidBlockers, grid, touchWalls, mapMinX, mapMinY, gridCols, gridRows, CELL_SIZE);
  }

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

  // Signed loop counter: OpenMP 2.0 compilers (MSVC) require one.
  const ptrdiff_t activeCount = static_cast<ptrdiff_t>(activeSectors.size());

  // Find the cut points, and line up the pairs that involve one (FindCutPoints).
  CutPoints cuts;
  cuts = FindCutPoints(neighbors, num_secs);

  const bool prune = !cuts.list.empty();

  std::vector<std::vector<std::pair<uint32_t, uint32_t>>> cutLevels;

  // Copy of the matrix as it stood after the last finished level
  std::vector<uint8_t> blindSnap;

  if (prune)
  {
    cutLevels = BuildCutLevels(cuts, neighbors, num_secs, activeSectors);

    blindSnap.resize(level.reject_size);
    memcpy(blindSnap.data(), level.reject_matrix, level.reject_size);
  }

  // Is this pair blind because a cut point stands between the two sectors
  // and one of them cannot see that cut point?
  // Walk up the tree from both sectors until the two paths meet. Every sector passed on
  // the way, other than the two themselves, is a cut point standing between them.
  auto inferredBlind = [&](size_t a, size_t b) -> bool
  {
    if (cuts.compOf[a] != cuts.compOf[b]) return false;

    uint32_t x = static_cast<uint32_t>(a);
    uint32_t y = static_cast<uint32_t>(b);

    while (x != y)
    {
      uint32_t& mover = (cuts.depth[x] >= cuts.depth[y]) ? x : y;
      mover = cuts.parent[mover];

      const size_t cut = mover;
      if (cut >= num_secs || cut == a || cut == b) continue; // A block or one of the two sectors themselves

      const size_t p1 = a * num_secs + cut;
      const size_t p2 = b * num_secs + cut;
      if (((blindSnap[p1 >> 3] >> (p1 & 7)) & 1) || ((blindSnap[p2 >> 3] >> (p2 & 7)) & 1))
      {
        return true;
      }
    }
    return false;
  };

  // Parallelizes the loop across available CPU cores.
  // Dynamic scheduling lets each idle core pick up the next sector immediately,
  // because some sectors take far longer than others.
  #ifdef _OPENMP
    #pragma omp parallel
  #endif
  {
    // One ray scratch space per thread.
    RayWalkSpace scratch(solidBlockers.size());

    // Everything that decides one pair of sectors.
    auto testPair = [&](size_t view, size_t target)
    {
      // Sectors in different groups were already marked blind in bulk.
      if (groupOf[view] != groupOf[target])
      {
        return;
      }

      // Wow Yet Again Another Accuracy Fix! :(
      // If these sectors touch directly, they are unconditionally visible!
      // Skipping curved alternative geometric checks prevents tunnel/cave blindness bugs.
      const size_t adjIdx = view * num_secs + target;
      if ((adjacencyMatrix[adjIdx >> 3] & (1 << (adjIdx & 7))) != 0)
      {
        return;
      }

      const auto& boxA = sectorBoxes[view];
      const auto& boxB = sectorBoxes[target];

      // Every ray endpoint lies inside its sector's bbox, so no ray can be shorter
      // than the gap between the boxes. IsRayBlocked rejects anything longer than
      // maxSectorSpanSq anyway, so such a pair is blind without casting a ray.
      if (CheckGapTooWide(boxA, boxB, maxSectorSpanSq))
      {
        SetPairBitsAtomic(level, view, target, num_secs);
        return;
      }

      // A cut point blocks this pair, no rays needed.
      if (prune && inferredBlind(view, target))
      {
        SetPairBitsAtomic(level, view, target, num_secs);
        return;
      }

      // Each ray now walks the grid on its own inside IsRayBlocked via DDA steps.
      if (!DoVisibilityTest(sectorPoints[view], sectorPoints[target],
                            solidBlockers, grid, touchWalls, boxB, scratch, maxSectorSpanSq))
      {
        SetPairBitsAtomic(level, view, target, num_secs);
      }
    };

    // Pairs that involve a cut point first, nearest pairs first. Each level is finished
    // before the next one starts, so the results it relies on are all in place.
    for (size_t lv = 0; lv < cutLevels.size(); ++lv)
    {
      const ptrdiff_t levelCount = static_cast<ptrdiff_t>(cutLevels[lv].size());

      #ifdef _OPENMP
        #pragma omp for schedule(dynamic)
      #endif
      for (ptrdiff_t k = 0; k < levelCount; k++)
      {
        const auto& cutPair = cutLevels[lv][static_cast<size_t>(k)];
        testPair(cutPair.first, cutPair.second);
      }

      #ifdef _OPENMP
        #pragma omp single
      #endif
      {
        memcpy(blindSnap.data(), level.reject_matrix, level.reject_size);
      }
    }

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

      for (size_t b = 0; b < a; b++)
      {
        // Map our active target iterator index back to the real sector index
        const size_t target = activeSectors[b];

        // Pairs with a cut point were already done above.
        if (prune && (cuts.isCut[view] || cuts.isCut[target]))
        {
          continue;
        }

        testPair(view, target);
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
  if (level.skip_rej)
  {
    io.LevelCopyLump(level.lumps.label, "REJECT");
    return;
  }

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
