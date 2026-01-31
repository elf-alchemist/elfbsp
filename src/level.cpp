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

#include <algorithm>
#include <utility>
#include <vector>

static constexpr std::uint32_t DUMMY_DUP = 0xFFFF;

Wad_file *cur_wad;

static int block_x, block_y;
static size_t block_w, block_h;
static size_t block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static uint16_t **block_lines;

static uint16_t *block_ptrs;
static uint16_t *block_dups;

static int32_t block_compression;
static bool block_overflowed;

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

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymax - y1) / static_cast<double>(y2 - y1));
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

      x1 = x1
           + static_cast<int32_t>(static_cast<double>(x2 - x1) * static_cast<double>(ymin - y1) / static_cast<double>(y2 - y1));
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

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmax - x1) / static_cast<double>(x2 - x1));
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

      y1 = y1
           + static_cast<int32_t>(static_cast<double>(y2 - y1) * static_cast<double>(xmin - x1) / static_cast<double>(x2 - x1));
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

static constexpr uint32_t BK_NUM = 0;
static constexpr uint32_t BK_MAX = 1;
static constexpr uint32_t BK_XOR = 2;
static constexpr uint32_t BK_FIRST = 3;
static constexpr uint32_t BK_QUANTUM = 32;

static void BlockAdd(size_t blk_num, size_t line_index)
{
  uint16_t *cur = block_lines[blk_num];

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Block %zu has line %zu", __func__, blk_num, line_index);
  }

  if (blk_num >= block_count)
  {
    PrintLine(LOG_ERROR, "BlockAdd: bad block number %zu", blk_num);
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

    block_lines[blk_num] = cur = UtilRealloc(cur, cur[BK_MAX] * sizeof(uint16_t));
  }

  // compute new checksum
  cur[BK_XOR] = static_cast<uint16_t>(cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12);
  cur[BK_XOR] ^= static_cast<uint16_t>(line_index & 0xFFFF);

  cur[BK_FIRST + cur[BK_NUM]] = GetLittleEndian(static_cast<uint16_t>(line_index));
  cur[BK_NUM]++;
}

static void BlockAddLine(const linedef_t *L)
{
  int32_t x1 = static_cast<int32_t>(L->start->x);
  int32_t y1 = static_cast<int32_t>(L->start->y);
  int32_t x2 = static_cast<int32_t>(L->end->x);
  int32_t y2 = static_cast<int32_t>(L->end->y);

  size_t line_index = L->index;

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu (%d,%d) -> (%d,%d)", __func__, line_index, x1, y1, x2, y2);
  }

  int32_t bx1_temp = (std::min(x1, x2) - block_x) / 128;
  int32_t by1_temp = (std::min(y1, y2) - block_y) / 128;
  int32_t bx2_temp = (std::max(x1, x2) - block_x) / 128;
  int32_t by2_temp = (std::max(y1, y2) - block_y) / 128;

  // handle truncated blockmaps
  size_t bx1 = static_cast<size_t>(std::max(bx1_temp, 0));
  size_t by1 = static_cast<size_t>(std::max(by1_temp, 0));
  size_t bx2 = static_cast<size_t>(std::min(bx2_temp, static_cast<int32_t>(block_w - 1)));
  size_t by2 = static_cast<size_t>(std::min(by2_temp, static_cast<int32_t>(block_h - 1)));

  if (bx2 < bx1 || by2 < by1)
  {
    return;
  }

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = static_cast<size_t>(by1 * block_w + bx);
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (size_t by = by1; by <= by2; by++)
    {
      size_t blk_num = static_cast<size_t>(by * block_w + bx1);
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (size_t by = by1; by <= by2; by++)
  {
    for (size_t bx = bx1; bx <= bx2; bx++)
    {
      size_t blk_num = static_cast<size_t>(bx + by * block_w);

      int32_t minx = block_x + 128 * static_cast<int32_t>(bx);
      int32_t miny = block_y + 128 * static_cast<int32_t>(by);
      int32_t maxx = minx + 127;
      int32_t maxy = miny + 127;

      if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
      {
        BlockAdd(blk_num, line_index);
      }
    }
  }
}

static void CreateBlockmap(void)
{
  block_lines = UtilCalloc<uint16_t *>(block_count * sizeof(uint16_t *));

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    // ignore zero-length lines
    if (L->zero_len)
    {
      continue;
    }

    if (L->no_blockmap)
    {
      continue;
    }

    BlockAddLine(L);
  }
}

