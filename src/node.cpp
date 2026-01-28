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

//
// To be able to divide the nodes down, this routine must decide which
// is the best Seg to use as a nodeline. It does this by selecting the
// line with least splits and has least difference of Segs on either
// side of it.
//
// Credit to Raphael Quinet and DEU, this routine is a copy of the
// nodeline picker used in DEU5beta. I am using this method because
// the method I originally used was not so good.
//
// Rewritten by Lee Killough to significantly improve performance,
// while not affecting results one bit in >99% of cases (some tiny
// differences due to roundoff error may occur, but they are
// insignificant).
//
// Rewritten again by Andrew Apted (-AJA-), 1999-2000.
//

static constexpr uint32_t PRECIOUS_MULTIPLY = 100;
static constexpr uint32_t SEG_FAST_THRESHHOLD = 200;

struct eval_info_t
{
  double cost;
  size_t splits;
  size_t iffy;
  size_t near_miss;

  size_t real_left;
  size_t real_right;
  size_t mini_left;
  size_t mini_right;

  void BumpLeft(const linedef_t *linedef)
  {
    if (linedef != nullptr)
    {
      real_left++;
    }
    else
    {
      mini_left++;
    }
  }

  void BumpRight(const linedef_t *linedef)
  {
    if (linedef != nullptr)
    {
      real_right++;
    }
    else
    {
      mini_right++;
    }
  }
};

std::vector<intersection_t> alloc_cuts;

intersection_t *NewIntersection(void)
{
  intersection_t cut = {};
  alloc_cuts.push_back(cut);
  return &alloc_cuts.back();
}

void FreeIntersections(void)
{
  alloc_cuts.clear();
}

//
// Fill in the fields 'angle', 'len', 'pdx', 'pdy', etc...
//
void Recompute(seg_t *seg)
{
  seg->psx = seg->start->x;
  seg->psy = seg->start->y;
  seg->pex = seg->end->x;
  seg->pey = seg->end->y;
  seg->pdx = seg->pex - seg->psx;
  seg->pdy = seg->pey - seg->psy;

  seg->p_length = hypot(seg->pdx, seg->pdy);

  if (seg->p_length <= 0)
  {
    PrintLine(LOG_ERROR, "Seg %p has zero p_length.", seg);
  }

  seg->p_perp = seg->psy * seg->pdx - seg->psx * seg->pdy;
  seg->p_para = -seg->psx * seg->pdx - seg->psy * seg->pdy;
}

//
// -AJA- Splits the given seg at the point (x,y).  The new seg is
//       returned.  The old seg is shortened (the original start
//       vertex is unchanged), whereas the new seg becomes the cut-off
//       tail (keeping the original end vertex).
//
//       If the seg has a partner, than that partner is also split.
//       NOTE WELL: the new piece of the partner seg is inserted into
//       the same list as the partner seg (and after it) -- thus ALL
//       segs (except the one we are currently splitting) must exist
//       on a singly-linked list somewhere.
//
seg_t *SplitSeg(seg_t *old_seg, double x, double y)
{
  if (HAS_BIT(config.debug, DEBUG_SPLIT))
  {
    if (old_seg->linedef)
    {
      PrintLine(LOG_DEBUG, "[%s] Splitting Linedef %zu (%p) at (%1.1f,%1.1f)", __func__, old_seg->linedef->index, old_seg, x,
                y);
    }
    else
    {
      PrintLine(LOG_DEBUG, "[%s] Splitting Miniseg %p at (%1.1f,%1.1f)", __func__, old_seg, x, y);
    }
  }

  vertex_t *new_vert = NewVertexFromSplitSeg(old_seg, x, y);
  seg_t *new_seg = NewSeg();

  // copy seg info
  new_seg[0] = old_seg[0];
  new_seg->next = nullptr;

  old_seg->end = new_vert;
  new_seg->start = new_vert;

  Recompute(old_seg);
  Recompute(new_seg);

  if (HAS_BIT(config.debug, DEBUG_SPLIT))
  {
    PrintLine(LOG_DEBUG, "[%s] Splitting Vertex is %zu at (%1.1f,%1.1f)", __func__, new_vert->index, new_vert->x, new_vert->y);
  }

  // handle partners

  if (old_seg->partner)
  {
    if (HAS_BIT(config.debug, DEBUG_SPLIT))
    {
      PrintLine(LOG_DEBUG, "[%s] Splitting Partner %p", __func__, old_seg->partner);
    }

    new_seg->partner = NewSeg();

    // copy seg info
    // [ including the "next" field ]
    new_seg->partner[0] = old_seg->partner[0];

    // IMPORTANT: keep partner relationship valid.
    new_seg->partner->partner = new_seg;

    old_seg->partner->start = new_vert;
    new_seg->partner->end = new_vert;

    Recompute(old_seg->partner);
    Recompute(new_seg->partner);

    // link it into list
    old_seg->partner->next = new_seg->partner;
  }

  return new_seg;
}

//
// -AJA- In the quest for slime-trail annihilation :->, this routine
//       calculates the intersection location between the current seg
//       and the partitioning seg, and takes advantage of some common
//       situations like horizontal/vertical lines.
//
inline void ComputeIntersection(seg_t *seg, seg_t *part, double perp_c, double perp_d, double *x, double *y)
{
  // horizontal partition against vertical seg
  if (part->pdy == 0 && seg->pdx == 0)
  {
    *x = seg->psx;
    *y = part->psy;
    return;
  }

  // vertical partition against horizontal seg
  if (part->pdx == 0 && seg->pdy == 0)
  {
    *x = part->psx;
    *y = seg->psy;
    return;
  }

  // 0 = start, 1 = end
  double ds = perp_c / (perp_c - perp_d);

  if (seg->pdx == 0)
  {
    *x = seg->psx;
  }
  else
  {
    *x = seg->psx + (seg->pdx * ds);
  }

  if (seg->pdy == 0)
  {
    *y = seg->psy;
  }
  else
  {
    *y = seg->psy + (seg->pdy * ds);
  }
}

