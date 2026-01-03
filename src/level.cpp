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
#include "parse.hpp"
#include "wad.hpp"

static constexpr std::uint32_t DUMMY_DUP = 0xFFFF;

Wad_file *cur_wad;

static int block_x, block_y;
static int block_w, block_h;
static size_t block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static uint16_t **block_lines;

static uint16_t *block_ptrs;
static uint16_t *block_dups;

static int32_t block_compression;
static bool block_overflowed;

void GetBlockmapBounds(int *x, int *y, int *w, int *h)
{
  *x = block_x;
  *y = block_y;
  *w = block_w;
  *h = block_h;
}

int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax, int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax)
      {
        return false;
      }

      x1 = x1 + (int)((x2 - x1) * (double)(ymax - y1) / (double)(y2 - y1));
      y1 = ymax;

      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin)
      {
        return false;
      }

      x1 = x1 + (int)((x2 - x1) * (double)(ymin - y1) / (double)(y2 - y1));
      y1 = ymin;

      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax)
      {
        return false;
      }

      y1 = y1 + (int)((y2 - y1) * (double)(xmax - x1) / (double)(x2 - x1));
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin)
      {
        return false;
      }

      y1 = y1 + (int)((y2 - y1) * (double)(xmin - x1) / (double)(x2 - x1));
      x1 = xmin;

      count = 2;
      continue;
    }

    count--;

    if (count == 0)
    {
      break;
    }

    // swap end points
    tmp = x1;
    x1 = x2;
    x2 = tmp;
    tmp = y1;
    y1 = y2;
    y2 = tmp;
  }

  // linedef touches block
  return true;
}

/* ----- create blockmap ------------------------------------ */

static constexpr std::uint32_t BK_NUM = 0;
static constexpr std::uint32_t BK_MAX = 1;
static constexpr std::uint32_t BK_XOR = 2;
static constexpr std::uint32_t BK_FIRST = 3;
static constexpr std::uint32_t BK_QUANTUM = 32;

static void BlockAdd(size_t blk_num, size_t line_index)
{
  uint16_t *cur = block_lines[blk_num];

  if constexpr (DEBUG_BLOCKMAP)
  {
    Debug("Block %d has line %d\n", blk_num, line_index);
  }

  if (blk_num >= block_count)
  {
    FatalError("BlockAdd: bad block number %zu\n", blk_num);
  }

  if (!cur)
  {
    // create empty block
    block_lines[blk_num] = cur = UtilCalloc<uint16_t>(BK_QUANTUM * sizeof(uint16_t));
    cur[BK_NUM] = 0;
    cur[BK_MAX] = BK_QUANTUM;
    cur[BK_XOR] = 0x1234;
  }

  if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
  {
    // no more room, so allocate some more...
    cur[BK_MAX] += BK_QUANTUM;

    block_lines[blk_num] = cur = (uint16_t *)UtilRealloc(cur, cur[BK_MAX] * sizeof(uint16_t));
  }

  // compute new checksum
  cur[BK_XOR] = (((cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12)) ^ line_index);

  cur[BK_FIRST + cur[BK_NUM]] = GetLittleEndian((uint16_t)line_index);
  cur[BK_NUM]++;
}

static void BlockAddLine(const linedef_t *L)
{
  int x1 = (int)L->start->x;
  int y1 = (int)L->start->y;
  int x2 = (int)L->end->x;
  int y2 = (int)L->end->y;

  size_t line_index = L->index;

  if constexpr (DEBUG_BLOCKMAP)
  {
    Debug("BlockAddLine: %d (%d,%d) -> (%d,%d)\n", line_index, x1, y1, x2, y2);
  }

  int bx1_temp = (std::min(x1, x2) - block_x) / 128;
  int by1_temp = (std::min(y1, y2) - block_y) / 128;
  int bx2_temp = (std::max(x1, x2) - block_x) / 128;
  int by2_temp = (std::max(y1, y2) - block_y) / 128;

  // handle truncated blockmaps
  size_t bx1 = (size_t)std::max(bx1_temp, 0);
  size_t by1 = (size_t)std::max(by1_temp, 0);
  size_t bx2 = (size_t)std::min(bx2_temp, block_w - 1);
  size_t by2 = (size_t)std::min(by2_temp, block_h - 1);

  if (bx2 < bx1 || by2 < by1)
  {
    return;
  }

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = by1 * block_w + bx;
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = by * block_w + bx1;
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = bx + by * block_w;

      int minx = block_x + 128 * bx;
      int miny = block_y + 128 * by;
      int maxx = minx + 127;
      int maxy = miny + 127;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        BlockAdd(blk_num, line_index);
      }
    }
  }
}

static void CreateBlockmap(void)
{
  extern map_format_e lev_format;

  block_lines = UtilCalloc<uint16_t *>(block_count * sizeof(uint16_t *));

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    // ignore zero-length lines
    if (L->zero_len)
    {
      continue;
    }

    if (lev_format == MAPF_Doom && L->special == Special_NoBlockmap)
    {
      continue;
    }

    BlockAddLine(L);
  }
}

static int BlockCompare(const void *p1, const void *p2)
{
  int blk_num1 = ((const uint16_t *)p1)[0];
  int blk_num2 = ((const uint16_t *)p2)[0];

  const uint16_t *A = block_lines[blk_num1];
  const uint16_t *B = block_lines[blk_num2];

  if (A == B)
  {
    return 0;
  }
  if (A == nullptr)
  {
    return -1;
  }
  if (B == nullptr)
  {
    return +1;
  }

  if (A[BK_NUM] != B[BK_NUM])
  {
    return A[BK_NUM] - B[BK_NUM];
  }

  if (A[BK_XOR] != B[BK_XOR])
  {
    return A[BK_XOR] - B[BK_XOR];
  }

  return memcmp(A + BK_FIRST, B + BK_FIRST, A[BK_NUM] * sizeof(uint16_t));
}