static int BlockCompare(const void *p1, const void *p2)
{
  int blk_num1 = (static_cast<const uint16_t *>(p1))[0];
  int blk_num2 = (static_cast<const uint16_t *>(p2))[0];

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
    block_dups[i] = static_cast<uint16_t>(i);
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
      block_ptrs[blk_num] = static_cast<uint16_t>(4 + block_count);
      block_dups[i] = DUMMY_DUP;

      orig_size += 2;
      continue;
    }

    size_t count = 2 + block_lines[blk_num][BK_NUM];

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.
    if (i + 1 < block_count && BlockCompare(block_dups + i, block_dups + i + 1) == 0)
    {
      block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
      block_dups[i] = DUMMY_DUP;

      // free the memory of the duplicated block
      UtilFree(block_lines[blk_num]);
      block_lines[blk_num] = nullptr;

      if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
      {
        dup_count++;
      }

      orig_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.
    block_ptrs[blk_num] = static_cast<uint16_t>(cur_offset);
    cur_offset += count;
    orig_size += count;
    new_size += count;
  }

  if (cur_offset > 65535)
  {
    block_overflowed = true;
    return;
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Last ptr = %zu  duplicates = %zu", __func__, cur_offset, dup_count);
  }

  block_compression = static_cast<int32_t>((static_cast<int32_t>(orig_size) - static_cast<int32_t>(new_size)) * 100
                                           / static_cast<int32_t>(orig_size));

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

    size += static_cast<size_t>(((blk[BK_NUM]) + 1 + 1) * 2);
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

  header.x_origin = GetLittleEndian(static_cast<int16_t>(block_x));
  header.y_origin = GetLittleEndian(static_cast<int16_t>(block_y));
  header.x_blocks = GetLittleEndian(static_cast<int16_t>(block_w));
  header.y_blocks = GetLittleEndian(static_cast<int16_t>(block_h));

  lump->Write(&header, sizeof(header));

  // handle pointers
  for (size_t i = 0; i < block_count; i++)
  {
    uint16_t ptr = GetLittleEndian(block_ptrs[i]);

    if (ptr == 0)
    {
      PrintLine(LOG_ERROR, "WriteBlockmap: offset %zu not set.", i);
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
  double mid_x = 0;
  double mid_y = 0;

  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    const linedef_t *L = lev_linedefs[i];

    if (L->no_blockmap)
    {
      continue;
    }

    if (!L->zero_len)
    {
      double x1 = L->start->x;
      double y1 = L->start->y;
      double x2 = L->end->x;
      double y2 = L->end->y;

      int32_t lx = static_cast<int32_t>(floor(std::min(x1, x2)));
      int32_t ly = static_cast<int32_t>(floor(std::min(y1, y2)));
      int32_t hx = static_cast<int32_t>(ceil(std::max(x1, x2)));
      int32_t hy = static_cast<int32_t>(ceil(std::max(y1, y2)));

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
    block_mid_x = static_cast<int32_t>(floor(mid_x / static_cast<double>(lev_linedefs.size())));
    block_mid_y = static_cast<int32_t>(floor(mid_y / static_cast<double>(lev_linedefs.size())));
  }

  if (HAS_BIT(config.debug, DEBUG_BLOCKMAP))
  {
    PrintLine(LOG_DEBUG, "[%s] Blockmap lines centered at (%d,%d)", __func__, block_mid_x, block_mid_y);
  }
}

static void InitBlockmap(void)
{
  bbox_t map_bbox;

  // find limits of linedefs, and store as map limits
  FindBlockmapLimits(&map_bbox);

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Map limits: (%d,%d) to (%d,%d)", map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);
  }

  block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  block_w = static_cast<size_t>((map_bbox.maxx - block_x) / 128) + 1;
  block_h = static_cast<size_t>((map_bbox.maxy - block_y) / 128) + 1;

  block_count = block_w * block_h;
}

static void PutBlockmap(void)
{
  if (lev_linedefs.size() == 0)
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
    PrintLine(LOG_NORMAL, "WARNING: Blockmap overflowed (lump will be empty)");
    config.total_warnings++;
  }
  else
  {
    WriteBlockmap();
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Blockmap size: %zux%zu (compression: %d%%)", block_w, block_h, block_compression);
    }
  }

  FreeBlockmap();
}

//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------

static uint8_t *rej_matrix;
static size_t rej_total_size; // in bytes
static std::vector<size_t> rej_sector_groups;

//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init(void)
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

static void Reject_Free(void)
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

static void Reject_ProcessSectors(void)
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
static void PutReject(void)
{
  if (lev_sectors.size() == 0)
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
  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Reject size: %zu", rej_total_size);
  }
}

//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------

// Note: ZDoom format support based on code (C) 2002,2003 Marisa "Randi" Heit

// per-level variables

size_t lev_current_idx;
size_t lev_current_start;

map_format_e lev_format;

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

static void FreeVertices(void)
{
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    UtilFree(lev_vertices[i]);
  }

  lev_vertices.clear();
}

static void FreeLinedefs(void)
{
  for (size_t i = 0; i < lev_linedefs.size(); i++)
  {
    UtilFree(lev_linedefs[i]);
  }

  lev_linedefs.clear();
}

