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

  level.reject_groups.resize(level.sectors.size());

  for (size_t i = 0; i < level.sectors.size(); i++)
  {
    level.reject_groups[i] = i;
  }
}

static void Reject_Free(level_t &level)
{
  delete[] level.reject_matrix;
  level.reject_matrix = nullptr;
  level.reject_groups.clear();
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

    if (!line->right || !line->left)
    {
      continue;
    }

    sector_t *sec1 = line->right->sector;
    sector_t *sec2 = line->left->sector;

    if (!sec1 || !sec2 || sec1 == sec2)
    {
      continue;
    }

    // already in the same group ?
    size_t group1 = level.reject_groups[sec1->index];
    size_t group2 = level.reject_groups[sec2->index];

    if (group1 == group2)
    {
      continue;
    }

    // prefer the group numbers to become lower
    if (group1 > group2)
    {
      std::swap(group1, group2);
    }

    // merge the groups
    for (size_t s = 0; s < level.sectors.size(); s++)
    {
      if (level.reject_groups[s] == group2)
      {
        level.reject_groups[s] = group1;
      }
    }
  }
}

static void Reject_ProcessSectors(level_t &level)
{
  for (size_t view = 0; view < level.sectors.size(); view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      if (level.reject_groups[view] == level.reject_groups[target])
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

static void Reject_WriteLump(level_t &level)
{
  Lump_c *lump = CreateLevelLump(level, "REJECT", level.reject_size);
  lump->Write(level.reject_matrix, level.reject_size);
  lump->Finish();
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
  Reject_WriteLump(level);
  Reject_Free(level);
  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Reject size: %zu", level.reject_size);
  }
}