static void CompressBlockmap(void)
{
  size_t cur_offset;
  size_t dup_count = 0;

  size_t orig_size, new_size;

  block_ptrs = UtilCalloc<uint16_t>(block_count * sizeof(uint16_t));
  block_dups = UtilCalloc<uint16_t>(block_count * sizeof(uint16_t));

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  for (size_t i = 0; i < block_count; i++)
  {
    block_dups[i] = (uint16_t)i;
  }

  qsort(block_dups, block_count, sizeof(uint16_t), BlockCompare);

  // scan duplicate array and build up offset array
  cur_offset = 4 + block_count + 2;
  orig_size = 4 + block_count;
  new_size = cur_offset;

  for (size_t i = 0; i < block_count; i++)
  {
    size_t blk_num = block_dups[i];

    // empty block ?
    if (block_lines[blk_num] == nullptr)
    {
      block_ptrs[blk_num] = (uint16_t)(4 + block_count);
      block_dups[i] = DUMMY_DUP;

      orig_size += 2;
      continue;
    }

    size_t count = 2 + block_lines[blk_num][BK_NUM];

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.
    if (i + 1 < block_count && BlockCompare(block_dups + i, block_dups + i + 1) == 0)
    {
      block_ptrs[blk_num] = (uint16_t)cur_offset;
      block_dups[i] = DUMMY_DUP;

      // free the memory of the duplicated block
      UtilFree(block_lines[blk_num]);
      block_lines[blk_num] = nullptr;

      if constexpr (DEBUG_BLOCKMAP)
      {
        dup_count++;
      }

      orig_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    block_ptrs[blk_num] = (uint16_t)cur_offset;
    cur_offset += count;
    orig_size += count;
    new_size += count;
  }

  if (cur_offset > 65535)
  {
    block_overflowed = true;
    return;
  }

  if constexpr (DEBUG_BLOCKMAP)
  {
    Debug("Blockmap: Last ptr = %d  duplicates = %d\n", cur_offset, dup_count);
  }

  block_compression = int32_t((orig_size - new_size) * 100 / orig_size);

  // there's a tiny chance of new_size > orig_size
  if (block_compression < 0)
  {
    block_compression = 0;
  }
}

static size_t CalcBlockmapSize(void)
{
  // compute size of final BLOCKMAP lump.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  // header + null_block + a bit extra
  size_t size = 20;

  // the pointers (offsets to the line lists)
  size = size + block_count * 2;

  // add size of each block
  for (size_t i = 0; i < block_count; i++)
  {
    size_t blk_num = block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = block_lines[blk_num];
    SYS_ASSERT(blk);

    size += ((blk[BK_NUM]) + 1 + 1) * 2;
  }

  return size;
}

static void WriteBlockmap(void)
{
  size_t max_size = CalcBlockmapSize();
  Lump_c *lump = CreateLevelLump("BLOCKMAP", max_size);

  uint16_t null_block[2] = {0x0000, 0xFFFF};
  uint16_t m_zero = 0x0000;
  uint16_t m_neg1 = 0xFFFF;

  // fill in header
  raw_blockmap_header_t header;

  header.x_origin = GetLittleEndian((int16_t)block_x);
  header.y_origin = GetLittleEndian((int16_t)block_y);
  header.x_blocks = GetLittleEndian((int16_t)block_w);
  header.y_blocks = GetLittleEndian((int16_t)block_h);

  lump->Write(&header, sizeof(header));

  // handle pointers
  for (size_t i = 0; i < block_count; i++)
  {
    uint16_t ptr = GetLittleEndian(block_ptrs[i]);

    if (ptr == 0)
    {
      FatalError("WriteBlockmap: offset %zu not set.\n", i);
    }

    lump->Write(&ptr, sizeof(uint16_t));
  }

  // add the null block which *all* empty blocks will use
  lump->Write(null_block, sizeof(null_block));

  // handle each block list
  for (size_t i = 0; i < block_count; i++)
  {
    int blk_num = block_dups[i];

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
    {
      continue;
    }

    uint16_t *blk = block_lines[blk_num];
    SYS_ASSERT(blk);

    lump->Write(&m_zero, sizeof(uint16_t));
    lump->Write(blk + BK_FIRST, blk[BK_NUM] * sizeof(uint16_t));
    lump->Write(&m_neg1, sizeof(uint16_t));
  }

  lump->Finish();
}

static void FreeBlockmap(void)
{
  for (size_t i = 0; i < block_count; i++)
  {
    if (block_lines[i])
    {
      UtilFree(block_lines[i]);
    }
  }

  UtilFree(block_lines);
  UtilFree(block_ptrs);
  UtilFree(block_dups);
}

static void FindBlockmapLimits(bbox_t *bbox)
{
  extern map_format_e lev_format;
  double mid_x = 0;
  double mid_y = 0;

  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    if (lev_format == MAPF_Doom && L->special == Special_NoBlockmap)
    {
      continue;
    }

    if (!L->zero_len)
    {
      double x1 = L->start->x;
      double y1 = L->start->y;
      double x2 = L->end->x;
      double y2 = L->end->y;

      int lx = (int)floor(std::min(x1, x2));
      int ly = (int)floor(std::min(y1, y2));
      int hx = (int)ceil(std::max(x1, x2));
      int hy = (int)ceil(std::max(y1, y2));

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

      // compute middle of cluster
      mid_x += (lx + hx) >> 1;
      mid_y += (ly + hy) >> 1;
    }
  }

  if (lev_linedefs.size() > 0)
  {
    block_mid_x = (int32_t)floor(mid_x / (double)lev_linedefs.size());
    block_mid_y = (int32_t)floor(mid_y / (double)lev_linedefs.size());
  }

  if constexpr (DEBUG_BLOCKMAP)
  {
    Debug("Blockmap lines centered at (%d,%d)\n", block_mid_x, block_mid_y);
  }
}

void InitBlockmap(void)
{
  bbox_t map_bbox;

  // find limits of linedefs, and store as map limits
  FindBlockmapLimits(&map_bbox);

  cur_info->Print_Verbose("    Map limits: (%d,%d) to (%d,%d)\n", map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);

  block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  block_w = ((map_bbox.maxx - block_x) / 128) + 1;
  block_h = ((map_bbox.maxy - block_y) / 128) + 1;

  block_count = block_w * block_h;
}

void PutBlockmap(void)
{
  if (!cur_info->do_blockmap || lev_linedefs.size() == 0)
  {
    // just create an empty blockmap lump
    CreateLevelLump("BLOCKMAP")->Finish();
    return;
  }

  block_overflowed = false;

  // initial phase: create internal blockmap containing the index of
  // all lines in each block.
  CreateBlockmap();

  // -AJA- second phase: compress the blockmap.  We do this by sorting
  //       the blocks, which is a typical way to detect duplicates in
  //       a large list.  This also detects BLOCKMAP overflow.
  CompressBlockmap();

  // final phase: write it out in the correct format
  if (block_overflowed)
  {
    // leave an empty blockmap lump
    CreateLevelLump("BLOCKMAP")->Finish();
    Warning("Blockmap overflowed (lump will be empty)\n");
  }
  else
  {
    WriteBlockmap();
    cur_info->Print_Verbose("    Blockmap size: %dx%d (compression: %d%%)\n", block_w, block_h, block_compression);
  }

  FreeBlockmap();
}

//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------

static uint8_t *rej_matrix;
static size_t rej_total_size; // in bytes

//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init(void)
{
  rej_total_size = (lev_sectors.size() * lev_sectors.size() + 7) / 8;

  rej_matrix = new uint8_t[rej_total_size];
  memset(rej_matrix, 0, rej_total_size);

  for (size_t i = 0; i < lev_sectors.size(); i++)
  {
    sector_t *sec = lev_sectors[i];

    sec->rej_group = i;
    sec->rej_next = sec->rej_prev = sec;
  }
}

static void Reject_Free(void)
{
  delete[] rej_matrix;
  rej_matrix = nullptr;
}

//
// Algorithm: Initially all sectors are in individual groups.
// Now we scan the linedef list.  For each two-sectored line,
// merge the two sector groups into one.  That's it !
//
static void Reject_GroupSectors(void)
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
    sector_t *tmp;

    if (!sec1 || !sec2 || sec1 == sec2)
    {
      continue;
    }

    // already in the same group ?
    if (sec1->rej_group == sec2->rej_group)
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