static void FreeSidedefs(void)
{
  for (size_t i = 0; i < lev_sidedefs.size(); i++)
  {
    UtilFree(lev_sidedefs[i]);
  }

  lev_sidedefs.clear();
}

static void FreeSectors(void)
{
  for (size_t i = 0; i < lev_sectors.size(); i++)
  {
    UtilFree(lev_sectors[i]);
  }

  lev_sectors.clear();
}

static void FreeThings(void)
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

static void FreeWallTips(void)
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
    PrintLine(LOG_ERROR, "illegal vertex number #%zu", num);
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
    PrintLine(LOG_ERROR, "illegal sector number #%zu", static_cast<size_t>(num));
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

static void GetVertices(void)
{
  size_t count = 0;

  Lump_c *lump = FindLevelLump("VERTEXES");

  if (lump)
  {
    count = lump->Length() / sizeof(raw_vertex_t);
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  if (lump == nullptr || count == 0)
  {
    return;
  }

  if (!lump->Seek(0))
  {
    PrintLine(LOG_ERROR, "Error seeking to vertices.");
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_vertex_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading vertices.");
    }

    vertex_t *vert = NewVertex();

    vert->x = static_cast<double>(GetLittleEndian(raw.x));
    vert->y = static_cast<double>(GetLittleEndian(raw.y));
  }

  num_old_vert = lev_vertices.size();
}

static void GetSectors(void)
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
    PrintLine(LOG_ERROR, "Error seeking to sectors.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sector_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading sectors.");
    }

    sector_t *sector = NewSector();

    sector->height_floor = static_cast<double>(GetLittleEndian(raw.floorh));
    sector->height_ceiling = static_cast<double>(GetLittleEndian(raw.ceilh));
  }
}

static void GetThings(void)
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
    PrintLine(LOG_ERROR, "Error seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_thing_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading things.");
    }

    thing_t *thing = NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = GetLittleEndian(raw.type);
  }
}

static void GetThingsHexen(void)
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
    PrintLine(LOG_ERROR, "Error seeking to things.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_hexen_thing_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading things.");
    }

    thing_t *thing = NewThing();

    thing->x = GetLittleEndian(raw.x);
    thing->y = GetLittleEndian(raw.y);
    thing->type = GetLittleEndian(raw.type);
  }
}

static void GetSidedefs(void)
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
    PrintLine(LOG_ERROR, "Error seeking to sidedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_sidedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading sidedefs.");
    }

    sidedef_t *side = NewSidedef();

    side->sector = SafeLookupSector(GetLittleEndian(raw.sector));
  }
}

static void GetLinedefs(void)
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
    PrintLine(LOG_ERROR, "Error seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_linedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading linedefs.");
    }

    linedef_t *line;
    size_t start_id = GetLittleEndian(raw.start);
    size_t end_id = GetLittleEndian(raw.end);

    vertex_t *start = SafeLookupVertex(start_id);
    vertex_t *end = SafeLookupVertex(end_id);

    start->is_used = true;
    end->is_used = true;

    line = NewLinedef();

    line->start = start;
    line->end = end;
    line->start_id = start_id;
    line->end_id = end_id;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && (fabs(start->y - end->y) < DIST_EPSILON);

    line->special = GetLittleEndian(raw.special);
    line->tag = GetLittleEndian(raw.tag);
    line->flags = GetLittleEndian(raw.flags);

    line->two_sided = (line->flags & MLF_TWOSIDED) != 0;
    line->is_precious = (line->tag >= 900 && line->tag < 1000);

    line->dont_render = (line->tag == Tag_DoNotRender);

    line->no_blockmap = (line->tag == Tag_NoBlockmap);

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