void AddIntersection(intersection_t **cut_list, vertex_t *vert, seg_t *part, bool self_ref)
{
  bool open_before = CheckOpen(vert, -part->pdx, -part->pdy);
  bool open_after = CheckOpen(vert, part->pdx, part->pdy);

  double along_dist = part->ParallelDist(vert->x, vert->y);

  intersection_t *cut;
  intersection_t *after;

  /* merge with any existing vertex? */
  for (cut = (*cut_list); cut; cut = cut->next)
  {
    if (Overlaps(vert, cut->vertex))
    {
      return;
    }
  }

  /* create new intersection */
  cut = NewIntersection();

  cut->vertex = vert;
  cut->along_dist = along_dist;
  cut->self_ref = self_ref;
  cut->open_before = open_before;
  cut->open_after = open_after;

  /* insert the new intersection into the list */

  for (after = (*cut_list); after && after->next; after = after->next)
  {
  }

  while (after && cut->along_dist < after->along_dist)
  {
    after = after->prev;
  }

  /* link it in */
  cut->next = after ? after->next : (*cut_list);
  cut->prev = after;

  if (after)
  {
    if (after->next)
    {
      after->next->prev = cut;
    }

    after->next = cut;
  }
  else
  {
    if (*cut_list)
    {
      (*cut_list)->prev = cut;
    }

    (*cut_list) = cut;
  }
}

//
// Returns true if a "bad seg" was found early.
//
bool EvalPartitionWorker(quadtree_c *tree, seg_t *part, double best_cost, double split_cost, eval_info_t *info)
{
  // -AJA- this is the heart of the superblock idea, it tests the
  //       *whole* quad against the partition line to quickly handle
  //       all the segs within it at once.  Only when the partition
  //       line intercepts the box do we need to go deeper into it.

  int side = OnLineSide(tree, part);

  if (side < 0)
  {
    // LEFT
    info->real_left += tree->real_num;
    info->mini_left += tree->mini_num;

    return false;
  }
  else if (side > 0)
  {
    // RIGHT
    info->real_right += tree->real_num;
    info->mini_right += tree->mini_num;

    return false;
  }

  /* check partition against all Segs */
  for (seg_t *check = tree->list; check; check = check->next)
  {
    // This is the heart of my pruning idea - it catches
    // bad segs early on. Killough
    if (info->cost > best_cost)
    {
      return true;
    }

    double qnty;

    double a = 0, fa = 0;
    double b = 0, fb = 0;

    /* get state of lines' relation to each other */
    if (check->source_line != part->source_line)
    {
      a = part->PerpDist(check->psx, check->psy);
      b = part->PerpDist(check->pex, check->pey);

      fa = fabs(a);
      fb = fabs(b);
    }

    /* check for being on the same line */
    if (fa <= DIST_EPSILON && fb <= DIST_EPSILON)
    {
      // this seg runs along the same line as the partition.  Check
      // whether it goes in the same direction or the opposite.

      if (check->pdx * part->pdx + check->pdy * part->pdy < 0)
      {
        info->BumpLeft(check->linedef);
      }
      else
      {
        info->BumpRight(check->linedef);
      }

      continue;
    }

    // -AJA- check for passing through a vertex.  Normally this is fine
    //       (even ideal), but the vertex could on a sector that we
    //       DONT want to split, and the normal linedef-based checks
    //       may fail to detect the sector being cut in half.  Thanks
    //       to Janis Legzdinsh for spotting this obscure bug.
    if (fa <= DIST_EPSILON || fb <= DIST_EPSILON)
    {
      if (check->linedef != nullptr && check->linedef->is_precious)
      {
        info->cost += 40.0 * split_cost * PRECIOUS_MULTIPLY;
      }
    }

    /* check for right side */
    if (a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
      info->BumpRight(check->linedef);

      /* check for a near miss */
      if ((a >= IFFY_LEN && b >= IFFY_LEN) || (a <= DIST_EPSILON && b >= IFFY_LEN) || (b <= DIST_EPSILON && a >= IFFY_LEN))
      {
        continue;
      }

      info->near_miss++;

      // -AJA- near misses are bad, since they have the potential to
      //       cause really short minisegs to be created in future
      //       processing.  Thus the closer the near miss, the higher
      //       the cost.
      if (a <= DIST_EPSILON || b <= DIST_EPSILON)
      {
        qnty = IFFY_LEN / std::max(a, b);
      }
      else
      {
        qnty = IFFY_LEN / std::min(a, b);
      }

      info->cost += 70.0 * split_cost * (qnty * qnty - 1.0);
      continue;
    }

    /* check for left side */
    if (a < DIST_EPSILON && b < DIST_EPSILON)
    {
      info->BumpLeft(check->linedef);

      /* check for a near miss */
      if ((a <= -IFFY_LEN && b <= -IFFY_LEN) || (a >= -DIST_EPSILON && b <= -IFFY_LEN)
          || (b >= -DIST_EPSILON && a <= -IFFY_LEN))
      {
        continue;
      }

      info->near_miss++;

      // the closer the miss, the higher the cost (see note above)
      if (a >= -DIST_EPSILON || b >= -DIST_EPSILON)
      {
        qnty = IFFY_LEN / -std::min(a, b);
      }
      else
      {
        qnty = IFFY_LEN / -std::max(a, b);
      }

      info->cost += 70.0 * split_cost * (qnty * qnty - 1.0);
      continue;
    }

    // When we reach here, we have a and b non-zero and opposite sign,
    // hence this seg will be split by the partition line.

    info->splits++;

    // If the linedef associated with this seg has a tag >= 900, treat
    // it as precious; i.e. don't split it unless all other options
    // are exhausted.  This is used to protect deep water and invisible
    // lifts/stairs from being messed up accidentally by splits.
    if (check->linedef && check->linedef->is_precious)
    {
      info->cost += 100.0 * split_cost * PRECIOUS_MULTIPLY;
    }
    else
    {
      info->cost += 100.0 * split_cost;
    }

    // -AJA- check if the split point is very close to one end, which
    //       is an undesirable situation (producing very short segs).
    //       This is perhaps _one_ source of those darn slime trails.
    //       Hence the name "IFFY segs", and a rather hefty surcharge.
    if (fa < IFFY_LEN || fb < IFFY_LEN)
    {
      info->iffy++;

      // the closer to the end, the higher the cost
      qnty = IFFY_LEN / std::min(fa, fb);
      info->cost += 140.0 * split_cost * (qnty * qnty - 1.0);
    }
  }

  /* handle sub-blocks recursively */
  for (int c = 0; c < 2; c++)
  {
    if (info->cost > best_cost)
    {
      return true;
    }

    if (tree->subs[c] != nullptr && !tree->subs[c]->Empty())
    {
      if (EvalPartitionWorker(tree->subs[c], part, best_cost, split_cost, info))
      {
        return true;
      }
    }
  }

  /* no "bad seg" was found */
  return false;
}