void Reject_DebugGroups(void)
{
  // Note: this routine is destructive to the group numbers
  for (size_t i = 0; i < lev_sectors.size(); i++)
  {
    sector_t *sec = lev_sectors[i];
    sector_t *tmp;

    size_t group = sec->rej_group;
    int num = 0;

    if (group == NO_INDEX)
    {
      continue;
    }

    sec->rej_group = NO_INDEX;
    num++;

    for (tmp = sec->rej_next; tmp != sec; tmp = tmp->rej_next)
    {
      tmp->rej_group = NO_INDEX;
      num++;
    }

    Debug("Group %d  Sectors %d\n", group, num);
  }
}

static void Reject_ProcessSectors(void)
{
  for (size_t view = 0; view < lev_sectors.size(); view++)
  {
    for (size_t target = 0; target < view; target++)
    {
      sector_t *view_sec = lev_sectors[view];
      sector_t *targ_sec = lev_sectors[target];

      if (view_sec->rej_group == targ_sec->rej_group)
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

static void Reject_WriteLump(void)
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
  if (!cur_info->do_reject || lev_sectors.size() == 0)
  {
    // just create an empty reject lump
    CreateLevelLump("REJECT")->Finish();
    return;
  }

  Reject_Init();
  Reject_GroupSectors();
  Reject_ProcessSectors();

  if constexpr (DEBUG_REJECT)
  {
    Reject_DebugGroups();
  }

  Reject_WriteLump();
  Reject_Free();
  cur_info->Print_Verbose("    Reject size: %d\n", rej_total_size);
}

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------

// Note: ZDoom format support based on code (C) 2002,2003 Randy Heit

// per-level variables

const char *lev_current_name;

size_t lev_current_idx;
size_t lev_current_start;

map_format_e lev_format;
bool lev_force_xnod;

bool lev_long_name;
bool lev_overflows;

// objects of loaded level, and stuff we've built
std::vector<vertex_t *> lev_vertices;
std::vector<linedef_t *> lev_linedefs;
std::vector<sidedef_t *> lev_sidedefs;
std::vector<sector_t *> lev_sectors;
std::vector<thing_t *> lev_things;

std::vector<seg_t *> lev_segs;
std::vector<subsec_t *> lev_subsecs;
std::vector<node_t *> lev_nodes;
std::vector<walltip_t *> lev_walltips;

size_t num_old_vert = 0;
size_t num_new_vert = 0;
size_t num_real_lines = 0;

/* ----- allocation routines ---------------------------- */

vertex_t *NewVertex(void)
{
  vertex_t *V = UtilCalloc<vertex_t>(sizeof(vertex_t));
  V->index = lev_vertices.size();
  lev_vertices.push_back(V);
  return V;
}

linedef_t *NewLinedef(void)
{
  linedef_t *L = UtilCalloc<linedef_t>(sizeof(linedef_t));
  L->index = lev_linedefs.size();
  lev_linedefs.push_back(L);
  return L;
}

sidedef_t *NewSidedef(void)
{
  sidedef_t *S = UtilCalloc<sidedef_t>(sizeof(sidedef_t));
  S->index = lev_sidedefs.size();
  lev_sidedefs.push_back(S);
  return S;
}

sector_t *NewSector(void)
{
  sector_t *S = UtilCalloc<sector_t>(sizeof(sector_t));
  S->index = lev_sectors.size();
  lev_sectors.push_back(S);
  return S;
}

thing_t *NewThing(void)
{
  thing_t *T = UtilCalloc<thing_t>(sizeof(thing_t));
  T->index = lev_things.size();
  lev_things.push_back(T);
  return T;
}

seg_t *NewSeg(void)
{
  seg_t *S = UtilCalloc<seg_t>(sizeof(seg_t));
  lev_segs.push_back(S);
  return S;
}

subsec_t *NewSubsec(void)
{
  subsec_t *S = UtilCalloc<subsec_t>(sizeof(subsec_t));
  lev_subsecs.push_back(S);
  return S;
}

node_t *NewNode(void)
{
  node_t *N = UtilCalloc<node_t>(sizeof(node_t));
  lev_nodes.push_back(N);
  return N;
}

walltip_t *NewWallTip(void)
{
  walltip_t *WT = UtilCalloc<walltip_t>(sizeof(walltip_t));
  lev_walltips.push_back(WT);
  return WT;
}

/* ----- free routines ---------------------------- */

void FreeVertices(void)
{
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    UtilFree(lev_vertices[i]);
  }

  lev_vertices.clear();
}

void FreeLinedefs(void)
{
  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    UtilFree(lev_linedefs[i]);
  }

  lev_linedefs.clear();
}

void FreeSidedefs(void)
{
  for (size_t i = 0; i < lev_sidedefs.size(); i++)
  {
    UtilFree(lev_sidedefs[i]);
  }

  lev_sidedefs.clear();
}

void FreeSectors(void)
{
  for (size_t i = 0; i < lev_sectors.size(); i++)
  {
    UtilFree(lev_sectors[i]);
  }

  lev_sectors.clear();
}

void FreeThings(void)
{
  for (size_t i = 0; i < lev_things.size(); i++)
  {
    UtilFree(lev_things[i]);
  }

  lev_things.clear();
}

void FreeSegs(void)
{
  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    UtilFree(lev_segs[i]);
  }

  lev_segs.clear();
}

void FreeSubsecs(void)
{
  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    UtilFree(lev_subsecs[i]);
  }

  lev_subsecs.clear();
}

void FreeNodes(void)
{
  for (size_t i = 0; i < lev_nodes.size(); i++)
  {
    UtilFree(lev_nodes[i]);
  }

  lev_nodes.clear();
}

void FreeWallTips(void)
{
  for (size_t i = 0; i < lev_walltips.size(); i++)
  {
    UtilFree(lev_walltips[i]);
  }

  lev_walltips.clear();
}

/* ----- reading routines ------------------------------ */

static vertex_t *SafeLookupVertex(size_t num)
{
  if (num >= lev_vertices.size())
  {
    FatalError("illegal vertex number #%zu\n", num);
  }

  return lev_vertices[num];
}

static sector_t *SafeLookupSector(uint16_t num)
{
  if (num >= NO_INDEX_INT16)
  {
    return nullptr;
  }

  if (num >= lev_sectors.size())
  {
    FatalError("illegal sector number #%d\n", (int)num);
  }

  return lev_sectors[num];
}

static inline sidedef_t *SafeLookupSidedef(uint16_t num)
{
  if (num >= NO_INDEX_INT16)
  {
    return nullptr;
  }

  // silently ignore illegal sidedef numbers
  if (num >= lev_sidedefs.size())
  {
    return nullptr;
  }

  return lev_sidedefs[num];
}

void GetVertices(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("VERTEXES");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_vertex_t);
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetVertices: num = %d\n", count);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to vertices.\n");
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_vertex_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading vertices.\n");
    }

    vertex_t *vert = NewVertex();

    vert->x = (double)GetLittleEndian(raw.x);
    vert->y = (double)GetLittleEndian(raw.y);
  }

  num_old_vert = lev_vertices.size();
}

void GetSectors(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("SECTORS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sector_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to sectors.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetSectors: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sector_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading sectors.\n");
    }

    sector_t *sector = NewSector();

    (void)sector;
  }
}

void GetThings(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_thing_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to things.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetThings: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading things.\n");
    }

    thing_t *thing = NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = GetLittleEndian(raw.type);
  }
}