static void GetLinedefsHexen(void)
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
    PrintLine(LOG_ERROR, "Error seeking to linedefs.");
  }

  if (HAS_BIT(config.debug, DEBUG_LOAD))
  {
    PrintLine(LOG_DEBUG, "[%s] num = %zu", __func__, count);
  }

  for (size_t i = 0; i < count; i++)
  {
    raw_hexen_linedef_t raw;

    if (!lump->Read(&raw, sizeof(raw)))
    {
      PrintLine(LOG_ERROR, "Error reading linedefs.");
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

    line->special = static_cast<uint8_t>(raw.special);
    uint16_t flags = GetLittleEndian(raw.flags);

    // -JL- Added missing twosided flag handling that caused a broken reject
    line->two_sided = (flags & MLF_TWOSIDED) != 0;

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

  return static_cast<uint16_t>(floor(hypot(sx - lx, sy - ly) + 0.5));
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

  if (lev_format != MapFormat_Doom)
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
    result += DegreesToShortBAM(static_cast<uint16_t>(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateDegreesHard)
  {
    result = DegreesToShortBAM(static_cast<uint16_t>(seg->linedef->tag));
  }
  else if (seg->linedef->special == Special_RotateAngleT)
  {
    result += static_cast<short_angle_t>(seg->linedef->tag);
  }
  else if (seg->linedef->special == Special_RotateAngleTHard)
  {
    result = static_cast<short_angle_t>(seg->linedef->tag);
  }

  return result;
}

/* ----- UDMF reading routines ------------------------- */

static constexpr uint32_t UDMF_THING = 1;
static constexpr uint32_t UDMF_VERTEX = 2;
static constexpr uint32_t UDMF_SECTOR = 3;
static constexpr uint32_t UDMF_SIDEDEF = 4;
static constexpr uint32_t UDMF_LINEDEF = 5;

static void ParseThingField(thing_t *thing, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "x")
  {
    thing->x = LEX_Double(value);
  }

  if (key == "y")
  {
    thing->y = LEX_Double(value);
  }

  if (key == "type")
  {
    thing->type = LEX_Int16(value);
  }
}

static void ParseVertexField(vertex_t *vertex, const std::string &key, token_kind_e kind, const std::string &value)
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

static void ParseSectorField(sector_t *sector, const std::string &key, token_kind_e kind, const std::string &value)
{
  // nothing actually needed
}

static void ParseSidedefField(sidedef_t *side, const std::string &key, token_kind_e kind, const std::string &value)
{
  if (key == "sector")
  {
    size_t num = LEX_Index(value);

    if (num >= lev_sectors.size())
    {
      PrintLine(LOG_ERROR, "illegal sector number #%zu", static_cast<size_t>(num));
    }

    side->sector = lev_sectors[num];
  }
}

static void ParseLinedefField(linedef_t *line, const std::string &key, token_kind_e kind, const std::string &value)
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

static void ParseUDMF_Block(lexer_c &lex, int cur_type)
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
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: unclosed block");
    }

    if (tok != TOK_Ident)
    {
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing key");
    }

    if (!lex.Match("="))
    {
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing '='");
    }

    tok = lex.Next(value);

    if (tok == TOK_EOF || tok == TOK_ERROR || value == "}")
    {
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing value");
    }

    if (!lex.Match(";"))
    {
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing ';'");
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
      PrintLine(LOG_ERROR, "Linedef #%zu is missing a vertex!", line->index);
    }

    if (line->right || line->left)
    {
      num_real_lines++;
    }

    line->self_ref = (line->left && line->right && (line->left->sector == line->right->sector));
  }
}

static void ParseUDMF_Pass(const std::string &data, int pass)
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
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump.");
      return;
    }

    // ignore top-level assignments
    if (lex.Match("="))
    {
      lex.Next(section);
      if (!lex.Match(";"))
      {
        PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing ';'");
      }
      continue;
    }

    if (!lex.Match("{"))
    {
      PrintLine(LOG_ERROR, "Malformed TEXTMAP lump: missing '{'");
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
    PrintLine(LOG_ERROR, "Error finding TEXTMAP lump.");
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
      PrintLine(LOG_ERROR, "Error reading TEXTMAP lump.");
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

static void MarkOverflow(int flags)
{
  // flags are ignored
  lev_overflows = true;
}

static void PutVertices(void)
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

    raw.x = GetLittleEndian(static_cast<int16_t>(floor(vert->x)));
    raw.y = GetLittleEndian(static_cast<int16_t>(floor(vert->y)));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  lump->Finish();

  if (count != num_old_vert)
  {
    PrintLine(LOG_ERROR, "PutVertices miscounted (%zu != %zu)", count, num_old_vert);
  }

  if (count > 65534)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of vertices has overflowed.");
    MarkOverflow(LIMIT_VERTEXES);
  }
}

static inline uint16_t VertexIndex16Bit(const vertex_t *v)
{
  if (v->is_new)
  {
    return static_cast<uint16_t>(v->index | 0x8000U);
  }

  return static_cast<uint16_t>(v->index);
}

static inline uint32_t VertexIndex_XNOD(const vertex_t *v)
{
  if (v->is_new)
  {
    return static_cast<uint32_t>(num_old_vert + v->index);
  }

  return static_cast<uint32_t>(v->index);
}

static void PutSegs_Vanilla(void)
{
  // this size is worst-case scenario
  size_t size = lev_segs.size() * sizeof(raw_seg_vanilla_t);

  Lump_c *lump = CreateLevelLump("SEGS", size);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    raw_seg_vanilla_t raw;

    const seg_t *seg = lev_segs[i];

    raw.start = GetLittleEndian(VertexIndex16Bit(seg->start));
    raw.end = GetLittleEndian(VertexIndex16Bit(seg->end));
    raw.angle = GetLittleEndian(VanillaSegAngle(seg));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef->index));
    raw.flip = GetLittleEndian(seg->side);
    raw.dist = GetLittleEndian(VanillaSegDist(seg));

    // -Elf- ZokumBSP
    if ((seg->linedef->dont_render_back && seg->side) || (seg->linedef->dont_render_front && !seg->side))
    {
      raw = {};
    }

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  Vert %04X->%04X  Line %04X %s  Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__,
                seg->index, GetLittleEndian(raw.start), GetLittleEndian(raw.end), GetLittleEndian(raw.linedef),
                seg->side ? "L" : "R", GetLittleEndian(raw.angle), seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    }
  }

  lump->Finish();

  if (lev_segs.size() > 65534)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of segs has overflowed.");
    MarkOverflow(LIMIT_SEGS);
  }
}

