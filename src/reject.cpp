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
#include "wad.hpp"

//-----------------------------------------------------------------------------
// REJECT : Generate the reject matrix
//-----------------------------------------------------------------------------

static uint8_t *rej_matrix;
static size_t rej_total_size; // in bytes
static std::vector<size_t> rej_sector_groups;

//
// Allocate the matrix, init sectors into individual groups.
//
static inline void Reject_Init(void)
{
  rej_total_size = (lev_sectors.size() * lev_sectors.size() + 7) / 8;

  rej_matrix = new uint8_t[rej_total_size];
  memset(rej_matrix, 0, rej_total_size);

  rej_sector_groups.resize(lev_sectors.size());

  for (size_t i = 0; i < lev_sectors.size(); i++)
  {
    rej_sector_groups[i] = i;
  }
}

static inline void Reject_Free(void)
{
  delete[] rej_matrix;
  rej_matrix = nullptr;
  rej_sector_groups.clear();
}

//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static inline void Reject_GroupSectors(void)
{
  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *line = lev_linedefs[i];

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
    size_t group1 = rej_sector_groups[sec1->index];
    size_t group2 = rej_sector_groups[sec2->index];

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
    for (size_t s = 0; s < lev_sectors.size(); s++)
    {
      if (rej_sector_groups[s] == group2)
      {
        rej_sector_groups[s] = group1;
      }
    }
  }
}

static inline void Reject_ProcessSectors(void)
{
  for (size_t view = 0; view < lev_sectors.size(); view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      if (rej_sector_groups[view] == rej_sector_groups[target])
      {
        continue;
      }

      // for symmetry, do both sides at same time

      size_t p1 = view * lev_sectors.size() + target;
      size_t p2 = target * lev_sectors.size() + view;

      rej_matrix[p1 >> 3] |= (1 << (p1 & 7));
      rej_matrix[p2 >> 3] |= (1 << (p2 & 7));
    }
  }
}

static inline void Reject_WriteLump(void)
{
  Lump_c *lump = CreateLevelLump("REJECT", rej_total_size);
  lump->Write(rej_matrix, rej_total_size);
  lump->Finish();
}

//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject(void)
{
  if (config.no_reject || lev_sectors.size() == 0)
  {
    // just create an empty reject lump
    CreateLevelLump("REJECT")->Finish();
    return;
  }

  Reject_Init();
  Reject_GroupSectors();
  Reject_ProcessSectors();
  Reject_WriteLump();
  Reject_Free();
  config.Print_Verbose("    Reject size: %zu\n", rej_total_size);
}