void GetThingsHexen(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("THINGS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_hexen_thing_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to things.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetThingsHexen: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_hexen_thing_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading things.\n");
    }

    thing_t *thing = NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = GetLittleEndian(raw.type);
  }
}

void GetSidedefs(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("SIDEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_sidedef_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to sidedefs.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetSidedefs: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sidedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading sidedefs.\n");
    }

    sidedef_t *side = NewSidedef();

    side->sector = SafeLookupSector(GetLittleEndian(raw.sector));
  }
}

void GetLinedefs(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_linedef_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to linedefs.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetLinedefs: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading linedefs.\n");
    }

    linedef_t *line;

    vertex_t *start = SafeLookupVertex(GetLittleEndian(raw.start));
    vertex_t *end = SafeLookupVertex(GetLittleEndian(raw.end));

    start->is_used = true;
    end->is_used = true;

    line = NewLinedef();

    line->start = start;
    line->end = end;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && (fabs(start->y - end->y) < DIST_EPSILON);

    line->special = GetLittleEndian(raw.special);
    line->tag = GetLittleEndian(raw.tag);
    line->flags = GetLittleEndian(raw.flags);

    line->two_sided = (line->flags & MLF_TwoSided) != 0;
    line->is_precious = (line->tag >= 900 && line->tag < 1000);

    // The following three are only valid in MAPF_Doom
    line->dont_render = (line->special == Special_DoNotRender);

    line->dont_render_front = (line->special == Special_DoNotRenderFrontSeg || line->special == Special_DoNotRenderAnySeg);

    line->dont_render_back = (line->special == Special_DoNotRenderBackSeg || line->special == Special_DoNotRenderAnySeg);

    line->right = SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = SafeLookupSidedef(GetLittleEndian(raw.left));

    if (line->right || line->left)
    {
      num_real_lines++;
    }

    line->self_ref = (line->left && line->right && (line->left->sector == line->right->sector));
  }
}

void GetLinedefsHexen(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("LINEDEFS");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_hexen_linedef_t);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    FatalError("Error seeking to linedefs.\n");
  }

  if constexpr (DEBUG_LOAD)
  {
    Debug("GetLinedefsHexen: num = %d\n", count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_hexen_linedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      FatalError("Error reading linedefs.\n");
    }

    linedef_t *line;

    vertex_t *start = SafeLookupVertex(GetLittleEndian(raw.start));
    vertex_t *end = SafeLookupVertex(GetLittleEndian(raw.end));

    start->is_used = true;
    end->is_used = true;

    line = NewLinedef();

    line->start = start;
    line->end = end;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && (fabs(start->y - end->y) < DIST_EPSILON);

    line->special = (uint8_t)raw.special;
    uint16_t flags = GetLittleEndian(raw.flags);

    // -JL- Added missing twosided flag handling that caused a broken reject
    line->two_sided = (flags & MLF_TwoSided) != 0;

    line->right = SafeLookupSidedef(GetLittleEndian(raw.right));
    line->left = SafeLookupSidedef(GetLittleEndian(raw.left));

    if (line->right || line->left)
    {
      num_real_lines++;
    }

    line->self_ref = (line->left && line->right && (line->left->sector == line->right->sector));
  }
}

static inline uint16_t VanillaSegDist(const seg_t *seg)
{
  double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  // use the "true" starting coord (as stored in the wad)
  double sx = round(seg->start->x);
  double sy = round(seg->start->y);

  return (uint16_t)floor(hypot(sx - lx, sy - ly) + 0.5);
}

static inline short_angle_t VanillaSegAngle(const seg_t *seg)
{
  // compute the "true" delta
  double dx = round(seg->end->x) - round(seg->start->x);
  double dy = round(seg->end->y) - round(seg->start->y);

  double angle = ComputeAngle(dx, dy);

  if (angle < 0)
  {
    angle += 360.0;
  }

  short_angle_t result = short_angle_t(floor(angle * 65536.0 / 360.0 + 0.5));

  if (lev_format != MAPF_Doom)
  {
    return result;
  }

  // -Elf- ZokumBSP
  // 1080 => Additive degrees stored in tag
  // 1081 => Set to degrees stored in tag
  // 1082 => Additive BAM stored in tag
  // 1083 => Set to BAM stored in tag
  if (seg->linedef->special == Special_RotateDegrees)
  {
    result += DegreesToShortBAM(uint32_t(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateDegreesHard)
  {
    result = DegreesToShortBAM(uint32_t(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateAngleT)
  {
    result += short_angle_t(seg->linedef->tag);
  }
  else if (seg->linedef->special == Special_RotateAngleTHard)
  {
    result = short_angle_t(seg->linedef->tag);
  }

  return result;
}

/* ----- UDMF reading routines ------------------------- */

static constexpr std::uint32_t UDMF_THING = 1;
static constexpr std::uint32_t UDMF_VERTEX = 2;
static constexpr std::uint32_t UDMF_SECTOR = 3;
static constexpr std::uint32_t UDMF_SIDEDEF = 4;
static constexpr std::uint32_t UDMF_LINEDEF = 5;

void ParseThingField(thing_t *thing, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    thing->x = (int32_t)(LEX_Double(value) * 65536.0);
  }

  if (key == "y")
  {
    thing->y = (int32_t)(LEX_Double(value) * 65536.0);
  }

  if (key == "type")
  {
    thing->type = LEX_Int(value);
  }
}

void ParseVertexField(vertex_t *vertex, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    vertex->x = LEX_Double(value);
  }

  if (key == "y")
  {
    vertex->y = LEX_Double(value);
  }
}

void ParseSectorField(sector_t *sector, const std::string &key, token_kind_e kind, const std::string &value)
{
  // nothing actually needed
}

void ParseSidedefField(sidedef_t *side, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "sector")
  {
    size_t num = LEX_Index(value);

    if (num >= lev_sectors.size())
    {
      FatalError("illegal sector number #%d\n", (int)num);
    }

    side->sector = lev_sectors[num];
  }
}

void ParseLinedefField(linedef_t *line, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "v1")
  {
    line->start = SafeLookupVertex(LEX_Index(value));
  }

  if (key == "v2")
  {
    line->end = SafeLookupVertex(LEX_Index(value));
  }

  if (key == "special")
  {
    line->special = LEX_UInt(value);
  }

  if (key == "twosided")
  {
    line->two_sided = LEX_Boolean(value);
  }

  if (key == "sidefront")
  {
    size_t num = LEX_Index(value);

    if (num >= lev_sidedefs.size())
    {
      line->right = nullptr;
    }
    else
    {
      line->right = lev_sidedefs[num];
    }
  }

  if (key == "sideback")
  {
    size_t num = LEX_Index(value);

    if (num >= lev_sidedefs.size())
    {
      line->left = nullptr;
    }
    else
    {
      line->left = lev_sidedefs[num];
    }
  }
}

