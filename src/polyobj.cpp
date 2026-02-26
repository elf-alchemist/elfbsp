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

static constexpr uint32_t POLY_BOX_SZ = 10;

/* ----- polyobj handling ----------------------------- */

void MarkPolyobjSector(sector_t *sector)
{
  if (sector == nullptr)
  {
    return;
  }

  if (HAS_BIT(config.debug, DEBUG_POLYOBJ))
  {
    PrintLine(LOG_DEBUG, "[%s] Marking Polyobj SECTOR %zu", __func__, sector->index);
  }

  /* already marked ? */
  if (sector->has_polyobj)
  {
    return;
  }

  // mark all lines of this sector as precious, to prevent (ideally)
  // the sector from being split.
  sector->has_polyobj = true;

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    linedef_t *L = lev_linedefs[i];

    if ((L->right && L->right->sector == sector) || (L->left && L->left->sector == sector))
    {
      L->effects |= FX_DoNotSplitSeg;
    }
  }
}

void MarkPolyobjPoint(double x, double y)
{
  size_t inside_count = 0;

  double best_dist = 999999;
  const linedef_t *best_match = nullptr;
  sector_t *sector = nullptr;

  // -AJA- First we handle the "awkward" cases where the polyobj sits
  //       directly on a linedef or even a vertex.  We check all lines
  //       that intersect a small box around the spawn point.

  int32_t bminx = static_cast<int32_t>(x - POLY_BOX_SZ);
  int32_t bminy = static_cast<int32_t>(y - POLY_BOX_SZ);
  int32_t bmaxx = static_cast<int32_t>(x + POLY_BOX_SZ);
  int32_t bmaxy = static_cast<int32_t>(y + POLY_BOX_SZ);

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    if (CheckLinedefInsideBox(bminx, bminy, bmaxx, bmaxy, static_cast<int32_t>(L->start->x), static_cast<int32_t>(L->start->y),
                              static_cast<int32_t>(L->end->x), static_cast<int32_t>(L->end->y)))
    {
      if (HAS_BIT(config.debug, DEBUG_POLYOBJ))
      {
        PrintLine(LOG_DEBUG, "[%s] Touching line was %zu", __func__, L->index);
      }

      if (L->left)
      {
        MarkPolyobjSector(L->left->sector);
      }

      if (L->right)
      {
        MarkPolyobjSector(L->right->sector);
      }

      inside_count++;
    }
  }

  if (inside_count > 0)
  {
    return;
  }

  // -AJA- Algorithm is just like in DEU: we cast a line horizontally
  //       from the given (x,y) position and find all linedefs that
  //       intersect it, choosing the one with the closest distance.
  //       If the point is sitting directly on a (two-sided) line,
  //       then we mark the sectors on both sides.

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    double x1 = L->start->x;
    double y1 = L->start->y;
    double x2 = L->end->x;
    double y2 = L->end->y;

    /* check vertical range */
    if (fabs(y2 - y1) < DIST_EPSILON)
    {
      continue;
    }

    if ((y > (y1 + DIST_EPSILON) && y > (y2 + DIST_EPSILON)) || (y < (y1 - DIST_EPSILON) && y < (y2 - DIST_EPSILON)))
    {
      continue;
    }

    double x_cut = x1 + (x2 - x1) * (y - y1) / (y2 - y1) - x;

    if (fabs(x_cut) < fabs(best_dist))
    {
      /* found a closer linedef */

      best_match = L;
      best_dist = x_cut;
    }
  }

  if (best_match == nullptr)
  {
    PrintLine(LOG_NORMAL, "WARNING: Bad polyobj thing at (%1.0f,%1.0f).", x, y);
    config.total_warnings++;
    return;
  }

  double y1 = best_match->start->y;
  double y2 = best_match->end->y;

  if (HAS_BIT(config.debug, DEBUG_POLYOBJ))
  {
    PrintLine(LOG_DEBUG, "[%s] Closest line was %zu Y=%1.0f..%1.0f (dist=%1.1f)", __func__, best_match->index, y1, y2,
              best_dist);
  }

  /* sanity check: shouldn't be directly on the line */
  if (HAS_BIT(config.debug, DEBUG_POLYOBJ))
  {
    if (fabs(best_dist) < DIST_EPSILON)
    {
      PrintLine(LOG_DEBUG, "[%s] Polyobj directly on the line (%zu)", __func__, best_match->index);
    }
  }

  /* check orientation of line, to determine which side the polyobj is
   * actually on.
   */
  if ((y1 > y2) == (best_dist > 0))
  {
    sector = best_match->right ? best_match->right->sector : nullptr;
  }
  else
  {
    sector = best_match->left ? best_match->left->sector : nullptr;
  }

  if (HAS_BIT(config.debug, DEBUG_POLYOBJ))
  {
    PrintLine(LOG_DEBUG, "[%s] Sector %zu contains the polyobj.", __func__, sector ? sector->index : NO_INDEX);
  }

  if (sector == nullptr)
  {
    PrintLine(LOG_NORMAL, "WARNING: Invalid Polyobj thing at (%1.0f,%1.0f).", x, y);
    config.total_warnings++;
    return;
  }

  MarkPolyobjSector(sector);
}

//
// Based on code courtesy of Janis Legzdinsh.
//
void DetectPolyobjSectors(buildinfo_t &ctx)
{
  // -JL- First go through all lines to see if level contains any polyobjs
  size_t i;
  for (i = 0; i < lev_linedefs.size(); i++)
  {
    linedef_t *L = lev_linedefs[i];

    if (L->special == Polyobj_StartLine || L->special == Polyobj_ExplicitLine)
    {
      break;
    }
  }

  if (i == lev_linedefs.size())
  {
    // -JL- No polyobjs in this level
    return;
  }

  if (HAS_BIT(ctx.debug, DEBUG_POLYOBJ))
  {
    PrintLine(LOG_DEBUG, "[%s] Detected %s style polyobj things", __func__,
              (ctx.polyobj.anchor == ZDoom_PolyObj_Anchor) ? "ZDOOM" : "HEXEN");
  }

  for (size_t j = 0; j < lev_things.size(); j++)
  {
    thing_t *T = lev_things[j];

    double x = T->x;
    double y = T->y;
    doomednum_t t = T->type;

    // ignore everything except polyobj start spots
    if (t != ctx.polyobj.anchor && t != ctx.polyobj.spawn && t != ctx.polyobj.spawn_crush && t != ctx.polyobj.spawn_hurt)
    {
      continue;
    }

    if (HAS_BIT(ctx.debug, DEBUG_POLYOBJ))
    {
      PrintLine(LOG_DEBUG, "[%s] Thing %zu at (%1.0f,%1.0f) is a polyobj spawner.", __func__, i, x, y);
    }

    MarkPolyobjPoint(x, y);
  }
}