//
// -AJA- Evaluate a partition seg & determine the cost, taking into
//       account the number of splits, difference between left &
//       right, and linedefs that are tagged 'precious'.
//
// Returns the computed cost, or a negative value if the seg should be
// skipped altogether.
//
double EvalPartition(quadtree_c *tree, seg_t *part, double best_cost, double split_cost)
{
  eval_info_t info;

  /* initialise info structure */
  info.cost = 0;
  info.splits = 0;
  info.iffy = 0;
  info.near_miss = 0;

  info.real_left = 0;
  info.real_right = 0;
  info.mini_left = 0;
  info.mini_right = 0;

  if (EvalPartitionWorker(tree, part, best_cost, split_cost, &info))
  {
    return -1.0;
  }

  /* make sure there is at least one real seg on each side */
  if (info.real_left == 0 || info.real_right == 0)
  {
    if (HAS_BIT(config.debug, DEBUG_PICKNODE))
    {
      PrintLine(LOG_DEBUG, "[%s] No real segs on %s%sside", __func__, info.real_left ? "" : "left ",
                info.real_right ? "" : "right ");
    }

    return -1;
  }

  /* increase cost by the difference between left & right */
  info.cost += 100.0 * abs(int32_t(info.real_left) - int32_t(info.real_right));

  // -AJA- allow miniseg counts to affect the outcome, but to a
  //       lesser degree than real segs.
  info.cost += 50.0 * abs(int32_t(info.mini_left) - int32_t(info.mini_right));

  // -AJA- Another little twist, here we show a slight preference for
  //       partition lines that lie either purely horizontally or
  //       purely vertically.
  if (part->pdx != 0 && part->pdy != 0)
  {
    info.cost += 25.0;
  }

  if (HAS_BIT(config.debug, DEBUG_PICKNODE))
  {
    PrintLine(LOG_DEBUG, "[%s] %p splits=%zu iffy=%zu near=%zu left=%zu+%zu right=%zu+%zu cost=%1.4f", __func__, part,
              info.splits, info.iffy, info.near_miss, info.real_left, info.mini_left, info.real_right, info.mini_right,
              info.cost);
  }

  return info.cost;
}

void EvaluateFastWorker(quadtree_c *tree, seg_t **best_H, seg_t **best_V, int mid_x, int mid_y)
{
  for (seg_t *part = tree->list; part; part = part->next)
  {
    /* ignore minisegs as partition candidates */
    if (part->linedef == nullptr)
    {
      continue;
    }

    if (part->pdy == 0)
    {
      // horizontal seg
      if (!*best_H)
      {
        *best_H = part;
      }
      else
      {
        double old_dist = fabs((*best_H)->psy - mid_y);
        double new_dist = fabs((part)->psy - mid_y);

        if (new_dist < old_dist)
        {
          *best_H = part;
        }
      }
    }
    else if (part->pdx == 0)
    {
      // vertical seg
      if (!*best_V)
      {
        *best_V = part;
      }
      else
      {
        double old_dist = fabs((*best_V)->psx - mid_x);
        double new_dist = fabs((part)->psx - mid_x);

        if (new_dist < old_dist)
        {
          *best_V = part;
        }
      }
    }
  }

  /* handle sub-blocks recursively */
  for (int c = 0; c < 2; c++)
  {
    if (tree->subs[c] != nullptr && !tree->subs[c]->Empty())
    {
      EvaluateFastWorker(tree->subs[c], best_H, best_V, mid_x, mid_y);
    }
  }
}

seg_t *FindFastSeg(quadtree_c *tree, double split_cost)
{
  seg_t *best_H = nullptr;
  seg_t *best_V = nullptr;

  int mid_x = (tree->x1 + tree->x2) / 2;
  int mid_y = (tree->y1 + tree->y2) / 2;

  EvaluateFastWorker(tree, &best_H, &best_V, mid_x, mid_y);

  double H_cost = -1.0;
  double V_cost = -1.0;

  if (best_H)
  {
    H_cost = EvalPartition(tree, best_H, 1.0e99, split_cost);
  }

  if (best_V)
  {
    V_cost = EvalPartition(tree, best_V, 1.0e99, split_cost);
  }

  if (HAS_BIT(config.debug, DEBUG_PICKNODE))
  {
    PrintLine(LOG_DEBUG, "[%s] best_H=%p (cost %1.4f) | best_V=%p (cost %1.4f)", __func__, best_H, H_cost, best_V, V_cost);
  }

  if (H_cost < 0 && V_cost < 0)
  {
    return nullptr;
  }

  if (H_cost < 0)
  {
    return best_V;
  }
  if (V_cost < 0)
  {
    return best_H;
  }

  return (V_cost < H_cost) ? best_V : best_H;
}

/* returns false if cancelled */
bool PickNodeWorker(quadtree_c *part_list, quadtree_c *tree, seg_t **best, double *best_cost, double split_cost)
{
  /* try each Seg as partition */
  for (seg_t *part = part_list->list; part; part = part->next)
  {
    if (HAS_BIT(config.debug, DEBUG_PICKNODE))
    {
      PrintLine(LOG_DEBUG, "[%s]   %sSEG %p  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, part->linedef ? "" : "MINI", part,
                part->start->x, part->start->y, part->end->x, part->end->y);
    }

    /* ignore minisegs as partition candidates */
    if (part->linedef == nullptr)
    {
      continue;
    }

    double cost = EvalPartition(tree, part, *best_cost, split_cost);

    /* seg unsuitable or too costly ? */
    if (cost < 0 || cost >= *best_cost)
    {
      continue;
    }

    /* we have a new better choice */
    (*best_cost) = cost;

    /* remember which Seg */
    (*best) = part;
  }

  /* recursively handle sub-blocks */
  for (int c = 0; c < 2; c++)
  {
    if (part_list->subs[c] != nullptr && !part_list->subs[c]->Empty())
    {
      if (!PickNodeWorker(part_list->subs[c], tree, best, best_cost, split_cost))
      {
        return false;
      }
    }
  }

  return true;
}

//
// Find the best seg in the seg_list to use as a partition line.
//
seg_t *PickNode(quadtree_c *tree, int depth, double split_cost, bool fast)
{
  seg_t *best = nullptr;

  double best_cost = 1.0e99;

  if (HAS_BIT(config.debug, DEBUG_PICKNODE))
  {
    PrintLine(LOG_DEBUG, "[%s] BEGUN (depth %d)", __func__, depth);
  }

  /* -AJA- here is the logic for "fast mode".  We look for segs which
   *       are axis-aligned and roughly divide the current group into
   *       two halves.  This can save *heaps* of times on large levels.
   */
  if (fast && tree->real_num >= SEG_FAST_THRESHHOLD)
  {
    if (HAS_BIT(config.debug, DEBUG_PICKNODE))
    {
      PrintLine(LOG_DEBUG, "[%s] Looking for Fast node...", __func__);
    }

    best = FindFastSeg(tree, split_cost);

    if (best != nullptr)
    {
      if (HAS_BIT(config.debug, DEBUG_PICKNODE))
      {
        PrintLine(LOG_DEBUG, "[%s] Using Fast node (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, best->start->x, best->start->y,
                  best->end->x, best->end->y);
      }
      return best;
    }
  }

  if (!PickNodeWorker(tree, tree, &best, &best_cost, split_cost))
  {
    /* hack here : BuildNodes will detect the cancellation */
    return nullptr;
  }

  if (HAS_BIT(config.debug, DEBUG_PICKNODE))
  {
    if (!best)
    {
      PrintLine(LOG_DEBUG, "[%s] NO BEST FOUND !", __func__);
    }
    else
    {
      PrintLine(LOG_DEBUG, "[%s] Best has score %1.4f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, best_cost, best->start->x,
                best->start->y, best->end->x, best->end->y);
    }
  }

  return best;
}