void ParseUDMF_Block(lexer_c &lex, int cur_type)
{
  vertex_t *vertex = nullptr;
  thing_t *thing = nullptr;
  sector_t *sector = nullptr;
  sidedef_t *side = nullptr;
  linedef_t *line = nullptr;

  switch (cur_type)
  {
    case UDMF_VERTEX:
      vertex = NewVertex();
      break;
    case UDMF_THING:
      thing = NewThing();
      break;
    case UDMF_SECTOR:
      sector = NewSector();
      break;
    case UDMF_SIDEDEF:
      side = NewSidedef();
      break;
    case UDMF_LINEDEF:
      line = NewLinedef();
      break;
    default:
      break;
  }

  for (;;)
  {
    if (lex.Match("}"))
    {
      break;
    }

    std::string key;
    std::string value;

    token_kind_e tok = lex.Next(key);

    if (tok == TOK_EOF)
    {
      FatalError("Malformed TEXTMAP lump: unclosed block\n");
    }

    if (tok != TOK_Ident)
    {
      FatalError("Malformed TEXTMAP lump: missing key\n");
    }

    if (!lex.Match("="))
    {
      FatalError("Malformed TEXTMAP lump: missing '='\n");
    }

    tok = lex.Next(value);

    if (tok == TOK_EOF || tok == TOK_ERROR || value == "}")
    {
      FatalError("Malformed TEXTMAP lump: missing value\n");
    }

    if (!lex.Match(";"))
    {
      FatalError("Malformed TEXTMAP lump: missing ';'\n");
    }

    switch (cur_type)
    {
      case UDMF_VERTEX:
        ParseVertexField(vertex, key, tok, value);
        break;
      case UDMF_THING:
        ParseThingField(thing, key, tok, value);
        break;
      case UDMF_SECTOR:
        ParseSectorField(sector, key, tok, value);
        break;
      case UDMF_SIDEDEF:
        ParseSidedefField(side, key, tok, value);
        break;
      case UDMF_LINEDEF:
        ParseLinedefField(line, key, tok, value);
        break;

      default: /* just skip it */
        break;
    }
  }

  // validate stuff

  if (line != nullptr)
  {
    if (line->start == nullptr || line->end == nullptr)
    {
      FatalError("Linedef #%zu is missing a vertex!\n", line->index);
    }

    if (line->right || line->left)
    {
      num_real_lines++;
    }

    line->self_ref = (line->left && line->right && (line->left->sector == line->right->sector));
  }
}

void ParseUDMF_Pass(const std::string &data, int pass)
{
  // pass = 1 : vertices, sectors, things
  // pass = 2 : sidedefs
  // pass = 3 : linedefs

  lexer_c lex(data);

  for (;;)
  {
    std::string section;
    token_kind_e tok = lex.Next(section);

    if (tok == TOK_EOF)
    {
      return;
    }

    if (tok != TOK_Ident)
    {
      FatalError("Malformed TEXTMAP lump.\n");
      return;
    }

    // ignore top-level assignments
    if (lex.Match("="))
    {
      lex.Next(section);
      if (!lex.Match(";"))
      {
        FatalError("Malformed TEXTMAP lump: missing ';'\n");
      }
      continue;
    }

    if (!lex.Match("{"))
    {
      FatalError("Malformed TEXTMAP lump: missing '{'\n");
    }

    int cur_type = 0;

    if (section == "thing")
    {
      if (pass == 1)
      {
        cur_type = UDMF_THING;
      }
    }
    else if (section == "vertex")
    {
      if (pass == 1)
      {
        cur_type = UDMF_VERTEX;
      }
    }
    else if (section == "sector")
    {
      if (pass == 1)
      {
        cur_type = UDMF_SECTOR;
      }
    }
    else if (section == "sidedef")
    {
      if (pass == 2)
      {
        cur_type = UDMF_SIDEDEF;
      }
    }
    else if (section == "linedef")
    {
      if (pass == 3)
      {
        cur_type = UDMF_LINEDEF;
      }
    }

    // process the block
    ParseUDMF_Block(lex, cur_type);
  }
}

void ParseUDMF(void)
{
  Lump_c *lump = FindLevelLump("TEXTMAP");

  if (lump == nullptr || !lump->Seek(0))
  {
    FatalError("Error finding TEXTMAP lump.\n");
  }

  size_t remain = lump->Length();

  // load the lump into this string
  std::string data;

  while (remain > 0)
  {
    char buffer[4096];

    size_t want = std::min(remain, sizeof(buffer));

    if (!lump->Read(buffer, want))
    {
      FatalError("Error reading TEXTMAP lump.\n");
    }

    data.append(buffer, want);

    remain -= want;
  }

  // now parse it...

  // the UDMF spec does not require objects to be in a dependency order.
  // for example: sidedefs may occur *after* the linedefs which refer to
  // them.  hence we perform multiple passes over the TEXTMAP data.
  ParseUDMF_Pass(data, 1);
  ParseUDMF_Pass(data, 2);
  ParseUDMF_Pass(data, 3);

  num_old_vert = lev_vertices.size();
}

/* ----- writing routines ------------------------------ */

void MarkOverflow(int flags)
{
  // flags are ignored
  lev_overflows = true;
}