static void PutSubsecs_Vanilla(void)
{
  size_t size = lev_subsecs.size() * sizeof(raw_subsec_vanilla_t);

  Lump_c *lump = CreateLevelLump("SSECTORS", size);

  for (size_t i = 0; i < lev_subsecs.size(); i++)
  {
    raw_subsec_vanilla_t raw;

    const subsec_t *sub = lev_subsecs[i];

    raw.first = GetLittleEndian(static_cast<uint16_t>(sub->seg_list->index));
    raw.num = GetLittleEndian(static_cast<uint16_t>(sub->seg_count));

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] %zu  First %04X  Num %04X", __func__, sub->index, GetLittleEndian(raw.first),
                GetLittleEndian(raw.num));
    }
  }

  if (lev_subsecs.size() > 32767)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of subsectors has overflowed.");
    MarkOverflow(LIMIT_SSECTORS);
  }

  lump->Finish();
}

static size_t node_cur_index;

static void PutOneNode_Vanilla(node_t *node, Lump_c *lump)
{
  if (node->r.node)
  {
    PutOneNode_Vanilla(node->r.node, lump);
  }

  if (node->l.node)
  {
    PutOneNode_Vanilla(node->l.node, lump);
  }

  node->index = node_cur_index++;

  raw_node_vanilla_t raw;

  // note that x/y/dx/dy are always integral in non-UDMF maps
  raw.x = GetLittleEndian(static_cast<int16_t>(floor(node->x)));
  raw.y = GetLittleEndian(static_cast<int16_t>(floor(node->y)));
  raw.dx = GetLittleEndian(static_cast<int16_t>(floor(node->dx)));
  raw.dy = GetLittleEndian(static_cast<int16_t>(floor(node->dy)));

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint16_t>(node->r.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint16_t>(node->l.subsec->index | 0x8000));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in node %zu", node->index);
  }

  lump->Write(&raw, sizeof(raw));

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %04X  Right %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Vanilla(node_t *root)
{
  // this can be bigger than the actual size, but never smaller
  size_t max_size = (lev_nodes.size() + 1) * sizeof(raw_node_vanilla_t);

  Lump_c *lump = CreateLevelLump("NODES", max_size);

  node_cur_index = 0;

  if (root != nullptr)
  {
    PutOneNode_Vanilla(root, lump);
  }

  lump->Finish();

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }

  if (node_cur_index > 32767)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Number of nodes has overflowed.");
    MarkOverflow(LIMIT_NODES);
  }
}

static void CheckBinaryFormatLimits(void)
{
  // this could potentially be 65536, since there are no reserved values
  // for sectors, but there may be source ports or tools treating 0xFFFF
  // as a special value, so we are extra cautious here (and in some of
  // the other checks below, like the vertex counts).
  if (lev_sectors.size() > 65535)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sectors.");
    MarkOverflow(LIMIT_SECTORS);
  }

  // the sidedef 0xFFFF is reserved to mean "no side" in DOOM map format
  if (lev_sidedefs.size() > 65535)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many sidedefs.");
    MarkOverflow(LIMIT_SIDEDEFS);
  }

  // the linedef 0xFFFF is reserved for minisegs in GL nodes
  if (lev_linedefs.size() > 65535)
  {
    PrintLine(LOG_NORMAL, "FAILURE: Map has too many linedefs.");
    MarkOverflow(LIMIT_LINEDEFS);
  }

  if (config.bsp_type < BSP_XNOD)
  {
    if (num_old_vert > 32767 || num_new_vert > 32767 || lev_segs.size() > 32767 || lev_nodes.size() > 32767)
    {
      PrintLine(LOG_NORMAL, "WARNING: Forcing XNOD format nodes due to overflows.");
      config.total_warnings++;
      config.bsp_type = BSP_XNOD;
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

static void PutVertices_Xnod(Lump_c *lump)
{
  size_t orgverts = GetLittleEndian(num_old_vert);
  size_t newverts = GetLittleEndian(num_new_vert);

  lump->Write(&orgverts, 4);
  lump->Write(&newverts, 4);

  size_t count = 0;
  for (size_t i = 0; i < lev_vertices.size(); i++)
  {
    raw_xnod_vertex_t raw;

    const vertex_t *vert = lev_vertices[i];

    if (!vert->is_new)
    {
      continue;
    }

    raw.x = GetLittleEndian(static_cast<int32_t>(floor(vert->x * 65536.0)));
    raw.y = GetLittleEndian(static_cast<int32_t>(floor(vert->y * 65536.0)));

    lump->Write(&raw, sizeof(raw));

    count++;
  }

  if (count != num_new_vert)
  {
    PrintLine(LOG_ERROR, "PutZVertices miscounted (%zu != %zu)", count, num_new_vert);
  }
}

static void PutSubsecs_Xnod(Lump_c *lump)
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
        PrintLine(LOG_ERROR, "PutZSubsecs: seg index mismatch in sub %zu (%zu != %zu)", i, cur_seg_index, seg->index);
      }

      count++;
    }

    if (count != sub->seg_count)
    {
      PrintLine(LOG_ERROR, "PutZSubsecs: miscounted segs in sub %zu (%zu != %zu)", i, count, sub->seg_count);
    }
  }

  if (cur_seg_index != lev_segs.size())
  {
    PrintLine(LOG_ERROR, "PutZSubsecs miscounted segs (%zu != %zu)", cur_seg_index, lev_segs.size());
  }
}