//
// Apply the partition line to the given seg, taking the necessary
// action (moving it into either the left list, right list, or
// splitting it).
//
// -AJA- I have rewritten this routine based on the EvalPartition
//       routine above (which I've also reworked, heavily).  I think
//       it is important that both these routines follow the exact
//       same logic when determining which segs should go left, right
//       or be split.
//
void DivideOneSeg(seg_t *seg, seg_t *part, seg_t **left_list, seg_t **right_list, intersection_t **cut_list)
{
  /* get state of lines' relation to each other */
  double a = part->PerpDist(seg->psx, seg->psy);
  double b = part->PerpDist(seg->pex, seg->pey);

  bool self_ref = seg->linedef ? seg->linedef->self_ref : false;

  if (seg->source_line == part->source_line)
  {
    a = b = 0;
  }

  /* check for being on the same line */
  if (fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
  {
    AddIntersection(cut_list, seg->start, part, self_ref);
    AddIntersection(cut_list, seg->end, part, self_ref);

    // this seg runs along the same line as the partition.  check
    // whether it goes in the same direction or the opposite.
    if (seg->pdx * part->pdx + seg->pdy * part->pdy < 0)
    {
      ListAddSeg(left_list, seg);
    }
    else
    {
      ListAddSeg(right_list, seg);
    }

    return;
  }

  /* check for right side */
  if (a > -DIST_EPSILON && b > -DIST_EPSILON)
  {
    if (a < DIST_EPSILON)
    {
      AddIntersection(cut_list, seg->start, part, self_ref);
    }
    else if (b < DIST_EPSILON)
    {
      AddIntersection(cut_list, seg->end, part, self_ref);
    }

    ListAddSeg(right_list, seg);
    return;
  }

  /* check for left side */
  if (a < DIST_EPSILON && b < DIST_EPSILON)
  {
    if (a > -DIST_EPSILON)
    {
      AddIntersection(cut_list, seg->start, part, self_ref);
    }
    else if (b > -DIST_EPSILON)
    {
      AddIntersection(cut_list, seg->end, part, self_ref);
    }

    ListAddSeg(left_list, seg);
    return;
  }

  // when we reach here, we have a and b non-zero and opposite sign,
  // hence this seg will be split by the partition line.
  double x, y;
  ComputeIntersection(seg, part, a, b, &x, &y);

  seg_t *new_seg = SplitSeg(seg, x, y);

  AddIntersection(cut_list, seg->end, part, self_ref);

  if (a < 0)
  {
    ListAddSeg(left_list, seg);
    ListAddSeg(right_list, new_seg);
  }
  else
  {
    ListAddSeg(right_list, seg);
    ListAddSeg(left_list, new_seg);
  }
}

void SeparateSegs(quadtree_c *tree, seg_t *part, seg_t **left_list, seg_t **right_list, intersection_t **cut_list)
{
  while (tree->list != nullptr)
  {
    seg_t *seg = tree->list;
    tree->list = seg->next;

    seg->quad = nullptr;
    DivideOneSeg(seg, part, left_list, right_list, cut_list);
  }

  // recursively handle sub-blocks
  if (tree->subs[0] != nullptr)
  {
    SeparateSegs(tree->subs[0], part, left_list, right_list, cut_list);
    SeparateSegs(tree->subs[1], part, left_list, right_list, cut_list);
  }

  // this quadtree_c is empty now
}

// compute the boundary of the list of segs
void FindLimits2(seg_t *list, bbox_t *bbox)
{
  // empty list?
  if (list == nullptr)
  {
    bbox->minx = 0;
    bbox->miny = 0;
    bbox->maxx = 4;
    bbox->maxy = 4;
    return;
  }

  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  for (; list != nullptr; list = list->next)
  {
    double x1 = list->start->x;
    double y1 = list->start->y;
    double x2 = list->end->x;
    double y2 = list->end->y;

    int32_t lx = static_cast<int32_t>(floor(std::min(x1, x2) - 0.2));
    int32_t ly = static_cast<int32_t>(floor(std::min(y1, y2) - 0.2));
    int32_t hx = static_cast<int32_t>(ceil(std::max(x1, x2) + 0.2));
    int32_t hy = static_cast<int32_t>(ceil(std::max(y1, y2) + 0.2));

    if (lx < bbox->minx)
    {
      bbox->minx = lx;
    }
    if (ly < bbox->miny)
    {
      bbox->miny = ly;
    }
    if (hx > bbox->maxx)
    {
      bbox->maxx = hx;
    }
    if (hy > bbox->maxy)
    {
      bbox->maxy = hy;
    }
  }
}

void AddMinisegs(intersection_t *cut_list, seg_t *part, seg_t **left_list, seg_t **right_list)
{
  intersection_t *cut, *next;

  if (HAS_BIT(config.debug, DEBUG_CUTLIST))
  {
    PrintLine(LOG_DEBUG, "[%s] CUT LIST:", __func__);
    PrintLine(LOG_DEBUG, "[%s] PARTITION: (%1.1f,%1.1f) += (%1.1f,%1.1f)", __func__, part->psx, part->psy, part->pdx,
              part->pdy);

    for (cut = cut_list; cut; cut = cut->next)
    {
      PrintLine(LOG_DEBUG, "[%s] Vertex %zu (%1.1f,%1.1f)  Along %1.2f  [%d/%d]  %s", __func__, cut->vertex->index,
                cut->vertex->x, cut->vertex->y, cut->along_dist, cut->open_before ? 1 : 0, cut->open_after ? 1 : 0,
                cut->self_ref ? "SELFREF" : "");
    }
  }

  // find open gaps in the intersection list, convert to minisegs

  for (cut = cut_list; cut && cut->next; cut = cut->next)
  {
    next = cut->next;

    // sanity check
    double len = next->along_dist - cut->along_dist;
    if (len < -0.001)
    {
      PrintLine(LOG_ERROR, "Bad order in intersect list: %1.3f > %1.3f", cut->along_dist, next->along_dist);
    }

    bool A = cut->open_after;
    bool B = next->open_before;

    // nothing possible when both ends are CLOSED
    if (!(A || B))
    {
      continue;
    }

    if (A != B)
    {
      // a mismatch indicates something wrong with level geometry.
      // warning about it is probably not worth it, so ignore it.
      continue;
    }

    // righteo, here we have definite open space.
    // create a miniseg pair....

    seg_t *seg = NewSeg();
    seg_t *buddy = NewSeg();

    seg->partner = buddy;
    buddy->partner = seg;

    seg->start = cut->vertex;
    seg->end = next->vertex;

    buddy->start = next->vertex;
    buddy->end = cut->vertex;

    seg->index = buddy->index = NO_INDEX;
    seg->linedef = buddy->linedef = nullptr;
    seg->side = buddy->side = 0;

    seg->source_line = buddy->source_line = part->linedef;

    Recompute(seg);
    Recompute(buddy);

    // add the new segs to the appropriate lists
    ListAddSeg(right_list, seg);
    ListAddSeg(left_list, buddy);

    if (HAS_BIT(config.debug, DEBUG_CUTLIST))
    {

      PrintLine(LOG_DEBUG, "[%s] %p RIGHT  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, seg, seg->start->x, seg->start->y,
                seg->end->x, seg->end->y);

      PrintLine(LOG_DEBUG, "[%s] %p LEFT   (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, seg, buddy->start->x, buddy->start->y,
                buddy->end->x, buddy->end->y);
    }
  }
}

//------------------------------------------------------------------------
// NODE : Recursively create nodes and return the pointers.
//------------------------------------------------------------------------

//
// Split a list of segs into two using the method described at bottom
// of the file, this was taken from OBJECTS.C in the DEU5beta source.
//
// This is done by scanning all of the segs and finding the one that
// does the least splitting and has the least difference in numbers of
// segs on either side.
//
// If the ones on the left side make a SSector, then create another SSector
// else put the segs into lefts list.
// If the ones on the right side make a SSector, then create another SSector
// else put the segs into rights list.
//
// Rewritten by Andrew Apted (-AJA-), 1999-2000.
//

void SetPartition(node_t *node, const seg_t *part)
{
  SYS_ASSERT(part->linedef);

  if (part->side == 0)
  {
    node->x = part->linedef->start->x;
    node->y = part->linedef->start->y;
    node->dx = part->linedef->end->x - node->x;
    node->dy = part->linedef->end->y - node->y;
  }
  else /* left side */
  {
    node->x = part->linedef->end->x;
    node->y = part->linedef->end->y;
    node->dx = part->linedef->start->x - node->x;
    node->dy = part->linedef->start->y - node->y;
  }

  /* check for very long partition (overflow of dx,dy in NODES) */

  if (fabs(node->dx) > 32766 || fabs(node->dy) > 32766)
  {
    // XGL3 nodes are 16.16 fixed point, hence we still need
    // to reduce the delta.
    node->dx = node->dx / 2.0;
    node->dy = node->dy / 2.0;
  }
}

//
// Returns -1 for left, +1 for right, or 0 for intersect.
//
int PointOnLineSide(const seg_t *seg, double x, double y)
{
  double perp = seg->PerpDist(x, y);

  if (fabs(perp) <= DIST_EPSILON)
  {
    return 0;
  }

  return (perp < 0) ? -1 : +1;
}

/* ----- quad-tree routines ------------------------------------ */

quadtree_c::quadtree_c(int _x1, int _y1, int _x2, int _y2)
    : x1(_x1), y1(_y1), x2(_x2), y2(_y2), real_num(0), mini_num(0), list(nullptr)
{
  int dx = x2 - x1;
  int dy = y2 - y1;

  if (dx <= 320 && dy <= 320)
  {
    // leaf node
    subs[0] = nullptr;
    subs[1] = nullptr;
  }
  else if (dx >= dy)
  {
    subs[0] = new quadtree_c(x1, y1, x1 + dx / 2, y2);
    subs[1] = new quadtree_c(x1 + dx / 2, y1, x2, y2);
  }
  else
  {
    subs[0] = new quadtree_c(x1, y1, x2, y1 + dy / 2);
    subs[1] = new quadtree_c(x1, y1 + dy / 2, x2, y2);
  }
}

quadtree_c::~quadtree_c(void)
{
  if (subs[0] != nullptr)
  {
    delete subs[0];
  }
  if (subs[1] != nullptr)
  {
    delete subs[1];
  }
}

void AddSeg(quadtree_c *quadtree, seg_t *seg)
{
  // update seg counts
  if (seg->linedef != nullptr)
  {
    quadtree->real_num++;
  }
  else
  {
    quadtree->mini_num++;
  }

  if (quadtree->subs[0] != nullptr)
  {
    double x_min = std::min(seg->start->x, seg->end->x);
    double y_min = std::min(seg->start->y, seg->end->y);

    double x_max = std::max(seg->start->x, seg->end->x);
    double y_max = std::max(seg->start->y, seg->end->y);

    if ((quadtree->x2 - quadtree->x1) >= (quadtree->y2 - quadtree->y1))
    {
      if (x_min > quadtree->subs[1]->x1)
      {
        AddSeg(quadtree->subs[1], seg);
        return;
      }
      else if (x_max < quadtree->subs[0]->x2)
      {
        AddSeg(quadtree->subs[0], seg);
        return;
      }
    }
    else
    {
      if (y_min > quadtree->subs[1]->y1)
      {
        AddSeg(quadtree->subs[1], seg);
        return;
      }
      else if (y_max < quadtree->subs[0]->y2)
      {
        AddSeg(quadtree->subs[0], seg);
        return;
      }
    }
  }

  // link into this node

  ListAddSeg(&quadtree->list, seg);

  seg->quad = quadtree;
}

void AddList(quadtree_c *quadtree, seg_t *new_list)
{
  while (new_list != nullptr)
  {
    seg_t *seg = new_list;
    new_list = seg->next;

    AddSeg(quadtree, seg);
  }
}

void ConvertToList(quadtree_c *quadtree, seg_t **_list)
{
  while (quadtree->list != nullptr)
  {
    seg_t *seg = quadtree->list;
    quadtree->list = seg->next;

    ListAddSeg(_list, seg);
  }

  if (quadtree->subs[0] != nullptr)
  {
    ConvertToList(quadtree->subs[0], _list);
    ConvertToList(quadtree->subs[1], _list);
  }

  // this quadtree is empty now
}

int OnLineSide(quadtree_c *quadtree, const seg_t *part)
{
  // expand bounds a bit, adds some safety and loses nothing
  double tx1 = static_cast<double>(quadtree->x1) - 0.4;
  double ty1 = static_cast<double>(quadtree->y1) - 0.4;
  double tx2 = static_cast<double>(quadtree->x2) + 0.4;
  double ty2 = static_cast<double>(quadtree->y2) + 0.4;

  int p1, p2;

  // handle simple cases (vertical & horizontal lines)
  if (part->pdx == 0)
  {
    p1 = (tx1 > part->psx) ? +1 : -1;
    p2 = (tx2 > part->psx) ? +1 : -1;

    if (part->pdy < 0)
    {
      p1 = -p1;
      p2 = -p2;
    }
  }
  else if (part->pdy == 0)
  {
    p1 = (ty1 < part->psy) ? +1 : -1;
    p2 = (ty2 < part->psy) ? +1 : -1;

    if (part->pdx < 0)
    {
      p1 = -p1;
      p2 = -p2;
    }
  }
  // now handle the cases of positive and negative slope
  else if (part->pdx * part->pdy > 0)
  {
    p1 = PointOnLineSide(part, tx1, ty2);
    p2 = PointOnLineSide(part, tx2, ty1);
  }
  else // NEGATIVE
  {
    p1 = PointOnLineSide(part, tx1, ty1);
    p2 = PointOnLineSide(part, tx2, ty2);
  }

  // line goes through or touches the box?
  if (p1 != p2)
  {
    return 0;
  }

  return p1;
}

seg_t *CreateOneSeg(linedef_t *line, vertex_t *start, vertex_t *end, sidedef_t *side, int what_side)
{
  seg_t *seg = NewSeg();

  // check for bad sidedef
  if (side->sector == nullptr)
  {
    PrintLine(LOG_NORMAL, "WARNING: Bad sidedef on linedef #%zu (Z_CheckHeap error)", line->index);
    config.total_warnings++;
  }

  // handle overlapping vertices, pick a nominal one
  if (start->overlap)
  {
    start = start->overlap;
  }
  if (end->overlap)
  {
    end = end->overlap;
  }

  seg->start = start;
  seg->end = end;
  seg->linedef = line;
  seg->side = what_side;
  seg->partner = nullptr;

  seg->source_line = seg->linedef;
  seg->index = NO_INDEX;

  Recompute(seg);

  return seg;
}

//
// Initially create all segs, one for each linedef.
// Must be called *after* InitBlockmap().
//
seg_t *CreateSegs(void)
{
  seg_t *list = nullptr;

  for (auto &line : lev_linedefs)
  {

    seg_t *left = nullptr;
    seg_t *right = nullptr;

    // ignore zero-length lines
    if (line.zero_len)
    {
      continue;
    }

    // ignore overlapping lines
    if (line.overlap != nullptr)
    {
      continue;
    }

    // -Elf- ZokumBSP
    if (line.dont_render)
    {
      continue;
    }

    // check for extremely long lines
    if (hypot(line.start->x - line.end->x, line.start->y - line.end->y) >= 32000)
    {
      PrintLine(LOG_NORMAL, "WARNING: Linedef #%zu is VERY long, it may cause problems", line.index);
      config.total_warnings++;
    }

    if (line.right != nullptr)
    {
      right = CreateOneSeg(&line, line.start, line.end, line.right, 0);
      ListAddSeg(&list, right);
    }
    else
    {
      PrintLine(LOG_NORMAL, "WARNING: Linedef #%zu has no right sidedef!", line.index);
      config.total_warnings++;
    }

    if (line.left != nullptr)
    {
      left = CreateOneSeg(&line, line.end, line.start, line.left, 1);
      ListAddSeg(&list, left);

      if (right != nullptr)
      {
        // -AJA- Partner segs.  These always maintain a one-to-one
        //       correspondence, so if one of the gets split, the
        //       other one must be split too.

        left->partner = right;
        right->partner = left;
      }
    }
    else
    {
      if (line.two_sided)
      {
        PrintLine(LOG_NORMAL, "WARNING: Linedef #%zu is 2s but has no left sidedef", line.index);
        config.total_warnings++;
        line.two_sided = false;
      }
    }
  }

  return list;
}

quadtree_c *TreeFromSegList(seg_t *list, const bbox_t *bounds)
{
  quadtree_c *tree = new quadtree_c(bounds->minx, bounds->miny, bounds->maxx, bounds->maxy);
  AddList(tree, list);
  return tree;
}

void DetermineMiddle(subsec_t *subsec)
{
  subsec->mid_x = 0;
  subsec->mid_y = 0;

  int total = 0;

  // compute middle coordinates
  for (seg_t *seg = subsec->seg_list; seg; seg = seg->next)
  {
    subsec->mid_x += seg->start->x + seg->end->x;
    subsec->mid_y += seg->start->y + seg->end->y;

    total += 2;
  }

  if (total > 0)
  {
    subsec->mid_x /= total;
    subsec->mid_y /= total;
  }
}

void AddToTail(subsec_t *subsec, seg_t *seg)
{
  seg->next = nullptr;

  if (subsec->seg_list == nullptr)
  {
    subsec->seg_list = seg;
    return;
  }

  seg_t *tail = subsec->seg_list;
  while (tail->next != nullptr)
  {
    tail = tail->next;
  }

  tail->next = seg;
}

void ClockwiseOrder(subsec_t *subsec)
{
  seg_t *seg;

  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] Clockwising %zu", __func__, subsec->index);
  }

  std::vector<seg_t *> array;

  for (seg = subsec->seg_list; seg; seg = seg->next)
  {
    // compute angles now
    seg->cmp_angle = ComputeAngle(seg->start->x - subsec->mid_x, seg->start->y - subsec->mid_y);
    array.push_back(seg);
  }

  // sort segs by angle (from the middle point to the start vertex).
  // the desired order (clockwise) means descending angles.
  // since # of segs is usually small, a bubble sort is fast enough.

  size_t i = 0;

  while (i + 1 < array.size())
  {
    seg_t *A = array[i];
    seg_t *B = array[i + 1];

    if (A->cmp_angle < B->cmp_angle)
    {
      // swap 'em
      array[i] = B;
      array[i + 1] = A;

      // bubble down
      if (i > 0)
      {
        i--;
      }
    }
    else
    {
      // bubble up
      i++;
    }
  }

  // choose the seg that will be first (the game engine will typically use
  // that to determine the sector).  In particular, we don't like self
  // referencing linedefs (they are often used for deep-water effects).
  size_t first = 0;
  int score = -1;

  for (size_t j = 0; i < array.size(); i++)
  {
    int cur_score = 3;

    if (!array[j]->linedef)
    {
      cur_score = 0;
    }
    else if (array[j]->linedef->self_ref)
    {
      cur_score = 2;
    }

    if (cur_score > score)
    {
      first = j;
      score = cur_score;
    }
  }

  // transfer sorted array back into sub
  subsec->seg_list = nullptr;

  for (size_t j = 0; j < array.size(); j++)
  {
    size_t k = (first + j) % array.size();
    AddToTail(subsec, array[k]);
  }

  if (HAS_BIT(config.debug, DEBUG_SORTER))
  {
    PrintLine(LOG_DEBUG, "[%s] Sorted SEGS around (%1.1f,%1.1f)", __func__, subsec->mid_x, subsec->mid_y);

    for (seg = subsec->seg_list; seg; seg = seg->next)
    {
      PrintLine(LOG_DEBUG, "[%s] Seg %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, seg, seg->cmp_angle,
                seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }
}

void SanityCheckClosed(subsec_t *subsec)
{
  int gaps = 0;
  int total = 0;

  seg_t *seg, *next;

  for (seg = subsec->seg_list; seg; seg = seg->next)
  {
    next = seg->next ? seg->next : subsec->seg_list;

    double dx = seg->end->x - next->start->x;
    double dy = seg->end->y - next->start->y;

    if (fabs(dx) > DIST_EPSILON || fabs(dy) > DIST_EPSILON)
    {
      gaps++;
    }

    total++;
  }

  if (gaps > 0)
  {
    if (config.verbose)
    {
      PrintLine(LOG_WARN, "MINOR ISSUE: Subsector #%zu near (%1.1f,%1.1f) is not closed (%d gaps, %d segs)", subsec->index,
                subsec->mid_x, subsec->mid_y, gaps, total);
    }

    if (HAS_BIT(config.debug, DEBUG_SUBSEC))
    {
      for (seg = subsec->seg_list; seg; seg = seg->next)
      {
        PrintLine(LOG_DEBUG, "[%s] SEG %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)", __func__, seg, seg->start->x, seg->start->y,
                  seg->end->x, seg->end->y);
      }
    }
  }
}

void SanityCheckHasRealSeg(subsec_t *subsec)
{
  for (seg_t *seg = subsec->seg_list; seg; seg = seg->next)
  {
    if (seg->linedef != nullptr)
    {
      return;
    }
  }

  PrintLine(LOG_ERROR, "Subsector #%zu near (%1.1f,%1.1f) has no real seg!", subsec->index, subsec->mid_x, subsec->mid_y);
}

void RenumberSegs(subsec_t *subsec, size_t &cur_seg_index)
{
  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] Renumbering %zu", __func__, subsec->index);
  }

  subsec->seg_count = 0;

  for (seg_t *seg = subsec->seg_list; seg; seg = seg->next)
  {
    seg->index = cur_seg_index;
    cur_seg_index += 1;

    subsec->seg_count++;

    if (HAS_BIT(config.debug, DEBUG_SUBSEC))
    {
      PrintLine(LOG_DEBUG, "[%s]   %zu: Seg %p  Index %zu", __func__, subsec->seg_count, seg, seg->index);
    }
  }
}