void PutVertices(void)
{
  // this size is worst-case scenario
  size_t size = lev_vertices.size() * sizeof(raw_vertex_t);
  Lump_c *lump = CreateLevelLump("VERTEXES", size);

  size_t count = 0;
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    raw_vertex_t raw;

    const vertex_t *vert = lev_vertices[i];

    if (vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian((int16_t)floor(vert->x));
    raw.y = GetLittleEndian((int16_t)floor(vert->y));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  lump->Finish();

  if (count != num_old_vert)
  {
    FatalError("PutVertices miscounted (%zu != %zu)\n", count, num_old_vert);
  }

  if (count > 65534)
  {
    Failure("Number of vertices has overflowed.\n");
    MarkOverflow(LIMIT_VERTEXES);
  }
}

static inline uint16_t VertexIndex16Bit(const vertex_t *v)
{
  if (v->is_new)
  {
    return (uint16_t)(v->index | 0x8000U);
  }

  return (uint16_t)v->index;
}

static inline uint32_t VertexIndex_XNOD(const vertex_t *v)
{
  if (v->is_new)
  {
    return (uint32_t)(num_old_vert + v->index);
  }

  return (uint32_t)v->index;
}

void PutSegs(void)
{
  // this size is worst-case scenario
  size_t size = lev_segs.size() * sizeof(raw_seg_t);

  Lump_c *lump = CreateLevelLump("SEGS", size);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    raw_seg_t raw;

    const seg_t *seg = lev_segs[i];

    raw.start = GetLittleEndian(VertexIndex16Bit(seg->start));
    raw.end = GetLittleEndian(VertexIndex16Bit(seg->end));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian((uint16_t)seg->linedef->index);
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    // -Elf- ZokumBSP
    if ((seg->linedef->dont_render_back && seg->side) || (seg->linedef->dont_render_front && !seg->side))
    {
      raw = {};
    }

    lump->Write(&raw, sizeof(raw));

    if constexpr (DEBUG_BSP)
    {
      Debug("PUT SEG: %04X  Vert %04X->%04X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index,
                      GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef), seg->side ? "L" : "R",
                      GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();

  if (lev_segs.size() > 65534)
  {
    Failure("Number of segs has overflowed.\n");
    MarkOverflow(LIMIT_SEGS);
  }
}

void PutSubsecs(void)
{
  size_t size = lev_subsecs.size() * sizeof(raw_subsec_t);

  Lump_c *lump = CreateLevelLump("SSECTORS", size);

  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    raw_subsec_t raw;

    const subsec_t *sub = lev_subsecs[i];

    raw.first = GetLittleEndian((uint16_t)sub->seg_list->index);
    raw.num = GetLittleEndian((uint16_t)sub->seg_count);

    lump->Write(&raw, sizeof(raw));

    if constexpr (DEBUG_BSP)
    {
      Debug("PUT SUBSEC %04X  First %04X  Num %04X\n", sub->index, GetLittleEndian(raw.first),
                      GetLittleEndian(raw.num));
    }
  }

  if (lev_subsecs.size() > 32767)
  {
    Failure("Number of subsectors has overflowed.\n");
    MarkOverflow(LIMIT_SSECTORS);
  }

  lump->Finish();
}

static size_t node_cur_index;

static void PutOneNode(node_t *node, Lump_c *lump)
{
  if (node->r.node)
  {
    PutOneNode(node->r.node, lump);
  }

  if (node->l.node)
  {
    PutOneNode(node->l.node, lump);
  }

  node->index = node_cur_index++;

  raw_node_t raw;

  // note that x/y/dx/dy are always integral in non-UDMF maps
  raw.x = GetLittleEndian((int16_t)floor(node->x));
  raw.y = GetLittleEndian((int16_t)floor(node->y));
  raw.dx = GetLittleEndian((int16_t)floor(node->dx));
  raw.dy = GetLittleEndian((int16_t)floor(node->dy));

  raw.b1.minx = GetLittleEndian((int16_t)node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian((int16_t)node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian((int16_t)node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian((int16_t)node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian((int16_t)node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian((int16_t)node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian((int16_t)node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian((int16_t)node->l.bounds.maxy);

  if (node->r.node)
  {
    raw.right = GetLittleEndian((uint16_t)node->r.node->index);
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian((uint16_t)(node->r.subsec->index | 0x8000));
  }
  else
  {
    FatalError("Bad right child in node %zu\n", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian((uint16_t)node->l.node->index);
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian((uint16_t)(node->l.subsec->index | 0x8000));
  }
  else
  {
    FatalError("Bad left child in node %zu\n", node->index);
  }

  lump->Write(&raw, sizeof(raw));

  if constexpr (DEBUG_BSP)
  {
    Debug("PUT NODE %04X  Left %04X  Right %04X  (%d,%d) -> (%d,%d)\n", node->index, GetLittleEndian(raw.left),
                    GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

void PutNodes(node_t *root)
{
  size_t struct_size = sizeof(raw_node_t);

  // this can be bigger than the actual size, but never smaller
  size_t max_size = (lev_nodes.size() + 1) * struct_size;

  Lump_c *lump = CreateLevelLump("NODES", max_size);

  node_cur_index = 0;

  if (root != nullptr)
  {
    PutOneNode(root, lump);
  }

  lump->Finish();

  if (node_cur_index != lev_nodes.size())
  {
    FatalError("PutNodes miscounted (%zu != %zu)\n", node_cur_index, lev_nodes.size());
  }

  if (node_cur_index > 32767)
  {
    Failure("Number of nodes has overflowed.\n");
    MarkOverflow(LIMIT_NODES);
  }
}

void CheckLimits(void)
{
  // this could potentially be 65536, since there are no reserved values
  // for sectors, but there may be source ports or tools treating 0xFFFF
  // as a special value, so we are extra cautious here (and in some of
  // the other checks below, like the vertex counts).
  if (lev_sectors.size() > 65535)
  {
    Failure("Map has too many sectors.\n");
    MarkOverflow(LIMIT_SECTORS);
  }

  // the sidedef 0xFFFF is reserved to mean "no side" in DOOM map format
  if (lev_sidedefs.size() > 65535)
  {
    Failure("Map has too many sidedefs.\n");
    MarkOverflow(LIMIT_SIDEDEFS);
  }

  // the linedef 0xFFFF is reserved for minisegs in GL nodes
  if (lev_linedefs.size() > 65535)
  {
    Failure("Map has too many linedefs.\n");
    MarkOverflow(LIMIT_LINEDEFS);
  }

  if (!(cur_info->force_xnod || cur_info->ssect_xgl3))
  {
    if (num_old_vert > 32767 || num_new_vert > 32767 || lev_segs.size() > 32767 || lev_nodes.size() > 32767)
    {
      Warning("Forcing XNOD format nodes due to overflows.\n");
      lev_force_xnod = true;
    }
  }
}

struct Compare_seg_pred
{
  inline bool operator()(const seg_t *A, const seg_t *B) const
  {
    return A->index < B->index;
  }
};

void SortSegs(void)
{
  // sort segs into ascending index
  std::sort(lev_segs.begin(), lev_segs.end(), Compare_seg_pred());

  // remove unwanted segs
  while (lev_segs.size() > 0 && lev_segs.back()->index == SEG_IS_GARBAGE)
  {
    UtilFree(lev_segs.back());
    lev_segs.pop_back();
  }
}

/* ----- ZDoom format writing --------------------------- */

void PutZVertices(Lump_c *lump)
{
  size_t orgverts = GetLittleEndian(num_old_vert);
  size_t newverts = GetLittleEndian(num_new_vert);

  lump->Write(&orgverts, 4);
  lump->Write(&newverts, 4);

  size_t count = 0;
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    raw_zdoom_vertex_t raw;

    const vertex_t *vert = lev_vertices[i];

    if (!vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian((int32_t)floor(vert->x * 65536.0));
    raw.y = GetLittleEndian((int32_t)floor(vert->y * 65536.0));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  if (count != num_new_vert)
  {
    FatalError("PutZVertices miscounted (%zu != %zu)\n", count, num_new_vert);
  }
}

void PutZSubsecs(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_subsecs.size());
  lump->Write(&raw_num, 4);

  size_t cur_seg_index = 0;
  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    const subsec_t *sub = lev_subsecs[i];

    raw_num = GetLittleEndian(sub->seg_count);
    lump->Write(&raw_num, 4);

    // sanity check the seg index values
    size_t count = 0;
    for (const seg_t *seg = sub->seg_list; seg; seg = seg->next, cur_seg_index++)
    {
      if (cur_seg_index != seg->index)
      {
        FatalError("PutZSubsecs: seg index mismatch in sub %zu (%zu != %zu)\n", i, cur_seg_index, seg->index);
      }

      count++;
    }

    if (count != sub->seg_count)
    {
      FatalError("PutZSubsecs: miscounted segs in sub %zu (%zu != %zu)\n", i, count, sub->seg_count);
    }
  }

  if (cur_seg_index != lev_segs.size())
  {
    FatalError("PutZSubsecs miscounted segs (%zu != %zu)\n", cur_seg_index, lev_segs.size());
  }
}

void PutZSegs(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      FatalError("PutZSegs: seg index mismatch (%zu != %zu)\n", seg->index, i);
    }

    raw_zdoom_seg_t raw = {};

    raw.start = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.end = GetLittleEndian(VertexIndex_XNOD(seg->end));
    raw.linedef = GetLittleEndian((uint16_t)seg->linedef->index);
    raw.side = seg->side;

    // -Elf- ZokumBSP
    if ((seg->linedef->dont_render_back && seg->side) || (seg->linedef->dont_render_front && !seg->side))
    {
      raw = {};
    }

    lump->Write(&raw, sizeof(raw));
  }
}

void PutXGL3Segs(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      FatalError("PutXGL3Segs: seg index mismatch (%zu != %zu)\n", seg->index, i);
    }

    raw_xgl2_seg_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.partner = GetLittleEndian((uint16_t)(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian((uint16_t)(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->Write(&raw, sizeof(raw));

    if constexpr (DEBUG_BSP)
    {
      fprintf(stderr, "SEG[%zu] v1=%d partner=%d line=%d side=%d\n", i, raw.vertex, raw.partner, raw.linedef, raw.side);
    }
  }
}

static void PutOneZNode(Lump_c *lump, node_t *node, bool xgl3)
{
  raw_zdoom_node_t raw;

  if (node->r.node)
  {
    PutOneZNode(lump, node->r.node, xgl3);
  }

  if (node->l.node)
  {
    PutOneZNode(lump, node->l.node, xgl3);
  }

  node->index = node_cur_index++;

  if (xgl3)
  {
    int32_t x = GetLittleEndian((int32_t)floor(node->x * 65536.0));
    int32_t y = GetLittleEndian((int32_t)floor(node->y * 65536.0));
    int32_t dx = GetLittleEndian((int32_t)floor(node->dx * 65536.0));
    int32_t dy = GetLittleEndian((int32_t)floor(node->dy * 65536.0));

    lump->Write(&x, 4);
    lump->Write(&y, 4);
    lump->Write(&dx, 4);
    lump->Write(&dy, 4);
  }
  else
  {
    raw.x = GetLittleEndian((int16_t)floor(node->x));
    raw.y = GetLittleEndian((int16_t)floor(node->y));
    raw.dx = GetLittleEndian((int16_t)floor(node->dx));
    raw.dy = GetLittleEndian((int16_t)floor(node->dy));

    lump->Write(&raw.x, 2);
    lump->Write(&raw.y, 2);
    lump->Write(&raw.dx, 2);
    lump->Write(&raw.dy, 2);
  }

  raw.b1.minx = GetLittleEndian((int16_t)node->r.bounds.minx);
  raw.b1.miny = GetLittleEndian((int16_t)node->r.bounds.miny);
  raw.b1.maxx = GetLittleEndian((int16_t)node->r.bounds.maxx);
  raw.b1.maxy = GetLittleEndian((int16_t)node->r.bounds.maxy);

  raw.b2.minx = GetLittleEndian((int16_t)node->l.bounds.minx);
  raw.b2.miny = GetLittleEndian((int16_t)node->l.bounds.miny);
  raw.b2.maxx = GetLittleEndian((int16_t)node->l.bounds.maxx);
  raw.b2.maxy = GetLittleEndian((int16_t)node->l.bounds.maxy);

  lump->Write(&raw.b1, sizeof(raw.b1));
  lump->Write(&raw.b2, sizeof(raw.b2));

  if (node->r.node)
  {
    raw.right = GetLittleEndian((uint32_t)(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian((uint32_t)(node->r.subsec->index | 0x80000000U));
  }
  else
  {
    FatalError("Bad right child in ZDoom node %zu\n", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian((uint32_t)(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian((uint32_t)(node->l.subsec->index | 0x80000000U));
  }
  else
  {
    FatalError("Bad left child in ZDoom node %zu\n", node->index);
  }

  lump->Write(&raw.right, 4);
  lump->Write(&raw.left, 4);

  if constexpr (DEBUG_BSP)
  {
    Debug("PUT Z NODE %08X  Left %08X  Right %08X  (%d,%d) -> (%d,%d)\n", node->index, GetLittleEndian(raw.left),
                    GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

void PutZNodes(Lump_c *lump, node_t *root, bool xgl3)
{
  size_t raw_num = GetLittleEndian(lev_nodes.size());
  lump->Write(&raw_num, 4);

  node_cur_index = 0;

  if (root)
  {
    PutOneZNode(lump, root, xgl3);
  }

  if (node_cur_index != lev_nodes.size())
  {
    FatalError("PutZNodes miscounted (%zu != %zu)\n", node_cur_index, lev_nodes.size());
  }
}

static size_t CalcZDoomNodesSize(void)
{
  // compute size of the ZDoom format nodes.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  size_t size = 32; // header + a bit extra

  size += 8 + lev_vertices.size() * 8;
  size += 4 + lev_subsecs.size() * 4;
  size += 4 + lev_segs.size() * 11;
  size += 4 + lev_nodes.size() * sizeof(raw_zdoom_node_t);

  return size;
}

void SaveZDFormat(node_t *root_node)
{
  SortSegs();

  size_t max_size = CalcZDoomNodesSize();

  Lump_c *lump = CreateLevelLump("NODES", max_size);

  lump->Write(XNOD_MAGIC, 4);

  PutZVertices(lump);
  PutZSubsecs(lump);
  PutZSegs(lump);
  PutZNodes(lump, root_node, false);

  lump->Finish();
  lump = nullptr;
}

void SaveXGL3Format(Lump_c *lump, node_t *root_node)
{
  SortSegs();

  // WISH : compute a max_size

  lump->Write(XGL3_MAGIC, 4);

  PutZVertices(lump);
  PutZSubsecs(lump);
  PutXGL3Segs(lump);
  PutZNodes(lump, root_node, true);

  lump->Finish();
  lump = nullptr;
}

/* ----- whole-level routines --------------------------- */

void LoadLevel(void)
{
  Lump_c *LEV = cur_wad->GetLump(lev_current_start);

  lev_current_name = LEV->Name();
  lev_long_name = false;
  lev_overflows = false;

  cur_info->ShowMap(lev_current_name);

  num_new_vert = 0;
  num_real_lines = 0;

  if (lev_format == MAPF_UDMF)
  {
    ParseUDMF();
  }
  else
  {
    GetVertices();
    GetSectors();
    GetSidedefs();

    if (lev_format == MAPF_Hexen)
    {
      GetLinedefsHexen();
      GetThingsHexen();
    }
    else
    {
      GetLinedefs();
      GetThings();
    }

    // always prune vertices at end of lump, otherwise all the
    // unused vertices from seg splits would keep accumulating.
    PruneVerticesAtEnd();
  }

  cur_info->Print_Verbose("    Loaded %d vertices, %d sectors, %d sides, %d lines, %d things\n", lev_vertices.size(),
                          lev_sectors.size(), lev_sidedefs.size(), lev_linedefs.size(), lev_things.size());

  DetectOverlappingVertices();
  DetectOverlappingLines();

  CalculateWallTips();

  // -JL- Find sectors containing polyobjs
  switch (lev_format)
  {
    case MAPF_Hexen:
      DetectPolyobjSectors(false);
      break;
    case MAPF_UDMF:
      DetectPolyobjSectors(true);
      break;
    default:
      break;
  }
}

void FreeLevel(void)
{
  FreeVertices();
  FreeSidedefs();
  FreeLinedefs();
  FreeSectors();
  FreeThings();
  FreeSegs();
  FreeSubsecs();
  FreeNodes();
  FreeWallTips();
  FreeIntersections();
}

static void AddMissingLump(const char *name, const char *after)
{
  if (cur_wad->LevelLookupLump(lev_current_idx, name) != NO_INDEX)
  {
    return;
  }

  size_t exist = cur_wad->LevelLookupLump(lev_current_idx, after);

  // if this happens, the level structure is very broken
  if (exist != NO_INDEX)
  {
    Warning("Missing %s lump -- level structure is broken\n", after);

    exist = cur_wad->LevelLastLump(lev_current_idx);
  }

  cur_wad->InsertPoint(exist + 1);

  cur_wad->AddLump(name)->Finish();
}

build_result_e SaveLevel(node_t *root_node)
{
  // Note: root_node may be nullptr

  cur_wad->BeginWrite();

  // ensure all necessary level lumps are present
  AddMissingLump("SEGS", "VERTEXES");
  AddMissingLump("SSECTORS", "SEGS");
  AddMissingLump("NODES", "SSECTORS");
  AddMissingLump("REJECT", "SECTORS");
  AddMissingLump("BLOCKMAP", "REJECT");

  // user preferences
  lev_force_xnod = cur_info->force_xnod;

  // check for overflows...
  // this sets the force_xxx vars if certain limits are breached
  CheckLimits();

  /* --- Normal nodes --- */
  if ((lev_force_xnod || cur_info->ssect_xgl3) && num_real_lines > 0)
  {
    // leave SEGS empty
    CreateLevelLump("SEGS")->Finish();

    if (cur_info->ssect_xgl3)
    {
      Lump_c *lump = CreateLevelLump("SSECTORS");
      SaveXGL3Format(lump, root_node);
    }
    else
    {
      CreateLevelLump("SSECTORS")->Finish();
    }

    if (lev_force_xnod)
    {
      // remove all the mini-segs from subsectors
      NormaliseBspTree();

      SaveZDFormat(root_node);
    }
    else
    {
      CreateLevelLump("NODES")->Finish();
    }
  }
  else
  {
    // remove all the mini-segs from subsectors
    NormaliseBspTree();

    // reduce vertex precision for classic DOOM nodes.
    // some segs can become "degenerate" after this, and these
    // are removed from subsectors.
    RoundOffBspTree();

    SortSegs();

    PutVertices();

    PutSegs();
    PutSubsecs();
    PutNodes(root_node);
  }

  PutBlockmap();
  PutReject();

  cur_wad->EndWrite();

  if (lev_overflows)
  {
    // no message here
    // [ in verbose mode, each overflow already printed a message ]
    // [ in normal mode, we don't want any messages at all ]
    return BUILD_LumpOverflow;
  }

  return BUILD_OK;
}

build_result_e SaveUDMF(node_t *root_node)
{
  cur_wad->BeginWrite();

  // remove any existing ZNODES lump
  cur_wad->RemoveZNodes(lev_current_idx);

  Lump_c *lump = CreateLevelLump("ZNODES", NO_INDEX);

  // -Elf- Ensure needed lumps exist
  AddMissingLump("REJECT", "ZNODES");
  AddMissingLump("BLOCKMAP", "REJECT");

  if (num_real_lines == 0)
  {
    lump->Finish();
  }
  else
  {
    SaveXGL3Format(lump, root_node);
  }

  // -Elf-
  PutBlockmap();
  PutReject();

  cur_wad->EndWrite();

  return BUILD_OK;
}

/* ---------------------------------------------------------------- */

Lump_c *FindLevelLump(const char *name)
{
  size_t idx = cur_wad->LevelLookupLump(lev_current_idx, name);
  return (idx != NO_INDEX) ? cur_wad->GetLump(idx) : nullptr;
}

Lump_c *CreateLevelLump(const char *name, size_t max_size)
{
  // look for existing one
  Lump_c *lump = FindLevelLump(name);

  if (lump)
  {
    cur_wad->RecreateLump(lump, max_size);
  }
  else
  {
    size_t last_idx = cur_wad->LevelLastLump(lev_current_idx);

    // in UDMF maps, insert before the ENDMAP lump, otherwise insert
    // after the last known lump of the level.
    if (lev_format != MAPF_UDMF)
    {
      last_idx += 1;
    }

    cur_wad->InsertPoint(last_idx);

    lump = cur_wad->AddLump(name, max_size);
  }

  return lump;
}

//------------------------------------------------------------------------
// MAIN STUFF
//------------------------------------------------------------------------

buildinfo_t *cur_info = nullptr;

void SetInfo(buildinfo_t *info)
{
  cur_info = info;
}

void OpenWad(const char *filename)
{
  cur_wad = Wad_file::Open(filename, 'a');
  if (cur_wad == nullptr)
  {
    FatalError("Cannot open file: %s\n", filename);
  }

  if (cur_wad->IsReadOnly())
  {
    delete cur_wad;
    cur_wad = nullptr;
    FatalError("file is read only: %s\n", filename);
  }
}

void CloseWad(void)
{
  if (cur_wad != nullptr)
  {
    // this closes the file
    delete cur_wad;
    cur_wad = nullptr;
  }
}

size_t LevelsInWad(void)
{
  if (cur_wad == nullptr)
  {
    return 0;
  }

  return cur_wad->LevelCount();
}

const char *GetLevelName(size_t lev_idx)
{
  SYS_ASSERT(cur_wad != nullptr);

  size_t lump_idx = cur_wad->LevelHeader(lev_idx);

  return cur_wad->GetLump(lump_idx)->Name();
}

/* ----- build nodes for a single level ----- */

build_result_e BuildLevel(size_t lev_idx)
{
  if (cur_info->cancelled)
  {
    return BUILD_Cancelled;
  }

  node_t *root_node = nullptr;
  subsec_t *root_sub = nullptr;

  lev_current_idx = lev_idx;
  lev_current_start = cur_wad->LevelHeader(lev_idx);
  lev_format = cur_wad->LevelFormat(lev_idx);

  LoadLevel();

  InitBlockmap();

  build_result_e ret = BUILD_OK;

  if (num_real_lines > 0)
  {
    bbox_t dummy;

    // create initial segs
    seg_t *list = CreateSegs();

    // recursively create nodes
    ret = BuildNodes(list, 0, &dummy, &root_node, &root_sub);
  }

  if (ret == BUILD_OK)
  {
    cur_info->Print_Verbose("    Built %d NODES, %d SSECTORS, %d SEGS, %d VERTEXES\n", lev_nodes.size(), lev_subsecs.size(),
                            lev_segs.size(), num_old_vert + num_new_vert);

    if (root_node != nullptr)
    {
      cur_info->Print_Verbose("    Heights of subtrees: %d / %d\n", ComputeBspHeight(root_node->r.node),
                              ComputeBspHeight(root_node->l.node));
    }

    ClockwiseBspTree();

    switch (lev_format)
    {
      case MAPF_Doom:
      case MAPF_Hexen:
        ret = SaveLevel(root_node);
        break;
      case MAPF_UDMF:
        ret = SaveUDMF(root_node);
        break;
      default:
        break;
    }
  }
  else
  {
    /* build was Cancelled by the user */
  }

  FreeLevel();

  return ret;
}