static void PutSegs_Xnod(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "PutZSegs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_xnod_seg_t raw = {};

    raw.start = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.end = GetLittleEndian(VertexIndex_XNOD(seg->end));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef->index));
    raw.side = seg->side;
    lump->Write(&raw, sizeof(raw));
  }
}

static void PutSegs_Xgl2(Lump_c *lump)
{
  size_t raw_num = GetLittleEndian(lev_segs.size());
  lump->Write(&raw_num, 4);

  for (size_t i = 0; i < lev_segs.size(); i++)
  {
    const seg_t *seg = lev_segs[i];

    if (seg->index != i)
    {
      PrintLine(LOG_ERROR, "PutXGL3Segs: seg index mismatch (%zu != %zu)", seg->index, i);
    }

    raw_xgl2_seg_t raw = {};

    raw.vertex = GetLittleEndian(VertexIndex_XNOD(seg->start));
    raw.partner = GetLittleEndian(static_cast<uint16_t>(seg->partner ? seg->partner->index : NO_INDEX));
    raw.linedef = GetLittleEndian(static_cast<uint16_t>(seg->linedef ? seg->linedef->index : NO_INDEX));
    raw.side = seg->side;

    lump->Write(&raw, sizeof(raw));

    if (HAS_BIT(config.debug, DEBUG_BSP))
    {
      PrintLine(LOG_DEBUG, "[%s] SEG[%zu] v1=%d partner=%d line=%d side=%d", __func__, i, raw.vertex, raw.partner, raw.linedef,
                raw.side);
    }
  }
}

static void PutOneNode_Xnod(Lump_c *lump, node_t *node, bool xgl3)
{
  raw_xgl3_node_t raw;

  if (node->r.node)
  {
    PutOneNode_Xnod(lump, node->r.node, xgl3);
  }

  if (node->l.node)
  {
    PutOneNode_Xnod(lump, node->l.node, xgl3);
  }

  node->index = node_cur_index++;

  if (xgl3)
  {
    int32_t x = GetLittleEndian(static_cast<int32_t>(floor(node->x * 65536.0)));
    int32_t y = GetLittleEndian(static_cast<int32_t>(floor(node->y * 65536.0)));
    int32_t dx = GetLittleEndian(static_cast<int32_t>(floor(node->dx * 65536.0)));
    int32_t dy = GetLittleEndian(static_cast<int32_t>(floor(node->dy * 65536.0)));

    lump->Write(&x, 4);
    lump->Write(&y, 4);
    lump->Write(&dx, 4);
    lump->Write(&dy, 4);
  }
  else
  {
    raw.x = GetLittleEndian(static_cast<int16_t>(floor(node->x)));
    raw.y = GetLittleEndian(static_cast<int16_t>(floor(node->y)));
    raw.dx = GetLittleEndian(static_cast<int16_t>(floor(node->dx)));
    raw.dy = GetLittleEndian(static_cast<int16_t>(floor(node->dy)));

    lump->Write(&raw.x, 2);
    lump->Write(&raw.y, 2);
    lump->Write(&raw.dx, 2);
    lump->Write(&raw.dy, 2);
  }

  raw.b1.minx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.minx));
  raw.b1.miny = GetLittleEndian(static_cast<int16_t>(node->r.bounds.miny));
  raw.b1.maxx = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxx));
  raw.b1.maxy = GetLittleEndian(static_cast<int16_t>(node->r.bounds.maxy));

  raw.b2.minx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.minx));
  raw.b2.miny = GetLittleEndian(static_cast<int16_t>(node->l.bounds.miny));
  raw.b2.maxx = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxx));
  raw.b2.maxy = GetLittleEndian(static_cast<int16_t>(node->l.bounds.maxy));

  lump->Write(&raw.b1, sizeof(raw.b1));
  lump->Write(&raw.b2, sizeof(raw.b2));

  if (node->r.node)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.node->index));
  }
  else if (node->r.subsec)
  {
    raw.right = GetLittleEndian(static_cast<uint32_t>(node->r.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad right child in ZDoom node %zu", node->index);
  }

  if (node->l.node)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.node->index));
  }
  else if (node->l.subsec)
  {
    raw.left = GetLittleEndian(static_cast<uint32_t>(node->l.subsec->index | 0x80000000U));
  }
  else
  {
    PrintLine(LOG_ERROR, "Bad left child in ZDoom node %zu", node->index);
  }

  lump->Write(&raw.right, 4);
  lump->Write(&raw.left, 4);

  if (HAS_BIT(config.debug, DEBUG_BSP))
  {
    PrintLine(LOG_DEBUG, "[%s] %zu  Left %08X  Right %08X  (%f,%f) -> (%f,%f)", __func__, node->index,
              GetLittleEndian(raw.left), GetLittleEndian(raw.right), node->x, node->y, node->x + node->dx, node->y + node->dy);
  }
}

