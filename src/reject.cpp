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
static void Reject_Init(level_t &level)
{
  level.reject_size = (level.sectors.size() * level.sectors.size() + 7) / 8;

  level.reject_matrix = new uint8_t[level.reject_size];
  memset(level.reject_matrix, 0, level.reject_size);

  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    sector_t *sec = level.sectors[i];

    sec->rej_group = i;
    sec->rej_next = sec->rej_prev = sec;
  }
}

static void Reject_WriteLump(level_t &level)
{
  Lump_c *lump = CreateLevelLump(level, "REJECT", level.reject_size);
  lump->Write(level.reject_matrix, level.reject_size);
  lump->Finish();
}

static void Reject_Free(level_t &level)
{
  delete[] level.reject_matrix;
  level.reject_matrix = nullptr;
}

//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static void Reject_GroupSectors(level_t &level)
{
  for (size_t i = 0; i < level.linedefs.size(); i++)
  {
    const linedef_t *line = level.linedefs[i];

    // must be valid two-sided line
    if (!line->right || !line->left)
    {
      continue;
    }

    sector_t *sec1 = line->right->sector;
    sector_t *sec2 = line->left->sector;
    sector_t *tmp;

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

static void Reject_ProcessSectors(level_t &level)
{
  for (size_t view = 0; view < level.sectors.size(); view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      sector_t *view_sec = level.sectors[view];
      sector_t *targ_sec = level.sectors[target];

      if (view_sec->rej_group == targ_sec->rej_group)
      {
        continue;
      }

      // for symmetry, do both sides at same time

      size_t p1 = view * level.sectors.size() + target;
      size_t p2 = target * level.sectors.size() + view;

      level.reject_matrix[p1 >> 3] |= (1 << (p1 & 7));
      level.reject_matrix[p2 >> 3] |= (1 << (p2 & 7));
    }
  }
}

// Note: this routine is destructive to the group numbers
static void Reject_DebugGroups(level_t &level)
{
  if (HAS_NONE(config.debug, DEBUG_REJECT)) return;

  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    sector_t *sec = level.sectors[i];
    size_t group = sec->rej_group;
    size_t num = 0;

    if (group == NO_INDEX) continue;

    sec->rej_group = NO_INDEX;
    num++;

    for (sector_t *tmp = sec->rej_next; tmp != sec; tmp = tmp->rej_next)
    {
      tmp->rej_group = NO_INDEX;
      num++;
    }

    PrintLine(LOG_NORMAL, "[%s] Group %zu  Sectors %zu", __func__, group, num);
  }
}

//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject(level_t &level)
{
  auto mark = Benchmarker(__func__);
  if (level.sectors.size() == 0)
  {
    // just create an empty reject lump
    CreateLevelLump(level, "REJECT")->Finish();
    return;
  }

  Reject_Init(level);
  Reject_GroupSectors(level);
  Reject_ProcessSectors(level);
  Reject_DebugGroups(level);
  Reject_WriteLump(level);
  Reject_Free(level);
  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Reject size: %zu", level.reject_size);
  }
}