//
// Create a subsector from a list of segs.
//
subsec_t *CreateSubsec(quadtree_c *tree)
{
  subsec_t *sub = NewSubsec();

  // compute subsector's index
  sub->index = lev_subsecs.size() - 1;

  // copy segs into subsector
  sub->seg_list = nullptr;
  ConvertToList(tree, &sub->seg_list);
  DetermineMiddle(sub);

  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] Creating %zu", __func__, sub->index);
  }

  return sub;
}

int ComputeBspHeight(const node_t *node)
{
  if (node == nullptr)
  {
    return 1;
  }

  int right = ComputeBspHeight(node->r.node);
  int left = ComputeBspHeight(node->l.node);

  return std::max(left, right) + 1;
}

build_result_e BuildNodes(seg_t *list, int depth, bbox_t *bounds, node_t **N, subsec_t **S, double split_cost, bool fast,
                          bool analysis)
{
  *N = nullptr;
  *S = nullptr;

  if (HAS_BIT(config.debug, DEBUG_BUILDER))
  {
    PrintLine(LOG_DEBUG, "[%s] BEGUN @ %d", __func__, depth);
    for (const seg_t *seg = list; seg; seg = seg->next)
    {
      PrintLine(LOG_DEBUG, "[%s]   %sSEG %p  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, seg->linedef ? "" : "MINI", seg,
                seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  // determine bounds of segs
  FindLimits2(list, bounds);

  quadtree_c *tree = TreeFromSegList(list, bounds);

  /* pick partition line, NONE indicates convexicity */
  seg_t *part = PickNode(tree, depth, split_cost, fast);

  if (part == nullptr)
  {
    if (HAS_BIT(config.debug, DEBUG_BUILDER))
    {
      PrintLine(LOG_DEBUG, "[%s] CONVEX", __func__);
    }

    *S = CreateSubsec(tree);
    delete tree;

    return BUILD_OK;
  }

  if (HAS_BIT(config.debug, DEBUG_BUILDER))
  {
    PrintLine(LOG_DEBUG, "[%s] PARTITION %p (%1.0f,%1.0f) -> (%1.0f,%1.0f)", __func__, part, part->start->x, part->start->y,
              part->end->x, part->end->y);
  }

  node_t *node = NewNode();
  *N = node;

  /* divide the segs into two lists: left & right */
  seg_t *lefts = nullptr;
  seg_t *rights = nullptr;
  intersection_t *cut_list = nullptr;

  SeparateSegs(tree, part, &lefts, &rights, &cut_list);

  delete tree;
  tree = nullptr;

  /* sanity checks... */
  if (rights == nullptr)
  {
    PrintLine(LOG_ERROR, "Separated seg-list has empty RIGHT side");
  }

  if (lefts == nullptr)
  {
    PrintLine(LOG_ERROR, "Separated seg-list has empty LEFT side");
  }

  if (cut_list != nullptr)
  {
    AddMinisegs(cut_list, part, &lefts, &rights);
  }

  SetPartition(node, part);

  if (HAS_BIT(config.debug, DEBUG_BUILDER))
  {
    PrintLine(LOG_DEBUG, "[%s] Going LEFT", __func__);
  }

  build_result_e ret;

  // recursively build the left side
  ret = BuildNodes(lefts, depth + 1, &node->l.bounds, &node->l.node, &node->l.subsec, split_cost, fast, analysis);
  if (ret != BUILD_OK)
  {
    return ret;
  }

  if (HAS_BIT(config.debug, DEBUG_BUILDER))
  {
    PrintLine(LOG_DEBUG, "[%s] Going RIGHT", __func__);
  }

  // recursively build the right side
  ret = BuildNodes(rights, depth + 1, &node->r.bounds, &node->r.node, &node->r.subsec, split_cost, fast, analysis);
  if (ret != BUILD_OK)
  {
    return ret;
  }

  if (HAS_BIT(config.debug, DEBUG_BUILDER))
  {
    PrintLine(LOG_DEBUG, "[%s] DONE", __func__);
  }

  return BUILD_OK;
}

void ClockwiseBspTree(void)
{
  size_t cur_seg_index = 0;

  for (auto &sub : lev_subsecs)
  {
    ClockwiseOrder(&sub);
    RenumberSegs(&sub, cur_seg_index);

    // do some sanity checks
    SanityCheckClosed(&sub);
    SanityCheckHasRealSeg(&sub);
  }
}

void Normalise(subsec_t *subsec)
{
  // use head + tail to maintain same order of segs
  seg_t *new_head = nullptr;
  seg_t *new_tail = nullptr;

  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] Normalising %zu", __func__, subsec->index);
  }

  while (subsec->seg_list)
  {
    // remove head
    seg_t *seg = subsec->seg_list;
    subsec->seg_list = seg->next;

    // filter out minisegs
    if (seg->linedef == nullptr)
    {
      if (HAS_BIT(config.debug, DEBUG_SUBSEC))
      {
        PrintLine(LOG_DEBUG, "[%s] Removing miniseg %p", __func__, seg);
      }

      // this causes SortSegs() to remove the seg
      seg->index = SEG_IS_GARBAGE;
      continue;
    }

    // add it to the new list
    seg->next = nullptr;

    if (new_tail)
    {
      new_tail->next = seg;
    }
    else
    {
      new_head = seg;
    }

    new_tail = seg;

    // this updated later
    seg->index = NO_INDEX;
  }

  if (new_head == nullptr)
  {
    PrintLine(LOG_ERROR, "Subsector %zu normalised to being EMPTY", subsec->index);
  }

  subsec->seg_list = new_head;
}

void NormaliseBspTree(void)
{
  // unlinks all minisegs from each subsector
  size_t cur_seg_index = 0;

  for (auto &sub : lev_subsecs)
  {
    Normalise(&sub);
    RenumberSegs(&sub, cur_seg_index);
  }
}

void RoundOffVertices(void)
{
  for (auto &vert : lev_vertices)
  {
    if (vert.is_new)
    {
      vert.is_new = false;
      vert.index = num_old_vert;
      num_old_vert++;
    }
  }
}

void RoundOff(subsec_t *subsec)
{
  // use head + tail to maintain same order of segs
  seg_t *new_head = nullptr;
  seg_t *new_tail = nullptr;

  seg_t *seg;
  seg_t *last_real_degen = nullptr;

  int real_total = 0;

  int degen_total = 0;
  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] Rounding off %zu", __func__, subsec->index);
  }

  // do an initial pass, just counting the degenerates
  for (seg = subsec->seg_list; seg; seg = seg->next)
  {
    // is the seg degenerate ?
    if (static_cast<int32_t>(floor(seg->start->x)) == static_cast<int32_t>(floor(seg->end->x))
        && static_cast<int32_t>(floor(seg->start->y)) == static_cast<int32_t>(floor(seg->end->y)))
    {
      seg->is_degenerate = true;

      if (seg->linedef != nullptr)
      {
        last_real_degen = seg;
      }

      if (HAS_BIT(config.debug, DEBUG_SUBSEC))
      {
        degen_total++;
      }

      continue;
    }

    if (seg->linedef != nullptr)
    {
      real_total++;
    }
  }

  if (HAS_BIT(config.debug, DEBUG_SUBSEC))
  {
    PrintLine(LOG_DEBUG, "[%s] degen=%d real=%d", __func__, degen_total, real_total);
  }

  // handle the (hopefully rare) case where all of the real segs
  // became degenerate.
  if (real_total == 0)
  {
    if (last_real_degen == nullptr)
    {
      PrintLine(LOG_ERROR, "Subsector %zu rounded off with NO real segs", subsec->index);
    }

    if (HAS_BIT(config.debug, DEBUG_SUBSEC))
    {
      PrintLine(LOG_DEBUG, "[%s] Degenerate before: (%1.2f,%1.2f) -> (%1.2f,%1.2f)", __func__, last_real_degen->start->x,
                last_real_degen->start->y, last_real_degen->end->x, last_real_degen->end->y);
    }

    // create a new vertex for this baby
    last_real_degen->end = NewVertexDegenerate(last_real_degen->start, last_real_degen->end);

    if (HAS_BIT(config.debug, DEBUG_SUBSEC))
    {
      PrintLine(LOG_DEBUG, "[%s] Degenerate after:  (%d,%d) -> (%d,%d)", __func__,
                static_cast<int32_t>(floor(last_real_degen->start->x)), static_cast<int32_t>(floor(last_real_degen->start->y)),
                static_cast<int32_t>(floor(last_real_degen->end->x)), static_cast<int32_t>(floor(last_real_degen->end->y)));
    }

    last_real_degen->is_degenerate = false;
  }

  // second pass, remove the blighters...
  while (subsec->seg_list != nullptr)
  {
    // remove head
    seg = subsec->seg_list;
    subsec->seg_list = seg->next;

    if (seg->is_degenerate)
    {
      if (HAS_BIT(config.debug, DEBUG_SUBSEC))
      {
        PrintLine(LOG_DEBUG, "[%s] Removing degenerate %p", __func__, seg);
      }
      // this causes SortSegs() to remove the seg
      seg->index = SEG_IS_GARBAGE;
      continue;
    }

    // add it to new list
    seg->next = nullptr;

    if (new_tail)
    {
      new_tail->next = seg;
    }
    else
    {
      new_head = seg;
    }

    new_tail = seg;

    // this updated later
    seg->index = NO_INDEX;
  }

  if (new_head == nullptr)
  {
    PrintLine(LOG_ERROR, "Subsector %zu rounded off to being EMPTY", subsec->index);
  }

  subsec->seg_list = new_head;
}

void RoundOffBspTree(void)
{
  size_t cur_seg_index = 0;

  RoundOffVertices();

  for (auto &sub : lev_subsecs)
  {
    RoundOff(&sub);
    RenumberSegs(&sub, cur_seg_index);
  }
}