static void PutNodes_Xnod(Lump_c *lump, node_t *root)
{
  size_t raw_num = GetLittleEndian(lev_nodes.size());
  lump->Write(&raw_num, 4);

  node_cur_index = 0;

  if (root)
  {
    PutOneNode_Xnod(lump, root, false);
  }

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutZNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }
}

static void PutNodes_Xgl3(Lump_c *lump, node_t *root)
{
  size_t raw_num = GetLittleEndian(lev_nodes.size());
  lump->Write(&raw_num, 4);

  node_cur_index = 0;

  if (root)
  {
    PutOneNode_Xnod(lump, root, true);
  }

  if (node_cur_index != lev_nodes.size())
  {
    PrintLine(LOG_ERROR, "PutZNodes miscounted (%zu != %zu)", node_cur_index, lev_nodes.size());
  }
}

static size_t CalcXnodNodesSize(void)
{
  // compute size of the ZDoom format nodes.
  // it does not need to be exact, but it *does* need to be bigger
  // (or equal) to the actual size of the lump.

  size_t size = 32; // header + a bit extra

  size += 8 + lev_vertices.size() * sizeof(raw_xnod_vertex_t);
  size += 4 + lev_subsecs.size() * sizeof(raw_xnod_subsec_t);
  size += 4 + lev_segs.size() * sizeof(raw_xnod_seg_t);
  size += 4 + lev_nodes.size() * sizeof(raw_xnod_node_t);

  return size;
}

void SaveFormat_Xnod(Lump_c *lump, node_t *root_node)
{
  SortSegs();

  lump->Write(BSP_MAGIC_XNOD, 4);

  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xnod(lump);
  PutNodes_Xnod(lump, root_node);

  lump->Finish();
  lump = nullptr;
}

static void SaveFormat_Xgl3(Lump_c *lump, node_t *root_node)
{
  SortSegs();

  // WISH : compute a max_size

  lump->Write(BSP_MAGIC_XGL3, 4);

  PutVertices_Xnod(lump);
  PutSubsecs_Xnod(lump);
  PutSegs_Xgl2(lump);
  PutNodes_Xgl3(lump, root_node);

  lump->Finish();
  lump = nullptr;
}

/* ----- whole-level routines --------------------------- */

void LoadLevel(void)
{
  Lump_c *LEV = cur_wad->GetLump(lev_current_start);

  lev_overflows = false;

  PrintLine(LOG_NORMAL, "%s", LEV->Name());

  num_new_vert = 0;
  num_real_lines = 0;

  if (lev_format == MapFormat_UDMF)
  {
    ParseUDMF();
  }
  else
  {
    GetVertices();
    GetSectors();
    GetSidedefs();

    if (lev_format == MapFormat_Hexen)
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

  if (config.verbose)
  {
    PrintLine(LOG_NORMAL, "Loaded %zu vertices, %zu sectors, %zu sides, %zu lines, %zu things", lev_vertices.size(),
              lev_sectors.size(), lev_sidedefs.size(), lev_linedefs.size(), lev_things.size());
  }

  DetectOverlappingVertices();
  DetectOverlappingLines();

  CalculateWallTips();

  // -JL- Find sectors containing polyobjs
  switch (lev_format)
  {
  case MapFormat_Hexen:
    DetectPolyobjSectors(false);
    break;
  case MapFormat_UDMF:
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
  if (exist == NO_INDEX)
  {
    PrintLine(LOG_NORMAL, "WARNING: Missing %s lump -- level structure is broken", after);
    config.total_warnings++;

    exist = cur_wad->LevelLastLump(lev_current_idx);
  }

  cur_wad->InsertPoint(exist + 1);

  cur_wad->AddLump(name)->Finish();
}

build_result_e SaveBinaryFormatLevel(node_t *root_node)
{
  // Note: root_node may be nullptr

  cur_wad->BeginWrite();

  // ensure all necessary level lumps are present
  AddMissingLump("SEGS", "VERTEXES");
  AddMissingLump("SSECTORS", "SEGS");
  AddMissingLump("NODES", "SSECTORS");
  AddMissingLump("SECTORS", "NODES");
  AddMissingLump("REJECT", "SECTORS");
  AddMissingLump("BLOCKMAP", "REJECT");

  // check for overflows...
  // this sets the force_xxx vars if certain limits are breached
  CheckBinaryFormatLimits();

  /* --- Normal nodes --- */
  if (config.bsp_type == BSP_XGL3 && num_real_lines > 0)
  {
    // leave SEGS empty
    CreateLevelLump("SEGS")->Finish();
    Lump_c *lump = CreateLevelLump("SSECTORS");
    SaveFormat_Xgl3(lump, root_node);
    CreateLevelLump("NODES")->Finish();
  }
  else if (config.bsp_type == BSP_XNOD && num_real_lines > 0)
  {
    CreateLevelLump("SEGS")->Finish();
    CreateLevelLump("SSECTORS")->Finish();

    // remove all the mini-segs from subsectors
    NormaliseBspTree();

    Lump_c *lump = CreateLevelLump("NODES", CalcXnodNodesSize());
    SaveFormat_Xnod(lump, root_node);
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

    PutSegs_Vanilla();
    PutSubsecs_Vanilla();
    PutNodes_Vanilla(root_node);
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

build_result_e SaveTextMapLevel(node_t *root_node)
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
    SaveFormat_Xgl3(lump, root_node);
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
    if (lev_format != MapFormat_UDMF)
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

void OpenWad(const char *filename)
{
  cur_wad = Wad_file::Open(filename, 'a');
  if (cur_wad == nullptr)
  {
    PrintLine(LOG_ERROR, "Cannot open file: %s", filename);
  }

  if (cur_wad->IsReadOnly())
  {
    delete cur_wad;
    cur_wad = nullptr;
    PrintLine(LOG_ERROR, "file is read only: %s", filename);
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

build_result_e BuildLevel(size_t lev_idx, const char *filename)
{
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
    if (config.analysis)
    {
      // normal mode, across all default costs
      for (double split_cost = 1.0; split_cost <= 32.0; split_cost++)
      {
        bbox_t dummy;
        root_node = nullptr;
        root_sub = nullptr;
        BuildNodes(CreateSegs(), 0, &dummy, &root_node, &root_sub, split_cost, false, true);
        AnalysisPushLine(lev_current_idx, false, split_cost, lev_segs.size(), lev_subsecs.size(), lev_nodes.size(),
                         ComputeBspHeight(root_node->l.node), ComputeBspHeight(root_node->r.node));
        FreeNodes();
        FreeSubsecs();
        FreeSegs();
      }
      // fast mode, also across all default costs
      for (double split_cost = 1.0; split_cost <= 32.0; split_cost++)
      {
        bbox_t dummy;
        root_node = nullptr;
        root_sub = nullptr;
        BuildNodes(CreateSegs(), 0, &dummy, &root_node, &root_sub, split_cost, true, true);
        AnalysisPushLine(lev_current_idx, true, split_cost, lev_segs.size(), lev_subsecs.size(), lev_nodes.size(),
                         ComputeBspHeight(root_node->l.node), ComputeBspHeight(root_node->r.node));
        FreeNodes();
        FreeSubsecs();
        FreeSegs();
      }
    }

    bbox_t dummy;
    // recursively create nodes
    ret = BuildNodes(CreateSegs(), 0, &dummy, &root_node, &root_sub, config.split_cost, config.fast, false);
  }

  if (ret == BUILD_OK)
  {
    if (config.verbose)
    {
      PrintLine(LOG_NORMAL, "Built %zu NODES, %zu SSECTORS, %zu SEGS, %zu VERTEXES", lev_nodes.size(), lev_subsecs.size(),
                lev_segs.size(), num_old_vert + num_new_vert);
    }

    if (root_node != nullptr)
    {
      if (config.verbose)
      {
        PrintLine(LOG_NORMAL, "Heights of subtrees: %d / %d", ComputeBspHeight(root_node->r.node),
                  ComputeBspHeight(root_node->l.node));
      }
    }

    ClockwiseBspTree();

    switch (lev_format)
    {
    case MapFormat_Doom:
    case MapFormat_Hexen:
      ret = SaveBinaryFormatLevel(root_node);
      break;
    case MapFormat_UDMF:
      ret = SaveTextMapLevel(root_node);
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


  if (config.analysis)
  {
    WriteAnalysis(filename);
  }

  return ret;
}
